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
		// Ensure the buffer was created successfully.
		if (!m_buffer.valid())
		{
			return TestResult::fail("Failed to create buffer");
		}

		// Map the buffer for CPU readback.
		GfxMappedBuffer mapped = Gfx_MapBuffer(m_buffer);
		if (!mapped.data || mapped.size < m_expected.size() * sizeof(u32))
		{
			Gfx_UnmapBuffer(mapped);
			return TestResult::fail("Failed to map buffer for readback");
		}

		// Compare each element against the expected pattern.
		const u32* data = reinterpret_cast<const u32*>(mapped.data);
		for (size_t i = 0; i < m_expected.size(); ++i)
		{
			if (data[i] != m_expected[i])
			{
				Gfx_UnmapBuffer(mapped);
				return TestResult::fail("Buffer mismatch at %zu: got 0x%08X expected 0x%08X",
				    i, data[i], m_expected[i]);
			}
		}

		// Unmap and return success.
		Gfx_UnmapBuffer(mapped);
		return TestResult::pass();
	}

private:
	GfxOwn<GfxBuffer> m_buffer;
	DynamicArray<u32> m_expected;
};

RUSH_REGISTER_TEST(BufferBasicTest, "gfx",
	"Validates CPU write/readback for a host-visible buffer.");
