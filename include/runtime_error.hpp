// SPDX-License-Identifier: MIT
//
// runtime error class with details
//

#pragma once

#include <stdint.h>

#include <filesystem>
#include <format>
#include <source_location>
#include <stdexcept>
#include <string>

namespace BonDriver_LinuxMirakurun
{

class RuntimeError : public std::runtime_error
{
public:
	enum Level : int32_t {
		none,
		fatal,
		error,
		warning,
		info,
		debug,
		verbose,
	};

	explicit RuntimeError(
		const std::string &message,
		int32_t code = 0,
		Level level = Level::error,
		const std::source_location loc = std::source_location::current())
		: std::runtime_error(message), code_(code), level_(level), location_(loc)
	{
		std::string filename = std::filesystem::path(location_.file_name()).filename().string();

		std::string code_str = (code_ != 0)
								   ? std::format(" (code = {})", code_)
								   : "";

		formatted_message_ = std::format(
			"[{}:{}] {}{}", filename, location_.line(), message, code_str);
	}

	virtual const char *what() const noexcept override
	{
		return formatted_message_.c_str();
	}

	int32_t code() const noexcept { return code_; }
	Level level() const noexcept { return level_; }
	const std::source_location &where() const noexcept { return location_; }

private:
	int32_t code_;
	Level level_;
	std::source_location location_;
	std::string formatted_message_;
};

} // namespace BonDriver_LinuxMirakurun