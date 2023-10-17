#pragma comment(lib, "pathcch.lib")

#include "Logger.h"

#include <time.h>
#include <Windows.h>
#include <strsafe.h>

#define LOG_MESSAGE_MAX_LENGTH 512
#define DAY_INFO_BUFFER_LENGTH 16

Logger Logger::mInstance;
HANDLE Logger::mhLogFile;
HANDLE Logger::mhMonitorFile;
ELogLevel Logger::mLogLevel = ELogLevel::Debug;

// 콘솔에 로그를 출력
#define LOG_TO_CONSOLE(Log) wprintf(Log)

// 로그 파일에 로그를 출력
#define LOG_TO_LOG_FILE(Log) do {																				\
	if (WriteFile(mhLogFile, (Log), (DWORD)(wcslen(Log) * sizeof(WCHAR)), nullptr, nullptr) == FALSE)			\
	{																											\
		wprintf(L"[!] File Logging ERROR - GetLastError() = %d\n", GetLastError());								\
	}																											\
																												\
	FlushFileBuffers(mhLogFile);																				\
} while (false)																									\

// 모니터링 파일에 로그를 출력
#define LOG_TO_MONITOR_FILE(Log) do {																			\
	if (WriteFile(mhMonitorFile, (Log), (DWORD)(wcslen(Log) * sizeof(WCHAR)), nullptr, nullptr) == FALSE)		\
	{																											\
		wprintf(L"[!] File Logging ERROR - GetLastError() = %d\n", GetLastError());								\
	}																											\
																												\
	FlushFileBuffers(mhMonitorFile);																			\
} while (false)																									\

void Logger::LogMonitor(const WCHAR * formatMessage, ...)
{
	WCHAR completedMessage[LOG_MESSAGE_MAX_LENGTH];

	va_list ap;
	va_start(ap, formatMessage);
	{
		StringCchVPrintfW(completedMessage, LOG_MESSAGE_MAX_LENGTH, formatMessage, ap);
	}
	va_end(ap);

	WCHAR log[LOG_MESSAGE_MAX_LENGTH];

	StringCchPrintfW(log, LOG_MESSAGE_MAX_LENGTH, L"%s\n", completedMessage);

	LOG_TO_CONSOLE(log);
	LOG_TO_MONITOR_FILE(log);
}

void Logger::LogMessage(ELogLevel logLevel, const WCHAR * message)
{
	if (mLogLevel > logLevel)
	{
		return;
	}

	WCHAR log[LOG_MESSAGE_MAX_LENGTH];

	StringCchPrintfW(log, LOG_MESSAGE_MAX_LENGTH, L"%s\n", message);

	LOG_TO_CONSOLE(log);
	LOG_TO_LOG_FILE(log);
}

void Logger::LogMessageWithTime(ELogLevel logLevel, const WCHAR* message)
{
	if (mLogLevel > logLevel)
	{
		return;
	}

	WCHAR log[LOG_MESSAGE_MAX_LENGTH];
	WCHAR dayInfo[DAY_INFO_BUFFER_LENGTH];
	getCurrentTimeInfo(dayInfo);

	StringCchPrintfW(log, LOG_MESSAGE_MAX_LENGTH, L"[%s] : %s\n", dayInfo, message);

	LOG_TO_CONSOLE(log);
	LOG_TO_LOG_FILE(log);
}

void Logger::LogMessageWithTime(ELogLevel logLevel, const WCHAR* message, const WCHAR* fileName, int line)
{
	if (mLogLevel > logLevel)
	{
		return;
	}

	WCHAR log[LOG_MESSAGE_MAX_LENGTH];
	WCHAR dayInfo[DAY_INFO_BUFFER_LENGTH];
	getCurrentTimeInfo(dayInfo);

	StringCchPrintfW(log, LOG_MESSAGE_MAX_LENGTH, L"[%s][%s line:%4d] : %s\n", dayInfo, fileName, line, message);

	LOG_TO_CONSOLE(log);
	LOG_TO_LOG_FILE(log);
}

void Logger::LogFormat(ELogLevel logLevel, const WCHAR* formatMessage, ...)
{
	if (mLogLevel > logLevel)
	{
		return;
	}

	WCHAR completedMessage[LOG_MESSAGE_MAX_LENGTH];

	va_list ap;
	va_start(ap, formatMessage);
	{
		StringCchVPrintfW(completedMessage, LOG_MESSAGE_MAX_LENGTH, formatMessage, ap);
	}
	va_end(ap);

	Logger::LogMessage(logLevel, completedMessage);
}

void Logger::LogCurrentTime(void)
{
	WCHAR log[LOG_MESSAGE_MAX_LENGTH];
	WCHAR dayInfo[DAY_INFO_BUFFER_LENGTH];
	getCurrentTimeInfo(dayInfo);

	StringCchPrintfW(log, LOG_MESSAGE_MAX_LENGTH, L"<%s>\n", dayInfo);

	LOG_TO_CONSOLE(log);
	LOG_TO_MONITOR_FILE(log);
}

void Logger::LogFormatWithTime(ELogLevel logLevel, const WCHAR* formatMessage, ...)
{
	if (mLogLevel > logLevel)
	{
		return;
	}

	WCHAR completedMessage[LOG_MESSAGE_MAX_LENGTH];

	va_list ap;
	va_start(ap, formatMessage);
	{
		StringCchVPrintfW(completedMessage, LOG_MESSAGE_MAX_LENGTH, formatMessage, ap);
	}
	va_end(ap);

	Logger::LogMessageWithTime(logLevel, completedMessage);
}

void Logger::getCurrentTimeInfo(WCHAR* outBuffer)
{
	time_t startTime = time(nullptr);
	tm localTime;
	localtime_s(&localTime, &startTime);

	int YYYY = localTime.tm_year + 1900;
	int MM = localTime.tm_mon + 1;
	int DD = localTime.tm_mday;
	int hh = localTime.tm_hour;
	int mm = localTime.tm_min;
	int ss = localTime.tm_sec;
	StringCchPrintfW(outBuffer, DAY_INFO_BUFFER_LENGTH, L"%04d%02d%02d_%02d%02d%02d", YYYY, MM, DD, hh, mm, ss);
}

#pragma warning(push)
#pragma warning(disable: 6011)
void Logger::RaiseCrash()
{
	int* nullPointer = 0x00000000;
	*nullPointer = 0;
}
#pragma warning(pop)

Logger::Logger()
{
	timeBeginPeriod(1);

	WCHAR fileName[_MAX_PATH * 2];

	// get logFileName
	WCHAR dayInfo[DAY_INFO_BUFFER_LENGTH];
	getCurrentTimeInfo(dayInfo);

	// get process path
	DWORD dwPID = GetCurrentProcessId();
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwPID);
	WCHAR processPath[MAX_PATH];
	if (GetModuleFileNameW(nullptr, processPath, MAX_PATH) == 0)
	{
		CloseHandle(hProcess);
		exit(1);
	}
	CloseHandle(hProcess);

	wsprintfW(fileName, L"%s Log", processPath);
	CreateDirectoryW(fileName, nullptr); // create directory

	wsprintfW(processPath, L"%s", fileName); // path update (log file directory)

	wsprintfW(fileName, L"%s\\Log_%s.txt", processPath, dayInfo);
	mhLogFile = ::CreateFile(fileName, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (mhLogFile == INVALID_HANDLE_VALUE)
	{
		printf("Log File CreateFile() Failed %d\n", GetLastError());
		RaiseCrash();
	}

	wsprintfW(fileName, L"%s\\Monitor_%s.txt", processPath, dayInfo);
	mhMonitorFile = ::CreateFile(fileName, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (mhMonitorFile == INVALID_HANDLE_VALUE)
	{
		printf("Monitoring Log File CreateFile() Failed %d\n", GetLastError());
		RaiseCrash();
	}

	// Write BOM
	constexpr unsigned short BOM_UTF_16_LE = 0xFEFF;
	WriteFile(mhLogFile, &BOM_UTF_16_LE, sizeof(BOM_UTF_16_LE), nullptr, nullptr);
	WriteFile(mhMonitorFile, &BOM_UTF_16_LE, sizeof(BOM_UTF_16_LE), nullptr, nullptr);
}

Logger::~Logger()
{
	CloseHandle(mhLogFile);
}