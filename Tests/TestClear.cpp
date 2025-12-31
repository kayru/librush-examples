#include "TestFramework.h"

#include <Rush/GfxDevice.h>
#include <Rush/Platform.h>


using namespace Test;

class ClearColorTest final : public GfxScreenshotTestCase
{
public:
	void render(GfxContext* ctx) override
	{
		GfxPassDesc passDesc;
		passDesc.flags          = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(11, 22, 33);
		Gfx_BeginPass(ctx, passDesc);
		Gfx_EndPass(ctx);
	}

	TestResult validate(GfxContext*, const TestImage* image) override
	{
		// Ensure a screenshot was captured.
		if (!image || image->pixels.empty())
		{
			return TestResult::fail("Missing screenshot data");
		}

		// Validate dimensions for a usable center pixel.
		if (image->size.x == 0 || image->size.y == 0)
		{
			return TestResult::fail("Invalid screenshot size");
		}

		// Sample the center pixel.
		const u32 centerX = image->size.x / 2;
		const u32 centerY = image->size.y / 2;
		const u32 index   = centerY * image->size.x + centerX;

		const ColorRGBA8 expected(11, 22, 33);
		const ColorRGBA8 actual = image->pixels[index];

		// Compare against the expected clear color.
		if (actual.r != expected.r || actual.g != expected.g || actual.b != expected.b)
		{
			return TestResult::fail("Unexpected clear color (%u,%u,%u) expected (%u,%u,%u)",
			    actual.r, actual.g, actual.b, expected.r, expected.g, expected.b);
		}

		return TestResult::pass();
	}
};

RUSH_REGISTER_TEST(ClearColorTest, "gfx",
	"Clears the backbuffer and validates the center pixel color.");
