/***************************************************************************
						  commandline.c  -  description
							 -------------------
	copyright            : (C) 2003 by ShadowPrince
	email                : shadow@emulation64.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// Based on code from 1964 by Schibo and Rice
// Slightly improved command line params parsing function to work with spaced arguments
#include <Windows.h>
#include "Commandline.h"

#include <thread>
#include <view/lua/LuaConsole.h>
#include "Main.h"
#include "Loggers.h"
#include <shared/Messenger.h>
#include <core/memory/savestates.h>
#include <shared/helpers/StlExtensions.h>
#include "../../lib/argh.h"
#include <core/r4300/r4300.h>
#include <core/r4300/vcr.h>
#include <view/capture/EncodingManager.h>

#include "features/Dispatcher.h"
#include "shared/AsyncExecutor.h"
#include "shared/Config.hpp"

std::filesystem::path commandline_rom;
std::filesystem::path commandline_lua;
std::filesystem::path commandline_st;
std::filesystem::path commandline_movie;
std::filesystem::path commandline_avi;
bool commandline_close_on_movie_end;
bool dacrate_changed;

bool is_movie_from_start;
bool first_emu_launched = true;
bool first_dacrate_changed = true;

void commandline_set()
{
    argh::parser cmdl(__argc, __argv,
                      argh::parser::PREFER_PARAM_FOR_UNREG_OPTION);

    commandline_rom = cmdl({"--rom", "-g"}, "").str();
    commandline_lua = cmdl({"--lua", "-lua"}, "").str();
    commandline_st = cmdl({"--st", "-st"}, "").str();
    commandline_movie = cmdl({"--movie", "-m64"}, "").str();
    commandline_avi = cmdl({"--avi", "-avi"}, "").str();
    commandline_close_on_movie_end = cmdl["--close-on-movie-end"];

    // handle "Open With...":
    if (cmdl.size() == 2 && cmdl.params().empty())
    {
        commandline_rom = cmdl[1];
    }

    // COMPAT: Old mupen closes emu when movie ends and avi flag is specified.
    if (!commandline_avi.empty())
    {
        commandline_close_on_movie_end = true;
    }

    // HACK: When playing a movie from start, the rom will start normally and signal us to do our work via EmuLaunchedChanged.
    // The work is started, but then the rom is reset. At that point, the dacrate changes and breaks the capture in some cases.
    // To avoid this, we store the movie's start flag prior to doing anything, and ignore the first EmuLaunchedChanged if it's set.
    const auto movie_path = commandline_rom.extension() == ".m64" ? commandline_rom : commandline_movie;
    if (!movie_path.empty())
    {
        t_movie_header hdr{};
        VCR::parse_header(movie_path, &hdr);
        is_movie_from_start = hdr.startFlags & MOVIE_START_FROM_NOTHING;
    }
}

void commandline_start_rom()
{
    if (commandline_rom.empty())
    {
        return;
    }

    AsyncExecutor::invoke_async([]
    {
        const auto result = vr_start_rom(commandline_rom);
        show_error_dialog_for_result(result);
        
        // Special case for "Open With..."
        if (commandline_rom.extension() == ".m64")
        {
            const auto result = VCR::start_playback(commandline_rom);
            show_error_dialog_for_result(result);
        }
    });
}

void commandline_load_st()
{
    if (commandline_st.empty())
    {
        return;
    }

    ++g_vr_wait_before_input_poll;
    AsyncExecutor::invoke_async([=]
    {
        --g_vr_wait_before_input_poll;
        Savestates::do_file(commandline_st.c_str(), Savestates::Job::Load);
    });
}

void commandline_start_lua()
{
    if (commandline_lua.empty())
    {
        return;
    }

    g_main_window_dispatcher->invoke([&]
    {
        // To run multiple lua scripts, a semicolon-separated list is provided
        std::stringstream stream;
        std::string script;
        stream << commandline_lua.string();
        while (std::getline(stream, script, ';'))
        {
            lua_create_and_run(script.c_str());
        }
    });
}

void commandline_start_movie()
{
    if (commandline_movie.empty())
    {
        return;
    }

    g_config.vcr_readonly = true;
    AsyncExecutor::invoke_async([]
    {
        auto result = VCR::start_playback(commandline_movie);
        show_error_dialog_for_result(result);
    });
}

void commandline_start_capture()
{
    if (commandline_avi.empty())
    {
        return;
    }

    g_main_window_dispatcher->invoke([]
    {
        EncodingManager::start_capture(commandline_avi.string().c_str(), static_cast<EncoderType>(g_config.encoder_type), false);
    });
}

void commandline_on_movie_playback_stop()
{
    if (commandline_close_on_movie_end)
    {
        g_main_window_dispatcher->invoke([]
        {
            EncodingManager::stop_capture();
            PostMessage(g_main_hwnd, WM_CLOSE, 0, 0);
        });
    }
}

namespace Cli
{
    void on_task_changed(std::any data)
    {
        auto value = std::any_cast<e_task>(data);
        static auto previous_value = value;

        if (task_is_playback(previous_value) && !task_is_playback(value))
        {
            commandline_on_movie_playback_stop();
        }

        previous_value = value;
    }

    void on_emu_launched_changed(std::any data)
    {
        auto value = std::any_cast<bool>(data);

        if (!value)
            return;

        if (first_emu_launched && is_movie_from_start)
        {
            g_view_logger->info("[Cli] Ignoring EmuLaunchedChanged due to from-start movie first reset!");
            first_emu_launched = false;
            return;
        }

        dacrate_changed = false;

        // start movies, st and lua scripts
        commandline_load_st();
        commandline_start_lua();
        commandline_start_movie();
    }

    void on_app_ready(std::any)
    {
        commandline_start_rom();
    }

    void on_dacrate_changed(std::any)
    {
        if (dacrate_changed)
        {
            return;
        }

        if (first_dacrate_changed && is_movie_from_start)
        {
            g_view_logger->info("[Cli] Ignoring DacrateChanged due to from-start movie first reset!");
            first_dacrate_changed = false;
            return;
        }

        commandline_start_capture();
        dacrate_changed = true;
    }


    void init()
    {
        Messenger::subscribe(Messenger::Message::EmuLaunchedChanged, on_emu_launched_changed);
        Messenger::subscribe(Messenger::Message::AppReady, on_app_ready);
        Messenger::subscribe(Messenger::Message::TaskChanged, on_task_changed);
        Messenger::subscribe(Messenger::Message::DacrateChanged, on_dacrate_changed);
        commandline_set();
    }
}
