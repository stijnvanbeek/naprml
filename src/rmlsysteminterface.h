/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

// External includes
#include <chrono>
#include <unordered_map>
#include <RmlUi/Core/SystemInterface.h>

struct SDL_Cursor;

namespace nap
{
	/**
	 * RmlUi system interface, implements the parts of RmlUi that talk to the
	 * platform: time, logging and the mouse cursor.
	 *
	 * Mouse cursors are created from the SDL system cursor set on first use
	 * and cached by name, so RCSS properties such as `cursor: pointer` only
	 * pay the cost of a lookup after the first hover.
	 */
	class RmlSystemInterface final : public Rml::SystemInterface
	{
	public:
		RmlSystemInterface() = default;
		virtual ~RmlSystemInterface();

		// Rml::SystemInterface overrides
		double GetElapsedTime() override;
		bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
		void SetMouseCursor(const Rml::String& cursor_name) override;

	private:
		// Start time, used by GetElapsedTime()
		std::chrono::steady_clock::time_point mStartTime = std::chrono::steady_clock::now();

		// SDL cursors created on demand, keyed by the RmlUi cursor name
		std::unordered_map<Rml::String, SDL_Cursor*> mCursors;
	};
}
