/***************************************************************************
						  guifuncs.c  -  description
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

#include <lua/LuaConsole.h>
#include <Windows.h>
#include <commctrl.h>
#include <shared/guifuncs.h>
#include <main/win/main_win.h>
#include <r4300/tracelog.h>
#include <r4300/vcr.h>
#include <main/capture/EncodingManager.h>
#include <shared/Config.hpp>


bool confirm_user_exit()
{
	if (Config.silent_mode)
	{
		return true;
	}

	int res = 0;
	int warnings = 0;

	std::string final_message;
	if (VCR::get_task() == e_task::recording)
	{
		final_message.append("Movie recording ");
		warnings++;
	}
	if (EncodingManager::is_capturing())
	{
		if (warnings > 0) { final_message.append(","); }
		final_message.append(" AVI capture ");
		warnings++;
	}
	if (tracelog::active())
	{
		if (warnings > 0) { final_message.append(","); }
		final_message.append(" Trace logging ");
		warnings++;
	}
	final_message.
		append("is running. Are you sure you want to stop emulation?");
	if (warnings > 0)
		res = MessageBox(mainHWND, final_message.c_str(), "Stop emulation?",
		                 MB_YESNO | MB_ICONWARNING);

	return res == IDYES || warnings == 0;
}

bool show_ask_dialog(const char* str, const char* title, bool warning, void* hwnd)
{
	if (Config.silent_mode)
	{
		return true;
	}
	return MessageBox(static_cast<HWND>(hwnd ? hwnd : mainHWND), str, title, MB_YESNO | (warning ? MB_ICONWARNING : MB_ICONQUESTION)) == IDYES;
}

void show_warning(const char* str, const char* title, void* hwnd)
{
	if (!Config.silent_mode)
	{
		MessageBox(static_cast<HWND>(hwnd ? hwnd : mainHWND), str, title, MB_ICONWARNING);
	}
}

void show_error(const char* str, const char* title, void* hwnd)
{
	if (!Config.silent_mode)
	{
		MessageBox(static_cast<HWND>(hwnd ? hwnd : mainHWND), str, title, MB_ICONERROR);
	}
}

void show_information(const char* str, const char* title, void* hwnd)
{
	if (!Config.silent_mode)
	{
		MessageBox(static_cast<HWND>(hwnd ? hwnd : mainHWND), str, title, MB_OK | MB_ICONINFORMATION);
	}
}

bool is_on_gui_thread()
{
	return GetCurrentThreadId() == g_ui_thread_id;
}

std::filesystem::path get_app_path()
{
	return app_path;
}
