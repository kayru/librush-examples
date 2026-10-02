#include "TestFramework.h"

#include <Common/Utils.h>
#include <Rush/GfxDevice.h>
#include <Rush/UtilLog.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace Test;
using namespace Rush;

namespace
{

constexpr u32 SpinThreadCount = 64 * 1024;
constexpr u32 MaxWaitFrames   = 60;

// Times are compared across clock sources (command buffer times and timestamps)
constexpr u64 ClockToleranceNs = 500'000;

struct ScopeCopy
{
	std::string    name;
	u32            parent  = ~0u;
	GfxContextType queue   = GfxContextType::Graphics;
	u64         beginNs = 0;
	u64         endNs   = 0;

	u64 duration() const { return endNs - beginNs; }
};

struct FrameCopy
{
	bool                   received = false;
	GfxFrameTimes          times;
	std::vector<ScopeCopy> scopes;
};

const char* levelName(GfxTimingLevel level)
{
	switch (level)
	{
	case GfxTimingLevel::Frame: return "Frame";
	case GfxTimingLevel::Scopes: return "Scopes";
	case GfxTimingLevel::Isolated: return "Isolated";
	}
	return "?";
}

}

// Records the same nested scopes once per timing level and checks what comes back
class GpuTimingTest final : public GfxTestCase
{
public:
	static constexpr GfxTimingLevel Levels[] = {GfxTimingLevel::Frame, GfxTimingLevel::Scopes, GfxTimingLevel::Isolated};
	static constexpr u32 LevelCount = RUSH_COUNTOF(Levels);

	explicit GpuTimingTest(GfxContext* ctx)
	{
		if (!ctx)
		{
			return;
		}

		GfxShaderSource shaderSource = loadShaderFromFile(RUSH_SHADER_NAME("TestGpuTiming.hlsl"));
		if (shaderSource.empty())
		{
			skip("Failed to load compute shader.");
			return;
		}
		m_shader = Gfx_CreateComputeShader(shaderSource);

		GfxComputePipelineDesc pipelineDesc;
		pipelineDesc.cs = m_shader.get();
		pipelineDesc.bindings.descriptorSets[0].rwBuffers  = 1;
		pipelineDesc.bindings.descriptorSets[0].stageFlags = GfxStageFlags::Compute;
		pipelineDesc.workGroupSize = {64, 1, 1};
		m_pipeline = Gfx_CreateComputePipeline(pipelineDesc);

		GfxBufferDesc bufferDesc(GfxBufferFlags::Storage, GfxFormat_Unknown, SpinThreadCount, sizeof(u32));
		bufferDesc.debugName = "TestGpuTiming";
		m_buffer = Gfx_CreateBuffer(bufferDesc);

		m_async = Gfx_GetCapability().asyncCompute;
		if (m_async)
		{
			bufferDesc.debugName = "TestGpuTimingAsync";
			m_asyncBuffer = Gfx_CreateBuffer(bufferDesc);
		}

		if (!m_shader.valid() || !m_pipeline.valid() || !m_buffer.valid())
		{
			skip("Failed to create compute resources.");
			return;
		}

		m_originalLevel = Gfx_GetTimingLevel();
		Gfx_SetTimingLevel(Levels[0]);
		m_ready = true;
	}

	~GpuTimingTest() override
	{
		if (m_ready)
		{
			Gfx_SetTimingLevel(m_originalLevel);
		}
	}

	bool needsMoreFrames() const override { return m_ready && !m_done; }

	void render(GfxContext* ctx, GfxTexture renderTarget) override
	{
		if (!m_ready)
		{
			logSkipOnce();
			return;
		}

		if (m_recordedCount < LevelCount)
		{
			m_recordedFrames[m_recordedCount] = Gfx_GetFrameIndex();
			recordWorkload(ctx, renderTarget);
			++m_recordedCount;
			Gfx_SetTimingLevel(m_recordedCount < LevelCount ? Levels[m_recordedCount] : m_originalLevel);
		}

		GfxFrameTimes times;
		while (Gfx_GetFrameTimes(times))
		{
			for (u32 i = 0; i < m_recordedCount; ++i)
			{
				if (times.frame == m_recordedFrames[i])
				{
					copyFrame(times, m_frames[i]);
				}
			}
		}

		bool allReceived = m_recordedCount == LevelCount;
		for (const FrameCopy& frame : m_frames)
		{
			allReceived &= frame.received;
		}
		m_done = allReceived || ++m_waitFrames > MaxWaitFrames;
	}

	TestResult validate(GfxContext*, const TestImage*) override
	{
		if (!m_ready)
		{
			return TestResult::pass();
		}

		const GfxCapability& caps = Gfx_GetCapability();

		for (u32 i = 0; i < LevelCount; ++i)
		{
			const FrameCopy& frame = m_frames[i];
			const char*      level = levelName(Levels[i]);
			if (!frame.received)
			{
				return TestResult::fail("%s: frame %llu never delivered", level, (unsigned long long)m_recordedFrames[i]);
			}
			if (frame.times.level != Levels[i])
			{
				return TestResult::fail("%s: frame recorded at level %s", level, levelName(frame.times.level));
			}

			const TestResult result = validateFrame(frame, caps, level);
			if (!result.passed)
			{
				logFrame(frame, level);
				return result;
			}
		}

		if (caps.timestamps)
		{
			// The spin dispatches depend on each other, so isolation should not change their cost much
			const u64 scoped   = m_frames[1].scopes[1].duration() + m_frames[1].scopes[2].duration();
			const u64 isolated = m_frames[2].scopes[1].duration() + m_frames[2].scopes[2].duration();
			if (isolated * 2 < scoped || scoped * 2 < isolated)
			{
				return TestResult::fail("Scopes and Isolated disagree: %.3f ms vs %.3f ms", scoped * 1e-6, isolated * 1e-6);
			}
		}

		return TestResult::pass();
	}

private:
	void spin(GfxContext* ctx, GfxBufferArg buffer)
	{
		Gfx_SetComputePipeline(ctx, m_pipeline);
		Gfx_SetStorageBuffer(ctx, 0, buffer);
		Gfx_Dispatch(ctx, SpinThreadCount / 64, 1, 1);
	}

	void recordWorkload(GfxContext* ctx, GfxTexture renderTarget)
	{
		{
			GfxScope outer(ctx, "Outer");
			{
				GfxScope scope(ctx, "A");
				spin(ctx, m_buffer);
			}
			{
				GfxScope scope(ctx, "A");
				spin(ctx, m_buffer);
			}
			{
				GfxScope scope(ctx, "Empty");
			}

			Gfx_AddImageBarrier(ctx, renderTarget, GfxResourceState_RenderTarget);
			GfxPassDesc pass;
			pass.color[0]       = renderTarget;
			pass.flags          = GfxPassFlags::ClearColor;
			pass.clearColors[0] = ColorRGBA(0.25f, 0.5f, 0.75f);
			pass.name           = "Pass";
			Gfx_BeginPass(ctx, pass);
			Gfx_EndPass(ctx);
		}

		// The name is copied, so it survives the caller's buffer
		char name[32];
		std::snprintf(name, sizeof(name), "Copied%u", m_recordedCount);
		Gfx_BeginScope(ctx, name);
		std::memset(name, 'x', sizeof(name) - 1);
		spin(ctx, m_buffer);
		Gfx_EndScope(ctx);

		if (m_async)
		{
			GfxContext* async = Gfx_BeginAsyncCompute(ctx);
			{
				GfxScope outer(async, "AsyncOuter");
				GfxScope scope(async, "AsyncA");
				spin(async, m_asyncBuffer);
			}
			Gfx_EndAsyncCompute(ctx, async);
		}
	}

	static void logFrame(const FrameCopy& frame, const char* level)
	{
		const GfxQueueTime& gfx = frame.times.graphics;
		RUSH_LOG("[Test] %s: status 0x%x, graphics busy %.3f ms, span %.3f ms", level, u32(frame.times.status),
		    gfx.busyNs * 1e-6, (gfx.endNs - gfx.beginNs) * 1e-6);
		for (const ScopeCopy& scope : frame.scopes)
		{
			RUSH_LOG("[Test]   %s (parent %d): +%.3f ms, %.3f ms", scope.name.c_str(), int(scope.parent),
			    (double(scope.beginNs) - double(gfx.beginNs)) * 1e-6, scope.duration() * 1e-6);
		}
	}

	static void copyFrame(const GfxFrameTimes& times, FrameCopy& out)
	{
		out.received = true;
		out.times    = times;
		out.times.scopes = {};
		out.scopes.clear();
		for (const GfxScopeTime& scope : times.scopes)
		{
			out.scopes.push_back({scope.name ? scope.name : "", scope.parent, scope.queue, scope.beginNs, scope.endNs});
		}
	}

	TestResult validateFrame(const FrameCopy& frame, const GfxCapability& caps, const char* level) const
	{
		const GfxFrameTimes& times = frame.times;
		if (!!(times.status & (GfxTimingStatus::Overflow | GfxTimingStatus::Invalid)))
		{
			return TestResult::fail("%s: unexpected status 0x%x", level, u32(times.status));
		}
		if (!!(times.status & GfxTimingStatus::Unsupported) == caps.timestamps)
		{
			return TestResult::fail("%s: Unsupported status does not match capabilities", level);
		}
		if (times.droppedFrames != 0)
		{
			return TestResult::fail("%s: %u frames dropped while polling every frame", level, times.droppedFrames);
		}

		auto checkQueue = [&](const GfxQueueTime& q, const char* queueName) {
			if (q.busyNs == 0 || q.endNs <= q.beginNs || q.busyNs > q.endNs - q.beginNs)
			{
				return TestResult::fail("%s: bad %s queue time: begin %llu end %llu busy %llu", level, queueName,
				    (unsigned long long)q.beginNs, (unsigned long long)q.endNs, (unsigned long long)q.busyNs);
			}
			return TestResult::pass();
		};

		const GfxQueueTime& gfx = times.graphics;
		const TestResult graphicsResult = checkQueue(gfx, "graphics");
		if (!graphicsResult.passed)
		{
			return graphicsResult;
		}

		const bool asyncTimed = m_async && caps.timestampsAsyncCompute;
		if (asyncTimed)
		{
			const TestResult computeResult = checkQueue(times.compute, "async compute");
			if (!computeResult.passed)
			{
				return computeResult;
			}
		}
		else if (times.compute.busyNs != 0)
		{
			return TestResult::fail("%s: async compute time reported without async compute timestamps", level);
		}

		const bool expectScopes = caps.timestamps && times.level != GfxTimingLevel::Frame;
		if (!expectScopes)
		{
			if (!frame.scopes.empty())
			{
				return TestResult::fail("%s: %zu scopes reported where none are timed", level, frame.scopes.size());
			}
			return TestResult::pass();
		}

		char copiedName[32];
		std::snprintf(copiedName, sizeof(copiedName), "Copied%u", u32(&frame - m_frames));

		struct Expected
		{
			const char*    name;
			u32            parent;
			GfxContextType queue;
		};
		const GfxContextType gq = GfxContextType::Graphics;
		const GfxContextType cq = GfxContextType::Compute;
		const Expected expected[] = {{"Outer", ~0u, gq}, {"A", 0, gq}, {"A", 0, gq}, {"Empty", 0, gq}, {"Pass", 0, gq},
		    {copiedName, ~0u, gq}, {"AsyncOuter", ~0u, cq}, {"AsyncA", 6, cq}};
		const size_t expectedCount = asyncTimed ? 8 : 6;

		if (frame.scopes.size() != expectedCount)
		{
			return TestResult::fail("%s: expected %zu scopes, got %zu", level, expectedCount, frame.scopes.size());
		}

		u64 previousEnd[u32(GfxContextType::count)] = {};
		for (size_t i = 0; i < frame.scopes.size(); ++i)
		{
			const ScopeCopy& scope = frame.scopes[i];
			if (scope.name != expected[i].name || scope.parent != expected[i].parent || scope.queue != expected[i].queue)
			{
				return TestResult::fail("%s: scope %zu is '%s' (parent %d, queue %u), expected '%s' (parent %d, queue %u)",
				    level, i, scope.name.c_str(), int(scope.parent), u32(scope.queue), expected[i].name,
				    int(expected[i].parent), u32(expected[i].queue));
			}
			if (scope.endNs < scope.beginNs)
			{
				return TestResult::fail("%s: scope %zu ends before it begins", level, i);
			}
			const GfxQueueTime& queueTime = scope.queue == cq ? times.compute : gfx;
			if (scope.beginNs + ClockToleranceNs < queueTime.beginNs || scope.endNs > queueTime.endNs + ClockToleranceNs)
			{
				return TestResult::fail("%s: scope %zu [%llu, %llu] outside its queue's span [%llu, %llu]", level, i,
				    (unsigned long long)scope.beginNs, (unsigned long long)scope.endNs,
				    (unsigned long long)queueTime.beginNs, (unsigned long long)queueTime.endNs);
			}
			if (scope.parent != ~0u)
			{
				const ScopeCopy& parent = frame.scopes[scope.parent];
				if (scope.beginNs < parent.beginNs || scope.endNs > parent.endNs)
				{
					return TestResult::fail("%s: scope %zu is not inside its parent", level, i);
				}
			}
			// Scopes follow each other on their queue's chained timeline; a first child starts with its parent
			u64& queueEnd = previousEnd[u32(scope.queue)];
			if (scope.parent + 1 != i && scope.beginNs < queueEnd)
			{
				return TestResult::fail("%s: scope %zu begins before the previous one ends", level, i);
			}
			queueEnd = scope.endNs;
		}

		const ScopeCopy* scopes = frame.scopes.data();
		if (scopes[1].duration() == 0 || scopes[2].duration() == 0 || scopes[5].duration() == 0
		    || (asyncTimed && scopes[7].duration() == 0))
		{
			return TestResult::fail("%s: spin scopes measured zero time", level);
		}
		if (scopes[3].duration() != 0)
		{
			return TestResult::fail("%s: empty scope measured %llu ns", level, (unsigned long long)scopes[3].duration());
		}
		const u64 childSum = scopes[1].duration() + scopes[2].duration() + scopes[3].duration() + scopes[4].duration();
		if (childSum > scopes[0].duration())
		{
			return TestResult::fail("%s: children sum %llu ns exceeds parent %llu ns", level,
			    (unsigned long long)childSum, (unsigned long long)scopes[0].duration());
		}

		return TestResult::pass();
	}

	GfxOwn<GfxComputeShader>   m_shader;
	GfxOwn<GfxComputePipeline> m_pipeline;
	GfxOwn<GfxBuffer>          m_buffer;
	GfxOwn<GfxBuffer>          m_asyncBuffer;
	bool                       m_async = false;
	GfxTimingLevel             m_originalLevel = GfxTimingLevel::Frame;

	u64       m_recordedFrames[LevelCount] = {};
	FrameCopy m_frames[LevelCount];
	u32       m_recordedCount = 0;
	u32       m_waitFrames    = 0;
	bool      m_done          = false;
};

RUSH_REGISTER_TEST(GpuTimingTest, "gfx",
	"Records nested, repeated, empty, pass and copied-name scopes at each timing level and validates the frame times.");
