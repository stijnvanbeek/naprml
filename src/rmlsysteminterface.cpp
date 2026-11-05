/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "rmlsysteminterface.h"

// External includes
#include <nap/logger.h>
#include <SDL3/SDL_mouse.h>
#include <RmlUi/Core/StringUtilities.h>

namespace nap
{
	RmlSystemInterface::~RmlSystemInterface()
	{
		for (auto& [name, cursor] : mCursors)
			SDL_DestroyCursor(cursor);
	}

	double RmlSystemInterface::GetElapsedTime()
	{
		const auto now = std::chrono::steady_clock::now();
		const auto duration = now - mStartTime;
		return std::chrono::duration<double>(duration).count();
	}

	bool RmlSystemInterface::LogMessage(Rml::Log::Type type, const Rml::String& message)
	{
		switch (type)
		{
			case Rml::Log::LT_ERROR:
			case Rml::Log::LT_ASSERT:
				nap::Logger::error(message.c_str());
				break;
			case Rml::Log::LT_WARNING:
				nap::Logger::warn(message.c_str());
				break;
			default:
				nap::Logger::info(message.c_str());
				break;
		}
		return true;
	}

	void RmlSystemInterface::SetMouseCursor(const Rml::String& cursor_name)
	{
		// Translate the RmlUi cursor name into an SDL system cursor,
		// following the names used by RmlUi's own platform backends.
		SDL_SystemCursor system_cursor;
		bool known = true;

		if (cursor_name.empty() || cursor_name == "arrow" || cursor_name == "default")
			system_cursor = SDL_SYSTEM_CURSOR_DEFAULT;
		else if (cursor_name == "pointer")
			system_cursor = SDL_SYSTEM_CURSOR_POINTER;
		else if (cursor_name == "text")
			system_cursor = SDL_SYSTEM_CURSOR_TEXT;
		else if (cursor_name == "cross")
			system_cursor = SDL_SYSTEM_CURSOR_CROSSHAIR;
		else if (cursor_name == "move" || Rml::StringUtilities::StartsWith(cursor_name, "rmlui-scroll"))
			system_cursor = SDL_SYSTEM_CURSOR_MOVE;
		else if (cursor_name == "resize")
			system_cursor = SDL_SYSTEM_CURSOR_NWSE_RESIZE;
		else if (cursor_name == "wait")
			system_cursor = SDL_SYSTEM_CURSOR_WAIT;
		else if (cursor_name == "progress")
			system_cursor = SDL_SYSTEM_CURSOR_PROGRESS;
		else if (cursor_name == "unavailable" || cursor_name == "not-allowed")
			system_cursor = SDL_SYSTEM_CURSOR_NOT_ALLOWED;
		else
		{
			// Unknown name: keep the current cursor
			nap::Logger::warn("RmlSystemInterface: unsupported cursor name '%s'", cursor_name.c_str());
			known = false;
		}

		if (!known)
			return;

		// Create the cursor on first use and cache it for later
		auto it = mCursors.find(cursor_name);
		if (it == mCursors.end())
		{
			SDL_Cursor* cursor = SDL_CreateSystemCursor(system_cursor);
			if (cursor == nullptr)
			{
				nap::Logger::error("RmlSystemInterface: failed to create cursor '%s': %s",
					cursor_name.c_str(), SDL_GetError());
				return;
			}
			it = mCursors.emplace(cursor_name, cursor).first;
		}

		SDL_SetCursor(it->second);
	}
}
