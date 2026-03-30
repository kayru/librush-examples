#pragma once

#include <Rush/Window.h>
#include <Rush/UtilArray.h>

namespace Rush
{

class PrimitiveBatch;
class BitmapFontRenderer;

class VirtualGamepad
{
public:
	enum class ControlShape
	{
		Circle,
		VerticalSlider,
	};

	struct Button
	{
		const char*  label    = nullptr;
		Vec2         position;
		float        radius   = 30.0f;
		float        height   = 0.0f;
		bool         pressed  = false;
		float        value    = 0.0f;
		Vec2         axis     = Vec2(0.0f, 1.0f);
		ControlShape shape    = ControlShape::Circle;
		u64          touchId  = 0;
	};

	int addButton(const char* label, const Vec2& position, float radius = 30.0f, const Vec2& axis = Vec2(0.0f, 1.0f));
	int addVerticalSlider(const Vec2& position, float width, float height);

	bool  isButtonPressed(int index) const;
	float getButtonValue(int index) const;

	bool update(Window* window);
	void draw(PrimitiveBatch* prim, BitmapFontRenderer* font, const Vec2& windowSize) const;

	Vec2 getLeftStick() const { return m_leftNorm; }
	Vec2 getRightStick() const { return m_rightNorm; }

	bool isLeftActive() const { return m_leftTouchId != 0; }
	bool isRightActive() const { return m_rightTouchId != 0; }

private:
	static constexpr float BaseRadius = 60.0f;
	static constexpr float KnobRadius = 25.0f;
	static constexpr float DeadZone   = 0.15f;

	bool isTouchClaimed(u64 id) const;

	Vec2 m_leftOrigin = Vec2(0.0f);
	Vec2 m_leftOffset = Vec2(0.0f);
	Vec2 m_leftNorm = Vec2(0.0f);
	u64  m_leftTouchId = 0;

	Vec2 m_rightOrigin = Vec2(0.0f);
	Vec2 m_rightOffset = Vec2(0.0f);
	Vec2 m_rightNorm = Vec2(0.0f);
	u64  m_rightTouchId = 0;

	DynamicArray<Button> m_buttons;
};

}
