// version 2.0.0
#include <cassert>
#include <iostream>
#include <cstring>
#include <Windows.h>

#include <unordered_map>

#include "Profiler.h"

#ifdef PROFILE_ON

struct ProfilingData
{
	WCHAR Tag[MAX_TAG_LENGTH];
	LARGE_INTEGER ElapsedTimeSum;
	LARGE_INTEGER ElapsedTimeMin;
	LARGE_INTEGER ElapsedTimeMax;
	LARGE_INTEGER LastBeginTick;
	DWORD64 CallCount;
};

ProfilingData* g_threadProfilingDatas[MAX_THREAD_COUNT];
size_t* g_profilingDataCounts[MAX_THREAD_COUNT];
DWORD g_threadIds[MAX_THREAD_COUNT];
DWORD g_threadCount = 0;

thread_local ProfilingData* g_profilingDatas;
thread_local size_t* g_profilingDataCount;

class ProfilerThreadInitializer
{
public:
	ProfilerThreadInitializer()
	{
		DWORD index = InterlockedIncrement(&g_threadCount) - 1;
		g_profilingDatas = new ProfilingData[MAX_PROFILING_DATA_COUNT];
		g_threadProfilingDatas[index] = g_profilingDatas;
		g_profilingDataCount = new size_t;
		*g_profilingDataCount = 0;
		g_profilingDataCounts[index] = g_profilingDataCount;
		g_threadIds[index] = GetCurrentThreadId();
	}
};

thread_local ProfilerThreadInitializer g_profilerThreadInitializer;

void ProfileBegin(const WCHAR* tag)
{
	for (size_t i = 0; i < *g_profilingDataCount; ++i)
	{
		if (wcscmp(g_profilingDatas[i].Tag, tag) == 0)
		{
			// 기존에 있는 데이터
			QueryPerformanceCounter(&g_profilingDatas[i].LastBeginTick);
			return;
		}
	}

	// 기존에 없는 데이터
	++(*g_profilingDataCount);
	wcscpy_s(g_profilingDatas[*g_profilingDataCount - 1].Tag, tag);
	g_profilingDatas[*g_profilingDataCount - 1].ElapsedTimeMin.QuadPart = INT64_MAX;
	g_profilingDatas[*g_profilingDataCount - 1].ElapsedTimeMax.QuadPart = 0;
	g_profilingDatas[*g_profilingDataCount - 1].ElapsedTimeSum.QuadPart = 0;
	g_profilingDatas[*g_profilingDataCount - 1].CallCount = 0;

	::QueryPerformanceCounter(&g_profilingDatas[*g_profilingDataCount - 1].LastBeginTick);
}

void ProfileEnd(const WCHAR* tag)
{
	for (size_t i = 0; i < *g_profilingDataCount; ++i)
	{
		if (wcscmp(g_profilingDatas[i].Tag, tag) == 0)
		{
			LARGE_INTEGER endTick;
			LARGE_INTEGER elapsedTick;

			::QueryPerformanceCounter(&endTick);

			elapsedTick.QuadPart = endTick.QuadPart - g_profilingDatas[i].LastBeginTick.QuadPart;

			g_profilingDatas[i].ElapsedTimeSum.QuadPart += elapsedTick.QuadPart;

			if (g_profilingDatas[i].ElapsedTimeMin.QuadPart > elapsedTick.QuadPart)
			{
				g_profilingDatas[i].ElapsedTimeMin.QuadPart = elapsedTick.QuadPart;
			}

			if (g_profilingDatas[i].ElapsedTimeMax.QuadPart < elapsedTick.QuadPart)
			{
				g_profilingDatas[i].ElapsedTimeMax.QuadPart = elapsedTick.QuadPart;
			}

			++g_profilingDatas[i].CallCount;
			return;
		}
	}

	assert(false);
}

void ProfileDataOutText(const WCHAR* fileName)
{
	FILE* file;
	LARGE_INTEGER frequency;

	::QueryPerformanceFrequency(&frequency);

	if (_wfopen_s(&file, fileName, L"w") == EINVAL || file == nullptr)
	{
		return;
	}

	fprintf_s(file, "--------------------------------------------------------------------------------------------------------------------------------------------------\n");
	fprintf_s(file, "%6s | %32s | %25s | %25s | %25s | %16s |\n", "Thread", "Name", "Average", "Min", "Max", "Call");
	fprintf_s(file, "--------------------------------------------------------------------------------------------------------------------------------------------------\n");

	for (DWORD threadIndex = 0; threadIndex < g_threadCount; ++threadIndex)
	{
		if (*g_profilingDataCounts[threadIndex] == 0)
		{
			continue;
		}

		for (size_t i = 0; i < *g_profilingDataCounts[threadIndex]; ++i)
		{
			float min = static_cast<float>(g_threadProfilingDatas[threadIndex][i].ElapsedTimeMin.QuadPart * 1'000'000) / frequency.QuadPart;
			float max = static_cast<float>(g_threadProfilingDatas[threadIndex][i].ElapsedTimeMax.QuadPart * 1'000'000) / frequency.QuadPart;
			LONGLONG sumExcludingOutliers = g_threadProfilingDatas[threadIndex][i].ElapsedTimeSum.QuadPart - g_threadProfilingDatas[threadIndex][i].ElapsedTimeMax.QuadPart - g_threadProfilingDatas[threadIndex][i].ElapsedTimeMin.QuadPart;

			float average;

			if (g_threadProfilingDatas[threadIndex][i].CallCount == 2)
			{
				average = (min + max) / 2.0f;
			}
			else
			{
				average = static_cast<float>((sumExcludingOutliers / (g_threadProfilingDatas[threadIndex][i].CallCount - 2)) * 1000000) / frequency.QuadPart;
			}

			fprintf_s(file, "%6d | %32ls | %16.4fmicrosecs | %16.4fmicrosecs | %16.4fmicrosecs | %16lld |\n", g_threadIds[threadIndex], g_threadProfilingDatas[threadIndex][i].Tag, average, min, max, g_threadProfilingDatas[threadIndex][i].CallCount);
		}

		fwprintf_s(file, L"--------------------------------------------------------------------------------------------------------------------------------------------------\n");
	}

	fclose(file);
}

#endif