#include "MonitoringServer.h"

#include "MonitorProtocol.h"
#include "NetLibrary/Logger/Logger.h"

#include <Psapi.h>
#include <strsafe.h>
#include <process.h>


#pragma comment(lib,"Pdh.lib")

void MonitoringServer::Start(const uint16_t port, const uint32_t maxSessionCount, const uint32_t iocpConcurrentThreadCount, const uint32_t iocpWorkerThreadCount)
{
	NetServer::Start(port, maxSessionCount, iocpConcurrentThreadCount, iocpWorkerThreadCount);

	if (mLogDBWriteIntervalMinutes > 0)
	{
		if (false == mDBConnection.Connect())
		{
			LOGF(ELogLevel::Assert, L"DB Connect Failed (error = %d)", mDBConnection.GetLastError());
			CrashDump::Crash();
			return;
		}

		LOGF(ELogLevel::System, L"DB successfully Connected");
	}

	for (int i = 0; i < en_PACKET_SS_MONITOR_DATA_UPDATE::MAX; ++i)
	{
		mMonitorInfos[i].Min = INT_MAX;
	}

	mMachineMonitorThread = reinterpret_cast<HANDLE>(::_beginthreadex(nullptr, 0, machineMonitorThread, this, 0, nullptr));
	
	if (mLogDBWriteIntervalMinutes > 0)
	{
		mLogDBWriteThread = reinterpret_cast<HANDLE>(::_beginthreadex(nullptr, 0, logDBWriteThread, this, 0, nullptr));
	}

	if (mTimeoutCheckInterval > 0)
	{
		mTimeoutThread = reinterpret_cast<HANDLE>(::_beginthreadex(nullptr, 0, timeoutThread, this, 0, nullptr));
	}

	mShutdownEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	ASSERT_LIVE(mShutdownEvent != INVALID_HANDLE_VALUE, L"mShutdownEvent create failed");

	mMachineMonitorEvent = ::CreateWaitableTimer(NULL, FALSE, NULL);
	ASSERT_LIVE(mMachineMonitorEvent != INVALID_HANDLE_VALUE, L"mMachineMonitorEvent create failed");

	LARGE_INTEGER machineMonitorEventTimerTime{};
	machineMonitorEventTimerTime.QuadPart = -1 * (10'000 * static_cast<LONGLONG>(1'000));
	::SetWaitableTimer(mMachineMonitorEvent, &machineMonitorEventTimerTime, 1'000, nullptr, nullptr, FALSE);

	if (mLogDBWriteIntervalMinutes > 0)
	{
		mLogDBWriteEvent = ::CreateWaitableTimer(NULL, FALSE, NULL);
		ASSERT_LIVE(mLogDBWriteEvent != INVALID_HANDLE_VALUE, L"mLogDBWriteEvent create failed");

		LARGE_INTEGER logDBWriteEventTimerTime{};
		logDBWriteEventTimerTime.QuadPart = -1 * (10'000 * static_cast<LONGLONG>(mLogDBWriteIntervalMinutes * 60 * 1'000));
		::SetWaitableTimer(mLogDBWriteEvent, &logDBWriteEventTimerTime, mLogDBWriteIntervalMinutes * 60 * 1'000, nullptr, nullptr, FALSE);
	}

	if (mTimeoutCheckInterval > 0)
	{
		mTimeoutCheckEvent = ::CreateWaitableTimer(NULL, FALSE, NULL);
		ASSERT_LIVE(mTimeoutCheckEvent != INVALID_HANDLE_VALUE, L"mTimeoutCheckEvent create failed");

		LARGE_INTEGER timeoutCheckEventTimerTime{};
		timeoutCheckEventTimerTime.QuadPart = -1 * (10'000 * static_cast<LONGLONG>(mTimeoutCheckInterval));
		::SetWaitableTimer(mTimeoutCheckEvent, &timeoutCheckEventTimerTime, mTimeoutCheckInterval, nullptr, nullptr, FALSE);

	}
	
	mbIsRunning = true;
}

void MonitoringServer::Shutdown(void)
{
	mbIsRunning = false;

	::SetEvent(mShutdownEvent);

	::WaitForSingleObject(mMachineMonitorThread, INFINITE);

	if (mLogDBWriteIntervalMinutes > 0)
	{
		::WaitForSingleObject(mLogDBWriteThread, INFINITE);
		::CloseHandle(mLogDBWriteThread);
		::CloseHandle(mLogDBWriteEvent);
		mDBConnection.Disconnect();
	}

	if (mTimeoutCheckInterval > 0)
	{
		::WaitForSingleObject(mTimeoutThread, INFINITE);
		::CloseHandle(mTimeoutThread);
		::CloseHandle(mTimeoutCheckEvent);
	}

	::CloseHandle(mMachineMonitorThread);
	::CloseHandle(mShutdownEvent);
	::CloseHandle(mMachineMonitorEvent);

	NetServer::Shutdown();
}

void MonitoringServer::OnAccept(const uint64_t sessionID)
{
	mUnknownsLock.lock();
	mUnknowns.insert(std::make_pair(sessionID, ::timeGetTime()));
	mUnknownsLock.unlock();
}

void MonitoringServer::OnReceive(const uint64_t sessionID, Serializer* packet)
{
	WORD messageType;

	*packet >> messageType;

	switch (messageType)
	{
	case en_PACKET_TYPE::en_PACKET_CS_MONITOR_TOOL_REQ_LOGIN:
	{
		char loginSessionKey[32]{};

		constexpr uint32_t PACKET_SIZE = sizeof(messageType) + sizeof(loginSessionKey);
		if (packet->GetUseSize() != PACKET_SIZE)
		{
			Disconnect(sessionID);
			break;
		}

		packet->GetByte((char*)loginSessionKey, sizeof(loginSessionKey));

		process_CS_MONITOR_TOOL_REQ_LOGIN(sessionID, loginSessionKey);
	}
	break;
	case en_PACKET_TYPE::en_PACKET_SS_MONITOR_LOGIN:
	{
		int serverNo;

		constexpr uint32_t PACKET_SIZE = sizeof(messageType) + sizeof(serverNo);
		if (packet->GetUseSize() != PACKET_SIZE)
		{
			Disconnect(sessionID);
			break;
		}

		*packet >> serverNo;
		process_SS_MONITOR_LOGIN(sessionID, serverNo);
	}
	break;
	case en_PACKET_TYPE::en_PACKET_SS_MONITOR_DATA_UPDATE:
	{
		BYTE dataType;
		int dataValue;
		int timeStamp;

		constexpr uint32_t PACKET_SIZE = sizeof(messageType) + sizeof(dataType) + sizeof(dataValue) + sizeof(timeStamp);
		if (packet->GetUseSize() != PACKET_SIZE)
		{
			Disconnect(sessionID);
			break;
		}

		*packet >> dataType >> dataValue >> timeStamp;

		process_SS_MONITOR_DATA_UPDATE(sessionID, dataType, dataValue, timeStamp);
	}
	break;
	default:
		Disconnect(sessionID);
	}

	Serializer::Free(packet);
}

void MonitoringServer::OnRelease(const uint64_t sessionID)
{
	mUnknownsLock.lock();
	mUnknowns.erase(sessionID);
	mUnknownsLock.unlock();

	mClientsLock.lock();
	mClients.remove(sessionID);
	mClientsLock.unlock();

	mServersLock.lock();
	mServers.erase(sessionID);
	mServersLock.unlock();
}

Serializer* MonitoringServer::Create_CS_MONITOR_TOOL_RES_LOGIN(const BYTE status)
{
	Serializer* packet = Serializer::Alloc();

	*packet << (WORD)en_PACKET_CS_MONITOR_TOOL_RES_LOGIN << status;

	return packet;
}

Serializer* MonitoringServer::Create_CS_MONITOR_TOOL_DATA_UPDATE(const BYTE serverNo, const BYTE dataType, const int dataValue, const int timeStamp)
{
	Serializer* packet = Serializer::Alloc();

	*packet << (WORD)en_PACKET_CS_MONITOR_TOOL_DATA_UPDATE << serverNo << dataType << dataValue << timeStamp;

	return packet;
}

void MonitoringServer::process_CS_MONITOR_TOOL_REQ_LOGIN(const uint64_t sessionID, const char loginSessionKey[])
{
	mUnknownsLock.lock();
	mUnknowns.erase(sessionID);
	mUnknownsLock.unlock();

	Serializer* CS_MONITOR_TOOL_RES_LOGIN;

	if (0 == strncmp(loginSessionKey, LOGIN_SESSION_KEY, LOGIN_SESSION_KEY_LENGTH))
	{
		mClientsLock.lock();
		mClients.push_back(sessionID);
		mClientsLock.unlock();

		CS_MONITOR_TOOL_RES_LOGIN = Create_CS_MONITOR_TOOL_RES_LOGIN(en_PACKET_CS_MONITOR_TOOL_RES_LOGIN::dfMONITOR_TOOL_LOGIN_OK);
		SendPacket(sessionID, CS_MONITOR_TOOL_RES_LOGIN);
	}
	else
	{
		LOGF(ELogLevel::System, L"client sessionID = %llu login failed", sessionID);
		CS_MONITOR_TOOL_RES_LOGIN = Create_CS_MONITOR_TOOL_RES_LOGIN(en_PACKET_CS_MONITOR_TOOL_RES_LOGIN::dfMONITOR_TOOL_LOGIN_ERR_SESSIONKEY);
		SendAndDisconnect(sessionID, CS_MONITOR_TOOL_RES_LOGIN);
	}

	Serializer::Free(CS_MONITOR_TOOL_RES_LOGIN);
}

void MonitoringServer::process_SS_MONITOR_LOGIN(const uint64_t sessionID, const int serverNo)
{
	mUnknownsLock.lock();
	mUnknowns.erase(sessionID);
	mUnknownsLock.unlock();

	mServersLock.lock();
	mServers.insert(std::make_pair(sessionID, serverNo));
	mServersLock.unlock();
}

void MonitoringServer::process_SS_MONITOR_DATA_UPDATE(const uint64_t sessionID, const BYTE dataType, const int dataValue, const int timeStamp)
{
	int32_t serverNo;
	
	// 서버 번호 찾기
	{
		mServersLock.lock_shared();

		auto it = mServers.find(sessionID);
		
		if (it == mServers.end())
		{
			LOGF(ELogLevel::System, L"ID = %d, not logged in server sent received monitor data", sessionID);
			Disconnect(sessionID);
			mServersLock.unlock_shared();
			return;
		}

		serverNo = it->second;

		mServersLock.unlock_shared();
	}

	// 취합된 모니터링 정보 갱신
	{
		mMonitorInfos[dataType].Lock.lock();

		mMonitorInfos[dataType].ServerNo = serverNo;
		mMonitorInfos[dataType].ValueSum += dataValue;
		mMonitorInfos[dataType].Count++;

		if (dataValue < mMonitorInfos[dataType].Min)
		{
			mMonitorInfos[dataType].Min = dataValue;
		}

		if (dataValue > mMonitorInfos[dataType].Max)
		{
			mMonitorInfos[dataType].Max = dataValue;
		}

		mMonitorInfos[dataType].Lock.unlock();
	}

	Broadcast_MONITOR_DATA_UPDATE(serverNo, dataType, dataValue, timeStamp);
}

void MonitoringServer::Broadcast_MONITOR_DATA_UPDATE(const BYTE serverNo, const BYTE dataType, const int32_t value, const int32_t timeStamp)
{
	Serializer* CS_MONITOR_TOOL_DATA_UPDATE = Create_CS_MONITOR_TOOL_DATA_UPDATE(serverNo, dataType, value, timeStamp);

	mClientsLock.lock_shared();
	for (const uint64_t client : mClients)
	{
		SendPacket(client, CS_MONITOR_TOOL_DATA_UPDATE);
	}
	mClientsLock.unlock_shared();

	Serializer::Free(CS_MONITOR_TOOL_DATA_UPDATE);
}

unsigned int MonitoringServer::machineMonitorThread(void* server)
{
	LOGF(ELogLevel::System, L"Machine Monitor Thread Start (ID : %d)", ::GetCurrentThreadId());

	MonitoringServer* monitoringServer = reinterpret_cast<MonitoringServer*>(server);

#pragma region 모니터링 서버 정보 얻기 PDH 사전 작업

	PDH_HQUERY queryHandle;

	PDH_HCOUNTER nonpagedBytes;
	PDH_HCOUNTER availableMBytes;

	// PDH 쿼리 핸들 생성
	PdhOpenQuery(NULL, NULL, &queryHandle);

	// PDH 리소스 카운터 생성
	ASSERT_LIVE(PdhAddCounter(queryHandle, L"\\Memory\\Pool Nonpaged Bytes", NULL, &nonpagedBytes) == ERROR_SUCCESS, L"PdhAddCounter Error");
	ASSERT_LIVE(PdhAddCounter(queryHandle, L"\\Memory\\Available MBytes", NULL, &availableMBytes) == ERROR_SUCCESS, L"PdhAddCounter Error");

	DWORD counterSize = 0;
	DWORD interfaceSize = 0;

	// 버퍼 길이 확인
	::PdhEnumObjectItems(NULL, NULL, L"Network Interface", NULL, &counterSize, NULL, &interfaceSize, PERF_DETAIL_WIZARD, 0);
	WCHAR* counters = new WCHAR[counterSize];
	WCHAR* interfaces = new WCHAR[interfaceSize];

	if (::PdhEnumObjectItems(NULL, NULL, L"Network Interface", counters, &counterSize, interfaces, &interfaceSize, PERF_DETAIL_WIZARD, 0) != ERROR_SUCCESS)
	{
		delete[] counters;
		delete[] interfaces;
		ASSERT_LIVE(false, L"PdhEnumObjectItems() Error");
	}

	constexpr int ETHERNET_MAX_COUNT = 8;
	EthernetPdhInfo ethernets[ETHERNET_MAX_COUNT]; // 랜카드 별 PDH 정보

	WCHAR* ptr_interfaces = interfaces;
	int ethernetCount = 0;

	// interfaces 에서 문자열 단위로 끊으면서 처리
	for (int i = 0; *ptr_interfaces != L'\0' && i < ETHERNET_MAX_COUNT; i++)
	{
		constexpr int QUERY_MAX_LENGTH = 1024;

		ethernetCount++;

		WCHAR query[QUERY_MAX_LENGTH];

		ethernets[i].Name[0] = L'\0';
		wcscpy_s(ethernets[i].Name, ptr_interfaces);

		query[0] = L'\0';
		::StringCbPrintf(query, sizeof(WCHAR) * QUERY_MAX_LENGTH, L"\\Network Interface(%s)\\Bytes Received/sec", ptr_interfaces);
		::PdhAddCounter(queryHandle, query, NULL, &ethernets[i].RecvBytes);

		query[0] = L'\0';
		::StringCbPrintf(query, sizeof(WCHAR) * QUERY_MAX_LENGTH, L"\\Network Interface(%s)\\Bytes Sent/sec", ptr_interfaces);
		::PdhAddCounter(queryHandle, query, NULL, &ethernets[i].SendBytes);

		ptr_interfaces += wcslen(ptr_interfaces) + 1;
	}

	// 첫 갱신
	::PdhCollectQueryData(queryHandle);

#pragma endregion

	HANDLE waitEvents[2];
	waitEvents[0] = monitoringServer->mShutdownEvent;
	waitEvents[1] = monitoringServer->mMachineMonitorEvent;

	for (;;)
	{
		::WaitForMultipleObjects(2, waitEvents, FALSE, INFINITE);

		if (false == monitoringServer->mbIsRunning)
		{
			break;
		}

		// PDH
		PdhCollectQueryData(queryHandle);

		PDH_FMT_COUNTERVALUE nonpagedBytesValue;
		PDH_FMT_COUNTERVALUE availableMBytesValue;

		::PdhGetFormattedCounterValue(nonpagedBytes, PDH_FMT_LONG, NULL, &nonpagedBytesValue);
		::PdhGetFormattedCounterValue(availableMBytes, PDH_FMT_LONG, NULL, &availableMBytesValue);

		int recvBytesSum = 0; // 총 Recv Bytes 모든 이더넷의 Recv 수치 합산
		int sendBytesSum = 0; // 총 Send Bytes 모든 이더넷의 Send 수치 합산

		// 이더넷 개수만큼 돌면서 총 합을 구한다
		for (int i = 0; i < ethernetCount; i++)
		{
			PDH_STATUS status;
			PDH_FMT_COUNTERVALUE counterValue;

			status = ::PdhGetFormattedCounterValue(ethernets[i].RecvBytes, PDH_FMT_LONG, NULL, &counterValue);
			if (status == 0)
			{
				recvBytesSum += counterValue.longValue;
			}

			status = ::PdhGetFormattedCounterValue(ethernets[i].SendBytes, PDH_FMT_LONG, NULL, &counterValue);
			if (status == 0)
			{
				sendBytesSum += counterValue.longValue;
			}
		}

		/**************************************************** 서버 컴퓨터 모니터링 정보 보내기 ****************************************************/

		MonitoringVariables monitoringInfo = monitoringServer->GetMonitoringInfo();

		int32_t timeStamp = static_cast<int32_t>(time(nullptr));

		int cpuTotal = static_cast<int>(monitoringInfo.ProcessorTimeTotal);
		int nonpagedMemory = nonpagedBytesValue.longValue / 1'000'000;
		int networkRecv = recvBytesSum / 1'000;
		int networkSend = sendBytesSum / 1'000;
		int availableMemory = availableMBytesValue.longValue;

		monitoringServer->Broadcast_MONITOR_DATA_UPDATE(0, en_PACKET_SS_MONITOR_DATA_UPDATE::dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL, cpuTotal, timeStamp);
		monitoringServer->Broadcast_MONITOR_DATA_UPDATE(0, en_PACKET_SS_MONITOR_DATA_UPDATE::dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY, nonpagedMemory, timeStamp);
		monitoringServer->Broadcast_MONITOR_DATA_UPDATE(0, en_PACKET_SS_MONITOR_DATA_UPDATE::dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV, networkRecv, timeStamp);
		monitoringServer->Broadcast_MONITOR_DATA_UPDATE(0, en_PACKET_SS_MONITOR_DATA_UPDATE::dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND, networkSend, timeStamp);
		monitoringServer->Broadcast_MONITOR_DATA_UPDATE(0, en_PACKET_SS_MONITOR_DATA_UPDATE::dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY, availableMemory, timeStamp);

		{
			constexpr int TYPE = en_PACKET_SS_MONITOR_DATA_UPDATE::dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL;

			monitoringServer->mMonitorInfos[TYPE].Lock.lock();
			monitoringServer->mMonitorInfos[TYPE].ServerNo = 10;
			monitoringServer->mMonitorInfos[TYPE].ValueSum += cpuTotal;
			monitoringServer->mMonitorInfos[TYPE].Count++;

			if (cpuTotal < monitoringServer->mMonitorInfos[TYPE].Min)
			{
				monitoringServer->mMonitorInfos[TYPE].Min = cpuTotal;
			}

			if (cpuTotal > monitoringServer->mMonitorInfos[TYPE].Max)
			{
				monitoringServer->mMonitorInfos[TYPE].Max = cpuTotal;
			}

			monitoringServer->mMonitorInfos[TYPE].Lock.unlock();
		}

		{
			constexpr int TYPE = en_PACKET_SS_MONITOR_DATA_UPDATE::dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY;

			monitoringServer->mMonitorInfos[TYPE].Lock.lock();
			monitoringServer->mMonitorInfos[TYPE].ServerNo = 10;
			monitoringServer->mMonitorInfos[TYPE].ValueSum += nonpagedMemory;
			monitoringServer->mMonitorInfos[TYPE].Count++;

			if (nonpagedMemory < monitoringServer->mMonitorInfos[TYPE].Min)
			{
				monitoringServer->mMonitorInfos[TYPE].Min = nonpagedMemory;
			}

			if (nonpagedMemory > monitoringServer->mMonitorInfos[TYPE].Max)
			{
				monitoringServer->mMonitorInfos[TYPE].Max = nonpagedMemory;
			}

			monitoringServer->mMonitorInfos[TYPE].Lock.unlock();
		}

		{
			constexpr int TYPE = en_PACKET_SS_MONITOR_DATA_UPDATE::dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV;

			monitoringServer->mMonitorInfos[TYPE].Lock.lock();
			monitoringServer->mMonitorInfos[TYPE].ServerNo = 10;
			monitoringServer->mMonitorInfos[TYPE].ValueSum += networkRecv;
			monitoringServer->mMonitorInfos[TYPE].Count++;

			if (networkRecv < monitoringServer->mMonitorInfos[TYPE].Min)
			{
				monitoringServer->mMonitorInfos[TYPE].Min = networkRecv;
			}

			if (networkRecv > monitoringServer->mMonitorInfos[TYPE].Max)
			{
				monitoringServer->mMonitorInfos[TYPE].Max = networkRecv;
			}

			monitoringServer->mMonitorInfos[TYPE].Lock.unlock();
		}

		{
			constexpr int TYPE = en_PACKET_SS_MONITOR_DATA_UPDATE::dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND;

			monitoringServer->mMonitorInfos[TYPE].Lock.lock();
			monitoringServer->mMonitorInfos[TYPE].ServerNo = 10;
			monitoringServer->mMonitorInfos[TYPE].ValueSum += networkSend;
			monitoringServer->mMonitorInfos[TYPE].Count++;

			if (networkSend < monitoringServer->mMonitorInfos[TYPE].Min)
			{
				monitoringServer->mMonitorInfos[TYPE].Min = networkSend;
			}

			if (networkSend > monitoringServer->mMonitorInfos[TYPE].Max)
			{
				monitoringServer->mMonitorInfos[TYPE].Max = networkSend;
			}

			monitoringServer->mMonitorInfos[TYPE].Lock.unlock();
		}

		{
			constexpr int TYPE = en_PACKET_SS_MONITOR_DATA_UPDATE::dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY;

			monitoringServer->mMonitorInfos[TYPE].Lock.lock();
			monitoringServer->mMonitorInfos[TYPE].ServerNo = 10;
			monitoringServer->mMonitorInfos[TYPE].ValueSum += availableMemory;
			monitoringServer->mMonitorInfos[TYPE].Count++;

			if (availableMemory < monitoringServer->mMonitorInfos[TYPE].Min)
			{
				monitoringServer->mMonitorInfos[TYPE].Min = availableMemory;
			}

			if (availableMemory > monitoringServer->mMonitorInfos[TYPE].Max)
			{
				monitoringServer->mMonitorInfos[TYPE].Max = availableMemory;
			}

			monitoringServer->mMonitorInfos[TYPE].Lock.unlock();
		}
	}

	LOGF(ELogLevel::System, L"Machine Monitor Thread End (ID : %d)", ::GetCurrentThreadId());

	return 0;
}

unsigned int MonitoringServer::logDBWriteThread(void* server)
{
	LOGF(ELogLevel::System, L"Log DB Write Thread Start (ID : %d)", ::GetCurrentThreadId());

	MonitoringServer* monitoringServer = reinterpret_cast<MonitoringServer*>(server);

	HANDLE waitEvents[2];
	waitEvents[0] = monitoringServer->mShutdownEvent;
	waitEvents[1] = monitoringServer->mLogDBWriteEvent;

	for (;;)
	{
		::WaitForMultipleObjects(2, waitEvents, FALSE, INFINITE);

		if (false == monitoringServer->mbIsRunning)
		{
			break;
		}

		time_t startTime = time(nullptr);
		tm localTime;
		localtime_s(&localTime, &startTime);

		for (int i = 1; i < en_PACKET_SS_MONITOR_DATA_UPDATE::MAX; ++i)
		{
			int serverNo;
			int valueSum;
			int count;
			int average;
			int max;
			int min;

			{
				monitoringServer->mMonitorInfos[i].Lock.lock();

				serverNo = monitoringServer->mMonitorInfos[i].ServerNo;
				valueSum = monitoringServer->mMonitorInfos[i].ValueSum;
				count = monitoringServer->mMonitorInfos[i].Count;
				max = monitoringServer->mMonitorInfos[i].Max;
				min = monitoringServer->mMonitorInfos[i].Min;

				monitoringServer->mMonitorInfos[i].ValueSum = 0;
				monitoringServer->mMonitorInfos[i].Count = 0;
				monitoringServer->mMonitorInfos[i].Max = 0;
				monitoringServer->mMonitorInfos[i].Min = INT_MAX;

				monitoringServer->mMonitorInfos[i].Lock.unlock();
			}

			if (count == 0)
			{
				average = 0;
				max = 0;
				min = 0;
			}
			else
			{
				average = valueSum / count; 
			}
			
			bool querySuccess = false;

			while (true)
			{
				bool querySuccess = monitoringServer->mDBConnection.Query(L"INSERT INTO logdb.monitorlog_%04d%02d (`logtime`, `serverno`, `type`, `avr`, `max`, `min`) VALUES (NOW(), %d, %d, %d, %d, %d)", localTime.tm_year + 1900, localTime.tm_mon + 1, serverNo, i, average, max, min);
				
				if (querySuccess)
				{
					break;
				}

				if (monitoringServer->mDBConnection.GetLastError() == 1146)
				{
					bool tableCreated = monitoringServer->mDBConnection.Query(L"CREATE TABLE logdb.monitorlog_%04d%02d LIKE logdb.monitorlog", localTime.tm_year + 1900, localTime.tm_mon + 1);

					if (false == tableCreated)
					{
						LOGF(ELogLevel::Assert, L"CREATE TABLE Query Failed (error = %d)", monitoringServer->mDBConnection.GetLastError());
						CrashDump::Crash();
					}
				}
				else
				{
					LOGF(ELogLevel::Assert, L"Query Failed (error = %d)", monitoringServer->mDBConnection.GetLastError());
					CrashDump::Crash();
				}
			}
		}

		LOGF(ELogLevel::System, L"LOG DB SAVED");
	}

	LOGF(ELogLevel::System, L"Log DB Write Thread End (ID : %d)", ::GetCurrentThreadId());

	return 0;
}

unsigned int MonitoringServer::timeoutThread(void* server)
{
	LOGF(ELogLevel::System, L"Timeout Thread Start (ID : %d)", ::GetCurrentThreadId());

	MonitoringServer* monitoringServer = reinterpret_cast<MonitoringServer*>(server);

	HANDLE waitEvents[2];
	waitEvents[0] = monitoringServer->mShutdownEvent;
	waitEvents[1] = monitoringServer->mTimeoutCheckEvent;

	for (;;)
	{
		::WaitForMultipleObjects(2, waitEvents, FALSE, INFINITE);

		if (false == monitoringServer->mbIsRunning)
		{
			break;
		}

		// 타임아웃에 걸린 세션들
		std::list<uint64_t> sessionToDisconnects;

		{
			monitoringServer->mUnknownsLock.lock();

			for (auto it = monitoringServer->mUnknowns.begin(); it != monitoringServer->mUnknowns.end(); ++it)
			{
				uint64_t sessionID = it->first;
				uint64_t acceptTick = it->second;

				if (::timeGetTime() - acceptTick >= monitoringServer->mMaxTimeout)
				{
					monitoringServer->Disconnect(sessionID);
					sessionToDisconnects.push_back(sessionID);
				}
			}

			for (const uint64_t sessionID : sessionToDisconnects)
			{
				monitoringServer->mUnknowns.erase(sessionID);
			}

			monitoringServer->mUnknownsLock.unlock();
		}

		for (const uint64_t sessionID : sessionToDisconnects)
		{
			LOGF(ELogLevel::System, L"sessionID = %llu timeouted", sessionID);
		}
	}

	LOGF(ELogLevel::System, L"Timeout Thread End (ID : %d)", ::GetCurrentThreadId());

	return 0;
}