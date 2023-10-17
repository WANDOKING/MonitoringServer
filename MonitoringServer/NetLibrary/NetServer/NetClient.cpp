#include <process.h>

#include "NetClient.h"
#include "NetUtils.h"
#include "Serializer.h"

NetClient::NetClient(const std::wstring serverIP, const uint16_t serverPortNumber)
    : mServerIP(serverIP)
    , mServerPortNumber(serverPortNumber)
{
    NetUtils::WSAStartup();
}

bool NetClient::Connect(void)
{
    if (mbIsConnected)
    {
        return false;
    }

    mSocket = NetUtils::CreateSocket();

    bool bConnectSuccess = NetUtils::TryConnect(mSocket, mServerIP.c_str(), mServerPortNumber);

    if (false == bConnectSuccess)
    {
        return false;
    }

    mRecvThread = (HANDLE)::_beginthreadex(nullptr, 0, recvThread, reinterpret_cast<void*>(this), 0, nullptr);
    mbIsConnected = true;

    if (mRecvThread != 0)
    {
        ::CloseHandle(mRecvThread);
    }

    return true;
}

void NetClient::Disconnect(void)
{
    if (mbIsConnected)
    {
        mbIsConnected = false;
        shutdown(mSocket, SD_BOTH); // 소켓 재사용 방지를 위해 shutdown 사용 -> RecvThread에서 close
    }
}

bool NetClient::SendPacket(Serializer* packet)
{
    if (false == mbIsConnected || packet == nullptr)
    {
        return false;
    }

    if (!packet->IsSendPrepared())
    {
        packet->prepareSend();
    }

    int retSend = ::send(mSocket, packet->GetFullBufferPointer(), packet->GetFullSize(), 0);

    if (retSend == SOCKET_ERROR)
    {
        mSendErrorCode = ::WSAGetLastError();
    }

    return true;
}

unsigned int NetClient::recvThread(void* netClient)
{
    NetClient* client = reinterpret_cast<NetClient*>(netClient);

    NetworkHeader header{};

    for (;;)
    {
        int retRecv = recv(client->mSocket, client->mRecvBuffer.GetRearBufferPtr(), client->mRecvBuffer.GetDirectEnqueueSize(), 0);
    
        if (retRecv == 0 || retRecv == SOCKET_ERROR)
        {
            // disconnect
            SOCKET socketBackup = client->mSocket;
            client->mbIsConnected = false;
            client->OnDisconnect(::WSAGetLastError());
            NetUtils::CloseSocket(socketBackup);
            break;
        }

        client->mRecvBuffer.MoveRear(retRecv);

        while (true)
        {
            // 1. header check
            if (client->mRecvBuffer.GetUseSize() < sizeof(NetworkHeader))
            {
                break;
            }

            bool retPeek = client->mRecvBuffer.Peek((char*)&header, sizeof(header));
            ASSERT_LIVE(retPeek == true, L"Client RecvBuffer Peek() Error");

#if NETWORK_HEADER_USE_TYPE == NETWORK_HEADER_TYPE_NET
            // NetClient - code check
            ASSERT_LIVE(header.Code == NETWORK_HEADER_CODE, L"client invalid NETWORK_HEADER_CODE received");
#endif

            // 2. payload check
            if (client->mRecvBuffer.GetUseSize() < sizeof(NetworkHeader) + header.Length)
            {
                ASSERT_LIVE(client->mRecvBuffer.GetFreeSize() > 0, L"client recvBuffer full");
                break;
            }

            // 3. packet copy
            Serializer* packet = Serializer::l_packetPool.Alloc();
            packet->IncrementRefCount();
            packet->Clear();

            bool retDequeue = client->mRecvBuffer.Dequeue(packet->GetFullBufferPointer(), header.Length + sizeof(NetworkHeader));
            ASSERT_LIVE(retDequeue == true, L"client RecvBuffer Dequeue() Error");
            packet->SetUseSize(header.Length);

#if NETWORK_HEADER_USE_TYPE == NETWORK_HEADER_TYPE_NET
            // NetClient - decode packet
            bool bIsValidPacket = packet->decode();
            ASSERT_LIVE(bIsValidPacket, L"client received invalid packet (decode failed)");
#endif

            // 5. OnReceive()
            client->OnReceive(packet);
        }
    }

    return 0;
}
