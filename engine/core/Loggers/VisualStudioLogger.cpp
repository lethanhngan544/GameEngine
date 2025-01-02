#include <Logger.h>


#include <Windows.h>
namespace eg
{
	void VisualStudioLogger::trace(const std::string& message)
	{
		OutputDebugStringA(Logger::formatMessage(message, "Engine", "Trace").c_str());
	}

	void VisualStudioLogger::info(const std::string& message)
	{
		OutputDebugStringA(Logger::formatMessage(message, "Engine", "Info").c_str());
	}

	void VisualStudioLogger::warn(const std::string& message)
	{
		OutputDebugStringA(Logger::formatMessage(message, "Engine", "Warn").c_str());
	}

	void VisualStudioLogger::error(const std::string& message)
	{
		OutputDebugStringA(Logger::formatMessage(message, "Engine", "Error").c_str());
	}
}