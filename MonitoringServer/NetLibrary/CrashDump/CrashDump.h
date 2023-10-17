#pragma once

#include <Windows.h>
#include <DbgHelp.h>

class CrashDump
{
public:
	static void Crash();
	inline static void Assert(bool bAssertion) { if (!bAssertion) Crash(); }

private:
	CrashDump();

	// 예외 핸들러 - 크래시 덤프를 저장함
	static LONG WINAPI myExceptionFilter(PEXCEPTION_POINTERS pExceptionPointer);

	static void myInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved) { Crash(); }
	static int _custom_Report_hook(int ireposttype, char* message, int* returnValue) { Crash(); return TRUE; }
	static void myPurecallHandler() { Crash(); }

private:
	static DWORD s_mDumpCount;
	static CrashDump s_mInstance;
};