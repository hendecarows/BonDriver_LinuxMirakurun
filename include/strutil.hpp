// SPDX-License-Identifier: MIT
//
// string utility functions
//

#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace strutil
{

inline constexpr std::string_view WHITE_SPACE = " \n\r\t\f\v";
inline constexpr std::string_view SEPARATOR = ", \t";

std::string_view LTrim(std::string_view str, std::string_view sep = WHITE_SPACE);
std::string_view RTrim(std::string_view str, std::string_view sep = WHITE_SPACE);
std::string_view Trim(std::string_view str, std::string_view sep = WHITE_SPACE);
std::string ToLower(std::string_view str);
std::string ToUpper(std::string_view str);
void Separate(std::string_view str, std::vector<std::string> &res, std::string_view sep = SEPARATOR);
std::vector<std::string> Split(std::string_view str, std::string_view sep = SEPARATOR);

inline std::string_view LTrim(std::string_view str, std::string_view sep)
{
	// 先頭から空白文字ではない文字の位置を探す
	auto pos = str.find_first_not_of(sep);
	return (pos == std::string::npos) ? "" : str.substr(pos);
}

inline std::string_view RTrim(std::string_view str, std::string_view sep)
{
	// 末尾から空白文字ではない文字の位置を探す
	auto pos = str.find_last_not_of(sep);

	// posは0始まりのインデックスなので、長さにするために+1が必要
	return (pos == std::string::npos) ? "" : str.substr(0, pos + 1);
}

inline std::string_view Trim(std::string_view str, std::string_view sep)
{
	return RTrim(LTrim(str, sep), sep);
}

inline std::string ToLower(std::string_view str)
{
	std::string res(str);
	std::transform(res.begin(), res.end(), res.begin(),
				   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return res;
}

inline std::string ToUpper(std::string_view str)
{
	std::string res(str);
	std::transform(res.begin(), res.end(), res.begin(),
				   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	return res;
}

inline void Separate(std::string_view str, std::vector<std::string> &res, std::string_view sep)
{
	size_t pos = 0;
	const auto tail = str.length();

	while (pos < tail) {
		// 文字列の先頭から区切り文字ではない文字の位置を探す
		auto h = str.find_first_not_of(sep, pos);
		if (h == std::string_view::npos) {
			// 見つからない場合
			break;
		}
		pos = h;

		// 次の区切り文字の位置を探す
		auto t = str.find_first_of(sep, pos);
		if (t == std::string_view::npos) {
			t = tail;
		}

		res.emplace_back(str.substr(pos, t - pos));
		pos = t;
	}
}

inline std::vector<std::string> Split(std::string_view str, std::string_view sep)
{
	std::vector<std::string> res;
	Separate(str, res, sep);
	return res;
}

} // namespace strutil
