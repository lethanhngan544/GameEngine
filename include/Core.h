#pragma once

#include <functional>
#include <string>

namespace eg
{
	//Commands
	namespace Command
	{
		using Fn = std::function<void(size_t argc, char* argv[])>;
		struct Var
		{
			std::string name;
			std::string string;
			double value;
		};



		void registerFn(const std::string& name, Fn&& fn);
		Var& registerVar(const std::string& name, const std::string& value);

		Var& findVar(const std::string& name);
		void setVar(Var& var, std::string string);

		void execute(const std::string& commandLine);
	};
}


