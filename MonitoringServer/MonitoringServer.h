#pragma once

#include "NetLibrary/NetServer/NetServer.h"

#include <list>
#include <map>
#include <mutex>
#include <shared_mutex>

#include <Pdh.h>

#include "MonitorProtocol.h"
#include "NetLibrary/DBConnector/DBConnector.h"

class MonitoringServer : public NetServer
{
public:
#pragma warning(push)
#pragma warning(disable: 26495) // 변수 초기화 경고 제거
    MonitoringServer() = default;
#pragma warning(pop)

    virtual ~MonitoringServer() = default;

public: // 서버 시작 전 세팅 함수들

    // DB 쓰기 간격 (분) 설정, 0으로 세팅하면 DB에 로그를 저장 하지 않는다
    void SetLogDBWriteIntervalMinutes(const uint32_t intervalMinutes) { mLogDBWriteIntervalMinutes = intervalMinutes; }

    // DB 연결 정보 세팅
    void SetDBConnectionInfo(const WCHAR* ip, const WCHAR* user, const WCHAR* password, const WCHAR* DBName, const uint32_t port) { mDBConnection.SetConnectionInfo(ip, user, password, DBName, port); }

    // 아직 로그인하지 않은 세션들에 대한 타임아웃 시간
    void SetMaxTimeout(uint32_t maxTimeout) { mMaxTimeout = maxTimeout; }

    // 타임아웃 스레드의 타임아웃 체크 간격
    // 0이면 타임아웃을 사용하지 않음
    void SetTimeoutCheckInterval(uint32_t timeoutCheckInterval) { mTimeoutCheckInterval = timeoutCheckInterval; }

public:

    // 서버 시작
    virtual void Start(
        const uint16_t port,
        const uint32_t maxSessionCount,
        const uint32_t iocpConcurrentThreadCount,
        const uint32_t iocpWorkerThreadCount) override;

    // 서버 종료 (모든 스레드가 종료될 때 까지 블락됨)
    virtual void Shutdown(void);

    // NetServer을(를) 통해 상속됨
    virtual void OnAccept(const uint64_t sessionID) override;
    virtual void OnReceive(const uint64_t sessionID, Serializer* packet) override;
    virtual void OnRelease(const uint64_t sessionID) override;

public: // 게터

    uint32_t    GetUnknownSessionCount(void) { return static_cast<uint32_t>(mUnknowns.size()); }
    uint32_t    GetConnectedClientCount(void) { return static_cast<uint32_t>(mClients.size()); }
    uint32_t    GetConnectedServerCount(void) { return static_cast<uint32_t>(mServers.size()); }
    uint32_t    GetLogDBWriteIntervalMinutes(void) { return mLogDBWriteIntervalMinutes; }
    
public:
    
    // 접속해 있는 모니터링 클라이언트들에게 모니터링 정보를 뿌린다
    void Broadcast_MONITOR_DATA_UPDATE(const BYTE serverNo, const BYTE dataType, const int32_t value, const int32_t timeStamp);

private: // 패킷 생성

    static Serializer* Create_CS_MONITOR_TOOL_RES_LOGIN(const BYTE status);
    static Serializer* Create_CS_MONITOR_TOOL_DATA_UPDATE(const BYTE serverNo, const BYTE dataType, const int dataValue, const int timeStamp);

private: // 패킷 처리

    // 모니터링 클라이언트의 로그인
    void process_CS_MONITOR_TOOL_REQ_LOGIN(const uint64_t sessionID, const char loginSessionKey[]);

    // 다른 서버의 모니터링 서버로의 로그인
    void process_SS_MONITOR_LOGIN(const uint64_t sessionID, const int serverNo);

    // 다른 서버의 모니터링 정보 제공
    void process_SS_MONITOR_DATA_UPDATE(const uint64_t sessionID, const BYTE dataType, const int dataValue, const int timeStamp);

private: // 스레드

    // 이더넷 하나에 대한 Send,Recv PDH 쿼리 정보
    // machineMonitorThread에서 사용
    struct EthernetPdhInfo
    {
        WCHAR           Name[128];
        PDH_HCOUNTER    RecvBytes;
        PDH_HCOUNTER    SendBytes;
    };

    // 모니터링 정보를 의미
    struct MonitorInfo
    {
        int         ServerNo;
        int         ValueSum;
        int         Count;
        int         Min;
        int         Max;
        std::mutex  Lock;
    };

    // 컴퓨터의 모니터링 정보를 초마다 클라이언트에게 송신하는 스레드 함수
    static unsigned int machineMonitorThread(void* server);

    // mLogDBWriteIntervalMinutes 분마다 DB에 로그를 저장
    static unsigned int logDBWriteThread(void* server);

    // mUnknowns에 대한 타임아웃 체크
    static unsigned int timeoutThread(void* server);

private:

    std::map<uint64_t, uint32_t>    mUnknowns;      // 연결된 세션, Accept tick (아직 로그인 안 함)
    std::shared_mutex               mUnknownsLock;
    uint32_t                        mMaxTimeout;
    uint32_t                        mTimeoutCheckInterval;

    std::list<uint64_t>             mClients;       // 연결된 모니터링 클라이언트들
    std::shared_mutex               mClientsLock;

    std::map<uint64_t, int32_t>     mServers;       // 모니터링 정보를 보내주는 서버들
    std::shared_mutex               mServersLock;

    // 모니터링 정보 취합을 위한 멤버
    // 인덱스가 곧 en_PACKET_SS_MONITOR_DATA_UPDATE 패킷 타입을 의미한다
    MonitorInfo     mMonitorInfos[en_PACKET_SS_MONITOR_DATA_UPDATE::MAX]{};

    DBConnector     mDBConnection;
    uint32_t        mLogDBWriteIntervalMinutes; // DB 저장 간격 (분)

    // 스레드 및 스레드 제어 이벤트
    bool            mbIsRunning = false;
    HANDLE          mMachineMonitorThread;
    HANDLE          mLogDBWriteThread;
    HANDLE          mTimeoutThread;
    HANDLE          mShutdownEvent;
    HANDLE          mMachineMonitorEvent;
    HANDLE          mLogDBWriteEvent;
    HANDLE          mTimeoutCheckEvent;

    // 로그인 세션 키
    const char*     LOGIN_SESSION_KEY = "ajfw@!cv980dSZ[fje#@fdj123948djf";
    const size_t    LOGIN_SESSION_KEY_LENGTH = 32;
};