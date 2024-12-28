#include <Core.h>


#include <Windows.h>

#include <sstream>

namespace eg
{
	std::unique_ptr<Logger> Logger::sLogger = nullptr;

	void Logger::create(std::unique_ptr<Logger> logger)
	{
		if (sLogger == nullptr)
		{
			sLogger = std::move(logger);
		}
		else
		{
			throw std::runtime_error("Logger is not null !");
		}
	}

	std::string Logger::formatMessage(const std::string& message, const char* callee, const char* level)
	{
		std::stringstream formattedMessage;
		formattedMessage << "[" << level << "] ";
		formattedMessage << message;
		formattedMessage << "\n";
		return formattedMessage.str();
	}

	void Logger::gTrace(const std::string& message)
	{
		sLogger->trace(message);
	}
	void Logger::gInfo(const std::string& message)
	{
		sLogger->info(message);
	}

	void Logger::gWarn(const std::string& message)
	{
		sLogger->warn(message);
	}

	void Logger::gError(const std::string& message)
	{
		sLogger->error(message);
	}


	void VisualStudioLogger::trace(const std::string& message)
	{
		OutputDebugStringA(Logger::formatMessage(message, "Engine", "Trace").c_str());
	}

	void VisualStudioLogger::info(const std::string& message)
	{
		OutputDebugStringA(Logger::formatMessage(message, "Engine", "Trace").c_str());
	}

	void VisualStudioLogger::warn(const std::string& message)
	{
		OutputDebugStringA(Logger::formatMessage(message, "Engine", "Trace").c_str());
	}

	void VisualStudioLogger::error(const std::string& message)
	{
		OutputDebugStringA(Logger::formatMessage(message, "Engine", "Trace").c_str());
	}
}
