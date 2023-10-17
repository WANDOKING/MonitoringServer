#pragma once
#pragma comment(lib, "NetLibrary/DBConnector/libmysql.lib")
#pragma comment(lib, "NetLibrary/DBConnector/mysqlclient.lib")

#include <iostream>
#include <string>
#include <strsafe.h>
#include <Windows.h>

#include "mysql.h"
#include "errmsg.h"
#include "../Logger/Logger.h"

class DBConnector
{
public:
#pragma warning(push)
#pragma warning(disable: 26495)
    DBConnector(void)
    {
        mysql_init(&mMySQL);
    }

    DBConnector(
        const std::wstring& ip, 
        const std::wstring& user,
        const std::wstring& password,
        const std::wstring& DBName,
        const uint32_t port)
        : mIP(ip)
        , mUser(user)
        , mPassword(password)
        , mName(DBName)
        , mPort(port)
        , mbIsConnectionInfoSet(true)
    {
        mysql_init(&mMySQL);
    }
#pragma warning(pop)

    virtual ~DBConnector()
    {
        if (IsConnected())
        {
            Disconnect();
        }
    }

    // MySQL 라이브러리 초기화 - DB 커넥터 클래스 사용 전 항상 호출할 것
    inline static void InitializeLibrary(void) { mysql_library_init(0, nullptr, nullptr); }

    inline bool IsConnected(void) const { return mConnection != nullptr; }
    inline int  GetLastError(void) const { return mLastError; }

    inline void SetConnectionInfo(
        const std::wstring& ip,
        const std::wstring& user,
        const std::wstring& password,
        const std::wstring& DBName,
        const uint32_t port)
    {
        mIP = ip;
        mUser = user;
        mPassword = password;
        mPort = port;
        mbIsConnectionInfoSet = true;
    }

    // DB 연결
    bool Connect(void)
    {
        if (false == mbIsConnectionInfoSet)
        {
            return false;
        }

        char ip[64];
        char user[64];
        char password[64];
        char name[64];

        WideCharToMultiByte(CP_ACP, 0, mIP.c_str(), static_cast<int>(mIP.length() + 1), ip, 64, NULL, NULL);
        WideCharToMultiByte(CP_ACP, 0, mUser.c_str(), static_cast<int>(mUser.length() + 1), user, 64, NULL, NULL);
        WideCharToMultiByte(CP_ACP, 0, mPassword.c_str(), static_cast<int>(mPassword.length() + 1), password, 64, NULL, NULL);
        WideCharToMultiByte(CP_ACP, 0, mName.c_str(), static_cast<int>(mName.length() + 1), name, 64, NULL, NULL);

        mConnection = mysql_real_connect(&mMySQL, ip, user, password, name, 3306, (char*)NULL, 0);
        if (mConnection == NULL)
        {
            mLastError = mysql_errno(&mMySQL);
            return false;
        }

        return true;
    }

    // DB 연결 종료
    void Disconnect(void) { mysql_close(mConnection); }

    // 쿼리 날리기
    bool Query(const WCHAR* formatMessage, ...)
    {
        va_list ap;
        va_start(ap, formatMessage);
        {
            StringCchVPrintfW(mQueryUtf16, MAX_QUERY_LENGTH, formatMessage, ap);
        }
        va_end(ap);

        size_t queryLength = wcslen(mQueryUtf16);

        // UTF16 -> UTF8
        WideCharToMultiByte(CP_ACP, 0, mQueryUtf16, static_cast<int>(queryLength + 1), mQueryUtf8, sizeof(mQueryUtf8), NULL, NULL);

        DWORD queryBeginTick = ::timeGetTime();
        int retQuery = mysql_query(mConnection, mQueryUtf8);

        DWORD elapsed = ::timeGetTime() - queryBeginTick;

        if (elapsed >= 100)
        {
            LOGF(ELogLevel::System, L"Query elapsed (%u) : %s", elapsed, mQueryUtf16);
        }

        if (retQuery != 0)
        {
            mLastError = mysql_errno(&mMySQL);
            return false;
        }

        mQueryResult = mysql_store_result(mConnection);

        return true;
    }

    // 쿼리 결과 행 얻기
    MYSQL_ROW FetchRowOrNull(void) { return mysql_fetch_row(mQueryResult); }

    // 쿼리 결과 해제
    void FreeResult(void) { mysql_free_result(mQueryResult); }

    enum
    {
        MAX_QUERY_LENGTH = 2048
    };

private:
    MYSQL           mMySQL;
    bool            mbIsConnectionInfoSet;
    std::wstring    mIP;
    std::wstring    mUser;
    std::wstring    mPassword;
    std::wstring    mName;
    uint32_t        mPort;
    MYSQL*          mConnection = nullptr;
    MYSQL_RES*      mQueryResult = nullptr;
    WCHAR           mQueryUtf16[MAX_QUERY_LENGTH];
    char            mQueryUtf8[MAX_QUERY_LENGTH];
    int             mLastError = 0;
};