#pragma once

#include <string>

#include "Serializer.h"
#include "../DataStructure/LockFreeStack.h"

class Session;
typedef void* HANDLE;
typedef unsigned long long SOCKET;

/************************** monitoring variables **************************/
struct MonitoringVariables
{
    uint32_t AcceptTPS;                 // 초당 접속 처리 횟수 (OnAccept 리턴 횟수)
    uint32_t RecvMessageTPS;            // 초당 메세지 처리 횟수 (OnReceive 리턴 횟수)
    uint32_t SendMessageTPS;            // 초당 메세지 송신 횟수
    uint32_t RecvPendingTPS;
    uint32_t SendPendingTPS;
    uint32_t AverageAcceptTPS;
    uint32_t AverageRecvMessageTPS;
    uint32_t AverageSendMessageTPS;
    uint32_t AverageRecvPendingTPS;
    uint32_t AverageSendPendingTPS;
    float ProcessorTimeTotal;
    float ProcessTimeTotal;
    float ProcessorTimeUser;
    float ProcessTimeUser;
    float ProcessorTimeKernel;
    float ProcessTimeKernel;
};
/************************** monitoring variables **************************/

class NetServer
{
    friend class Session;
    
public:
#pragma warning(push)
#pragma warning(disable: 26495) // 변수 초기화 경고 제거
    NetServer() = default;
#pragma warning(pop)

    virtual ~NetServer();

public: // 서버 시작 전 세팅 함수들 (+ TCP Options)

    // TCP_NODELAY 옵션 설정 (Nagle을 Off할 것인가)
    inline void SetTcpNodelay(bool bToSet) { mbIsTcpNodelay = bToSet; }

    // SND_BUF = 0 옵션 설정
    inline void SetSendBufferSizeToZero(bool bToSet) { mbIsSendBufferSizeZero = bToSet; }

    // 메세지의 최대 길이 (최대 길이를 넘어가는 메세지가 올 경우 연결을 끊는다)
    inline void SetMaxPayloadLength(const uint16_t length) { mMaxPayloadLength = length; }

    // 서버 시작
    virtual void Start(
        const uint16_t port,
        const uint32_t maxSessionCount,
        const uint32_t iocpConcurrentThreadCount,
        const uint32_t iocpWorkerThreadCount);

    // 서버 종료 (모든 스레드가 종료될 때 까지 블락됨)
    virtual void Shutdown(void);

public: // 세션 사용 함수

    // 패킷 전송 요청 - SendQueue에 Enqueue
    void SendPacket(const uint64_t sessionID, Serializer* packet);

    // 연결 종료 요청
    void Disconnect(const uint64_t sessionID);

    // 패킷을 보내고 연결 종료 요청
    void SendAndDisconnect(const uint64_t sessionID, Serializer* packet);

    // 세션의 주소를 얻는다
    bool GetSessionAddress(const uint64_t sessionID, SOCKADDR_IN* outAddress) const;

public: // Getters

    inline static std::wstring	GetServerVersion(void) { return L"6.7.0"; }

    inline bool					IsRunning(void) const { return mbIsRunning; }
    inline uint16_t				GetPortNumber(void) const { return mPort; }
    inline uint16_t				GetMaxPayloadLength(void) const { return mMaxPayloadLength; }
    inline uint64_t				GetTotalAcceptCount(void) const { return mSessionAcceptedCount; }
    inline uint64_t				GetTotalDisconnectCount(void) const { return mSessionDisconnectedCount; }
    inline MonitoringVariables	GetMonitoringInfo(void) const { return mMonitorResult; }
    inline uint32_t				GetSessionCount(void) const { return mSessionCount; }
    inline uint32_t				GetMaxSessionCount(void) const { return mMaxSessionCount; }

public: // 서버 핸들러 가상 함수들

    // 세션이 생성될 때 호출됨
    // 생성된 세션에 대한 최초 Recv가 호출되기 전에 이 함수가 호출됩니다.
    virtual void OnAccept(const uint64_t sessionID) = 0;

    // 완성된 패킷을 수신하였을 때 호출됩니다.
    // 전달된 패킷은 컨텐츠 영역에서 해제해주어야 합니다.
    virtual void OnReceive(const uint64_t sessionID, Serializer* packet) = 0;

    // 세션이 릴리즈될 때 호출됨
    // 이 함수가 호출되면 더 이상 해당 세션ID는 유효하지 않습니다.
    virtual void OnRelease(const uint64_t sessionID) = 0;

private: // 스레드 함수들

    static unsigned int acceptThread(void* netServerParam);     // Accept, 세션 생성 전담
    static unsigned int iocpWorkerThread(void* netServerParam); // IOCP 이벤트 처리
    static unsigned int monitorThread(void* netServerParam);    // 모니터링 정보 취합

private: // 내부 유틸 함수

    // 세션 ID로 실제 세션 객체를 얻어온다
    Session* findSessionOrNull(const uint64_t sessionID) const;

private:

    bool				    mbIsRunning;				// 서버가 실행중인지
    bool				    mbIsTcpNodelay;				// 옵션 - TCP_NODELAY를 사용하는가
    bool				    mbIsSendBufferSizeZero;		// 옵션 - SND_BUF 사이즈 0
    HANDLE				    mIOCP;						// IOCP 핸들
    SOCKET				    mListenSocket;				// 리슨 소켓
    uint16_t			    mPort;						// 포트 번호
    uint16_t			    mMaxPayloadLength;			// 페이로드의 최대 길이 (Header.Length)
    uint32_t			    mMaxSessionCount;			// 서버의 최대 세션 개수
    uint32_t			    mThreadCount;				// 서버의 스레드 개수
    HANDLE* mThreads;					                // 서버가 생성한 스레드들
    MonitoringVariables     mMonitoringVariables;		// 모니터링 결과를 계산하기 위한 변수
    MonitoringVariables     mMonitorResult;				// 모니터링 변수 결과 (매 초마다 갱신됨)
    uint64_t			    mSessionAcceptedCount;		// 서버가 시작된 후부터 지금까지 접속한 세션의 수
    uint64_t			    mSessionDisconnectedCount;	// 서버가 시작된 후부터 지금까지 끊긴 세션의 수
    uint32_t			    mSessionCount;				// 현재 접속 중인 세션의 개수

    Session* mSessionList;                              // 세션 리스트 (풀)
    LockFreeStack<uint32_t>	mUnusedSessionKeys;         // 사용하지 않은 세션 키들
};