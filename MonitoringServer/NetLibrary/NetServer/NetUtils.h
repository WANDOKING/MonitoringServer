// 항상 최상단에 인클루드 할 것

#pragma once
#pragma comment(lib, "ws2_32")

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#include "../Logger/Logger.h"

////////////////////////////////////////////////
// 소켓, 주소 관련 함수들을 래핑한 유틸 클래스
// 편의성 및 에러 처리를 좀 더 깔끔하게 하고자 하는 의도
////////////////////////////////////////////////
class NetUtils final
{
public:
    NetUtils() = delete;

    // WSAStartup()
    inline static void WSAStartup(void)
    {
        WSADATA wsaData;
        ASSERT_LIVE(::WSAStartup(MAKEWORD(2, 2), &wsaData) == 0, L"WSAStartup() failed");
    }

    // WSACleanup()
    inline static void WSACleanup(void)
    {
        ::WSACleanup();
    }

#pragma region 소켓 다루기

    // socket()
    inline static SOCKET    CreateSocket(void)
    {
        SOCKET newSocket = ::socket(AF_INET, SOCK_STREAM, 0);

        ASSERT_LIVE(newSocket != INVALID_SOCKET, L"CreateSocket() failed");

        return newSocket;
    }

    // bind()
    inline static void      BindSocket(const SOCKET socket, const uint16_t portNumber)
    {
        SOCKADDR_IN serverAddress{};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = ::htonl(INADDR_ANY);
        serverAddress.sin_port = ::htons(portNumber);

        int retBind = ::bind(socket, (const sockaddr*)&serverAddress, sizeof(serverAddress));

        ASSERT_LIVE(retBind != SOCKET_ERROR, L"BindSocket() failed");
    }

    // listen()
    inline static void      SetSocketListen(const SOCKET listenSocket, const int backlogQueueSize = SOMAXCONN)
    {
        int retListen = ::listen(listenSocket, backlogQueueSize);
        
        ASSERT_LIVE(retListen != SOCKET_ERROR, L"SetSocketListen() failed");
    }

    // connect()
    inline static bool      TryConnect(const SOCKET clientSocket, const std::wstring serverIP, const uint16_t serverPortNumber)
    {
        SOCKADDR_IN serverAddress;
        serverAddress.sin_family = AF_INET;
        InetPtonW(AF_INET, serverIP.c_str(), &serverAddress.sin_addr.s_addr);
        serverAddress.sin_port = ::htons(serverPortNumber);

        int retConnect = ::connect(clientSocket, (const SOCKADDR*)&serverAddress, sizeof(serverAddress));

        if (retConnect == SOCKET_ERROR)
        {
            return false;
        }

        return true;
    }
    
    // closesocket()
    inline static void      CloseSocket(const SOCKET socket)
    {
        ::closesocket(socket);
    }

    // accept()
    inline static SOCKET    GetAcceptedSocketOrInvalid(const SOCKET listenSocket, SOCKADDR_IN* outAddress)
    {
        int clientAddressLength = sizeof(SOCKADDR_IN);
        return ::accept(listenSocket, (SOCKADDR*)outAddress, &clientAddressLength);
    }

    // setsockopt(SO_LINGER)
    inline static void      SetLinger(const SOCKET socket, const u_short onoff, const u_short linger)
    {
        LINGER linegerOptval{};
        linegerOptval.l_onoff = onoff;
        linegerOptval.l_linger = linger;

        int retSetsockopt = ::setsockopt(socket, SOL_SOCKET, SO_LINGER, (const char*)&linegerOptval, sizeof(linegerOptval));
        
        ASSERT_LIVE(retSetsockopt != SOCKET_ERROR, L"SetLinger() failed");
    }

    // setsockopt(SO_SNDBUF)
    inline static void      SetSendBufferSize(const SOCKET socket, const int sendBufferSize)
    {
        int retSetsockopt = ::setsockopt(socket, SOL_SOCKET, SO_SNDBUF, (const char*)&sendBufferSize, sizeof(sendBufferSize));
        
        ASSERT_LIVE(retSetsockopt != SOCKET_ERROR, L"SetSendBufferSize() failed");
    }

    // setsockopt(TCP_NODELAY)
    inline static void      SetTcpNodelay(const SOCKET socket)
    {
        const int optionValue = 1;

        int retSetsockopt = ::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&optionValue, sizeof(optionValue));
    
        ASSERT_LIVE(retSetsockopt != SOCKET_ERROR, L"SetTcpNodelay() failed");
    }

#pragma endregion

#pragma region IOCP 관련

    // CreateIOCP(-1, 0, 0, iocpConcurrentThreadCount)
    inline static HANDLE    CreateNewIOCP(const uint32_t iocpConcurrentThreadCount)
    {
        HANDLE iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, iocpConcurrentThreadCount);
        ASSERT_LIVE(iocp != NULL, L"CreateNewIOCP() failed");
        
        return iocp;
    }

    // CreateIOCP(socket, iocpHandle, completionKey, 0)
    inline static void      RegisterIOCP(const SOCKET socket, const HANDLE iocpHandle, const ULONG_PTR completionKey)
    {
        HANDLE retCreateIOCP = ::CreateIoCompletionPort((HANDLE)socket, iocpHandle, completionKey, 0);
        ASSERT_LIVE(retCreateIOCP != NULL, L"RegisterIOCP() failed");
    }

#pragma endregion

#pragma region 주소 변환

    inline static std::wstring      GetIpAddress(const SOCKADDR_IN address)
    {
        WCHAR clientIP[16];
        ::InetNtopW(AF_INET, &address.sin_addr.s_addr, clientIP, 16);

        return std::wstring(clientIP);
    }

    inline static uint16_t          GetPortNumber(const SOCKADDR_IN address)
    {
        return ::ntohs(address.sin_port);
    }

    inline static bool              DomainToIp(const std::wstring domain, IN_ADDR* const outAddress)
    {
        ADDRINFOW* addrInfo;
        if (::GetAddrInfo(domain.c_str(), L"0", nullptr, &addrInfo) != 0)
        {
            return false;
        }

        SOCKADDR_IN* sockAddr = (SOCKADDR_IN*)addrInfo->ai_addr;
        *outAddress = sockAddr->sin_addr;
        ::FreeAddrInfo(addrInfo);

        return true;
    }

#pragma endregion
};