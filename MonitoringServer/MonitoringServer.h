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
#pragma warning(disable: 26495) // ���� �ʱ�ȭ ��� ����
    MonitoringServer() = default;
#pragma warning(pop)

    virtual ~MonitoringServer() = default;

public: // ���� ���� �� ���� �Լ���

    // DB ���� ���� (��) ����, 0���� �����ϸ� DB�� �α׸� ���� ���� �ʴ´�
    void SetLogDBWriteIntervalMinutes(const uint32_t intervalMinutes) { mLogDBWriteIntervalMinutes = intervalMinutes; }

    // DB ���� ���� ����
    void SetDBConnectionInfo(const WCHAR* ip, const WCHAR* user, const WCHAR* password, const WCHAR* DBName, const uint32_t port) { mDBConnection.SetConnectionInfo(ip, user, password, DBName, port); }

    // ���� �α������� ���� ���ǵ鿡 ���� Ÿ�Ӿƿ� �ð�
    void SetMaxTimeout(uint32_t maxTimeout) { mMaxTimeout = maxTimeout; }

    // Ÿ�Ӿƿ� �������� Ÿ�Ӿƿ� üũ ����
    // 0�̸� Ÿ�Ӿƿ��� ������� ����
    void SetTimeoutCheckInterval(uint32_t timeoutCheckInterval) { mTimeoutCheckInterval = timeoutCheckInterval; }

public:

    // ���� ����
    virtual void Start(
        const uint16_t port,
        const uint32_t maxSessionCount,
        const uint32_t iocpConcurrentThreadCount,
        const uint32_t iocpWorkerThreadCount) override;

    // ���� ���� (��� �����尡 ����� �� ���� �����)
    virtual void Shutdown(void);

    // NetServer��(��) ���� ��ӵ�
    virtual void OnAccept(const uint64_t sessionID) override;
    virtual void OnReceive(const uint64_t sessionID, Serializer* packet) override;
    virtual void OnRelease(const uint64_t sessionID) override;

public: // ����

    uint32_t    GetUnknownSessionCount(void) { return static_cast<uint32_t>(mUnknowns.size()); }
    uint32_t    GetConnectedClientCount(void) { return static_cast<uint32_t>(mClients.size()); }
    uint32_t    GetConnectedServerCount(void) { return static_cast<uint32_t>(mServers.size()); }
    uint32_t    GetLogDBWriteIntervalMinutes(void) { return mLogDBWriteIntervalMinutes; }
    
public:
    
    // ������ �ִ� ����͸� Ŭ���̾�Ʈ�鿡�� ����͸� ������ �Ѹ���
    void Broadcast_MONITOR_DATA_UPDATE(const BYTE serverNo, const BYTE dataType, const int32_t value, const int32_t timeStamp);

private: // ��Ŷ ����

    static Serializer* Create_CS_MONITOR_TOOL_RES_LOGIN(const BYTE status);
    static Serializer* Create_CS_MONITOR_TOOL_DATA_UPDATE(const BYTE serverNo, const BYTE dataType, const int dataValue, const int timeStamp);

private: // ��Ŷ ó��

    // ����͸� Ŭ���̾�Ʈ�� �α���
    void process_CS_MONITOR_TOOL_REQ_LOGIN(const uint64_t sessionID, const char loginSessionKey[]);

    // �ٸ� ������ ����͸� �������� �α���
    void process_SS_MONITOR_LOGIN(const uint64_t sessionID, const int serverNo);

    // �ٸ� ������ ����͸� ���� ����
    void process_SS_MONITOR_DATA_UPDATE(const uint64_t sessionID, const BYTE dataType, const int dataValue, const int timeStamp);

private: // ������

    // �̴��� �ϳ��� ���� Send,Recv PDH ���� ����
    // machineMonitorThread���� ���
    struct EthernetPdhInfo
    {
        WCHAR           Name[128];
        PDH_HCOUNTER    RecvBytes;
        PDH_HCOUNTER    SendBytes;
    };

    // ����͸� ������ �ǹ�
    struct MonitorInfo
    {
        int         ServerNo;
        int         ValueSum;
        int         Count;
        int         Min;
        int         Max;
        std::mutex  Lock;
    };

    // ��ǻ���� ����͸� ������ �ʸ��� Ŭ���̾�Ʈ���� �۽��ϴ� ������ �Լ�
    static unsigned int machineMonitorThread(void* server);

    // mLogDBWriteIntervalMinutes �и��� DB�� �α׸� ����
    static unsigned int logDBWriteThread(void* server);

    // mUnknowns�� ���� Ÿ�Ӿƿ� üũ
    static unsigned int timeoutThread(void* server);

private:

    std::map<uint64_t, uint32_t>    mUnknowns;      // ����� ����, Accept tick (���� �α��� �� ��)
    std::shared_mutex               mUnknownsLock;
    uint32_t                        mMaxTimeout;
    uint32_t                        mTimeoutCheckInterval;

    std::list<uint64_t>             mClients;       // ����� ����͸� Ŭ���̾�Ʈ��
    std::shared_mutex               mClientsLock;

    std::map<uint64_t, int32_t>     mServers;       // ����͸� ������ �����ִ� ������
    std::shared_mutex               mServersLock;

    // ����͸� ���� ������ ���� ���
    // �ε����� �� en_PACKET_SS_MONITOR_DATA_UPDATE ��Ŷ Ÿ���� �ǹ��Ѵ�
    MonitorInfo     mMonitorInfos[en_PACKET_SS_MONITOR_DATA_UPDATE::MAX]{};

    DBConnector     mDBConnection;
    uint32_t        mLogDBWriteIntervalMinutes; // DB ���� ���� (��)

    // ������ �� ������ ���� �̺�Ʈ
    bool            mbIsRunning = false;
    HANDLE          mMachineMonitorThread;
    HANDLE          mLogDBWriteThread;
    HANDLE          mTimeoutThread;
    HANDLE          mShutdownEvent;
    HANDLE          mMachineMonitorEvent;
    HANDLE          mLogDBWriteEvent;
    HANDLE          mTimeoutCheckEvent;

    // �α��� ���� Ű
    const char*     LOGIN_SESSION_KEY = "ajfw@!cv980dSZ[fje#@fdj123948djf";
    const size_t    LOGIN_SESSION_KEY_LENGTH = 32;
};