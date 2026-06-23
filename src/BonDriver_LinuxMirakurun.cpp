// SPDX-License-Identifier: MIT
//
// BonDriver for mirakurun
//

#include "BonDriver_LinuxMirakurun.hpp"
#include "runtime_error.hpp"
#include "strutil.hpp"

#include <dlfcn.h>
#include <memory>
#include <stdint.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <future>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <plog/Formatters/FuncMessageFormatter.h>
#include <plog/Initializers/ConsoleInitializer.h>
#include <plog/Log.h>

namespace BonDriver_LinuxMirakurun
{

BonDriver::BonDriver(config::Config &config)
{
	try {
		// INIファイルの読み込み
		// [BonDriver_LinuxMirakurun]
		auto sct = config.Get("BonDriver_LinuxMirakurun");
		auto loglevel = sct.GetInt("LogLevel", 4);
		if (!plog::get()) {
			plog::init<plog::FuncMessageFormatter>(static_cast<plog::Severity>(loglevel), plog::streamStdErr);
			PLOGD << "plog init = " << plog::get()->getInstance();
		} else {
			plog::get()->setMaxSeverity(static_cast<plog::Severity>(loglevel));
		}

		auto name = sct.GetStr("Name", "LinuxMirakurun");
		name_ = char_conv_.Convert<std::u16string>(name);
		server_address_ = sct.Get("ServerAddress");
		http_verbose_log_ = sct.GetBool("HttpVerboseLog", false);
		auto channel_list_timeout = sct.GetIntMinMax("ChannelListTimeout", 500, 0, 5000);
		session_timeout_ = sct.GetIntMinMax("SessionTimeout", 1000, 0, 5000);
		response_timeout_ = sct.GetIntMinMax("ResponseTimeout", 2000, 0, 5000);
		auto stream_buffer_size =
			sct.GetUIntMinMax("StreamBufferSize", 4 * 1024 * 1024, 2 * 1024 * 1024, 20 * 1024 * 1024);
		auto ts_packets_per_chunk = sct.GetIntMinMax("TsPacketsPerChunk", 2048, 100, 30000);
		auto max_channels = sct.GetIntMinMax("MaximumNumberOfChannels", 300, 300, 1000);

		stream_buffer_ = std::make_unique<StreamBuffer>(stream_buffer_size);
		stream_chunk_.resize(ts_packets_per_chunk * TS_PACKET_SIZE);

		PLOGD << "LogLevel = " << loglevel;
		PLOGD << "Name = " << name;
		PLOGD << "ServerAddress = " << server_address_;
		PLOGD << "HttpVerboseLog = " << http_verbose_log_;
		PLOGD << "ChannelListTimeout = " << channel_list_timeout;
		PLOGD << "SessionTimout = " << session_timeout_;
		PLOGD << "ResponseTimeout = " << response_timeout_;
		PLOGD << "StreamBufferSize = " << stream_buffer_size;
		PLOGD << "TsPacketsPerChunk = " << ts_packets_per_chunk;
		PLOGD << "MaximumNumberOfChannels = " << max_channels;

		// 接続プロトコルを ServerAddress の文字列から判定する
		DetermineProtocol(server_address_);

		// iniファイルで定義されたチャンネル設定を読み込む
		auto spaces = strutil::Split(config.Get("Space").Get("Space"));
		for (auto v : spaces) {
			v = "Space." + v;
			PLOGD << v;
			auto subspace_sct = config.Get(v);
			auto &space = spaces_.emplace_back(char_conv_, subspace_sct);
			if (space.GetUseManualChannelList()) {
				auto subspace_ch_sct = config.Get(v + ".Channel");
				space.AddManualChannelList(char_conv_, subspace_ch_sct, max_channels);
			}
		}

		// チャンネル設定を Mirakurun から取得
		for (auto &sp : spaces_) {
			auto type = sp.GetType();
			PLOGD << "Space = " << sp.GetNameUtf8();
			PLOGD << "Type = " << sp.GetType();
			if (sp.GetUseManualChannelList()) {
				PLOGD << "UseUseManualChannelList = true next space";
				continue;
			}

			// チャンネルリストを取得
			// GET /api/channels?type=GR
			auto session = MakeSession(channel_list_timeout);
			auto url = cpr::Url(std::format("{}/api/channels", server_address_));
			auto params = cpr::Parameters({{"type", type}});
			session->SetUrl(url);
			session->SetParameters(params);
			auto response = session->Get();
			PLOGD << "GET " << response.url;

			if (response.error) {
				throw RuntimeError(std::format("GET {} : {}", response.url.str(), response.error.message));
			} else if (response.status_code != 200) {
				PLOGW << "GET " << response.url << " status code = " << response.status_code;
				continue;
			}

			try {
				auto json = nlohmann::json::parse(response.text);
				sp.AddAutoChannelList(char_conv_, json, type);
			} catch (const std::exception &e) {
				PLOGW << "failed to add auto channel list : " << e.what();
			}
		}
	} catch (const std::exception &e) {
		PLOGE << e.what();
		throw;
	}
}

BonDriver::~BonDriver()
{
	CloseTuner();
}

bool BonDriver::OpenTuner(void)
{
	PLOGV << __func__;

	if (is_open_tuner_) {
		return true;
	}

	is_open_tuner_ = false;

	try {
		// /api/version で接続可否を確認
		auto session = MakeSession(session_timeout_);
		auto url = cpr::Url{std::format("{}/api/version", server_address_)};
		auto params = cpr::Parameters{};
		auto timeout = cpr::Timeout{session_timeout_};
		session->SetUrl(url);
		session->SetParameters(params);
		session->SetTimeout(timeout);
		auto response = session->Get();
		PLOGD << "GET " << response.url;

		if (response.error) {
			throw RuntimeError(std::format("GET {} : {}", response.url.str(), response.error.message));
		} else if (response.status_code != 200) {
			throw RuntimeError(std::format("GET {} : status code = {}", response.url.str(), response.status_code));
		}

		version_ = nlohmann::json::parse(response.text);
		is_open_tuner_ = true;
	} catch (const std::exception &e) {
		PLOGE << e.what();
	}

	return is_open_tuner_;
}

void BonDriver::CloseTuner(void)
{
	PLOGV << __func__;

	if (!is_open_tuner_) {
		return;
	}

	// TS転送を停止
	if (is_streaming_ && streaming_response_.valid()) {
		is_streaming_ = false;
		streaming_response_.wait_for(std::chrono::milliseconds(response_timeout_));
		PLOGD << "stop streaming";
	}

	current_space_.store(0, std::memory_order_release);
	current_channel_.store(0, std::memory_order_release);

	is_open_tuner_ = false;
}

bool BonDriver::SetChannel(const uint8_t bCh)
{
	return SetChannel(0, bCh);
}

float BonDriver::GetSignalLevel(void)
{
	return bitrate_mbps_;
}

uint32_t BonDriver::WaitTsStream(const uint32_t /*dwTimeOut*/)
{
	if (!is_open_tuner_) {
		return WAIT_ABANDONED;
	} else if (!stream_buffer_->UsedSpace()) {
		return WAIT_OBJECT_0;
	} else {
		return WAIT_TIMEOUT;
	}
}

uint32_t BonDriver::GetReadyCount(void)
{
	return stream_buffer_->UsedSpace() ? 1 : 0;
}

bool BonDriver::GetTsStream(uint8_t *pDst, uint32_t *pdwSize, uint32_t *pdwRemain)
{
	if (!pDst || !pdwSize) {
		return false;
	}

	uint8_t *psrc = nullptr;
	if (GetTsStream(&psrc, pdwSize, pdwRemain)) {
		if (*pdwSize > 0) {
			std::copy(psrc, psrc + *pdwSize, pDst);
		}
		return true;
	}

	return false;
}

bool BonDriver::GetTsStream(uint8_t **ppDst, uint32_t *pdwSize, uint32_t *pdwRemain)
{
	if (!ppDst || !pdwSize) {
		return false;
	}

	*ppDst = nullptr;
	*pdwSize = 0;

	auto size = stream_buffer_->Read(stream_chunk_.data(), stream_chunk_.size());
	if (size > 0) {
		*ppDst = stream_chunk_.data();
		*pdwSize = static_cast<uint32_t>(size);
	}

	if (pdwRemain) {
		*pdwRemain = static_cast<uint32_t>(stream_buffer_->UsedSpace());
	}

	return true;
}

void BonDriver::PurgeTsStream(void)
{
	PLOGV << __func__;

	stream_buffer_->Clear();
}

void BonDriver::Release(void)
{
	PLOGV << __func__;
	DestroyInstance();
}

const char16_t *BonDriver::GetTunerName(void)
{
	return name_.c_str();
}

bool BonDriver::IsTunerOpening(void)
{
	return is_open_tuner_;
}

const char16_t *BonDriver::EnumTuningSpace(const uint32_t dwSpace)
{
	try {
		return spaces_.at(dwSpace).GetName().c_str();
	} catch (const std::out_of_range &) {
		return nullptr;
	}
}

const char16_t *BonDriver::EnumChannelName(const uint32_t dwSpace, const uint32_t dwChannel)
{
	try {
		return spaces_.at(dwSpace).GetChannel(dwChannel).GetName().c_str();
	} catch (const std::out_of_range &) {
		return nullptr;
	}
}

bool BonDriver::SetChannel(const uint32_t dwSpace, const uint32_t dwChannel)
{
	PLOGV << __func__;
	PLOGD << "dwSpace = " << dwSpace << " dwChannel = " << dwChannel;

	try {
		auto &sp = spaces_.at(dwSpace);
		auto type = sp.GetType();
		auto channel = sp.GetChannel(dwChannel).GetChannel();
		auto priority = std::format("{}", sp.GetPriority());
		auto b25decode = std::format("{}", sp.GetB25Decode() ? 1 : 0);

		auto url = cpr::Url(std::format("{}/api/channels/{}/{}/stream", server_address_, type, channel));
		auto params = cpr::Parameters({{"X-Mirakurun-Priority", priority}, {"decode", b25decode}});
		auto callback = cpr::WriteCallback(
			[this](std::string_view data, intptr_t userdata) { return StreamHandler(data, userdata); });

		// TSスレッド停止
		if (is_streaming_ && streaming_response_.valid()) {
			is_streaming_ = false;
			streaming_response_.wait_for(std::chrono::milliseconds(response_timeout_));
			streaming_session_.reset();
			PLOGD << "stop streaming";
		}

		// HEAD で URL チェック
		streaming_session_ = MakeSession(session_timeout_);
		streaming_session_->SetUrl(url);
		streaming_session_->SetParameters(params);
		streaming_session_->SetWriteCallback(cpr::WriteCallback{});
		streaming_session_->SetTimeout(cpr::Timeout{session_timeout_});
		auto response = streaming_session_->Head();
		PLOGD << "HEAD " << response.url;

		if (response.error) {
			throw RuntimeError(std::format("HEAD {} : {}", response.url.str(), response.error.message));
		} else if (response.status_code != 200) {
			throw RuntimeError(std::format("HEAD {} : status code = {}", response.url.str(), response.status_code));
		}

		// TSスレッド開始
		PLOGD << "start streming";
		PLOGD << "GET " << url.str() << '?' << params.GetContent();
		stream_buffer_->Clear();
		is_streaming_ = true;
		streaming_session_->SetUrl(url);
		streaming_session_->SetParameters(params);
		streaming_session_->SetWriteCallback(callback);
		streaming_session_->SetTimeout(cpr::Timeout{STREAM_TIMEOUT});
		streaming_response_ = streaming_session_->GetAsync();

		// ビットレート用変数を初期化
		bitrate_size_ = 0;
		bitrate_mbps_ = 0.0f;
		bitrate_last_time_ = std::chrono::steady_clock::now();

		current_space_.store(dwSpace, std::memory_order_release);
		current_channel_.store(dwChannel, std::memory_order_release);
	} catch (const std::exception &e) {
		PLOGE << e.what();
		is_streaming_ = false;
		return false;
	}

	return true;
}

uint32_t BonDriver::GetCurSpace(void)
{
	return current_space_.load(std::memory_order_acquire);
}

uint32_t BonDriver::GetCurChannel(void)
{
	return current_channel_.load(std::memory_order_acquire);
}

BonDriver::Space::Channel::Channel(strutil::CharConv &cv, const std::string &name, const std::string &channel)
	: name_u8_(name), name_(cv.Convert<std::u16string>(name)), channel_(channel)
{
}

BonDriver::Space::Space(strutil::CharConv &cv, config::Config::Section &sct)
{
	name_u8_ = sct.Get("Name");
	name_ = cv.Convert<std::u16string>(name_u8_);
	type_ = sct.Get("Type");
	use_manual_channel_list_ = sct.GetBool("UseManualChannelList", false);
	b25_decode_ = sct.GetBool("B25Decode", false);
	priority_ = sct.GetIntMinMax("Priority", 0, 0, 1000);
}

void BonDriver::Space::AddManualChannelList(strutil::CharConv &cv, config::Config::Section &sct, int32_t max_channels)
{
	for (auto i = 0; i < max_channels; i++) {
		std::string str;

		auto ch = std::format("Ch{}", i);
		try {
			str = sct.GetStr(ch, "");
			if (str.empty()) {
				PLOGD << ch << " is undefined and exit";
				break;
			}

			auto data = strutil::Split(str);
			if (data.size() != 2) {
				PLOGD << ch << '=' << str << " data size mismatch and exit";
				break;
			}

			channel_.emplace_back(cv, data[0], data[1]);
		} catch (const std::exception &e) {
			PLOGE << e.what();
			break;
		}
	}
}

void BonDriver::Space::AddAutoChannelList(strutil::CharConv &cv, nlohmann::json &json, const std::string &type)
{
	for (const auto &j : json) {
		try {
			auto t = j.at("type").get<std::string>();
			if (t == type) {
				PLOGD << "type = " << j.at("type") << " name = " << j.at("name") << " channel = " << j.at("channel");
				channel_.emplace_back(cv, j.at("name"), j.at("channel"));
			}
		} catch (const std::exception &e) {
			PLOGE << e.what();
			break;
		}
	}
}

void BonDriver::DetermineProtocol(std::string_view address)
{
	auto protocol = Protocol::UNKNOWN;

	// Prefix で判断
	if (address.starts_with("http://") || address.starts_with("https://")) {
		protocol = Protocol::HTTP;
	} else if (address.starts_with("unix://")) {
		// Unix domain Socket の場合
		// 先頭の unix:// ７文字を削除したものがUnixソケットのパス
		// cpr ライブラリで必要なダミーの http://localhost に変更

		protocol = Protocol::UNIX_DOMAIN_SOCKET;
		PLOGD << "use unix domain socket: " << address;
		socket_path_ = server_address_.substr(7);
		server_address_ = "http://localhost";
	} else {
		throw RuntimeError(std::format("invalid protocol : {}", address));
	}

	connection_protocol_ = protocol;
}

std::unique_ptr<cpr::Session> BonDriver::MakeSession(int32_t timeout)
{
	auto session = std::make_unique<cpr::Session>();

	// Unix domain Socket の場合
	if (connection_protocol_ == Protocol::UNIX_DOMAIN_SOCKET) {
		session->SetUnixSocket(cpr::UnixSocket{socket_path_});
	}

	// ログ設定
	session->SetVerbose(cpr::Verbose{http_verbose_log_});

	// timeout
	session->SetTimeout(cpr::Timeout{timeout});

	return session;
}

bool BonDriver::StreamHandler(std::string_view data, intptr_t)
{
#ifdef READ_COUNT
	PLOGV << "count = " << data.size();
#endif

	if (!is_streaming_) {
		return false;
	}

	// TSをバッファに追加
	size_t lost_data = 0;
	stream_buffer_->Write(reinterpret_cast<const uint8_t *>(data.data()), data.size(), lost_data);
	if (lost_data > 0) {
		PLOGW << "Stream buffer overflow : " << lost_data << " byte lost";
	}

	// TSの転送サイズからビットレートを算出
	bitrate_size_ += data.size();
	auto now = std::chrono::steady_clock::now();
	auto dt = now - bitrate_last_time_;
	if (dt > bitrate_interval_) {
		auto sec = std::chrono::duration<double>(dt).count();
		auto current_size = bitrate_size_.exchange(0);
		bitrate_mbps_ = (current_size * 8.0) / (1024.0 * 1024.0 * sec);
		bitrate_last_time_ = now;
	}

	return true;
}

std::mutex BonDriver::instance_mtx_;
BonDriver *BonDriver::instance_ = nullptr;

BonDriver *BonDriver::GetInstance()
{
	std::lock_guard<std::mutex> lock(instance_mtx_);

	if (!instance_) {
		try {
			Dl_info dli;
			if (!::dladdr(reinterpret_cast<void *>(GetInstance), &dli)) {
				return nullptr;
			}

			std::filesystem::path p = dli.dli_fname;
			if (p.stem().empty() || p.extension() != ".so") {
				return nullptr;
			}

			// 以下の順で読み込む
			// (1)BonDriver_LinuxMirakurun.ini
			// (2)BonDriver_LinuxMirakurun.so.ini
			config::Config config;
			if (!config.Load(p.replace_extension(".ini"))) {
				if (!config.Load(p.replace_extension(".so.ini"))) {
					return nullptr;
				}
			}

			instance_ = new BonDriver(config);
		} catch (...) {
			return nullptr;
		}
	}

	return instance_;
}

void BonDriver::DestroyInstance()
{
	std::lock_guard<std::mutex> lock(instance_mtx_);

	if (instance_) {
		delete instance_;
		instance_ = nullptr;
	}

	return;
}

extern "C" IBonDriver *CreateBonDriver()
{
	return BonDriver::GetInstance();
}

} // namespace BonDriver_LinuxMirakurun
