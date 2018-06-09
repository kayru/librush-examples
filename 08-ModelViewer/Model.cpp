#include "Model.h"

#include <Common/Utils.h>
#include <Rush/UtilFile.h>
#include <Rush/UtilLog.h>

#ifdef __linux__
#define strcpy_s strcpy
#endif

const u32 Model::magic = 0xfe892a37;

bool Model::read(const char* filename)
{
	FileIn stream(filename);
	if (!stream.valid())
	{
		Log::error("Failed to open file '%s' for reading", filename);
		return false;
	}

	u32 actualMagic = 0;
	stream.readT(actualMagic);
	if (actualMagic != magic)
	{
		Log::error("Model format identifier mismatch. Expected 0x%08x, got 0x%08x.", magic, actualMagic);
		return false;
	}

	stream.readT(bounds);
	readContainer(stream, materials);
	readContainer(stream, segments);
	readContainer(stream, vertices);
	readContainer(stream, indices);

	return true;
}

void Model::write(const char* filename)
{
	FileOut stream(filename);
	if (!stream.valid())
	{
		Log::error("Failed to open file '%s' for writing", filename);
	}

	stream.writeT(magic);
	stream.writeT(bounds);
	writeContainer(stream, materials);
	writeContainer(stream, segments);
	writeContainer(stream, vertices);
	writeContainer(stream, indices);
}
