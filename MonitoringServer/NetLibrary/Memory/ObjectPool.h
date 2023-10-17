#pragma once

// 사용할 풀의 옵션
#define USING_OBJECT_POOL_OPTION POOL_OPTION_TLS_POOl

///////////////////////////////////////////////////////////////////////////////
// [사용법]
// OBJECT_POOL<Object> pool;
// Object* obj = pool.Alloc();
// pool.Free(obj);
///////////////////////////////////////////////////////////////////////////////

// TLS 오브젝트풀
#define POOL_OPTION_TLS_POOl 1

// 안전 모드 TLS 오브젝트 풀
#define POOL_OPTION_DEBUG_TLS_POOL 2

// 락 프리 오브젝트풀
#define POOL_OPTION_LOCK_FREE_POOl 3

// 안전 모드 락 프리 오브젝트풀
#define POOL_OPTION_DEBUG_LOCK_FREE_POOL 4

// 런타임 오버플로우 감지기
#define POOL_OPTION_OVERFLOW_CHECKER 5

// 기본 new, delete를 사용
#define POOL_OPTION_NEW_DELETE 6

#pragma region 풀 옵션에 따른 전처리기 설정

#if USING_OBJECT_POOL_OPTION == POOL_OPTION_TLS_POOl || USING_OBJECT_POOL_OPTION == POOL_OPTION_DEBUG_TLS_POOL

#define OBJECT_POOL thread_local TlsObjectPool

#elif USING_OBJECT_POOL_OPTION == POOL_OPTION_LOCK_FREE_POOl || USING_OBJECT_POOL_OPTION == POOL_OPTION_DEBUG_LOCK_FREE_POOL

#define OBJECT_POOL LockFreeObjectPool

#elif USING_OBJECT_POOL_OPTION == POOL_OPTION_OVERFLOW_CHECKER

#define OBJECT_POOL OverflowChecker

#elif USING_OBJECT_POOL_OPTION == POOL_OPTION_NEW_DELETE

template <typename T>
class NewDeleteAllocator
{
public:
    static T* Alloc(void)
    {
        return new T;
    }

    static void Free(T* address)
    {
        delete address;
    }
};

#define OBJECT_POOL NewDeleteAllocator

#else
static_assert(false, "invalid pool option selected");
#endif

#pragma endregion