#include "Core.h"

#include <unordered_map>
#include <sstream>
#include <string.h>

#include "Logger.h"

namespace eg::Command
{
	static std::unordered_map<std::string, std::vector<Fn>>		gCommandFns;
	static std::unordered_map<std::string, std::unique_ptr<Var>>	gCommandVars;
	static Var gEmptyVar = {"UNKNOWN", "ERROR", 69.69};

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
	Var* registerVar(const std::string& name, const std::string& value, double value2)
	{
		Logger::gInfo("Registering CVar: " + name + " = " + value);
		auto& var = gCommandVars[name];

		var = std::make_unique<Var>();

		std::strncpy(var->name, name.c_str(), MAX_VAR_NAME_SIZE);
		std::strncpy(var->string, value.c_str(), MAX_VAR_STRING_SIZE);
		var->value = value2;
		

		return var.get();
	}

	Var* findVar(const std::string& name)
	{
		auto it = gCommandVars.find(name);
		if (it != gCommandVars.end())
		{
			return it->second.get();
		}
		else
		{
			return registerVar(name, "0", 0.0);
		}
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
		Var* cv = findVar(cmd);
		if (cv != &gEmptyVar) {
			std::strncpy(cv->string, tokens[1].c_str(), MAX_VAR_STRING_SIZE);
			return;
		}

		//Print unknown command
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