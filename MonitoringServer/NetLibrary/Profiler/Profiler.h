#pragma once

#include <Windows.h>

#define MAX_THREAD_COUNT 200            // 최대 스레드 개수
#define MAX_PROFILING_DATA_COUNT 50     // 최대 프로파일링할 항목 개수
#define MAX_TAG_LENGTH 32               // 최대 태그 길이

/*************************** HOT TO USE ***************************/
// ProfileBegin(L"test");
// ...
// ProfileEnd(L"test");
// ProfileDataOutText(L"profile_result.txt");
/******************************************************************/

//#define PROFILE_ON

#ifdef PROFILE_ON

void ProfileBegin(const WCHAR* tag); // call this function at the starting point
void ProfileEnd(const WCHAR* tag);   // call this function at the end point
void ProfileDataOutText(const WCHAR* fileName); // profiling data save
#define PROFILE_BEGIN(Tag) ProfileBegin(Tag)
#define PROFILE_END(Tag) ProfileEnd(Tag)
#define PROFILE_SAVE(Tag) ProfileDataOutText(Tag);

#else

#define PROFILE_BEGIN(Tag)
#define PROFILE_END(Tag) 
#define PROFILE_SAVE(Tag)

#endif