#pragma comment(lib, "winmm")

#include <iostream>
#include <process.h>

#include "NetUtils.h"
#include "NetServer.h"
#include "NetworkHeader.h"
#include "Session.h"
#include "../Tool/CpuUsageMonitor.h"

NetServer::~NetServer()
{
	if (mbIsRunning)
	{
		Shutdown();
	}
}

void NetServer::Start(const uint16_t port, const uint32_t maxSessionCount, const uint32_t iocpConcurrentThreadCount, const uint32_t iocpWorkerThreadCount)
{
	ASSERT_LIVE(iocpConcurrentThreadCount > 0, L"iocpConcurrentThreadCount can not be zero");
	ASSERT_LIVE(iocpWorkerThreadCount > 0, L"iocpWorkerThreadCount can not be zero");

	mbIsRunning = true;

	mPort = port;
	mMaxSessionCount = maxSessionCount;
	mThreadCount = iocpWorkerThreadCount + 2;

	if (mMaxPayloadLength == 0)
	{
		mMaxPayloadLength = UINT16_MAX;
	}

	::timeBeginPeriod(1);

	NetUtils::WSAStartup();

	mIOCP = NetUtils::CreateNewIOCP(iocpConcurrentThreadCount);

	// Create Sessions
	mSessionList = new Session[mMaxSessionCount];

	for (uint32_t i = 0; i < mMaxSessionCount; ++i)
	{
		mSessionList[i].bDisconnected = true;
		mUnusedSessionKeys.Push(i);
	}

	// Create threads
	mThreads = new HANDLE[mThreadCount];

	// AcceptThread
	mThreads[0] = reinterpret_cast<HANDLE>(::_beginthreadex(nullptr, 0, acceptThread, this, 0, nullptr));

	// MonitorThread
	mThreads[1] = reinterpret_cast<HANDLE>(::_beginthreadex(nullptr, 0, monitorThread, this, 0, nullptr));

	// IOCP WorkerThreads
	for (uint32_t i = 2; i < mThreadCount; ++i)
	{
		mThreads[i] = reinterpret_cast<HANDLE>(::_beginthreadex(nullptr, 0, iocpWorkerThread, this, 0, nullptr));
	}

	LOGF(ELogLevel::System, L"NetServer Running (version %s)", GetServerVersion().c_str());
}

void NetServer::Shutdown(void)
{
	LOGF(ELogLevel::System, L"Server Shutdown Start");

	mbIsRunning = false;

	// accept thread
	::closesocket(mListenSocket);

	// IOCP worker threads
	for (uint32_t i = 0; i < mThreadCount - 2; ++i)
	{
		::PostQueuedCompletionStatus(mIOCP, 0, 0, 0);
	}

	for (uint32_t i = 0; i < mThreadCount; ++i)
	{
		::WaitForSingleObject(mThreads[i], INFINITE); // Multiple은 64개 제한이 있으므로 사용 X
	}

	for (uint32_t i = 0; i < mThreadCount; ++i)
	{
		::CloseHandle(mThreads[i]);
	}

	::CloseHandle(mIOCP);

	delete[] mThreads;
	mThreads = nullptr;

	delete[] mSessionList;
	mSessionList = nullptr;

	mbIsTcpNodelay = false;
	mbIsSendBufferSizeZero = false;
	mSessionAcceptedCount = 0;
	mSessionDisconnectedCount = 0;
	mPort = 0;
	mMaxPayloadLength = 0;
	mSessionCount = 0;
	mMaxSessionCount = 0;
	mThreadCount = 0;
	mIOCP = 0;
	mListenSocket = INVALID_SOCKET;
	::ZeroMemory(&mMonitoringVariables, sizeof(MonitoringVariables));
	::ZeroMemory(&mMonitorResult, sizeof(MonitoringVariables));

	LOGF(ELogLevel::System, L"Server Shutdown");
}

void NetServer::SendPacket(const uint64_t sessionID, Serializer* packet)
{
	if (packet == nullptr)
	{
		return;
	}

	Session* session = findSessionOrNull(sessionID);
	if (session == nullptr)
	{
		return;
	}

	int32_t retIoCount = static_cast<int32_t>(session->IncrementIoCount());

	if (retIoCount < 0 || session->bDisconnected || session->bDisconnectRegistered || session->ID != sessionID)
	{
		session->DecrementIoCount();
		return;
	}

	if (!packet->IsSendPrepared())
	{
		packet->prepareSend();
	}

	packet->IncrementRefCount();

	session->SendQueue.Enqueue(packet);

	session->PostSend();

	session->DecrementIoCount();
}

void NetServer::Disconnect(const uint64_t sessionID)
{
	Session* session = findSessionOrNull(sessionID);
	if (session == nullptr)
	{
		return;
	}

	int32_t retIoCount = static_cast<int32_t>(session->IncrementIoCount());

	if (retIoCount < 0 || session->bDisconnected || session->bDisconnectRegistered || session->ID != sessionID)
	{
		session->DecrementIoCount();
		return;
	}

	LOGF(ELogLevel::Debug, L"NetServer::Disconnect(%llu)", session->ID);

	InterlockedExchange8(reinterpret_cast<CHAR*>(&session->bDisconnectRegistered), true);
	
	::CancelIoEx(reinterpret_cast<HANDLE>(session->Socket), NULL); // 이미 걸려있는 IO를 해제

	session->DecrementIoCount();
}

void NetServer::SendAndDisconnect(const uint64_t sessionID, Serializer* packet)
{
	if (packet == nullptr)
	{
		return;
	}

	Session* session = findSessionOrNull(sessionID);
	if (session == nullptr)
	{
		return;
	}

	int32_t retIoCount = static_cast<int32_t>(session->IncrementIoCount());

	if (retIoCount < 0 || session->bDisconnected || session->bDisconnectRegistered || session->ID != sessionID)
	{
		session->DecrementIoCount();
		return;
	}

	// Disconnect register - SendPacket과는 이 부분만 차이가 있음
	session->bDisconnectRegistered = true;

	if (!packet->IsSendPrepared())
	{
		packet->prepareSend();
	}

	packet->IncrementRefCount();

	session->SendQueue.Enqueue(packet);

	session->PostSend();

	session->DecrementIoCount();
}

bool NetServer::GetSessionAddress(const uint64_t sessionID, SOCKADDR_IN* outAddress) const
{
	ASSERT_LIVE(outAddress != nullptr, L"GetSessionAddress() nullptr passed");

	Session* session = findSessionOrNull(sessionID);
	if (session == nullptr)
	{
		return false;
	}

	int32_t retIoCount = static_cast<int32_t>(session->IncrementIoCount());

	if (retIoCount < 0 || session->bDisconnected || session->bDisconnectRegistered || session->ID != sessionID)
	{
		session->DecrementIoCount();
		return false;
	}

	*outAddress = session->Address;

	session->DecrementIoCount();

	return true;
}

unsigned int NetServer::acceptThread(void* netServerParam)
{
	LOGF(ELogLevel::System, L"Accept Thread Start (ID : %d)", ::GetCurrentThreadId());

	NetServer* netServer = reinterpret_cast<NetServer*>(netServerParam);

	// create socket
	netServer->mListenSocket = NetUtils::CreateSocket();

	// bind
	NetUtils::BindSocket(netServer->mListenSocket, netServer->mPort);

	// listen
	NetUtils::SetSocketListen(netServer->mListenSocket);

	/************************* Server Listening Start *************************/

	LOGF(ELogLevel::System, L"Accept Start (Port = %d)", netServer->mPort);

	SOCKADDR_IN clientAddress{};	// 클라이언트 주소
	uint32_t newSessionKey;			// 새로운 세션의 Key (사용할 세션 객체를 결정)
	uint64_t newSessionID;			// 새로운 세션의 ID
	Session* newSession;			// 새로운 세션 객체

	while (netServer->mbIsRunning)
	{
		// accept()
		SOCKET clientSocket = NetUtils::GetAcceptedSocketOrInvalid(netServer->mListenSocket, &clientAddress);

		// error handling
		if (clientSocket == INVALID_SOCKET)
		{
			int errorCode = ::WSAGetLastError();

			bool bThreadExit = errorCode == WSAEINTR && netServer->mbIsRunning == false;
			if (bThreadExit)
			{
				break;
			}

			LOGF(ELogLevel::Error, L"AcceptThread accept() failed (errorCode = %d)", errorCode);
			CrashDump::Crash();
		}

		// socket option
		NetUtils::SetLinger(clientSocket, 1, 0);

		if (netServer->mbIsSendBufferSizeZero)
		{
			NetUtils::SetSendBufferSize(clientSocket, 0);
		}

		if (netServer->mbIsTcpNodelay)
		{
			NetUtils::SetTcpNodelay(clientSocket);
		}

		// 세션리스트로부터 세션을 얻어온다
		bool bPopSuccess = netServer->mUnusedSessionKeys.TryPop(newSessionKey);

		if (false == bPopSuccess)
		{
			NetUtils::CloseSocket(clientSocket);
			LOGF(ELogLevel::System, L"Max Session Count - Session Disconnected");

			continue;
		}

		// 새로운 세션 ID 생성
		newSessionID = (++(netServer->mSessionAcceptedCount) & (0x0000'0000'FFFF'FFFFULL)) | (static_cast<uint64_t>(newSessionKey) << 32);

		// 세션 얻어오기
		newSession = &netServer->mSessionList[newSessionKey];
		
		{
			newSession->IncrementIoCount();

			newSession->Init(clientSocket, clientAddress, netServer, newSessionID, newSessionKey);

			NetUtils::RegisterIOCP(clientSocket, netServer->mIOCP, reinterpret_cast<ULONG_PTR>(newSession));

			// accept log
			//LOGF(ELogLevel::Debug, L"Accept - %s:%d", NetUtils::GetIpAddress(newSession->Address).c_str(), NetUtils::GetPortNumber(newSession->Address));

			// monitoring
			InterlockedIncrement(&netServer->mSessionCount);

			netServer->OnAccept(newSession->ID);

			netServer->mMonitoringVariables.AcceptTPS++;

			newSession->PostRecv();

			newSession->DecrementIoCount();
		}
	}

	LOGF(ELogLevel::System, L"Accept Thread End (ID : %d)", ::GetCurrentThreadId());

	return 0;
}

unsigned int NetServer::iocpWorkerThread(void* netServerParam)
{
	LOGF(ELogLevel::System, L"IOCP Worker Thread Start (ID : %d)", ::GetCurrentThreadId());

	NetServer* netServer = reinterpret_cast<NetServer*>(netServerParam);

	bool retGQCS;
	NetworkHeader header{};

	while (netServer->mbIsRunning)
	{
		DWORD transferredBytes = 0;
		Session* session = 0;
		OVERLAPPED* overlapped = 0;

		retGQCS = ::GetQueuedCompletionStatus(netServer->mIOCP, &transferredBytes, reinterpret_cast<ULONG_PTR*>(&session), &overlapped, INFINITE);

		if (retGQCS) // GQCS return TRUE
		{
			if (transferredBytes == 0 && overlapped == 0)
			{
				if (session == 0)
				{
					// 스레드 종료 - 루프 탈출
					break;
				}
				else
				{
					// OnRelease 요청 PQCS 처리
					netServer->OnRelease(reinterpret_cast<const uint64_t>(session));
					continue;
				}
			}
		}
		else// GQCS return FALSE
		{
			int errorCode = ::GetLastError();

			switch (errorCode)
			{
			case ERROR_SEM_TIMEOUT:
			{
				LOGF(ELogLevel::Debug, L"GQCS FALSE, GetLastError() = %d (SEM_TIMEOUT) -> disconnect", errorCode, ::WSAGetLastError());
			}
			break;
			case ERROR_NETNAME_DELETED:
			case ERROR_CONNECTION_ABORTED:
			case ERROR_OPERATION_ABORTED:
			{
				ASSERT_LIVE(session != nullptr, L"retGQCS FALSE, but Session is nullptr");
			}
			break;
			default:
			{
				LOGF(ELogLevel::Error, L"GQCS FALSE, GetLastError() = %d, WSAGetLastError() = %d", errorCode, ::WSAGetLastError());
				CrashDump::Crash();
			}
			break;
			}

			// 정상 연결종료 처리

			goto DECREMENT_IO_COUNT;
		}

		// Process Disconnect & Send & Recv
		if (overlapped == &session->SendOverlapped)
		{
			// release registered packets
			uint32_t registeredPacketsCount = session->RegisteredPacketCount;
			session->RegisteredPacketCount = 0;

			InterlockedAdd64(reinterpret_cast<LONG64*>(&netServer->mMonitoringVariables.SendMessageTPS), registeredPacketsCount);

			for (uint32_t i = 0; i < registeredPacketsCount; ++i)
			{
				session->RegisteredPackets[i]->DecrementRefCount();
			}

			if (session->bDisconnectRegistered)
			{
				goto DECREMENT_IO_COUNT;
			}

			ASSERT_LIVE(InterlockedExchange(&session->bSendFlag, 0) == 1, L"more than 1 Send Error");

			session->PostSend();
		}
		else if (overlapped == &session->RecvOverlapped)
		{
			session->RecvBuffer.MoveRear(transferredBytes);

			// 연결 종료
			if (transferredBytes == 0 || session->bDisconnectRegistered)
			{
				goto DECREMENT_IO_COUNT;
			}

			// packet loop
			while (true)
			{
				// 1. header check
				if (session->RecvBuffer.GetUseSize() < sizeof(NetworkHeader))
				{
					break;
				}

				bool retPeek = session->RecvBuffer.Peek(reinterpret_cast<char*>(&header), sizeof(header));
				ASSERT_LIVE(retPeek == true, L"RecvBuffer Peek() Error");

#if NETWORK_HEADER_USE_TYPE == NETWORK_HEADER_TYPE_NET
				// NetServer - code check
				if (header.Code != NETWORK_HEADER_CODE)
				{
					goto DECREMENT_IO_COUNT;
				}
#endif

				// 2. header Length check
				if (header.Length > netServer->GetMaxPayloadLength())
				{
					goto DECREMENT_IO_COUNT;
				}

				// 3. payload check
				if (session->RecvBuffer.GetUseSize() < sizeof(NetworkHeader) + header.Length)
				{
					if (session->RecvBuffer.GetFreeSize() <= 0)
					{
						goto DECREMENT_IO_COUNT;
					}

					break;
				}

				// 4. packet copy
				Serializer* packet = Serializer::l_packetPool.Alloc();
				packet->IncrementRefCount();
				packet->Clear();

				bool retDequeue = session->RecvBuffer.Dequeue(packet->GetFullBufferPointer(), header.Length + sizeof(NetworkHeader));
				ASSERT_LIVE(retDequeue == true, L"RecvBuffer Dequeue() Error");
				packet->SetUseSize(header.Length);

#if NETWORK_HEADER_USE_TYPE == NETWORK_HEADER_TYPE_NET
				// NetServer - decode packet
				if (false == packet->decode())
				{
					packet->DecrementRefCount();
					goto DECREMENT_IO_COUNT;
				}
#endif

				// 5. OnReceive()
				netServer->OnReceive(session->ID, packet);

				InterlockedIncrement(&netServer->mMonitoringVariables.RecvMessageTPS);
			}

			// Post Recv
			session->PostRecv();
		}
		else
		{
			ASSERT_LIVE(false, L"Invalid OVERLAPPED");
		}


	DECREMENT_IO_COUNT:

		// 이번에 온 통지로 인한 IoCount를 1 감소함으로써 한 번의 루프를 정리하는 코드
		session->DecrementIoCount();
	}

	LOGF(ELogLevel::System, L"IOCP Worker Thread End (ID : %d)", ::GetCurrentThreadId());

	return 0;
}

unsigned int NetServer::monitorThread(void* netServerParam)
{
	LOGF(ELogLevel::System, L"Monitor Thread Start (ID : %d)", ::GetCurrentThreadId());

	NetServer* netServer = reinterpret_cast<NetServer*>(netServerParam);

	CpuUsageMonitor cpuTime;

	uint64_t sumAcceptTPS = 0;
	uint64_t sumRecvMessageTPS = 0;
	uint64_t sumSendMessageTPS = 0;
	uint64_t sumRecvPendingTPS = 0;
	uint64_t sumSendPendingTPS = 0;

	uint64_t sumCount = 0;

	while (netServer->mbIsRunning)
	{
		::Sleep(1'000);

		// CPU 사용률
		cpuTime.UpdateCpuTime();
		netServer->mMonitorResult.ProcessorTimeTotal = cpuTime.GetProcessorTimeTotal();
		netServer->mMonitorResult.ProcessTimeTotal = cpuTime.GetProcessTimeTotal();
		netServer->mMonitorResult.ProcessorTimeUser = cpuTime.GetProcessorTimeUser();
		netServer->mMonitorResult.ProcessTimeUser = cpuTime.GetProcessTimeUser();
		netServer->mMonitorResult.ProcessorTimeKernel = cpuTime.GetProcessorTimeKernel();
		netServer->mMonitorResult.ProcessTimeKernel = cpuTime.GetProcessTimeKernel();

		// TPS
		netServer->mMonitorResult.AcceptTPS = netServer->mMonitoringVariables.AcceptTPS;
		netServer->mMonitorResult.RecvMessageTPS = netServer->mMonitoringVariables.RecvMessageTPS;
		netServer->mMonitorResult.SendMessageTPS = netServer->mMonitoringVariables.SendMessageTPS;
		netServer->mMonitorResult.RecvPendingTPS = netServer->mMonitoringVariables.RecvPendingTPS;
		netServer->mMonitorResult.SendPendingTPS = netServer->mMonitoringVariables.SendPendingTPS;

		// Avg TPS
		sumAcceptTPS += netServer->mMonitorResult.AcceptTPS;
		sumRecvMessageTPS += netServer->mMonitorResult.RecvMessageTPS;
		sumSendMessageTPS += netServer->mMonitorResult.SendMessageTPS;
		sumRecvPendingTPS += netServer->mMonitorResult.RecvPendingTPS;
		sumSendPendingTPS += netServer->mMonitorResult.SendPendingTPS;
		sumCount++;

		netServer->mMonitorResult.AverageAcceptTPS = static_cast<uint32_t>(sumAcceptTPS / sumCount);
		netServer->mMonitorResult.AverageRecvMessageTPS = static_cast<uint32_t>(sumRecvMessageTPS / sumCount);
		netServer->mMonitorResult.AverageSendMessageTPS = static_cast<uint32_t>(sumSendMessageTPS / sumCount);
		netServer->mMonitorResult.AverageRecvPendingTPS = static_cast<uint32_t>(sumRecvPendingTPS / sumCount);
		netServer->mMonitorResult.AverageSendPendingTPS = static_cast<uint32_t>(sumSendPendingTPS / sumCount);

		// 모니터링 변수 초기화
		netServer->mMonitoringVariables.AcceptTPS = 0;
		netServer->mMonitoringVariables.RecvMessageTPS = 0;
		netServer->mMonitoringVariables.SendMessageTPS = 0;
		netServer->mMonitoringVariables.RecvPendingTPS = 0;
		netServer->mMonitoringVariables.SendPendingTPS = 0;
	}

	LOGF(ELogLevel::System, L"Monitor Thread End (ID : %d)", ::GetCurrentThreadId());

	return 0;
}

Session* NetServer::findSessionOrNull(const uint64_t sessionID) const
{
	uint32_t sessionKey = sessionID >> 32;

	if (sessionKey >= mMaxSessionCount)
	{
		return nullptr;
	}

	return mSessionList + sessionKey;
}