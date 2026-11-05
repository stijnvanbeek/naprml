#include "rmlfileinterface.h"

// NAP includes
#include <nap/logger.h>

// System includes
#include <cstdio>

namespace nap
{
	Rml::FileHandle RmlFileInterface::Open(const Rml::String& path)
	{
		FILE* file = fopen(path.c_str(), "rb");
		return reinterpret_cast<Rml::FileHandle>(file);
	}

	void RmlFileInterface::Close(Rml::FileHandle handle)
	{
		FILE* file = reinterpret_cast<FILE*>(handle);
		if (file != nullptr)
			fclose(file);
	}

	size_t RmlFileInterface::Read(void* buffer, size_t buffer_size, Rml::FileHandle handle)
	{
		FILE* file = reinterpret_cast<FILE*>(handle);
		if (file == nullptr)
			return 0;
		return fread(buffer, 1, buffer_size, file);
	}

	size_t RmlFileInterface::Length(Rml::FileHandle handle)
	{
		FILE* file = reinterpret_cast<FILE*>(handle);
		if (file == nullptr)
			return 0;
		long position = ftell(file);
		fseek(file, 0, SEEK_END);
		long length = ftell(file);
		fseek(file, position, SEEK_SET);
		return static_cast<size_t>(length);
	}

	bool RmlFileInterface::Seek(Rml::FileHandle handle, long offset, int origin)
	{
		FILE* file = reinterpret_cast<FILE*>(handle);
		if (file == nullptr)
			return false;
		return fseek(file, offset, origin) == 0;
	}

	size_t RmlFileInterface::Tell(Rml::FileHandle handle)
	{
		FILE* file = reinterpret_cast<FILE*>(handle);
		if (file == nullptr)
			return 0;
		return static_cast<size_t>(ftell(file));
	}
}
