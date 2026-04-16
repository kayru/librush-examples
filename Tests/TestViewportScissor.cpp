#include "TestFramework.h"

#include <Rush/GfxDevice.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/UtilLog.h>

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
		return TestResult::fail("%s at (%u,%u): got (%u,%u,%u) expected (%u,%u,%u)",
		    name, x, y, actual.r, actual.g, actual.b,
		    expected.r, expected.g, expected.b);
	}

	return TestResult::pass();
}

// Draw an isoceles triangle that fills the viewport:
// base spans the full bottom edge, apex touches the top center.
void drawViewportTriangle(PrimitiveBatch& prim, float w, float h, ColorRGBA8 color)
{
	prim.begin2D(w, h);
	prim.drawTriangle(
	    Vec2(w * 0.5f, 0.0f), // top center
	    Vec2(0.0f, h),         // bottom-left
	    Vec2(w, h),            // bottom-right
	    color);
	prim.end2D();
}

struct PixelCheck
{
	u32 x;
	u32 y;
	ColorRGBA8 expected;
	const char* name;
};

TestResult runPixelChecks(const TestImage* image, const PixelCheck* checks, size_t count)
{
	const u32 w = image->size.x;
	const u32 h = image->size.y;

	for (size_t i = 0; i < count; ++i)
	{
		const auto& check = checks[i];
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

} // namespace

// Draws a triangle into four quadrant viewports with distinct colors,
// then verifies pixel colors inside and outside each region.
// The triangle shape (vs a flat rect) tests that the viewport transform
// is applied correctly to non-trivial geometry.
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

	void render(GfxContext* ctx, GfxTexture renderTarget) override
	{
		if (!m_ready)
		{
			return;
		}

		GfxPassDesc passDesc;
		passDesc.color[0] = renderTarget;
		passDesc.flags = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(0, 0, 0, 255);

		Gfx_BeginPass(ctx, passDesc);

		const float fullW = static_cast<float>(kTestRenderWidth);
		const float fullH = static_cast<float>(kTestRenderHeight);
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
			drawViewportTriangle(*m_prim, draw.viewport.w, draw.viewport.h, draw.color);
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
		const u32 margin = 4;

		const ColorRGBA8 red(255, 0, 0);
		const ColorRGBA8 green(0, 255, 0);
		const ColorRGBA8 blue(0, 0, 255);
		const ColorRGBA8 yellow(255, 255, 0);
		const ColorRGBA8 black(0, 0, 0);

		const PixelCheck checks[] = {
			// Triangle apex: top-center of each quadrant viewport
			{ halfW / 2,         margin,              red,    "TL apex"         },
			{ halfW + halfW / 2, margin,              green,  "TR apex"         },
			{ halfW / 2,         halfH + margin,      blue,   "BL apex"         },
			{ halfW + halfW / 2, halfH + margin,      yellow, "BR apex"         },

			// Triangle base: bottom-center of each quadrant viewport
			{ halfW / 2,         halfH - margin,      red,    "TL base center"  },
			{ halfW + halfW / 2, halfH - margin,      green,  "TR base center"  },
			{ halfW / 2,         h - margin,           blue,   "BL base center"  },
			{ halfW + halfW / 2, h - margin,           yellow, "BR base center"  },

			// Bottom corners of each quadrant: should be colored (triangle base spans full width)
			{ margin,            halfH - margin,      red,    "TL bottom-left"  },
			{ halfW - margin,    halfH - margin,      red,    "TL bottom-right" },
			{ halfW + margin,    halfH - margin,      green,  "TR bottom-left"  },
			{ w - margin,        halfH - margin,      green,  "TR bottom-right" },

			// Top corners of each quadrant: should be clear (outside triangle)
			{ margin,            margin,              black,  "TL top-left corner"  },
			{ halfW - margin,    margin,              black,  "TL top-right corner" },
			{ halfW + margin,    margin,              black,  "TR top-left corner"  },
			{ w - margin,        margin,              black,  "TR top-right corner" },
		};

		return runPixelChecks(image, checks, sizeof(checks) / sizeof(checks[0]));
	}

private:
	std::unique_ptr<PrimitiveBatch> m_prim;
};

RUSH_REGISTER_TEST(ViewportTest, "gfx",
    "Validates quadrant viewports with triangle geometry.");

// Validates a viewport centered in the screen with non-trivial offsets
// from all edges of the render target. Renders a triangle into the
// centered viewport and checks that geometry is confined to it.
class ViewportOffsetTest final : public GfxScreenshotTestCase
{
public:
	explicit ViewportOffsetTest(GfxContext* ctx)
	{
		if (!ctx)
		{
			return;
		}

		m_prim = std::make_unique<PrimitiveBatch>();
		m_ready = true;
	}

	void render(GfxContext* ctx, GfxTexture renderTarget) override
	{
		if (!m_ready)
		{
			return;
		}

		const float fullW = static_cast<float>(kTestRenderWidth);
		const float fullH = static_cast<float>(kTestRenderHeight);

		// Viewport inset by 1/4 on each side
		const float insetX = fullW / 4.0f;
		const float insetY = fullH / 4.0f;
		const float vpW = fullW - 2.0f * insetX;
		const float vpH = fullH - 2.0f * insetY;

		GfxPassDesc passDesc;
		passDesc.color[0] = renderTarget;
		passDesc.flags = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(0, 0, 0, 255);

		Gfx_BeginPass(ctx, passDesc);

		Gfx_SetViewport(ctx, GfxViewport(insetX, insetY, vpW, vpH));
		Gfx_SetScissorRect(ctx, GfxRect{
		    static_cast<int>(insetX), static_cast<int>(insetY),
		    static_cast<int>(insetX + vpW), static_cast<int>(insetY + vpH)});
		drawViewportTriangle(*m_prim, vpW, vpH, ColorRGBA8(0, 255, 0));

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

		// Viewport spans [insetX .. w-insetX] x [insetY .. h-insetY]
		const u32 vpCenterX = w / 2;

		const PixelCheck checks[] = {
			// Triangle apex: top-center of viewport
			{ vpCenterX,             insetY + margin,     green, "apex"              },
			// Triangle base: bottom of viewport
			{ vpCenterX,             h - insetY - margin, green, "base center"       },
			{ insetX + margin,       h - insetY - margin, green, "base left"         },
			{ w - insetX - margin,   h - insetY - margin, green, "base right"        },

			// Top corners of viewport: should be clear (outside triangle)
			{ insetX + margin,       insetY + margin,     black, "vp top-left"       },
			{ w - insetX - margin,   insetY + margin,     black, "vp top-right"      },

			// Outside viewport: corners of the render target
			{ margin,                margin,              black, "rt top-left"        },
			{ w - margin,            margin,              black, "rt top-right"       },
			{ margin,                h - margin,           black, "rt bottom-left"    },
			{ w - margin,            h - margin,           black, "rt bottom-right"   },

			// Just outside viewport edges
			{ vpCenterX,             insetY - margin,     black, "above viewport"    },
			{ vpCenterX,             h - insetY + margin, black, "below viewport"    },
			{ insetX - margin,       h / 2,               black, "left of viewport"  },
			{ w - insetX + margin,   h / 2,               black, "right of viewport" },
		};

		return runPixelChecks(image, checks, sizeof(checks) / sizeof(checks[0]));
	}

private:
	std::unique_ptr<PrimitiveBatch> m_prim;
};

RUSH_REGISTER_TEST(ViewportOffsetTest, "gfx",
    "Validates a centered viewport with non-trivial offsets from all edges.");

// Validates that Gfx_SetScissorRect correctly clips rendering.
// Draws a full-backbuffer triangle with a scissor restricting output to a
// centered rectangle, then verifies that pixels outside the scissor remain
// the clear color while pixels inside show the triangle.
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

	void render(GfxContext* ctx, GfxTexture renderTarget) override
	{
		if (!m_ready)
		{
			return;
		}

		const float fullW = static_cast<float>(kTestRenderWidth);
		const float fullH = static_cast<float>(kTestRenderHeight);

		// Scissor inset by 1/4 on each side
		const int insetX = static_cast<int>(kTestRenderWidth) / 4;
		const int insetY = static_cast<int>(kTestRenderHeight) / 4;

		GfxPassDesc passDesc;
		passDesc.color[0] = renderTarget;
		passDesc.flags = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(0, 0, 0, 255);

		Gfx_BeginPass(ctx, passDesc);

		Gfx_SetViewport(ctx, GfxViewport(0.0f, 0.0f, fullW, fullH));
		Gfx_SetScissorRect(ctx, GfxRect{insetX, insetY,
		    static_cast<int>(kTestRenderWidth) - insetX,
		    static_cast<int>(kTestRenderHeight) - insetY});
		drawViewportTriangle(*m_prim, fullW, fullH, ColorRGBA8(0, 255, 0));

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

		const PixelCheck checks[] = {
			// Center of the scissor region: triangle covers this
			{ w / 2,                   h / 2,                   green, "center"                  },

			// Bottom of scissor: triangle base spans full width here
			{ insetX + margin,         h - insetY - margin - 1, green, "scissor bot-left inside"  },
			{ w - insetX - margin - 1, h - insetY - margin - 1, green, "scissor bot-right inside" },

			// Top of scissor: triangle apex is at top-center of the full viewport,
			// which is above the scissor, so the triangle is narrow here.
			// The center column should still be colored.
			{ w / 2,                   insetY + margin,         green, "scissor top-center inside"},

			// Top corners of scissor: triangle doesn't reach here (apex is narrow)
			{ insetX + margin,         insetY + margin,         black, "scissor top-left inside"  },
			{ w - insetX - margin - 1, insetY + margin,         black, "scissor top-right inside" },

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

		return runPixelChecks(image, checks, sizeof(checks) / sizeof(checks[0]));
	}

private:
	std::unique_ptr<PrimitiveBatch> m_prim;
};

RUSH_REGISTER_TEST(ScissorTest, "gfx",
    "Validates scissor rect clips triangle rendering to a centered sub-region.");
