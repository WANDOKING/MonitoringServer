#pragma once

#include <cstdint>

#define NETWORK_HEADER_TYPE_LAN 0 // LAN 서버 : 길이만 존재
#define NETWORK_HEADER_TYPE_NET 1 // NET 서버 : 고정 코드 추가 및 패킷 암호화/복호화 수행

#define NETWORK_HEADER_USE_TYPE NETWORK_HEADER_TYPE_NET

#if NETWORK_HEADER_USE_TYPE == NETWORK_HEADER_TYPE_LAN

struct NetworkHeader
{
	uint16_t Length;  // 페이로드의 길이 (헤더 제외)
};

static_assert(sizeof(NetworkHeader) == 2);

#elif NETWORK_HEADER_USE_TYPE == NETWORK_HEADER_TYPE_NET

#define NETWORK_HEADER_CODE 0x77      // 쓰레기 패킷을 걸러내기 위한 고정 값
#define NETWORK_HEADER_FIXED_KEY 0x32 // 메시지 암호화에 대한 고정 키

#pragma pack (push,1)
struct NetworkHeader
{
    uint8_t     Code;       // NETWORK_HEADER_CODE 고정 값
    uint16_t    Length;     // 페이로드의 길이 (헤더 제외)
    uint8_t     RandKey;    // 메시지 암호화에 대한 랜덤 키
    uint8_t     CheckSum;   // 복호화 시 위변조 판단을 위한 체크섬
};
#pragma pack (pop)

static_assert(sizeof(NetworkHeader) == 5);

#endif