#include "Core.h"

#include <map>
#include <sstream>

#include "Logger.h"

namespace eg::Command
{
	static std::map<std::string, std::vector<Fn>>		gCommandFns;
	static std::map<std::string, Var>					gCommandVars;
	static Var gEmptyVar = {"UNKNOWN", "ERROR", std::numeric_limits<double>::max()};

	std::vector<std::string> splitString(const std::string& str, char delimiter);

	void registerFn(const std::string& name, Fn&& fn)
	{
		if(gCommandFns.find(name) == gCommandFns.end())
			gCommandFns[name] = std::vector<Fn>();

		try
		{
			gCommandFns.at(name).push_back(std::move(fn));
		}
		catch (const std::exception&)
		{
			//TODO: Print error
		}
	}
	Var& registerVar(const std::string& name, const std::string& value)
	{
		auto& var = gCommandVars[name];
		var.name = name;
		var.string = value;
		try
		{
			var.value = std::stod(value);
		}
		catch (const std::exception&)
		{
			var.value = 0.0; // Default value if conversion fails
		}
		return var;
	}

	Var& findVar(const std::string& name)
	{
		auto it = gCommandVars.find(name);
		if (it != gCommandVars.end())
		{
			return it->second;
		}
		return gEmptyVar;
	}


	void execute(const std::string& commandLine)
	{
		std::vector<std::string> tokens = splitString(commandLine, ' ');
		if (tokens.empty()) return;

		std::string cmd = tokens[0];

		// command?
		auto it = gCommandFns.find(cmd);
		if (it != gCommandFns.end()) {
			// build argv
			std::vector<char*> argv;
			for (size_t i = 0; i < tokens.size(); i++)
				argv.push_back((char*)tokens[i].c_str());
			for (size_t i = 0; i < it->second.size(); i++)
			{
				auto fn = it->second[i];
				fn(argv.size(), argv.data());
			}
			return;
		}

		// cvar?
		Var& cv = findVar(cmd);
		if (&cv != &gEmptyVar) {
			setVar(cv, tokens[1]);
			return;
		}

		//Print unknown command
	}

	void setVar(Var& var, std::string string)
	{
		var.string = string;
		try
		{
			var.value = std::stod(string);

		} catch (const std::exception&)
		{
			var.value = gEmptyVar.value; // Default value if conversion fails
		}
	}

	std::vector<std::string> splitString(const std::string& str, char delimiter)
	{
		std::istringstream ss(str);
		std::vector<std::string> out;
		std::string tok;
		while (ss >> tok) out.push_back(tok);
		return out;
	}

}