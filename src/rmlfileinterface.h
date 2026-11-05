/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

// External includes
#include <RmlUi/Core/FileInterface.h>

namespace nap
{
	/**
	 * RmlUi file interface, loads '.rml' documents (and any resources they
	 * reference) from the application data directory.
	 */
	class RmlFileInterface final : public Rml::FileInterface
	{
	public:
		// Rml::FileInterface overrides
		Rml::FileHandle Open(const Rml::String& path) override;
		void Close(Rml::FileHandle handle) override;
		size_t Read(void* buffer, size_t buffer_size, Rml::FileHandle handle) override;
		size_t Length(Rml::FileHandle handle) override;
		bool Seek(Rml::FileHandle handle, long offset, int origin) override;
		size_t Tell(Rml::FileHandle handle) override;
	};
}
