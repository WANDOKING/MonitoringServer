#pragma once

#include <unordered_map>
#include <stack>

#include "RingBuffer.h"
#include "Serializer.h"
#include "../DataStructure/LockFreeQueue.h"

class NetServer;

class Session
{
	friend class NetServer;
private:

#pragma warning(push)
#pragma warning(disable: 26495) // 변수 초기화 경고 제거
	Session(void) = default;
#pragma warning(pop)

	~Session() = default;

	// 세션 개체 초기화
	void Init(const SOCKET sock, const SOCKADDR_IN address, NetServer* netServer, const uint64_t sessionID, const uint32_t sessionListKey);

	// IO Count를 증가(Interlocked)
	inline uint32_t IncrementIoCount(void) { return InterlockedIncrement(&IoCount); }

	// IO Count를 감소(Interlocked), 0이라면 release
	bool DecrementIoCount(void);

	// RecvBuffer를 대상으로 WSARecv()를 호출
	bool PostRecv(void);

	// SendBuffer를 대상으로 WSASend()를 호출, 내부적으로 bSendFlag로 WSASend를 1회 제한함
	// WSASend()에 대한 완료 통지가 왔을 때 Flag를 0으로 세팅해주고 다시 PostSend()를 호출할 것
	bool PostSend(void);

	// bDisconnected가 false라면, true로 세팅하고 소켓을 닫는다
	bool TryClosesocket(void);

private:

	enum
	{
		MAX_WSA_BUF_COUNT = 10
	};
	
	uint64_t					ID;			// [SessionList key(index) 32bit][ID++ 32bit]	
	SOCKET						Socket;
	SOCKADDR_IN					Address;
	NetServer*					Server;

	OVERLAPPED					SendOverlapped;
	OVERLAPPED					RecvOverlapped;
	
	uint32_t					IoCount;	// IoCount의 최상위 비트는 Release Flag로 사용
	uint32_t					bSendFlag;
	uint32_t					SessionListKey;
	bool						bDisconnected;
	bool						bDisconnectRegistered;

	RingBuffer					RecvBuffer;
	LockFreeQueue<Serializer*>	SendQueue;
	uint32_t					RegisteredPacketCount;
	Serializer*					RegisteredPackets[MAX_WSA_BUF_COUNT];
};