#include "VirtualGamepad.h"

#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/MathCommon.h>
#include <Rush/UtilColor.h>
#include <Rush/Window.h>

#include <cmath>

namespace Rush
{

static void drawCircle(PrimitiveBatch* prim, const Vec2& center, float radius, const ColorRGBA8& color)
{
	constexpr int segments = 24;
	constexpr float step = 2.0f * 3.14159265f / (float)segments;

	for (int i = 0; i < segments; ++i)
	{
		const float a0 = (float)i * step;
		const float a1 = (float)(i + 1) * step;
		const Vec2 p0 = center + Vec2(std::cos(a0), std::sin(a0)) * radius;
		const Vec2 p1 = center + Vec2(std::cos(a1), std::sin(a1)) * radius;
		prim->drawTriangle(center, p0, p1, color);
	}
}

static Vec2 applyDeadZone(const Vec2& raw, float deadZone)
{
	const float len = raw.length();
	if (len < deadZone)
	{
		return Vec2(0.0f);
	}
	const float remapped = (len - deadZone) / (1.0f - deadZone);
	return raw * (remapped / len);
}

int VirtualGamepad::addButton(const char* label, const Vec2& position, float radius, const Vec2& axis)
{
	Button btn;
	btn.label = label;
	btn.position = position;
	btn.radius = radius;
	btn.axis = axis;
	m_buttons.push_back(btn);
	return (int)m_buttons.size() - 1;
}

bool VirtualGamepad::isButtonPressed(int index) const
{
	if (index < 0 || index >= (int)m_buttons.size())
	{
		return false;
	}
	return m_buttons[index].pressed;
}

float VirtualGamepad::getButtonValue(int index) const
{
	if (index < 0 || index >= (int)m_buttons.size())
	{
		return 0.0f;
	}
	return m_buttons[index].value;
}

int VirtualGamepad::addVerticalSlider(const Vec2& position, float width, float height)
{
	Button btn;
	btn.label = nullptr;
	btn.position = position;
	btn.radius = width * 0.5f;
	btn.height = height * 0.5f;
	btn.axis = Vec2(0.0f, -1.0f); // drag up = positive
	btn.shape = ControlShape::VerticalSlider;
	m_buttons.push_back(btn);
	return (int)m_buttons.size() - 1;
}

static const Window::TouchPoint* findTouch(ArrayView<const Window::TouchPoint> touches, u64 id)
{
	for (const auto& t : touches)
	{
		if (t.id == id)
		{
			return &t;
		}
	}
	return nullptr;
}

bool VirtualGamepad::isTouchClaimed(u64 id) const
{
	if (id == m_leftTouchId || id == m_rightTouchId)
	{
		return true;
	}
	for (const auto& btn : m_buttons)
	{
		if (btn.touchId == id)
		{
			return true;
		}
	}
	return false;
}

bool VirtualGamepad::update(Window* window)
{
	const Vec2 windowSize = window->getSizeFloat();
	const float halfX = windowSize.x * 0.5f;
	const auto touches = window->getTouches();

	// Release sticks whose touches ended
	if (m_leftTouchId && !findTouch(touches, m_leftTouchId))
	{
		m_leftTouchId = 0;
		m_leftOffset = Vec2(0.0f);
	}
	if (m_rightTouchId && !findTouch(touches, m_rightTouchId))
	{
		m_rightTouchId = 0;
		m_rightOffset = Vec2(0.0f);
	}

	// Release buttons whose touches ended, update analog values for active ones
	for (auto& btn : m_buttons)
	{
		if (!btn.touchId)
		{
			continue;
		}
		const auto* touch = findTouch(touches, btn.touchId);
		if (!touch)
		{
			btn.touchId = 0;
			btn.pressed = false;
			btn.value = 0.0f;
		}
		else
		{
			const Vec2 delta = touch->pos - btn.position;
			const float proj = delta.x * btn.axis.x + delta.y * btn.axis.y;
			const float range = (btn.shape == ControlShape::VerticalSlider) ? btn.height : btn.radius;
			btn.value = clamp(proj / range, -1.0f, 1.0f);
		}
	}

	// Assign new touches to controls
	for (const auto& touch : touches)
	{
		if (isTouchClaimed(touch.id))
		{
			continue;
		}

		// Check buttons first (they take priority over sticks)
		bool claimed = false;
		for (auto& btn : m_buttons)
		{
			if (btn.touchId)
			{
				continue;
			}
			const Vec2 delta = touch.pos - btn.position;
			bool hit = false;
			if (btn.shape == ControlShape::VerticalSlider)
			{
				if (std::abs(delta.x) <= btn.radius && std::abs(delta.y) <= btn.height)
				{
					hit = true;
				}
				else
				{
					const Vec2 topCenter(0.0f, -btn.height);
					const Vec2 botCenter(0.0f,  btn.height);
					hit = (delta - topCenter).length() <= btn.radius
					   || (delta - botCenter).length() <= btn.radius;
				}
			}
			else
			{
				hit = delta.length() <= btn.radius;
			}
			if (hit)
			{
				btn.touchId = touch.id;
				btn.pressed = true;
				claimed = true;
				break;
			}
		}
		if (claimed)
		{
			continue;
		}

		const bool inStickZone = touch.pos.y > windowSize.y * 0.4f;
		if (inStickZone && touch.pos.x < halfX && !m_leftTouchId)
		{
			m_leftTouchId = touch.id;
			m_leftOrigin = touch.pos;
			m_leftOffset = Vec2(0.0f);
		}
		else if (inStickZone && touch.pos.x >= halfX && !m_rightTouchId)
		{
			m_rightTouchId = touch.id;
			m_rightOrigin = touch.pos;
			m_rightOffset = Vec2(0.0f);
		}
	}

	// Update left stick
	if (const auto* touch = findTouch(touches, m_leftTouchId))
	{
		Vec2 delta = touch->pos - m_leftOrigin;
		const float len = delta.length();
		if (len > BaseRadius)
		{
			delta = delta * (BaseRadius / len);
		}
		m_leftOffset = delta;
	}

	// Update right stick
	if (const auto* touch = findTouch(touches, m_rightTouchId))
	{
		Vec2 delta = touch->pos - m_rightOrigin;
		const float len = delta.length();
		if (len > BaseRadius)
		{
			delta = delta * (BaseRadius / len);
		}
		m_rightOffset = delta;
	}

	// Compute normalized values with dead zone
	const Vec2 leftRaw = (BaseRadius > 0.0f) ? m_leftOffset * (1.0f / BaseRadius) : Vec2(0.0f);
	const Vec2 rightRaw = (BaseRadius > 0.0f) ? m_rightOffset * (1.0f / BaseRadius) : Vec2(0.0f);
	m_leftNorm = applyDeadZone(leftRaw, DeadZone);
	m_rightNorm = applyDeadZone(rightRaw, DeadZone);

	// Suppress normal touch-to-mouse when gamepad is active
	bool anyActive = m_leftTouchId || m_rightTouchId;
	for (const auto& btn : m_buttons)
	{
		anyActive = anyActive || btn.pressed;
	}

	if (anyActive)
	{
		MouseState& ms = const_cast<MouseState&>(window->getMouseState());
		ms.buttons[0] = false;
	}

	return anyActive;
}

void VirtualGamepad::draw(PrimitiveBatch* prim, BitmapFontRenderer* font, const Vec2& windowSize) const
{
	const ColorRGBA8 baseColor(255, 255, 255, 40);
	const ColorRGBA8 knobColor(255, 255, 255, 80);
	const ColorRGBA8 activeColor(255, 255, 255, 140);

	// Left stick
	{
		const Vec2 defaultPos(BaseRadius + 40.0f, windowSize.y - BaseRadius - 40.0f);
		const Vec2 center = (m_leftTouchId) ? m_leftOrigin : defaultPos;
		drawCircle(prim, center, BaseRadius, baseColor);
		drawCircle(prim, center + m_leftOffset, KnobRadius,
			(m_leftTouchId) ? activeColor : knobColor);
	}

	// Right stick
	{
		const Vec2 defaultPos(windowSize.x - BaseRadius - 40.0f, windowSize.y - BaseRadius - 40.0f);
		const Vec2 center = (m_rightTouchId) ? m_rightOrigin : defaultPos;
		drawCircle(prim, center, BaseRadius, baseColor);
		drawCircle(prim, center + m_rightOffset, KnobRadius,
			(m_rightTouchId) ? activeColor : knobColor);
	}

	// Buttons and sliders
	for (const auto& btn : m_buttons)
	{
		if (btn.shape == ControlShape::VerticalSlider)
		{
			const Vec2 top(btn.position.x, btn.position.y - btn.height);
			const Vec2 bot(btn.position.x, btn.position.y + btn.height);
			const float r = btn.radius;
			constexpr int halfSegs = 12;
			constexpr float pi = 3.14159265f;

			// Top half-circle (pi to 2*pi)
			for (int i = 0; i < halfSegs; ++i)
			{
				const float a0 = pi + (float)i * pi / (float)halfSegs;
				const float a1 = pi + (float)(i + 1) * pi / (float)halfSegs;
				const Vec2 p0 = top + Vec2(std::cos(a0), std::sin(a0)) * r;
				const Vec2 p1 = top + Vec2(std::cos(a1), std::sin(a1)) * r;
				prim->drawTriangle(top, p0, p1, baseColor);
			}

			// Rectangle body
			prim->drawRect(Box2(
				btn.position.x - r, btn.position.y - btn.height,
				btn.position.x + r, btn.position.y + btn.height), baseColor);

			// Bottom half-circle (0 to pi)
			for (int i = 0; i < halfSegs; ++i)
			{
				const float a0 = (float)i * pi / (float)halfSegs;
				const float a1 = (float)(i + 1) * pi / (float)halfSegs;
				const Vec2 p0 = bot + Vec2(std::cos(a0), std::sin(a0)) * r;
				const Vec2 p1 = bot + Vec2(std::cos(a1), std::sin(a1)) * r;
				prim->drawTriangle(bot, p0, p1, baseColor);
			}

			// Knob at current value
			const float knobY = btn.position.y - btn.value * btn.height;
			drawCircle(prim, Vec2(btn.position.x, knobY), KnobRadius, btn.pressed ? activeColor : knobColor);

			// Arrow indicators
			const float arrowSize = r * 0.5f;
			const ColorRGBA8 arrowColor(200, 200, 200, 160);
			prim->drawTriangle(
				Vec2(top.x, top.y - arrowSize),
				Vec2(top.x - arrowSize, top.y + arrowSize * 0.3f),
				Vec2(top.x + arrowSize, top.y + arrowSize * 0.3f),
				arrowColor);
			prim->drawTriangle(
				Vec2(bot.x, bot.y + arrowSize),
				Vec2(bot.x + arrowSize, bot.y - arrowSize * 0.3f),
				Vec2(bot.x - arrowSize, bot.y - arrowSize * 0.3f),
				arrowColor);
		}
		else
		{
			drawCircle(prim, btn.position, btn.radius, btn.pressed ? activeColor : baseColor);
			if (font && btn.label)
			{
				const Vec2 textSize = font->measure(btn.label);
				const Vec2 textPos = btn.position - textSize * 0.5f;
				font->draw(prim, textPos, btn.label, btn.pressed ? ColorRGBA8::White() : ColorRGBA8(200, 200, 200));
			}
		}
	}
}

}
