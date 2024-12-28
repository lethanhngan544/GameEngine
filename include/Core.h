#pragma once


#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <MyVulkan.h>

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
		static void gTrace(const std::string& message);
		static void gInfo(const std::string& message);
		static void gWarn(const std::string& message);
		static void gError(const std::string& message);
	protected:
		static std::string formatMessage(const std::string& message, const char* callee, const char* level);
	private:
		static std::unique_ptr<Logger> sLogger;
	};

	

	class VisualStudioLogger  final : public Logger
	{
	public:
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

	
}


struct GLFWwindow;
namespace eg::Window
{
	void create(uint32_t width, uint32_t height, const char* title);
	void destroy();

	void poll();
	bool shouldClose();



	GLFWwindow* getHandle();

}

namespace eg::Renderer
{
	void create(uint32_t width, uint32_t height);
	void destory();


	vk::Instance getInstance();
}

