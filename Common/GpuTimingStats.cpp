#include "GpuTimingStats.h"

#include <algorithm>
#include <cstdio>

namespace Rush
{

void GpuTimingStats::addFrame(const GfxFrameTimes& frame)
{
	const double busy = double(frame.graphics.busyNs) * 1e-9;
	m_busy.add(busy);
	m_computeBusy.add(double(frame.compute.busyNs) * 1e-9);
	m_hasCompute |= frame.compute.busyNs != 0;
	m_status    = frame.status;
	m_hasScopes = !frame.scopes.empty();

	std::vector<std::string> paths(frame.scopes.size());
	std::vector<u32>         depths(frame.scopes.size(), 0);
	std::vector<Entry>       ordered;
	ordered.reserve(m_entries.size());

	double topLevelSeconds = 0.0;
	for (size_t i = 0; i < frame.scopes.size(); ++i)
	{
		const GfxScopeTime& scope    = frame.scopes[i];
		const double        duration = double(scope.endNs - scope.beginNs) * 1e-9;
		if (scope.parent == ~0u)
		{
			const bool graphics = scope.queue == GfxContextType::Graphics;
			paths[i] = graphics ? std::string(scope.name) : std::string("Async: ") + scope.name;
			if (graphics)
			{
				topLevelSeconds += duration;
			}
		}
		else
		{
			paths[i]  = paths[scope.parent] + "/" + scope.name;
			depths[i] = depths[scope.parent] + 1;
		}

		auto matches = [&](const Entry& e) { return e.path == paths[i]; };
		auto placed  = std::find_if(ordered.begin(), ordered.end(), matches);
		if (placed == ordered.end())
		{
			auto existing = std::find_if(m_entries.begin(), m_entries.end(), matches);
			if (existing != m_entries.end())
			{
				ordered.push_back(std::move(*existing));
				m_entries.erase(existing);
			}
			else
			{
				Entry entry;
				entry.path  = paths[i];
				entry.name  = scope.parent == ~0u ? paths[i] : std::string(scope.name);
				entry.depth = depths[i];
				ordered.push_back(std::move(entry));
			}
			placed = ordered.end() - 1;
		}
		placed->frameSeconds += duration;
		placed->lastSeenFrame = frame.frame;
	}

	for (Entry& entry : m_entries)
	{
		if (frame.frame - entry.lastSeenFrame <= 120)
		{
			ordered.push_back(std::move(entry));
		}
	}
	m_entries = std::move(ordered);

	for (Entry& entry : m_entries)
	{
		entry.seconds.add(entry.frameSeconds);
		entry.frameSeconds = 0.0;
	}

	m_unscoped.add(std::max(0.0, busy - topLevelSeconds));
}

std::string GpuTimingStats::format() const
{
	std::string result;
	char        line[256];

	if (!!(m_status & GfxTimingStatus::Unsupported))
	{
		result += "> Scopes: unsupported\n";
		return result;
	}
	if (!m_hasScopes)
	{
		return result;
	}

	for (const Entry& entry : m_entries)
	{
		std::snprintf(line, sizeof(line), "> %*s%s: %.2f ms\n", int(entry.depth * 2), "", entry.name.c_str(),
		    entry.seconds.get() * 1000.0);
		result += line;
	}
	std::snprintf(line, sizeof(line), "> Unscoped: %.2f ms\n", m_unscoped.get() * 1000.0);
	result += line;
	if (m_hasCompute)
	{
		std::snprintf(line, sizeof(line), "> Async compute busy: %.2f ms\n", m_computeBusy.get() * 1000.0);
		result += line;
	}

	if (!!(m_status & (GfxTimingStatus::Overflow | GfxTimingStatus::Invalid)))
	{
		result += "> (incomplete timing data)\n";
	}
	return result;
}

}
