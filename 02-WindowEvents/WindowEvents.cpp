#include <Rush/Platform.h>
#include <Rush/Rush.h>
#include <Rush/Window.h>
#include <Rush/GfxDevice.h>

#include <memory>
#include <stdio.h>

class WindowEventsApp : public Application
{
public:
	WindowEventsApp()
	{
		Window* window = Platform_GetWindow();
		m_events.setOwner(window);
	}

	void update()
	{
		for (const WindowEvent& e : m_events)
		{
			switch (e.type)
			{
			case WindowEventType_KeyDown:
				printf("WindowEventType_KeyDown: code=%d name=%s\n", e.code, toString(e.code));
				break;
			case WindowEventType_KeyUp:
				printf("WindowEventType_KeyUp: code=%d name=%s\n", e.code, toString(e.code));
				break;
			case WindowEventType_Resize:
				printf("WindowEventType_Resize: width=%d height=%d\n", e.width, e.height);
				break;
			case WindowEventType_Char: printf("WindowEventType_Char: character=%d\n", e.character); break;
			case WindowEventType_MouseDown:
				printf("WindowEventType_MouseDown: button=%d pos.x=%.2f pos.y=%.2f doubleclick=%d\n", e.button, e.pos.x,
				    e.pos.y, e.doubleClick);
				break;
			case WindowEventType_MouseUp:
				printf("WindowEventType_MouseUp: button=%d pos.x=%.2f pos.y=%.2f doubleclick=%d\n", e.button, e.pos.x,
				    e.pos.y, e.doubleClick);
				break;
			case WindowEventType_MouseMove:
				printf("WindowEventType_MouseMove: pos.x=%.2f pos.y=%.2f\n", e.pos.x, e.pos.y);
				break;
			case WindowEventType_Scroll:
				printf("WindowEventType_Scroll: scroll.x=%.2f scroll.y=%.2f\n", e.scroll.x, e.scroll.y);
				break;
			default: break;
			}
		}

		m_events.clear();

		GfxContext* gfxContext = Platform_GetGfxContext();

		GfxPassDesc passDesc;
		passDesc.flags          = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(11, 22, 33);
		Gfx_BeginPass(gfxContext, passDesc);
		Gfx_EndPass(gfxContext);
	}

private:
	WindowEventListener m_events;
};

int main()
{
	AppConfig cfg;

	cfg.name      = "WindowEvents";
	cfg.resizable = true;

#ifdef RUSH_DEBUG
	cfg.debug = true;
#endif

	return Platform_Main<WindowEventsApp>(cfg);
}
