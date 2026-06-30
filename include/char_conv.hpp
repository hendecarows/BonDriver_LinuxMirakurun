// SPDX-License-Identifier: MIT
//
// character encoding converter class
//

#pragma once

#include <algorithm>
#include <cerrno>
#include <iconv.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace strutil
{

class CharConv
{
public:
	CharConv(const std::string &from_code, const std::string &to_code);
	virtual ~CharConv();

	// コピー禁止
	CharConv(const CharConv &) = delete;
	CharConv &operator=(const CharConv &) = delete;

	// 汎用変換インターフェース
	// OutType 戻り値の型 (std::string, std::u16string等)
	// InType 入力のコンテナ型
	template <typename OutType, typename InType>
	requires requires(InType t) { t.data(); t.size(); }
	OutType Convert(const InType &input) const;

	// OutType 戻り値の型 (std::string, std::u16string等)
	// InType 入力のコンテナ型 char16_t*
	template <typename OutType, typename InType>
	OutType Convert(const InType *ptr) const;

protected:
	static inline const iconv_t INVALID_HANDLE = reinterpret_cast<iconv_t>(-1);
	static constexpr size_t ICONV_ERROR = static_cast<size_t>(-1);
	static constexpr size_t EXTRA_BUFFER_SIZE = 32;

	void IconvReset() const;
	std::vector<char> ConvertRaw(const void *input, size_t input_bytes) const;

	// ポインタと要素数による変換
	template <typename OutType, typename T>
	OutType ConvertFromRaw(const T *ptr, size_t element_count) const;

private:
	iconv_t iconv_ = INVALID_HANDLE;
};

inline CharConv::CharConv(const std::string &from_code, const std::string &to_code)
{
	iconv_ = iconv_open(to_code.c_str(), from_code.c_str());
	if (iconv_ == INVALID_HANDLE) {
		throw std::system_error(errno, std::generic_category(), "iconv_open error");
	}
}

inline CharConv::~CharConv()
{
	if (iconv_ != INVALID_HANDLE) {
		iconv_close(iconv_);
		iconv_ = INVALID_HANDLE;
	}
}

inline void CharConv::IconvReset() const
{
	iconv(iconv_, nullptr, nullptr, nullptr, nullptr);
}

inline std::vector<char> CharConv::ConvertRaw(const void *input, size_t input_bytes) const
{
	if (!input || input_bytes == 0) {
		return {};
	}

	auto in_ptr = static_cast<char *>(const_cast<void *>(input));
	auto in_left = input_bytes;

	// 初期バッファサイズとして入力バイト数の2倍と終端文字等の余裕量32バイト確保
	auto out_capacity = input_bytes * 2 + EXTRA_BUFFER_SIZE;
	std::vector<char> buffer(out_capacity);
	auto out_ptr = buffer.data();
	auto out_left = buffer.size();

	IconvReset();

	while (in_left > 0) {
		if (iconv(iconv_, &in_ptr, &in_left, &out_ptr, &out_left) == ICONV_ERROR) {
			if (errno == E2BIG) {
				auto done = buffer.size() - out_left;
				buffer.resize(buffer.size() * 2);
				out_ptr = buffer.data() + done;
				out_left = buffer.size() - done;
			} else {
				throw std::system_error(errno, std::generic_category(), "iconv conversion error");
			}
		}
	}

	// 終端処理
	while (iconv(iconv_, nullptr, nullptr, &out_ptr, &out_left) == ICONV_ERROR) {
		if (errno == E2BIG) {
			auto done = buffer.size() - out_left;
			buffer.resize(buffer.size() + EXTRA_BUFFER_SIZE);
			out_ptr = buffer.data() + done;
			out_left = buffer.size() - done;
		} else {
			throw std::system_error(errno, std::generic_category(), "iconv flush error");
		}
	}

	buffer.resize(buffer.size() - out_left);

	return buffer;
}

template <typename OutType, typename InType>
requires requires(InType t) { t.data(); t.size(); }
inline OutType CharConv::Convert(const InType &input) const
{
	return ConvertFromRaw<OutType>(input.data(), input.size());
}

template <typename OutType, typename InType>
inline OutType CharConv::Convert(const InType *ptr) const
{
	if (!ptr) {
		return OutType();
	}

	size_t length = std::char_traits<InType>::length(ptr);
	return ConvertFromRaw<OutType>(ptr, length);
}

template <typename OutType, typename T>
inline OutType CharConv::ConvertFromRaw(const T *ptr, size_t element_count) const
{
	auto tmp_buffer = ConvertRaw(ptr, element_count * sizeof(T));

	using OutValueType = typename OutType::value_type;
	return OutType(
		reinterpret_cast<const OutValueType *>(tmp_buffer.data()),
		tmp_buffer.size() / sizeof(OutValueType));
}

} // namespace strutil
