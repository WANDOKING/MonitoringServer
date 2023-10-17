#pragma once

#include "../Memory/LockFreeObjectPool.h"

template <typename T>
class LockFreeStack
{
public:
    LockFreeStack(void) = default;
    ~LockFreeStack(void) { Clear(); mNodePool.Clear(); }

    LockFreeStack(const LockFreeStack& other) = delete;
    LockFreeStack& operator=(const LockFreeStack& other) = delete;

    inline uint32_t GetCount(void) const { return mCount; }
    inline bool     IsEmpty(void) const { return mCount == 0; }

    void Push(T data)
    {
        Node* newNode = mNodePool.Alloc();
        newNode->Data = data;

        uint64_t localMyIdFlag = ((uint64_t)InterlockedIncrement(&mID) << ADDRESS_BIT_COUNT);
        PVOID newTopHopeToChange = (PVOID)((uint64_t)newNode | localMyIdFlag);

        Node* localMyTop; // 로컬에서 바라본 Top

        do
        {
            // a) Top 포인터를 로컬에 저장한다.
            // b) 생성한 노드의 Next를 내가 바라본 Top 포인터로 세팅한다
            // c) 현재 Top(mTop)이 내가 바라본 Top 포인터(localMyTop)과 동일한지 비교하고, 
            //    같다면 mTop을 내가 만든 새 노드(newNode)로 세팅

            localMyTop = mTop;
            newNode->Next = localMyTop;

        } while (InterlockedCompareExchangePointer((PVOID*)&mTop, newTopHopeToChange, localMyTop) != localMyTop);

        InterlockedIncrement(&mCount);
    }

    bool TryPop(T& outData)
    {
        Node* localMyTop;		// 로컬에서 바라본 Top
        Node* localMyPureTop;	// 로컬에서 바라본 Top에서 상위 16비트 ID 플래그를 제거한 값
        Node* localMyNext;		// 로컬에서 바라본 Next

        do
        {
            // a) Top 포인터를 로컬에 저장한다.
            //        - 내가 바라본 Top 포인터가 nullptr이라면 Pop 실패
            // b) 내가 바라본 Top 포인터의 Next를 로컬에 저장한다
            // c) 현재 Top(mTop)이 내가 바라본 Top 포인터(localMyTop)과 동일한지 비교하고, 
            //    같다면 mTop을 내가 바라본 Top의 Next(localMyNext)로 세팅

            localMyTop = mTop;

            if (localMyTop == nullptr)
            {
                return false;
            }

            localMyPureTop = getPurePointer(localMyTop);
            localMyNext = localMyPureTop->Next;

        } while (InterlockedCompareExchangePointer((PVOID*)&mTop, localMyNext, localMyTop) != localMyTop);

        InterlockedDecrement(&mCount);

        outData = localMyPureTop->Data;

        mNodePool.Free(localMyPureTop);

        return true;
    }

    // 스택 비우기
    // 스택에서 Pop()을 성공적으로 수행한 횟수를 반환
    uint32_t Clear(void)
    {
        uint32_t popCount = 0;
        T ignore;

        while (TryPop(ignore))
        {
            popCount++;
        }

        return popCount;
    }

private:
    struct Node
    {
        T Data;
        Node* Next;
    };

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
    Node*                       mTop = nullptr;
    uint32_t                    mID = 0;
    uint32_t                    mCount = 0;
    LockFreeObjectPool<Node>    mNodePool;
};