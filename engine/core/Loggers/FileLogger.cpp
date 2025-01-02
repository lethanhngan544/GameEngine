#include <Logger.h>

namespace eg
{
	FileLogger::FileLogger(const std::string& fileName)
		: mFileName(fileName)
	{
		mFile.open(fileName);
		if (!mFile.is_open())
		{
			throw std::runtime_error("Failed to open file: " + fileName);
		}
	}

	FileLogger::~FileLogger()
	{
		mFile.close();
	}

	void FileLogger::trace(const std::string& message)
	{
		mFile << formatMessage(message, "trace", "TRACE");
		mFile.close();
		mFile.open(mFileName, std::ofstream::app);
	}
	void FileLogger::info(const std::string& message)
	{
		mFile << formatMessage(message, "info", "INFO");
		mFile.close();
		mFile.open(mFileName, std::ofstream::app);
	}
	void FileLogger::warn(const std::string& message)
	{
		mFile << formatMessage(message, "warn", "WARN");
		mFile.close();
		mFile.open(mFileName, std::ofstream::app);
	}
	void FileLogger::error(const std::string& message)
	{
		mFile << formatMessage(message, "error", "ERROR");
		mFile.close();
		mFile.open(mFileName, std::ofstream::app);
	}
}