///////////////////////////////////////////////////////////////////////////////
// VirtualAlloc() / VirtualFree()을 사용하여 할당 영역 뒤에 NOACCESS 페이지를 붙임.
// 오버플로우 발생 시 크래시가 발생한다.
// 60K 이하 사이즈만 기능함.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <windows.h>

#include "../CrashDump/CrashDump.h"

template <typename T>
class OverflowChecker
{
public:
    static T* Alloc(void)
    {
        void* userAddress = ::VirtualAlloc(nullptr, PAGE_SIZE * NEED_PAGE, MEM_COMMIT, PAGE_READWRITE);
        void* noaccessAddress = ::VirtualAlloc(reinterpret_cast<char*>(userAddress) + PAGE_SIZE * (NEED_PAGE - 1), PAGE_SIZE, MEM_COMMIT, PAGE_NOACCESS);
        return reinterpret_cast<T*>(reinterpret_cast<char*>(noaccessAddress) - sizeof(T));
    }

    static void Free(T* address)
    {
        void* addressToFree = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(address) & 0xFFFF'FFFF'FFFF'0000);
        CrashDump::Assert(::VirtualFree(addressToFree, 0, MEM_RELEASE));
    }
private:
    enum
    {
        PAGE_SIZE = 4096,
        NEED_PAGE = (sizeof(T) - 1) / PAGE_SIZE + 2,
    };


    static_assert(sizeof(T) <= 60 * 1024, "too big size object");
};