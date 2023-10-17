#pragma once

#include <iostream>

/************************ recommend call this macros instead of Logger::functions ************************/

// 로거의 로깅 수준을 변경한다
#define SET_LOG_LEVEL(Level)			Logger::SetLogLevel(Level)

// 로그를 남긴다
#define LOG(Level, Log)					Logger::LogMessageWithTime(Level, Log)

// 로그와 함께 소스 파일명과 줄 번호를 남긴다
#define LOG_WITH_LINE(Level, Log)		Logger::LogMessageWithTime((Level), (Log), __FILEW__, __LINE__)

// 포매팅 로그
#define LOGF(Level, FormatString, ...)	Logger::LogFormatWithTime(Level, FormatString, __VA_ARGS__)

// 모니터링 로그 (날짜 표시 없이 로깅만 함)
#define LOG_MONITOR(FormatString, ...)	Logger::LogMonitor(FormatString, __VA_ARGS__)

// 모니터링 로그에 현재 시각에 대한 로그를 남긴다
#define LOG_CURRENT_TIME()				Logger::LogCurrentTime()

// Condition이 false이면 로깅 후 크래시
#define ASSERT_LIVE(Condition, Message) Logger::Assert((Condition), (Message), __FILEW__, __LINE__)

// 로그의 레벨 (현재 로그의 레벨과 같거나 보다 낮은 로그만 작동함)
enum class ELogLevel
{
	Debug,		// Debug (Log All)
	System,
	Error,
	Test,		// temporary log for test
	Assert,		// assertion
};

/*********************************************************************************************************/

typedef wchar_t WCHAR;
typedef void* HANDLE;

class Logger
{
public:

	// 로거의 로깅 수준을 변경한다
	inline static void SetLogLevel(ELogLevel logLevel) { mLogLevel = logLevel; }


	static void LogMonitor(const WCHAR* formatMessage, ...);

	static void LogMessage(ELogLevel logLevel, const WCHAR* message);

	static void LogMessageWithTime(ELogLevel logLevel, const WCHAR* message);
	static void LogMessageWithTime(ELogLevel logLevel, const WCHAR* message, const WCHAR* fileName, int line);
	static void LogFormatWithTime(ELogLevel logLevel, const WCHAR* formatMessage, ...);

	static void LogFormat(ELogLevel logLevel, const WCHAR* formatMessage, ...);
	 
	static void LogCurrentTime(void);

	inline static void Assert(bool condition, const WCHAR* message)
	{
		if (!condition)
		{
			LogMessageWithTime(ELogLevel::Assert, message);
			RaiseCrash();
		}
	}

	inline static void Assert(bool condition, const WCHAR* message, const WCHAR* fileName, int line)
	{
		if (!condition)
		{
			LogMessageWithTime(ELogLevel::Assert, message, fileName, line);
			RaiseCrash();
		}
	}

	// intentional crash
	static void RaiseCrash();
private:
	Logger();
	~Logger();

	static void getCurrentTimeInfo(WCHAR* outBuffer);

	static Logger mInstance;
	static HANDLE mhLogFile;
	static HANDLE mhMonitorFile;
	static ELogLevel mLogLevel;
};