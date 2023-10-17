#pragma once

#include <Windows.h>

class CpuUsageMonitor
{
public:
	CpuUsageMonitor(HANDLE hProcess)
	{
		Init(hProcess);
	}

	CpuUsageMonitor(void)
	{
		Init(GetCurrentProcess());
	}

	void Init(HANDLE hProcess)
	{
		mhProcess = hProcess;

		// 프로세스의 개수 얻어오기
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);
		mProcessorCount = systemInfo.dwNumberOfProcessors;

		UpdateCpuTime();
	}

public:

	float GetProcessorTimeTotal() { return mProcessorTotal; }
	float GetProcessorTimeUser() { return mProcessorUser; }
	float GetProcessorTimeKernel() { return mProcessorKernel; }

	float GetProcessTimeTotal() { return mProcessTotal; }
	float GetProcessTimeUser() { return mProcessUser; }
	float GetProcessTimeKernel() { return mProcessKernel; }

	void UpdateCpuTime()
	{
		/****************************** 프로세서 사용률 갱신 ******************************/

		// 1. 시스템 사용 시간을 얻어온다
		ULARGE_INTEGER systemTimeKernel; // 커널 사용 시간은 Idle 시간이 포함되어 있음
		ULARGE_INTEGER systemTimeUser;
		ULARGE_INTEGER systemTimeIdle;
		if (!GetSystemTimes((PFILETIME)&systemTimeIdle, (PFILETIME)&systemTimeKernel, (PFILETIME)&systemTimeUser))
		{
			return;
		}

		// 2. 차이 시간을 구한다
		ULONGLONG systemTimeDiffKernel = systemTimeKernel.QuadPart - mProcessorLastKernel.QuadPart;
		ULONGLONG systemTimeDiffUser = systemTimeUser.QuadPart - mProcessorLastUser.QuadPart;
		ULONGLONG systemTimeDiffIdle = systemTimeIdle.QuadPart - mProcessorLastIdle.QuadPart;

		// 3. 총 CPU 사용 시간 = 커널 시간 + 유저 시간
		ULONGLONG systemTimeTotal = systemTimeDiffKernel + systemTimeDiffUser;
		if (systemTimeTotal == 0)
		{
			mProcessorKernel = 0.0f;
			mProcessorUser = 0.0f;
			mProcessorTotal = 0.0f;
		}
		else
		{
			// 커널 사용 시간에 Idle 시간이 포함되어 있으므로 빼서 계산함
			mProcessorKernel = (float)((double)(systemTimeDiffKernel - systemTimeDiffIdle) / systemTimeTotal * 100.0f);
			mProcessorUser = (float)((double)systemTimeDiffUser / systemTimeTotal * 100.0f);
			mProcessorTotal = (float)((double)(systemTimeTotal - systemTimeDiffIdle) / systemTimeTotal * 100.0f);
		}

		// 4. Last 시간 저장
		mProcessorLastKernel = systemTimeKernel;
		mProcessorLastUser = systemTimeUser;
		mProcessorLastIdle = systemTimeIdle;

		/****************************** 프로세스 사용률 갱신 ******************************/

		// 1. 현재 100ns 단위 시간을 구한다
		ULARGE_INTEGER nowTime;
		GetSystemTimeAsFileTime((LPFILETIME)&nowTime);

		// 2. 프로세스 사용 시간 구하기
		ULARGE_INTEGER ignore;
		ULARGE_INTEGER processUsageTimeKernel;
		ULARGE_INTEGER processUsageTimeUser;
		GetProcessTimes(mhProcess, (LPFILETIME)&ignore, (LPFILETIME)&ignore, (LPFILETIME)&processUsageTimeKernel, (LPFILETIME)&processUsageTimeUser);

		// 3. 차이 시간을 구한다
		ULONGLONG timeDiffKernel = processUsageTimeKernel.QuadPart - mProcessLastKernel.QuadPart;
		ULONGLONG timeDiffUser = processUsageTimeUser.QuadPart - mProcessLastUser.QuadPart;
		ULONGLONG timeDiffTime = nowTime.QuadPart - mProcessLastTime.QuadPart;

		// 4. 총 CPU 사용 시간 = 커널 시간 + 유저 시간
		ULONGLONG timeTotal = timeDiffKernel + timeDiffUser;

		mProcessKernel = (float)(timeDiffKernel / (double)mProcessorCount / (double)timeDiffTime * 100.0f);
		mProcessUser = (float)(timeDiffUser / (double)mProcessorCount / (double)timeDiffTime * 100.0f);
		mProcessTotal = (float)(timeTotal / (double)mProcessorCount / (double)timeDiffTime * 100.0f);

		// 5. Last 시간 저장
		mProcessLastTime = nowTime;
		mProcessLastKernel = processUsageTimeKernel;
		mProcessLastUser = processUsageTimeUser;
	}
private:
	HANDLE mhProcess{};
	int mProcessorCount{};

	/************************ 단위 시간에 대한 % 결과들 ************************/

	float mProcessorKernel{};
	float mProcessorUser{};
	float mProcessorTotal{};

	float mProcessKernel{};
	float mProcessUser{};
	float mProcessTotal{};

	/************** UpdateCpuTime() 호출 시 계산을 위한 이전 값들 **************/

	ULARGE_INTEGER mProcessorLastKernel{};
	ULARGE_INTEGER mProcessorLastUser{};
	ULARGE_INTEGER mProcessorLastIdle{};

	ULARGE_INTEGER mProcessLastKernel{};
	ULARGE_INTEGER mProcessLastUser{};
	ULARGE_INTEGER mProcessLastTime{};
};