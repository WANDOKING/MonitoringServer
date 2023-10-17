#include <conio.h>
#include <Windows.h>
#include <process.h>

#include "NetLibrary/Tool/ConfigReader.h"
#include "NetLibrary/Logger/Logger.h"
#include "NetLibrary/Profiler/Profiler.h"

#include "MonitoringServer.h"

MonitoringServer g_monitoringServer;

int main(void)
{
#ifdef PROFILE_ON
    LOGF(ELogLevel::System, L"Profiler: PROFILE_ON");
#endif

#pragma region config 파일 읽기
    const WCHAR* CONFIG_FILE_NAME = L"MonitoringServer.config";

    /*************************************** Config - Logger ***************************************/

    WCHAR inputLogLevel[10];

    ASSERT_LIVE(ConfigReader::GetString(CONFIG_FILE_NAME, L"LOG_LEVEL", inputLogLevel, sizeof(inputLogLevel)), L"ERROR: config file read failed (LOG_LEVEL)");

    if (wcscmp(inputLogLevel, L"DEBUG") == 0)
    {
        Logger::SetLogLevel(ELogLevel::System);
    }
    else if (wcscmp(inputLogLevel, L"ERROR") == 0)
    {
        Logger::SetLogLevel(ELogLevel::Error);
    }
    else if (wcscmp(inputLogLevel, L"SYSTEM") == 0)
    {
        Logger::SetLogLevel(ELogLevel::System);
    }
    else
    {
        ASSERT_LIVE(false, L"ERROR: invalid LOG_LEVEL");
    }

    LOGF(ELogLevel::System, L"Logger Log Level = %s", inputLogLevel);

    /*************************************** Config - NetServer ***************************************/

    uint32_t inputPortNumber;
    uint32_t inputMaxSessionCount;
    uint32_t inputConcurrentThreadCount;
    uint32_t inputWorkerThreadCount;
    uint32_t inputSetTcpNodelay;
    uint32_t inputSetSendBufZero;

    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"PORT", &inputPortNumber), L"ERROR: config file read failed (PORT)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"MAX_SESSION_COUNT", &inputMaxSessionCount), L"ERROR: config file read failed (MAX_SESSION_COUNT)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"CONCURRENT_THREAD_COUNT", &inputConcurrentThreadCount), L"ERROR: config file read failed (CONCURRENT_THREAD_COUNT)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"WORKER_THREAD_COUNT", &inputWorkerThreadCount), L"ERROR: config file read failed (WORKER_THREAD_COUNT)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"TCP_NODELAY", &inputSetTcpNodelay), L"ERROR: config file read failed (TCP_NODELAY)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"SND_BUF_ZERO", &inputSetSendBufZero), L"ERROR: config file read failed (SND_BUF_ZERO)");

    LOGF(ELogLevel::System, L"CONCURRENT_THREAD_COUNT = %u", inputConcurrentThreadCount);
    LOGF(ELogLevel::System, L"WORKER_THREAD_COUNT = %u", inputWorkerThreadCount);

    if (inputSetTcpNodelay != 0)
    {
        g_monitoringServer.SetTcpNodelay(true);
        LOGF(ELogLevel::System, L"myChatServer.SetTcpNodelay(true)");
    }

    if (inputSetSendBufZero != 0)
    {
        g_monitoringServer.SetSendBufferSizeToZero(true);
        LOGF(ELogLevel::System, L"myChatServer.SetSendBufferSizeToZero(true)");
    }

    /*************************************** Config - MonitoringServer ***************************************/

    uint32_t inputLogDBWriteIntervalMinutes;
    uint32_t inputTimeout;
    uint32_t inputTimeoutCheckInterval;

    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"LOG_DB_INTERVAL_MINUTES", &inputLogDBWriteIntervalMinutes), L"ERROR: config file read failed (LOG_DB_INTERVAL_MINUTES)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"TIMEOUT", &inputTimeout), L"ERROR: config file read failed (TIMEOUT)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"TIMEOUT_CHECK_INTERVAL", &inputTimeoutCheckInterval), L"ERROR: config file read failed (TIMEOUT_CHECK_INTERVAL)");

    g_monitoringServer.SetLogDBWriteIntervalMinutes(inputLogDBWriteIntervalMinutes);
    g_monitoringServer.SetMaxTimeout(inputTimeout);
    g_monitoringServer.SetTimeoutCheckInterval(inputTimeoutCheckInterval);

    LOGF(ELogLevel::System, L"LOG_DB_INTERVAL_MINUTES = %u", inputLogDBWriteIntervalMinutes);
    LOGF(ELogLevel::System, L"TIMEOUT = %u", inputTimeout);
    LOGF(ELogLevel::System, L"TIMEOUT_CHECK_INTERVAL = %u", inputTimeoutCheckInterval);

    // DB Connection config input

    WCHAR inputDBIP[64];
    WCHAR inputDBUser[64];
    WCHAR inputDBPassword[64];
    WCHAR inputDBName[64];
    uint32_t inputDBPort;

    ASSERT_LIVE(ConfigReader::GetString(CONFIG_FILE_NAME, L"DB_IP", inputDBIP, 64), L"ERROR: config file read failed (DB_IP)");
    ASSERT_LIVE(ConfigReader::GetString(CONFIG_FILE_NAME, L"DB_USER", inputDBUser, 64), L"ERROR: config file read failed (DB_USER)");
    ASSERT_LIVE(ConfigReader::GetString(CONFIG_FILE_NAME, L"DB_PASSWORD", inputDBPassword, 64), L"ERROR: config file read failed (DB_PASSWORD)");
    ASSERT_LIVE(ConfigReader::GetString(CONFIG_FILE_NAME, L"DB_NAME", inputDBName, 64), L"ERROR: config file read failed (DB_NAME)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"DB_PORT", &inputDBPort), L"ERROR: config file read failed (DB_PORT)");

    g_monitoringServer.SetDBConnectionInfo(inputDBIP, inputDBUser, inputDBPassword, inputDBName, inputDBPort);

    LOGF(ELogLevel::System, L"DB IP = %s", inputDBIP);
    LOGF(ELogLevel::System, L"DB User = %s", inputDBUser);
    LOGF(ELogLevel::System, L"DB Password = %s", inputDBPassword);
    LOGF(ELogLevel::System, L"DB Name = %s", inputDBName);
    LOGF(ELogLevel::System, L"DB Port = %u", inputDBPort);
#pragma endregion

    DBConnector::InitializeLibrary();

    // MaxPayloadLength
    g_monitoringServer.SetMaxPayloadLength(39);

    // Server Run
    g_monitoringServer.Start(static_cast<uint16_t>(inputPortNumber), inputMaxSessionCount, inputConcurrentThreadCount, inputWorkerThreadCount);

    for (;;)
    {
        ::Sleep(1'000);

        if (_kbhit())
        {
            int input = _getch();
            if (input == 'Q' || input == 'q')
            {
                g_monitoringServer.Shutdown();
                break;
            }
        }

        MonitoringVariables monitoringInfo = g_monitoringServer.GetMonitoringInfo();

        wprintf(L"\n\n");
        wprintf(L"[Monitoring Server Running           (Q: quit)]\n");
        wprintf(L"===============================================\n");
        wprintf(L"Session Count          : %u / %u\n", g_monitoringServer.GetSessionCount(), g_monitoringServer.GetMaxSessionCount());
        wprintf(L"  - Monitoring Clients : %u\n", g_monitoringServer.GetConnectedClientCount());
        wprintf(L"  - Servers            : %u\n", g_monitoringServer.GetConnectedServerCount());
        wprintf(L"  - Unknown            : %u\n", g_monitoringServer.GetUnknownSessionCount());
        wprintf(L"===============================================\n");
        wprintf(L"Accept Total        = %llu\n", g_monitoringServer.GetTotalAcceptCount());
        wprintf(L"Disconnected Total  = %llu\n", g_monitoringServer.GetTotalDisconnectCount());
        wprintf(L"Packet Pool Size    = %u\n", Serializer::GetTotalPacketCount());
        wprintf(L"--------------------- TPS ---------------------\n");
        wprintf(L"Accept TPS           = %7u (Avg: %7u)\n", monitoringInfo.AcceptTPS, monitoringInfo.AverageAcceptTPS);
        wprintf(L"Send Message TPS     = %7u (Avg: %7u)\n", monitoringInfo.SendMessageTPS, monitoringInfo.AverageSendMessageTPS);
        wprintf(L"Recv Message TPS     = %7u (Avg: %7u)\n", monitoringInfo.RecvMessageTPS, monitoringInfo.AverageRecvMessageTPS);
        wprintf(L"Send Pending TPS     = %7u (Avg: %7u)\n", monitoringInfo.SendPendingTPS, monitoringInfo.AverageSendPendingTPS);
        wprintf(L"Recv Pending TPS     = %7u (Avg: %7u)\n", monitoringInfo.RecvPendingTPS, monitoringInfo.AverageRecvPendingTPS);
        wprintf(L"--------------------- CPU ---------------------\n");
        wprintf(L"Total  = Processor: %6.3f / Process: %6.3f\n", monitoringInfo.ProcessorTimeTotal, monitoringInfo.ProcessTimeTotal);
        wprintf(L"User   = Processor: %6.3f / Process: %6.3f\n", monitoringInfo.ProcessorTimeUser, monitoringInfo.ProcessTimeUser);
        wprintf(L"Kernel = Processor: %6.3f / Process: %6.3f\n", monitoringInfo.ProcessorTimeKernel, monitoringInfo.ProcessTimeKernel);
    }
}