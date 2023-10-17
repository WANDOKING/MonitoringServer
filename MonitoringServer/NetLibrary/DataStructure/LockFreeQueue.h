#pragma once

#include "../Memory/LockFreeObjectPool.h"

template <typename T>
class LockFreeQueue
{
public:
    LockFreeQueue(void)
    {
        Node* dummy = mNodePool.Alloc();
        dummy->Next = nullptr;
        mHead = dummy;
        mTail = dummy;
    }

    ~LockFreeQueue(void)
    {
        Clear();
        mNodePool.Free(getPurePointer(mHead));
        mNodePool.Clear();
    }

    LockFreeQueue(const LockFreeQueue& other) = delete;
    LockFreeQueue& operator=(const LockFreeQueue& other) = delete;

    inline uint32_t GetCount(void) const { return mCount; }
    inline bool     IsEmpty(void) const { return mCount == 0; }

    void Enqueue(T data)
    {
        uint64_t localMyIdFlag = (uint64_t)InterlockedIncrement(&mID) << ADDRESS_BIT_COUNT;

        Node* newNode = mNodePool.Alloc();
        newNode->Data = data;
        newNode->Next = nullptr;

        Node* localMyTail;
        Node* localMyPureTail;
        Node* localMyPureTailNext;

        PVOID newTailHopeToChange = (PVOID)((uint64_t)newNode | localMyIdFlag);

        do
        {
            localMyTail = mTail;
            localMyPureTail = getPurePointer(localMyTail);

            if (InterlockedCompareExchangePointer((PVOID*)&(localMyPureTail->Next), newTailHopeToChange, nullptr) == nullptr)
            {
                break;
            }
            else
            {
                localMyPureTailNext = localMyPureTail->Next;
                InterlockedCompareExchangePointer((PVOID*)&mTail, localMyPureTailNext, localMyTail);
            }

        } while (true);

        InterlockedCompareExchangePointer((PVOID*)&mTail, newTailHopeToChange, localMyTail);

        InterlockedIncrement(&mCount);
    }

    bool TryDequeue(T& outData)
    {
        if (mCount == 0)
        {
            return false;
        }

        Node* localMyHead;
        Node* localMyTail;
        Node* localMyPureHead;
        Node* localMyHeadNext;
        Node* localMyPureHeadNext;

        do
        {
        RETRY:
            localMyHead = mHead;
            localMyTail = mTail;
            localMyPureHead = getPurePointer(localMyHead);
            localMyHeadNext = localMyPureHead->Next;
            localMyPureHeadNext = getPurePointer(localMyHeadNext);

            if (localMyHead == localMyTail)
            {
                if (mCount == 0)
                {
                    return false;
                }
                else
                {
                    goto RETRY;
                }
            }

            if (localMyPureHeadNext == nullptr)
            {
                goto RETRY;
            }

            outData = localMyPureHeadNext->Data;
        } while (InterlockedCompareExchangePointer((PVOID*)&mHead, localMyHeadNext, localMyHead) != localMyHead);

        mNodePool.Free(localMyPureHead);

        InterlockedDecrement(&mCount);

        return true;
    }

    uint32_t Clear(void)
    {
        uint32_t dequeueCount = 0;
        T ignore;

        while (TryDequeue(ignore))
        {
            dequeueCount++;
        }

        return dequeueCount;
    }

private:
    struct Node
    {
        Node* Next;
        T Data;
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
    Node*                       mHead = nullptr;
    Node*                       mTail = nullptr;
    uint32_t                    mID = 0;
    uint32_t                    mCount = 0;
    LockFreeObjectPool<Node>    mNodePool;
};