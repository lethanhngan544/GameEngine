#pragma once

#include <functional>
#include <string>

namespace eg
{
	//Commands
	namespace Command
	{
		static constexpr size_t MAX_VAR_NAME_SIZE = 64;
		static constexpr size_t MAX_VAR_STRING_SIZE = 128;

		using Fn = std::function<void(size_t argc, char* argv[])>;
		struct Var
		{
			char name[MAX_VAR_NAME_SIZE];
			char string[MAX_VAR_STRING_SIZE];
			double value;
		};



		void registerFn(const std::string& name, Fn&& fn);
		Var* registerVar(const std::string& name, const std::string& value, double value2);

		Var* findVar(const std::string& name);
		void setVar(Var& var, std::string string);

		void execute(const std::string& commandLine);
	};
}


