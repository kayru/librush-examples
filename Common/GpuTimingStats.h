#pragma once

#include "Utils.h"

#include <Rush/GfxDevice.h>

#include <string>
#include <vector>

namespace Rush
{

// Drains Gfx_GetFrameTimes and keeps moving averages of GPU busy time and of each scope path
class GpuTimingStats
{
public:
	template <typename F> void update(F&& onFrame)
	{
		GfxFrameTimes frame;
		while (Gfx_GetFrameTimes(frame))
		{
			onFrame(frame);
			addFrame(frame);
		}
	}

	void update()
	{
		update([](const GfxFrameTimes&) {});
	}

	double busySeconds() const { return m_busy.get(); }

	std::string format() const;

private:
	void addFrame(const GfxFrameTimes& frame);

	struct Entry
	{
		std::string               path;
		std::string               name;
		u32                       depth = 0;
		u64                       lastSeenFrame = 0;
		double                    frameSeconds  = 0.0;
		MovingAverage<double, 60> seconds;
	};

	std::vector<Entry>        m_entries;
	MovingAverage<double, 60> m_busy;
	MovingAverage<double, 60> m_computeBusy;
	bool                      m_hasCompute = false;
	MovingAverage<double, 60> m_unscoped;
	GfxTimingStatus           m_status = GfxTimingStatus::None;
	bool                      m_hasScopes = false;
};

}
