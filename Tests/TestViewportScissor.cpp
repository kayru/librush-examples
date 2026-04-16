#include "TestFramework.h"

#include <Rush/GfxDevice.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Platform.h>
#include <Rush/UtilLog.h>
#include <Rush/Window.h>

using namespace Test;
using namespace Rush;

namespace
{

u32 absDiff(u8 a, u8 b)
{
	return a > b ? (a - b) : (b - a);
}

TestResult checkPixel(const TestImage* image, u32 x, u32 y, ColorRGBA8 expected, const char* name)
{
	const u32 idx = y * image->size.x + x;
	const ColorRGBA8 actual = image->pixels[idx];

	const u32 tolerance = 2;
	const bool rOk = absDiff(actual.r, expected.r) <= tolerance;
	const bool gOk = absDiff(actual.g, expected.g) <= tolerance;
	const bool bOk = absDiff(actual.b, expected.b) <= tolerance;

	if (!rOk || !gOk || !bOk)
	{
		return TestResult::fail("%s: got (%u,%u,%u) expected (%u,%u,%u)",
		    name, actual.r, actual.g, actual.b,
		    expected.r, expected.g, expected.b);
	}

	return TestResult::pass();
}

} // namespace

// Validates that Gfx_SetViewport correctly constrains rendering to
// non-trivial sub-regions of the backbuffer. Draws a full-screen rect
// into four different viewports (one per quadrant) with distinct colors
// using PrimitiveBatch, then verifies pixel colors inside and outside each region.
class ViewportTest final : public GfxScreenshotTestCase
{
public:
	explicit ViewportTest(GfxContext* ctx)
	{
		if (!ctx)
		{
			return;
		}

		m_prim = std::make_unique<PrimitiveBatch>();
		m_ready = true;
	}

	void render(GfxContext* ctx) override
	{
		if (!m_ready)
		{
			return;
		}

		GfxPassDesc passDesc;
		passDesc.flags = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(0, 0, 0, 255);

		Gfx_BeginPass(ctx, passDesc);

		const Tuple2i fbSize = Platform_GetWindow()->getFramebufferSize();
		const float fullW = static_cast<float>(fbSize.x);
		const float fullH = static_cast<float>(fbSize.y);
		const float halfW = fullW / 2.0f;
		const float halfH = fullH / 2.0f;

		struct QuadrantDraw
		{
			GfxViewport viewport;
			GfxRect scissor;
			ColorRGBA8 color;
		};

		const QuadrantDraw draws[] = {
			// Top-left: red
			{
				GfxViewport(0.0f, 0.0f, halfW, halfH),
				GfxRect{0, 0, static_cast<int>(halfW), static_cast<int>(halfH)},
				ColorRGBA8(255, 0, 0),
			},
			// Top-right: green
			{
				GfxViewport(halfW, 0.0f, halfW, halfH),
				GfxRect{static_cast<int>(halfW), 0, static_cast<int>(fullW), static_cast<int>(halfH)},
				ColorRGBA8(0, 255, 0),
			},
			// Bottom-left: blue
			{
				GfxViewport(0.0f, halfH, halfW, halfH),
				GfxRect{0, static_cast<int>(halfH), static_cast<int>(halfW), static_cast<int>(fullH)},
				ColorRGBA8(0, 0, 255),
			},
			// Bottom-right: yellow
			{
				GfxViewport(halfW, halfH, halfW, halfH),
				GfxRect{static_cast<int>(halfW), static_cast<int>(halfH), static_cast<int>(fullW), static_cast<int>(fullH)},
				ColorRGBA8(255, 255, 0),
			},
		};

		for (const auto& draw : draws)
		{
			Gfx_SetViewport(ctx, draw.viewport);
			Gfx_SetScissorRect(ctx, draw.scissor);

			// begin2D with the viewport dimensions so the rect fills the entire viewport
			m_prim->begin2D(draw.viewport.w, draw.viewport.h);
			m_prim->drawRect(Box2(0.0f, 0.0f, draw.viewport.w, draw.viewport.h), draw.color);
			m_prim->end2D();
		}

		Gfx_EndPass(ctx);
	}

	TestResult validate(GfxContext*, const TestImage* image) override
	{
		if (!m_ready)
		{
			return TestResult::pass();
		}

		const TestResult screenshotCheck = validateScreenshot(image);
		if (!screenshotCheck.passed)
		{
			return screenshotCheck;
		}

		const u32 w = image->size.x;
		const u32 h = image->size.y;

		const u32 halfW = w / 2;
		const u32 halfH = h / 2;

		struct QuadrantCheck
		{
			u32 x;
			u32 y;
			ColorRGBA8 expected;
			const char* name;
		};

		// Sample center of each quadrant
		const QuadrantCheck checks[] = {
			{ halfW / 2,          halfH / 2,          ColorRGBA8(255, 0,   0),   "top-left (red)"       },
			{ halfW + halfW / 2,  halfH / 2,          ColorRGBA8(0,   255, 0),   "top-right (green)"    },
			{ halfW / 2,          halfH + halfH / 2,  ColorRGBA8(0,   0,   255), "bottom-left (blue)"   },
			{ halfW + halfW / 2,  halfH + halfH / 2,  ColorRGBA8(255, 255, 0),   "bottom-right (yellow)"},
		};

		for (const auto& check : checks)
		{
			const TestResult result = checkPixel(image, check.x, check.y, check.expected, check.name);
			if (!result.passed)
			{
				return result;
			}
		}

		// Verify boundary: pixels just inside each quadrant edge to confirm viewports don't bleed
		const u32 margin = 4;
		const QuadrantCheck boundaryChecks[] = {
			{ halfW - margin,     halfH - margin,     ColorRGBA8(255, 0,   0),   "top-left inner edge"      },
			{ halfW + margin,     halfH - margin,     ColorRGBA8(0,   255, 0),   "top-right inner edge"     },
			{ halfW - margin,     halfH + margin,     ColorRGBA8(0,   0,   255), "bottom-left inner edge"   },
			{ halfW + margin,     halfH + margin,     ColorRGBA8(255, 255, 0),   "bottom-right inner edge"  },
		};

		for (const auto& check : boundaryChecks)
		{
			if (check.x >= w || check.y >= h)
			{
				continue;
			}

			const TestResult result = checkPixel(image, check.x, check.y, check.expected, check.name);
			if (!result.passed)
			{
				return result;
			}
		}

		return TestResult::pass();
	}

private:
	std::unique_ptr<PrimitiveBatch> m_prim;
};

RUSH_REGISTER_TEST(ViewportTest, "gfx",
    "Validates non-trivial viewports by drawing into four quadrants with distinct colors.");

// Validates that Gfx_SetScissorRect correctly clips rendering.
// Draws a full-backbuffer green rect with a scissor restricting output to an
// inner rectangle, then verifies that pixels outside the scissor remain the
// clear color while pixels inside are green.
class ScissorTest final : public GfxScreenshotTestCase
{
public:
	explicit ScissorTest(GfxContext* ctx)
	{
		if (!ctx)
		{
			return;
		}

		m_prim = std::make_unique<PrimitiveBatch>();
		m_ready = true;
	}

	void render(GfxContext* ctx) override
	{
		if (!m_ready)
		{
			return;
		}

		const Tuple2i fbSize = Platform_GetWindow()->getFramebufferSize();
		const float fullW = static_cast<float>(fbSize.x);
		const float fullH = static_cast<float>(fbSize.y);

		// Scissor inset by 1/4 on each side, producing a centered rectangle
		const int insetX = fbSize.x / 4;
		const int insetY = fbSize.y / 4;

		GfxPassDesc passDesc;
		passDesc.flags = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(0, 0, 0, 255);

		Gfx_BeginPass(ctx, passDesc);

		// Full-backbuffer viewport, but scissor clips to the center half
		Gfx_SetViewport(ctx, GfxViewport(0.0f, 0.0f, fullW, fullH));
		Gfx_SetScissorRect(ctx, GfxRect{insetX, insetY, fbSize.x - insetX, fbSize.y - insetY});

		m_prim->begin2D(fullW, fullH);
		m_prim->drawRect(Box2(0.0f, 0.0f, fullW, fullH), ColorRGBA8(0, 255, 0));
		m_prim->end2D();

		Gfx_EndPass(ctx);
	}

	TestResult validate(GfxContext*, const TestImage* image) override
	{
		if (!m_ready)
		{
			return TestResult::pass();
		}

		const TestResult screenshotCheck = validateScreenshot(image);
		if (!screenshotCheck.passed)
		{
			return screenshotCheck;
		}

		const u32 w = image->size.x;
		const u32 h = image->size.y;

		const u32 insetX = w / 4;
		const u32 insetY = h / 4;
		const u32 margin = 4;

		const ColorRGBA8 green(0, 255, 0);
		const ColorRGBA8 black(0, 0, 0);

		struct PixelCheck
		{
			u32 x;
			u32 y;
			ColorRGBA8 expected;
			const char* name;
		};

		const PixelCheck checks[] = {
			// Inside scissor (center of the backbuffer)
			{ w / 2,                   h / 2,                   green, "center"                  },
			{ insetX + margin,         insetY + margin,         green, "scissor top-left inside"  },
			{ w - insetX - margin - 1, insetY + margin,         green, "scissor top-right inside" },
			{ insetX + margin,         h - insetY - margin - 1, green, "scissor bot-left inside"  },
			{ w - insetX - margin - 1, h - insetY - margin - 1, green, "scissor bot-right inside" },

			// Outside scissor (corners of the backbuffer)
			{ margin,                  margin,                  black, "top-left corner outside"     },
			{ w - margin - 1,          margin,                  black, "top-right corner outside"    },
			{ margin,                  h - margin - 1,          black, "bottom-left corner outside"  },
			{ w - margin - 1,          h - margin - 1,          black, "bottom-right corner outside" },

			// Just outside scissor edges
			{ insetX - margin,         h / 2,                   black, "left of scissor"  },
			{ w - insetX + margin,     h / 2,                   black, "right of scissor" },
			{ w / 2,                   insetY - margin,         black, "above scissor"    },
			{ w / 2,                   h - insetY + margin,     black, "below scissor"    },
		};

		for (const auto& check : checks)
		{
			if (check.x >= w || check.y >= h)
			{
				continue;
			}

			const TestResult result = checkPixel(image, check.x, check.y, check.expected, check.name);
			if (!result.passed)
			{
				return result;
			}
		}

		return TestResult::pass();
	}

private:
	std::unique_ptr<PrimitiveBatch> m_prim;
};

RUSH_REGISTER_TEST(ScissorTest, "gfx",
    "Validates scissor rect clips rendering to a centered sub-region.");
