#pragma once

#include <string>
#include <cstdint>

#include "RingBuffer.h"

typedef unsigned long long SOCKET;
typedef void* HANDLE;

class RingBuffer;
class Serializer;

class NetClient
{
public:
    NetClient(const std::wstring serverIP, const uint16_t serverPortNumber);

    virtual ~NetClient(void) = default;

    // 서버로 연결
    bool Connect(void);

    // 연결 끊기
    void Disconnect(void);

    // 패킷 보내기
    bool SendPacket(Serializer* packet);

public: // Getters

    inline bool IsConnected(void) { return mbIsConnected; }
    inline int  GetSendErrorCode(void) { return mSendErrorCode; }

public: // 클라이언트 핸들러 가상 함수

    virtual void OnReceive(Serializer* packet) = 0;
    virtual void OnDisconnect(int errorCode) = 0;

private: // 스레드 함수들

    static unsigned int recvThread(void* netClientSocket);

private:
    SOCKET          mSocket = 0;
    HANDLE          mRecvThread = 0;
    bool            mbIsConnected = false;
    RingBuffer      mRecvBuffer;
    int             mSendErrorCode = 0;
    std::wstring    mServerIP;
    uint16_t        mServerPortNumber;
};