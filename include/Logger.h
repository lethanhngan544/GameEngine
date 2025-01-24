#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

#include <fstream>

namespace eg
{
	class Logger
	{
	public:
		virtual ~Logger() = default;
		virtual void trace(const std::string& message) = 0;
		virtual void info(const std::string& message) = 0;
		virtual void warn(const std::string& message) = 0;
		virtual void error(const std::string& message) = 0;
	public:
		static void create(std::unique_ptr<Logger> logger);
		static void destroy();
		static void gTrace(const std::string& message);
		static void gInfo(const std::string& message);
		static void gWarn(const std::string& message);
		static void gError(const std::string& message);
	protected:
		static std::string formatMessage(const std::string& message, const char* callee, const char* level);
	private:
		static std::unique_ptr<Logger> sLogger;
	};



	

	class FileLogger final : public Logger
	{
	private:
		std::string mFileName;
		std::ofstream mFile;
	public:
		FileLogger(const std::string& fileName);
		~FileLogger();
		virtual void trace(const std::string& message) final;
		virtual void info(const std::string& message) final;
		virtual void warn(const std::string& message) final;
		virtual void error(const std::string& message) final;
	};


	class ConsoleLogger final : public Logger
	{
	public:
		virtual void trace(const std::string& message) final;
		virtual void info(const std::string& message) final;
		virtual void warn(const std::string& message) final;
		virtual void error(const std::string& message) final;
	};

	class NullLogger final : public Logger
	{
	public:
		virtual void trace(const std::string& message) final {}
		virtual void info(const std::string& message) final {}
		virtual void warn(const std::string& message) final {}
		virtual void error(const std::string& message) final {}
	};
}