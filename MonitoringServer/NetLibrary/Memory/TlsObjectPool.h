///////////////////////////////////////////////////////////////////////////////
// TLS 오브젝트 풀
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// 오브젝트풀의 안전 모드
// 안전 모드 사용 시 오브젝트 앞 뒤로 오브젝트 풀 매니저의 this 포인터를 붙인다.
// Free()로 전달받은 포인터의 앞 뒤를 조사하여 이 값이 오염되었는지 확인한다.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <Windows.h>

#include "ObjectPool.h"
#include "../CrashDump/CrashDump.h"

template <typename T>
class TlsObjectPool
{
public:
    inline static bool      IsCallPlacementNewWhenAlloc(void) { return mbNeedPlacementNew; }
    inline static uint32_t  GetObjectPerChunkCount(void) { return OBJECT_COUNT_PER_CHUNK; }
    inline static uint32_t  GetTotalChunkCount(void) { return mPoolManager.mChunkTotalCount; }
    inline static uint32_t  GetTotalCreatedObjectCount(void) { return mPoolManager.mChunkTotalCount * OBJECT_COUNT_PER_CHUNK; }

    // 미리 청크를 만들어 놓는다
    static void PreCreateChunk(uint32_t chunkCount)
    {
        for (uint32_t i = 0; i < chunkCount; ++i)
        {
            mPoolManager.CreateChunk();
        }
    }

public:
    TlsObjectPool(bool bNeedPlacementNew = false) { mbNeedPlacementNew = bNeedPlacementNew; }

    inline uint32_t GetSize(void) const { return mSize; }

    // 오브젝트 풀로부터 오브젝트를 할당받는다
    T* Alloc(void)
    {
        // 비어있다면 오브젝트 풀 매니저로부터 오브젝트 덩어리를 가져온다
        if (mTop == nullptr)
        {
            mTop = mPoolManager.AllocChunk();
            mSize = OBJECT_COUNT_PER_CHUNK;
        }

        Node* retNode = mTop;
        mTop = mTop->Next;
        --mSize;

        if (mbNeedPlacementNew)
        {
            new (&(retNode->Data)) T();
        }

#if USING_OBJECT_POOL_OPTION == POOL_OPTION_DEBUG_POOL
        retNode->SafeBlock = (Node*)(&mPoolManager);
        retNode->Next = (Node*)(&mPoolManager);
#endif

        return &(retNode->Data);
    }

    // 오브젝트 풀에 오브젝트를 반납한다
    void Free(T* address)
    {
#if USING_OBJECT_POOL_OPTION == POOL_OPTION_DEBUG_POOL
        Node* node = (Node*)((unsigned char*)address - sizeof(Node*));

        // 메모리 오염 체크
        CrashDump::Assert(node->SafeBlock == (Node*)(&mPoolManager));
        CrashDump::Assert(node->Next == (Node*)(&mPoolManager));
#else
        Node* node = (Node*)address;
#endif

        if (mbNeedPlacementNew)
        {
            address->~T();
        }

        node->Next = mTop;
        mTop = node;
        ++mSize;

        if (mSize == MAX_OBJECT_COUNT_PER_THREAD)
        {
            Node* newTop = mHalfTop->Next;
            mHalfTop->Next = nullptr;
            mPoolManager.FreeChunk(mTop);
            mTop = newTop;
            mSize -= OBJECT_COUNT_PER_CHUNK;
        }
        else if (mSize == OBJECT_COUNT_PER_CHUNK + 1)
        {
            mHalfTop = mTop;
        }
    }

private:

#if USING_OBJECT_POOL_OPTION == POOL_OPTION_DEBUG_POOL
    struct Node
    {
        Node* SafeBlock;
        T Data;
        Node* Next;
    };
#else
    struct Node
    {
        T Data;
        Node* Next;
    };
#endif

    enum
    {
        MAX_CHUNK_COUNT = 100'000,
        OBJECT_COUNT_PER_CHUNK = 500,
        MAX_OBJECT_COUNT_PER_THREAD = OBJECT_COUNT_PER_CHUNK * 2,
    };

private:

    // 풀 마다 하나씩 존재할 중앙 풀 관리자
    // 풀 매니저로부터 청크를 할당받는다
    class ObjectPoolManager
    {
    public:
        ObjectPoolManager(void) { ::InitializeSRWLock(&mLock); }

        // 청크를 할당받는다
        Node* AllocChunk(void)
        {
            Node* ret = nullptr;
            bool bIsEmpty;

            {
                ::AcquireSRWLockExclusive(&mLock);

                bIsEmpty = mChunkInManagerCount == 0;

                if (!bIsEmpty)
                {
                    --mChunkInManagerCount;
                    ret = mChunks[mChunkInManagerCount];
                }

                ::ReleaseSRWLockExclusive(&mLock);
            }

            if (bIsEmpty)
            {
                // 새롭게 청크를 만들어 반환한다

                Node* prevNode = nullptr;

                for (int i = 0; i < OBJECT_COUNT_PER_CHUNK; ++i)
                {
                    Node* newNode = new Node;
                    newNode->Next = prevNode;
                    prevNode = newNode;
                }

                ret = prevNode;

                CrashDump::Assert(InterlockedIncrement(&mChunkTotalCount) <= MAX_CHUNK_COUNT);
            }

            return ret;
        }

        // 청크 반환
        void FreeChunk(Node* chunkTop)
        {
            ::AcquireSRWLockExclusive(&mLock);

            mChunks[mChunkInManagerCount] = chunkTop;
            ++mChunkInManagerCount;

            ::ReleaseSRWLockExclusive(&mLock);
        }

        // 청크를 만들어서 풀 매니저에 보관해 놓는다
        void CreateChunk(void)
        {
            Node* prevNode = nullptr;

            for (int i = 0; i < OBJECT_COUNT_PER_CHUNK; ++i)
            {
                Node* newNode = new Node;
                newNode->Next = prevNode;
                prevNode = newNode;
            }

            CrashDump::Assert(InterlockedIncrement(&mChunkTotalCount) <= MAX_CHUNK_COUNT);

            ::AcquireSRWLockExclusive(&mLock);

            mChunks[mChunkInManagerCount] = prevNode;
            ++mChunkInManagerCount;

            ::ReleaseSRWLockExclusive(&mLock);
        }

    public:
        SRWLOCK mLock;
        Node* mChunks[MAX_CHUNK_COUNT]{};
        uint32_t mChunkInManagerCount = 0;
        uint32_t mChunkTotalCount = 0;
    };

private:
    Node* mTop = nullptr;
    Node* mHalfTop = nullptr; // OBJECT_COUNT_PER_CHUNK + 1 번째 노드를 가리킨다
    uint32_t mSize = 0;

    inline static bool mbNeedPlacementNew;  // Alloc()/Free()호출 시, 생성자/소멸자를 호출 할 것인지에 대한 옵션
    inline static ObjectPoolManager mPoolManager;
};