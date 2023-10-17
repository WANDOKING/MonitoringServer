#pragma once

#include <cstdint>
#include <Windows.h>

#include "../CrashDump/CrashDump.h"

template <typename T>
class LockFreeObjectPool
{
public:
    LockFreeObjectPool(bool bNeedPlacementNew = false)
        : mbNeedPlacementNew(bNeedPlacementNew)
    {
        checkIdFlagBitCountIsValid();
    }

    LockFreeObjectPool(int capacity, bool bNeedPlacementNew = false)
        : mbNeedPlacementNew(bNeedPlacementNew)
    {
        checkIdFlagBitCountIsValid();

        T** allocAddresses = new T * [capacity];

        for (int i = 0; i < capacity; ++i)
        {
            allocAddresses[i] = Alloc();
        }

        for (int i = 0; i < capacity; ++i)
        {
            Free(allocAddresses[i]);
        }

        delete[] allocAddresses;
    }

    inline uint32_t	GetCapacity() { return mCapacity; }
    inline uint32_t	GetSize() { return mSize; }
    inline bool		IsCallPlacementNewWhenAlloc() { return mbNeedPlacementNew; }

    // 오브젝트 풀로부터 오브젝트를 할당받는다
    T* Alloc()
    {
        Node* retNode;			// 반환할 주소를 가지는 노드의 주소

        Node* localMyTop;
        Node* localMyNext;

        do
        {
            localMyTop = mTop;

            // 새로운 노드를 할당해서 반환
            if (localMyTop == nullptr)
            {
                InterlockedIncrement(&mCapacity);
                retNode = new Node;

                // 생성자 호출
                new (&(retNode->Data)) T();

                goto END;
            }

            localMyNext = getPurePointer(localMyTop)->Next;

        } while (InterlockedCompareExchangePointer((PVOID*)&mTop, localMyNext, localMyTop) != localMyTop);

        InterlockedDecrement(&mSize);

        retNode = getPurePointer(localMyTop);

        if (mbNeedPlacementNew)
        {
            new (&(retNode->Data)) T();
        }

    END:
#if USING_OBJECT_POOL_OPTION == POOL_OPTION_DEBUG_LOCK_FREE_POOL
        retNode->SafeBlock = (Node*)this;
        retNode->Next = (Node*)this;
#endif

        return &(retNode->Data);
    }

    // 오브젝트 풀에 오브젝트를 반납한다
    void Free(T* address)
    {
#if USING_OBJECT_POOL_OPTION == POOL_OPTION_DEBUG_LOCK_FREE_POOL
        Node* node = (Node*)((unsigned char*)address - sizeof(Node*));

        // memory pollution detect
        CrashDump::Assert(node->SafeBlock == (Node*)this);
        CrashDump::Assert(node->Next == (Node*)this);

#else
        Node* node = (Node*)address;
#endif

        if (mbNeedPlacementNew)
        {
            address->~T();
        }

        uint64_t localMyIdFlag = (uint64_t)InterlockedIncrement(&mID) << ADDRESS_BIT_COUNT;
        PVOID newTopHopeToChange = (PVOID)((uint64_t)node | localMyIdFlag);
        Node* localMyTop;

        do
        {
            localMyTop = mTop;
            node->Next = localMyTop;
        } while (InterlockedCompareExchangePointer((PVOID*)&mTop, newTopHopeToChange, localMyTop) != localMyTop);

        InterlockedIncrement(&mSize);
    }

    // 오브젝트 풀을 비운다
    // 이 함수는 오브젝트 풀에 모든 오브젝트가 반납된 상황이 아니라면 실패한다
    void Clear(void)
    {
        CrashDump::Assert(mCapacity == mSize);

        Node* visit = getPurePointer(mTop);

        while (visit != nullptr)
        {
            Node* next = getPurePointer(visit->Next);
            delete visit;
            visit = next;
        }

        mTop = nullptr;
        mID = 0;
        mCapacity = 0;
        mSize = 0;
    }
private:
    // Node
#if USING_OBJECT_POOL_OPTION == POOL_OPTION_DEBUG_LOCK_FREE_POOL
    struct Node
    {
        Node* SafeBlock{};
        T Data{};
        Node* Next{};
    };
#else
    struct Node
    {
        T Data{};
        Node* Next{};
    };
#endif

    // 유저 영역 주소 최대값을 확인하여 ID 비트를 문제 없이 사용할 수 있는지 확인
    static void checkIdFlagBitCountIsValid()
    {
        SYSTEM_INFO systemInfo;

        ::GetSystemInfo(&systemInfo);

        CrashDump::Assert(systemInfo.lpMaximumApplicationAddress == (LPVOID)0x0000'7FFF'FFFE'FFFF);
    }

    // 순수한 주소의 포인터 값을 계산 (ID 비트를 제거)
    static inline Node* getPurePointer(Node* address)
    {
        return (Node*)((uint64_t)address & PURE_POINTER_MASK);
    }

    enum : uint64_t
    {
        ID_FLAG_BIT_COUNT = 16,
        ADDRESS_BIT_COUNT = 48,
        PURE_POINTER_MASK = UINT64_MAX >> ID_FLAG_BIT_COUNT
    };
private:
    Node* mTop = nullptr;
    bool		mbNeedPlacementNew; // Alloc()/Free()호출 시, 생성자/소멸자를 호출 할 것인지에 대한 옵션
    uint32_t	mID = 0;			// Top의 상위 16비트를 ID로 사용할 것
    uint32_t	mCapacity = 0;
    uint32_t	mSize = 0;
};