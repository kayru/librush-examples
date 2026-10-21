#pragma once

#include <Rush/Rush.h>

#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

namespace Rush
{

struct CommandLineChoice
{
	const char* name;
	int         value;
};

// Long options accept "--name=value" and "--name value", short ones "-n value".
// "--" ends options. Unknown options and malformed values are errors.
class CommandLine
{
public:
	CommandLine();
	~CommandLine();

	// shortName 0 means none
	void addSwitch(const char* name, char shortName, bool* out, const char* help);
	void addU32(const char* name, char shortName, u32* out, const char* valueName, const char* help);
	void addString(const char* name, char shortName, std::string* out, const char* valueName, const char* help);
	void addList(const char* name, char shortName, std::vector<std::string>* out, const char* valueName, const char* help);
	void addChoice(const char* name, char shortName, int* out, std::initializer_list<CommandLineChoice> choices, const char* help);

	// Optional, filled in declaration order
	void addPositional(const char* name, std::string* out, const char* help);

	// Using option without prerequisite is an error
	void require(const char* option, const char* prerequisite);

	// Returns the exit code when the app must not run (--help or an error, both print usage)
	std::optional<int> parse(int argc, char** argv);

	bool isSet(const char* name) const;

	void printUsage() const;

private:
	struct Option;

	struct Requirement
	{
		const char* option;
		const char* prerequisite;
	};

	Option* findLong(const char* name, size_t nameLength);
	Option* findShort(char shortName);
	Option& add(const char* name, char shortName, const char* valueName, const char* help);
	int     fail(const char* format, ...) const;

	std::vector<Option>      m_options;
	std::vector<Requirement> m_requirements;
	std::vector<Option> m_positionals;
	std::string         m_programName;
};

}
