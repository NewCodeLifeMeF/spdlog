//
// Copyright(c) 2016 spdlog
// Distributed under the MIT License (http://opensource.org/licenses/MIT)
//

#pragma once

#include "sink.h"
#include "../common.h"
#include "../details/null_mutex.h"
#include "../details/traits.h"


#include <mutex>
#include <memory>
#include <string>
#include <unordered_map>
#include <wincon.h>

namespace spdlog {
namespace sinks {
/*
 * Windows color console sink. Uses WriteConsoleA to write to the console with colors
 */	
template<class HandleTrait, class ConsoleMutexTrait>
class wincolor_sink : public sink
{
public:
	
    const WORD BOLD = FOREGROUND_INTENSITY;
    const WORD RED = FOREGROUND_RED;
    const WORD GREEN = FOREGROUND_GREEN;
    const WORD CYAN = FOREGROUND_GREEN | FOREGROUND_BLUE;
    const WORD WHITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    const WORD YELLOW = FOREGROUND_RED | FOREGROUND_GREEN;

    wincolor_sink()
        : out_handle_(HandleTrait::handle()),
		_mutex(ConsoleMutexTrait::console_mutex())
    {
        colors_[level::trace] = WHITE;
        colors_[level::debug] = CYAN;
        colors_[level::info] = GREEN;
        colors_[level::warn] = YELLOW | BOLD;
        colors_[level::err] = RED | BOLD;                         // red bold
        colors_[level::critical] = BACKGROUND_RED | WHITE | BOLD; // white bold on red background
        colors_[level::off] = 0;
    }

	
    ~wincolor_sink() override
    {
        this->flush();
    }

    wincolor_sink(const wincolor_sink &other) = delete;
    wincolor_sink &operator=(const wincolor_sink &other) = delete;

    // change the color for the given level
    void set_color(level::level_enum level, WORD color)
    {
        std::lock_guard<mutex_t> lock(_mutex);
        colors_[level] = color;
    }

	void log(const details::log_msg &msg) SPDLOG_FINAL override
	{
		std::lock_guard<mutex_t> lock(_mutex);

		if (msg.color_range_end > msg.color_range_start)
		{
			// before color range
			_print_range(msg, 0, msg.color_range_start);

			// in color range
			auto orig_attribs = set_console_attribs(colors_[msg.level]);
			_print_range(msg, msg.color_range_start, msg.color_range_end);
			::SetConsoleTextAttribute(out_handle_, orig_attribs); // reset to orig colors
																  // after color range
			_print_range(msg, msg.color_range_end, msg.formatted.size());
		}
		else // print without colors if color range is invalid
		{
			_print_range(msg, 0, msg.formatted.size());
		}
	}

	void flush() SPDLOG_FINAL override
	{
		// windows console always flushed?
	}
    
private:  
	using mutex_t = typename ConsoleMutexTrait::mutex_t;
    // set color and return the orig console attributes (for resetting later)
    WORD set_console_attribs(WORD attribs)
    {
        CONSOLE_SCREEN_BUFFER_INFO orig_buffer_info;
        ::GetConsoleScreenBufferInfo(out_handle_, &orig_buffer_info);
        WORD back_color = orig_buffer_info.wAttributes;
        // retrieve the current background color
        back_color &= static_cast<WORD>(~(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY));
        // keep the background color unchanged
        ::SetConsoleTextAttribute(out_handle_, attribs | back_color);
        return orig_buffer_info.wAttributes; // return orig attribs
    }

    // print a range of formatted message to console
    void _print_range(const details::log_msg &msg, size_t start, size_t end)
    {
        auto size = static_cast<DWORD>(end - start);
        ::WriteConsoleA(out_handle_, msg.formatted.data() + start, size, nullptr, nullptr);
    }

	HANDLE out_handle_;
	mutex_t &_mutex;
	std::unordered_map<level::level_enum, WORD, level::level_hasher> colors_;
};

//
// windows color console to stdout
//

using stdout_color_mt = wincolor_sink<details::console_stdout_trait, details::console_mutex_trait>;
using stdout_color_st = wincolor_sink<details::console_stdout_trait, details::console_null_mutex_trait>;
using stderr_color_mt = wincolor_sink<details::console_stderr_trait, details::console_mutex_trait>;
using stderr_color_st = wincolor_sink<details::console_stderr_trait, details::console_null_mutex_trait>;

} // namespace sinks
} // namespace spdlog
