// BonDriver_LinuxMirakurun.hpp

#pragma once

#include "IBonDriver2.h"
#include "char_conv.hpp"
#include "config.hpp"
#include "stream_buffer.hpp"

#include <stdint.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

namespace BonDriver_LinuxMirakurun
{

class BonDriver final : public IBonDriver2
{
public:
	explicit BonDriver(config::Config &config);
	~BonDriver();

	// cannot copy
	BonDriver(const BonDriver &) = delete;
	BonDriver &operator=(const BonDriver &) = delete;

	// IBonDriver
	bool OpenTuner(void) override;
	void CloseTuner(void) override;

	bool SetChannel(const uint8_t bCh) override;
	float GetSignalLevel(void) override;

	uint32_t WaitTsStream(const uint32_t dwTimeOut = 0) override;
	uint32_t GetReadyCount(void) override;

	bool GetTsStream(uint8_t *pDst, uint32_t *pdwSize, uint32_t *pdwRemain) override;
	bool GetTsStream(uint8_t **ppDst, uint32_t *pdwSize, uint32_t *pdwRemain) override;

	void PurgeTsStream(void) override;

	void Release(void) override;

	// IBonDriver2
	const char16_t *GetTunerName(void) override;

	bool IsTunerOpening(void) override;

	const char16_t *EnumTuningSpace(const uint32_t dwSpace) override;
	const char16_t *EnumChannelName(const uint32_t dwSpace, const uint32_t dwChannel) override;

	bool SetChannel(const uint32_t dwSpace, const uint32_t dwChannel) override;

	uint32_t GetCurSpace(void) override;
	uint32_t GetCurChannel(void) override;

	static BonDriver *GetInstance();

private:
	class Space final
	{
	public:
		class Channel final
		{
		public:
			Channel(strutil::CharConv &cv, const std::string &name, const std::string &channel);
			~Channel() = default;

			// cannot copy
			Channel(const Channel &) = delete;
			Channel &operator=(const Channel &) = delete;

			// can move
			Channel(Channel &&) = default;
			Channel &operator=(Channel &&) = default;

			const std::string &GetNameUtf8() const { return name_u8_; }
			const std::u16string &GetName() const { return name_; }
			const std::string &GetChannel() const { return channel_; }

		private:
			std::string name_u8_;
			std::u16string name_;
			std::string channel_;
		};

		Space(strutil::CharConv &cv, config::Config::Section &sct);
		Space() = default;
		~Space() = default;

		// cannot copy
		Space(const Space &) = delete;
		Space &operator=(Space &) = delete;

		// can move
		Space(Space &&) = default;
		Space &operator=(Space &&) = default;

		const std::string &GetNameUtf8() const { return name_u8_; }
		const std::u16string &GetName() const { return name_; }
		const std::string &GetType() const { return type_; }
		const Channel &GetChannel(std::size_t pos) const { return channel_.at(pos); };
		bool GetUseManualChannelList() const { return use_manual_channel_list_; }
		bool GetB25Decode() const { return b25_decode_; }
		int32_t GetPriority() const { return priority_; }
		void AddManualChannelList(strutil::CharConv &cv, config::Config::Section &sct, int32_t max_channels);
		void AddAutoChannelList(strutil::CharConv &cv, nlohmann::json &json, const std::string &type);

	private:
		std::string name_u8_;
		std::u16string name_;
		std::string type_;
		bool use_manual_channel_list_ = false;
		bool b25_decode_ = false;
		int32_t priority_ = 0;
		std::vector<Channel> channel_;
	};

	static constexpr uint32_t TS_PACKET_SIZE = 188;
	static constexpr uint32_t INFINITE = 0xffffffff;
	static constexpr uint32_t WAIT_OBJECT_0 = 0x00000000;
	static constexpr uint32_t WAIT_ABANDONED = 0x00000080;
	static constexpr uint32_t WAIT_TIMEOUT = 0x00000102;
	static constexpr uint32_t STREAM_TIMEOUT = 0;

	enum class Protocol { HTTP, UNIX_DOMAIN_SOCKET, UNKNOWN };

	void DetermineProtocol(std::string_view address);
	std::unique_ptr<cpr::Session> MakeSession(int32_t timeout = 1000);
	bool StreamHandler(std::string_view data, intptr_t);

	std::mutex mtx_;
	strutil::CharConv char_conv_{"UTF-8", "UTF-16LE"};
	Protocol connection_protocol_ = Protocol::UNKNOWN;
	bool http_verbose_log_ = false;

	std::u16string name_;
	std::string server_address_;
	std::string socket_path_;

	std::atomic_bool is_open_tuner_ = false;
	std::atomic_bool is_streaming_ = false;
	std::atomic_uint32_t current_space_ = 0;
	std::atomic_uint32_t current_channel_ = 0;

	std::vector<Space> spaces_;

	nlohmann::json version_;
	std::shared_ptr<cpr::Session> streaming_session_;
	cpr::AsyncResponse streaming_response_;
	int32_t session_timeout_ = 1000;
	int32_t response_timeout_ = 2000;

	std::chrono::milliseconds bitrate_interval_{500};
	std::chrono::steady_clock::time_point bitrate_last_time_;
	std::atomic_size_t bitrate_size_ = 0;
	std::atomic<float> bitrate_mbps_ = 0.0f;

	std::unique_ptr<StreamBuffer> stream_buffer_;
	std::vector<uint8_t> stream_chunk_;

	static std::mutex instance_mtx_;
	static BonDriver *instance_;

	static void DestroyInstance();
};

} // namespace BonDriver_LinuxMirakurun
