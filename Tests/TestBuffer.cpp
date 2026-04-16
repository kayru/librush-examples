#include "TestFramework.h"

#include <Rush/GfxDevice.h>

#include <cstring>

using namespace Test;

class BufferBasicTest final : public GfxTestCase
{
public:
	explicit BufferBasicTest(GfxContext* ctx)
	{
		if (!ctx)
		{
			return;
		}

		// Describe a host-visible storage buffer for CPU write/readback.
		GfxBufferDesc desc;
		desc.flags       = GfxBufferFlags::Storage;
		desc.stride      = sizeof(u32);
		desc.count       = 16;
		desc.hostVisible = true;
		desc.debugName   = "TestBufferBasic";

		// Build the expected pattern to validate later.
		m_expected.resize(desc.count);
		for (u32 i = 0; i < desc.count; ++i)
		{
			m_expected[i] = 0xA0000000u + i;
		}

		// Create the buffer and map it for initial CPU writes.
		m_buffer = Gfx_CreateBuffer(desc);

		GfxMappedBuffer mapped = Gfx_MapBuffer(m_buffer);
		// Copy the pattern into the mapped buffer memory.
		if (mapped.data && mapped.size >= desc.count * desc.stride)
		{
			std::memcpy(mapped.data, m_expected.data(), desc.count * desc.stride);
		}
		// Unmap to finalize CPU writes.
		Gfx_UnmapBuffer(mapped);
	}

	TestResult validate(GfxContext*, const TestImage*) override
	{
		if (!m_buffer.valid())
		{
			return TestResult::fail("Failed to create buffer");
		}

		return validateBufferU32(m_buffer, m_expected.data(), m_expected.size());
	}

private:
	GfxOwn<GfxBuffer> m_buffer;
	DynamicArray<u32> m_expected;
};

RUSH_REGISTER_TEST(BufferBasicTest, "gfx",
	"Validates CPU write/readback for a host-visible buffer.");
