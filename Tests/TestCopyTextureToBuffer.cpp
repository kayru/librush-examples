#include "TestFramework.h"

#include <Rush/GfxDevice.h>

#include <cstring>

using namespace Test;

class CopyTextureToBufferBasicTest final : public GfxTestCase
{
public:
	explicit CopyTextureToBufferBasicTest(GfxContext* ctx)
	{
		if (!ctx)
		{
			return;
		}

		const u32 width  = 4;
		const u32 height = 4;

		// Create a source texture with known pixel data
		GfxTextureDesc texDesc = GfxTextureDesc::make2D(
		    width, height, GfxFormat_RGBA8_Unorm,
		    GfxUsageFlags::TransferSrc | GfxUsageFlags::TransferDst | GfxUsageFlags::ShaderResource);
		texDesc.debugName = "TestCopySrc";

		m_expected.resize(width * height);
		for (u32 i = 0; i < width * height; ++i)
		{
			m_expected[i] = ColorRGBA8(
			    u8(i * 17),
			    u8(i * 31),
			    u8(i * 53),
			    255);
		}

		GfxTextureData texData;
		texData.width  = width;
		texData.height = height;
		texData.depth  = 1;
		m_texture = Gfx_CreateTexture(texDesc, &texData, 1, m_expected.data());

		// Query copy info
		m_copyInfo = Gfx_GetImageCopyInfo(GfxFormat_RGBA8_Unorm, {width, height, 1});

		// Create destination buffer
		GfxBufferDesc bufDesc;
		bufDesc.flags       = GfxBufferFlags::Storage;
		bufDesc.stride      = 1;
		bufDesc.count       = m_copyInfo.bytesPerRow * m_copyInfo.rowCount;
		bufDesc.hostVisible = true;
		bufDesc.debugName   = "TestCopyDst";
		m_buffer = Gfx_CreateBuffer(bufDesc);

		m_width  = width;
		m_height = height;
	}

	void render(GfxContext* ctx) override
	{
		if (!m_texture.valid() || !m_buffer.valid())
		{
			return;
		}

		Gfx_AddImageBarrier(ctx, m_texture, GfxResourceState_TransferSrc);

		GfxImageRegion region;
		m_copyInfo = Gfx_CopyTextureToBuffer(ctx, m_texture, region, m_buffer);
	}

	TestResult validate(GfxContext*, const TestImage*) override
	{
		if (!m_texture.valid())
		{
			return TestResult::fail("Failed to create texture");
		}
		if (!m_buffer.valid())
		{
			return TestResult::fail("Failed to create buffer");
		}

		Gfx_Finish();

		GfxMappedBuffer mapped = Gfx_MapBuffer(m_buffer);
		if (!mapped.data)
		{
			return TestResult::fail("Failed to map buffer");
		}

		const u8* src = reinterpret_cast<const u8*>(mapped.data);

		for (u32 y = 0; y < m_height; ++y)
		{
			const ColorRGBA8* row = reinterpret_cast<const ColorRGBA8*>(
			    src + y * m_copyInfo.bytesPerRow);
			for (u32 x = 0; x < m_width; ++x)
			{
				const u32 idx = y * m_width + x;
				const ColorRGBA8& expected = m_expected[idx];
				const ColorRGBA8& actual   = row[x];
				if (actual.r != expected.r || actual.g != expected.g ||
				    actual.b != expected.b || actual.a != expected.a)
				{
					Gfx_UnmapBuffer(mapped);
					return TestResult::fail(
					    "Pixel (%u,%u): got (%u,%u,%u,%u) expected (%u,%u,%u,%u)",
					    x, y,
					    actual.r, actual.g, actual.b, actual.a,
					    expected.r, expected.g, expected.b, expected.a);
				}
			}
		}

		Gfx_UnmapBuffer(mapped);
		return TestResult::pass();
	}

private:
	GfxOwn<GfxTexture> m_texture;
	GfxOwn<GfxBuffer>  m_buffer;
	GfxImageCopyInfo   m_copyInfo = {};
	DynamicArray<ColorRGBA8> m_expected;
	u32 m_width  = 0;
	u32 m_height = 0;
};

RUSH_REGISTER_TEST(CopyTextureToBufferBasicTest, "gfx",
	"Copies a texture to a buffer and validates pixel data via readback.");

class CopyTextureToBufferSubregionTest final : public GfxTestCase
{
public:
	explicit CopyTextureToBufferSubregionTest(GfxContext* ctx)
	{
		if (!ctx)
		{
			return;
		}

		const u32 width  = 8;
		const u32 height = 8;

		GfxTextureDesc texDesc = GfxTextureDesc::make2D(
		    width, height, GfxFormat_RGBA8_Unorm,
		    GfxUsageFlags::TransferSrc | GfxUsageFlags::TransferDst | GfxUsageFlags::ShaderResource);
		texDesc.debugName = "TestCopySubregionSrc";

		DynamicArray<ColorRGBA8> pixels(width * height);
		for (u32 y = 0; y < height; ++y)
		{
			for (u32 x = 0; x < width; ++x)
			{
				pixels[y * width + x] = ColorRGBA8(u8(x * 32), u8(y * 32), 128, 255);
			}
		}

		GfxTextureData texData;
		texData.width  = width;
		texData.height = height;
		texData.depth  = 1;
		m_texture = Gfx_CreateTexture(texDesc, &texData, 1, pixels.data());

		// Copy a 4x4 subregion starting at (2,2)
		m_regionOffset = {2, 2, 0};
		m_regionSize   = {4, 4, 1};

		m_copyInfo = Gfx_GetImageCopyInfo(GfxFormat_RGBA8_Unorm, m_regionSize);

		GfxBufferDesc bufDesc;
		bufDesc.flags       = GfxBufferFlags::Storage;
		bufDesc.stride      = 1;
		bufDesc.count       = m_copyInfo.bytesPerRow * m_copyInfo.rowCount;
		bufDesc.hostVisible = true;
		bufDesc.debugName   = "TestCopySubregionDst";
		m_buffer = Gfx_CreateBuffer(bufDesc);

		// Store expected subregion pixels
		m_expected.resize(m_regionSize.x * m_regionSize.y);
		for (u32 y = 0; y < m_regionSize.y; ++y)
		{
			for (u32 x = 0; x < m_regionSize.x; ++x)
			{
				m_expected[y * m_regionSize.x + x] =
				    pixels[(y + m_regionOffset.y) * width + (x + m_regionOffset.x)];
			}
		}
	}

	void render(GfxContext* ctx) override
	{
		if (!m_texture.valid() || !m_buffer.valid())
		{
			return;
		}

		Gfx_AddImageBarrier(ctx, m_texture, GfxResourceState_TransferSrc);

		GfxImageRegion region;
		region.offset = m_regionOffset;
		region.size   = m_regionSize;
		m_copyInfo = Gfx_CopyTextureToBuffer(ctx, m_texture, region, m_buffer);
	}

	TestResult validate(GfxContext*, const TestImage*) override
	{
		if (!m_texture.valid())
		{
			return TestResult::fail("Failed to create texture");
		}
		if (!m_buffer.valid())
		{
			return TestResult::fail("Failed to create buffer");
		}

		Gfx_Finish();

		GfxMappedBuffer mapped = Gfx_MapBuffer(m_buffer);
		if (!mapped.data)
		{
			return TestResult::fail("Failed to map buffer");
		}

		const u8* src = reinterpret_cast<const u8*>(mapped.data);

		for (u32 y = 0; y < m_regionSize.y; ++y)
		{
			const ColorRGBA8* row = reinterpret_cast<const ColorRGBA8*>(
			    src + y * m_copyInfo.bytesPerRow);
			for (u32 x = 0; x < m_regionSize.x; ++x)
			{
				const u32 idx = y * m_regionSize.x + x;
				const ColorRGBA8& expected = m_expected[idx];
				const ColorRGBA8& actual   = row[x];
				if (actual.r != expected.r || actual.g != expected.g ||
				    actual.b != expected.b || actual.a != expected.a)
				{
					Gfx_UnmapBuffer(mapped);
					return TestResult::fail(
					    "Pixel (%u,%u): got (%u,%u,%u,%u) expected (%u,%u,%u,%u)",
					    x, y,
					    actual.r, actual.g, actual.b, actual.a,
					    expected.r, expected.g, expected.b, expected.a);
				}
			}
		}

		Gfx_UnmapBuffer(mapped);
		return TestResult::pass();
	}

private:
	GfxOwn<GfxTexture> m_texture;
	GfxOwn<GfxBuffer>  m_buffer;
	GfxImageCopyInfo   m_copyInfo = {};
	DynamicArray<ColorRGBA8> m_expected;
	Tuple3u m_regionOffset = {};
	Tuple3u m_regionSize   = {};
};

RUSH_REGISTER_TEST(CopyTextureToBufferSubregionTest, "gfx",
	"Copies a subregion of a texture to a buffer and validates pixel data.");

class CopyTextureToBufferWithOffsetTest final : public GfxTestCase
{
public:
	explicit CopyTextureToBufferWithOffsetTest(GfxContext* ctx)
	{
		if (!ctx)
		{
			return;
		}

		const u32 width  = 4;
		const u32 height = 4;

		GfxTextureDesc texDesc = GfxTextureDesc::make2D(
		    width, height, GfxFormat_RGBA8_Unorm,
		    GfxUsageFlags::TransferSrc | GfxUsageFlags::TransferDst | GfxUsageFlags::ShaderResource);
		texDesc.debugName = "TestCopyOffsetSrc";

		m_expected.resize(width * height);
		for (u32 i = 0; i < width * height; ++i)
		{
			m_expected[i] = ColorRGBA8(u8(i * 7), u8(i * 13), u8(i * 41), 255);
		}

		GfxTextureData texData;
		texData.width  = width;
		texData.height = height;
		texData.depth  = 1;
		m_texture = Gfx_CreateTexture(texDesc, &texData, 1, m_expected.data());

		m_copyInfo = Gfx_GetImageCopyInfo(GfxFormat_RGBA8_Unorm, {width, height, 1});

		// Allocate buffer with extra space at the front
		m_dstOffset = 256;
		GfxBufferDesc bufDesc;
		bufDesc.flags       = GfxBufferFlags::Storage;
		bufDesc.stride      = 1;
		bufDesc.count       = m_dstOffset + m_copyInfo.bytesPerRow * m_copyInfo.rowCount;
		bufDesc.hostVisible = true;
		bufDesc.debugName   = "TestCopyOffsetDst";
		m_buffer = Gfx_CreateBuffer(bufDesc);

		m_width  = width;
		m_height = height;
	}

	void render(GfxContext* ctx) override
	{
		if (!m_texture.valid() || !m_buffer.valid())
		{
			return;
		}

		Gfx_AddImageBarrier(ctx, m_texture, GfxResourceState_TransferSrc);

		GfxImageRegion region;
		m_copyInfo = Gfx_CopyTextureToBuffer(ctx, m_texture, region, m_buffer, m_dstOffset);
	}

	TestResult validate(GfxContext*, const TestImage*) override
	{
		if (!m_texture.valid())
		{
			return TestResult::fail("Failed to create texture");
		}
		if (!m_buffer.valid())
		{
			return TestResult::fail("Failed to create buffer");
		}

		Gfx_Finish();

		GfxMappedBuffer mapped = Gfx_MapBuffer(m_buffer);
		if (!mapped.data)
		{
			return TestResult::fail("Failed to map buffer");
		}

		const u8* base = reinterpret_cast<const u8*>(mapped.data) + m_dstOffset;

		for (u32 y = 0; y < m_height; ++y)
		{
			const ColorRGBA8* row = reinterpret_cast<const ColorRGBA8*>(
			    base + y * m_copyInfo.bytesPerRow);
			for (u32 x = 0; x < m_width; ++x)
			{
				const u32 idx = y * m_width + x;
				const ColorRGBA8& expected = m_expected[idx];
				const ColorRGBA8& actual   = row[x];
				if (actual.r != expected.r || actual.g != expected.g ||
				    actual.b != expected.b || actual.a != expected.a)
				{
					Gfx_UnmapBuffer(mapped);
					return TestResult::fail(
					    "Pixel (%u,%u): got (%u,%u,%u,%u) expected (%u,%u,%u,%u)",
					    x, y,
					    actual.r, actual.g, actual.b, actual.a,
					    expected.r, expected.g, expected.b, expected.a);
				}
			}
		}

		Gfx_UnmapBuffer(mapped);
		return TestResult::pass();
	}

private:
	GfxOwn<GfxTexture> m_texture;
	GfxOwn<GfxBuffer>  m_buffer;
	GfxImageCopyInfo   m_copyInfo = {};
	DynamicArray<ColorRGBA8> m_expected;
	u32 m_width     = 0;
	u32 m_height    = 0;
	u64 m_dstOffset = 0;
};

RUSH_REGISTER_TEST(CopyTextureToBufferWithOffsetTest, "gfx",
	"Copies a texture to a buffer at a non-zero offset and validates pixel data.");

class GetImageCopyInfoTest final : public CpuTestCase
{
public:
	TestResult validate(GfxContext*, const TestImage*) override
	{
		// RGBA8: 4 bytes per pixel
		{
			const GfxImageCopyInfo info = Gfx_GetImageCopyInfo(GfxFormat_RGBA8_Unorm, {16, 8, 1});
			if (info.bytesPerRow < 16 * 4)
			{
				return TestResult::fail("RGBA8 16-wide: bytesPerRow %u too small (expected >= %u)",
				    info.bytesPerRow, 16u * 4u);
			}
			if (info.rowCount != 8)
			{
				return TestResult::fail("RGBA8: rowCount %u expected 8", info.rowCount);
			}
		}

		// R8: 1 byte per pixel
		{
			const GfxImageCopyInfo info = Gfx_GetImageCopyInfo(GfxFormat_R8_Unorm, {32, 16, 1});
			if (info.bytesPerRow < 32)
			{
				return TestResult::fail("R8 32-wide: bytesPerRow %u too small (expected >= %u)",
				    info.bytesPerRow, 32u);
			}
			if (info.rowCount != 16)
			{
				return TestResult::fail("R8: rowCount %u expected 16", info.rowCount);
			}
		}

		// RGBA32F: 16 bytes per pixel
		{
			const GfxImageCopyInfo info = Gfx_GetImageCopyInfo(GfxFormat_RGBA32_Float, {4, 2, 1});
			if (info.bytesPerRow < 4 * 16)
			{
				return TestResult::fail("RGBA32F 4-wide: bytesPerRow %u too small (expected >= %u)",
				    info.bytesPerRow, 4u * 16u);
			}
			if (info.rowCount != 2)
			{
				return TestResult::fail("RGBA32F: rowCount %u expected 2", info.rowCount);
			}
		}

		return TestResult::pass();
	}
};

RUSH_REGISTER_TEST(GetImageCopyInfoTest, "gfx",
	"Validates Gfx_GetImageCopyInfo returns correct layout for various formats.");
