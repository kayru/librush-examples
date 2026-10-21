#include "CommandLine.h"

#include <Rush/UtilLog.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace Rush
{

namespace
{
enum class OptionKind : u8
{
	Switch,
	U32,
	String,
	List,
	Choice,
};

bool parseU32(const char* text, u32& out)
{
	if (!*text)
	{
		return false;
	}

	u64 value = 0;
	for (const char* c = text; *c; ++c)
	{
		if (*c < '0' || *c > '9')
		{
			return false;
		}
		value = value * 10 + u64(*c - '0');
		if (value > 0xFFFFFFFFull)
		{
			return false;
		}
	}

	out = u32(value);
	return true;
}

#if defined(__APPLE__)
// Cocoa reads "-Key value" pairs into NSUserDefaults (Xcode passes e.g.
// -NSDocumentRevisionsDebugMode YES). They never collide with "-x" short options.
bool isCocoaDefault(const char* arg)
{
	return arg[0] == '-' && arg[1] >= 'A' && arg[1] <= 'Z' && arg[2] != 0;
}
#endif
}

struct CommandLine::Option
{
	OptionKind  kind = OptionKind::Switch;
	const char* name = nullptr;
	char        shortName = 0;
	const char* valueName = nullptr;
	const char* help = nullptr;
	void*       out = nullptr;
	bool        set = false;

	std::vector<CommandLineChoice> choices;
};

CommandLine::CommandLine() = default;
CommandLine::~CommandLine() = default;

CommandLine::Option& CommandLine::add(const char* name, char shortName, const char* valueName, const char* help)
{
	RUSH_ASSERT_MSG(!findLong(name, std::strlen(name)), "Duplicate command line option");
	RUSH_ASSERT_MSG(shortName == 0 || !findShort(shortName), "Duplicate short command line option");

	Option& option = m_options.emplace_back();
	option.name = name;
	option.shortName = shortName;
	option.valueName = valueName;
	option.help = help;
	return option;
}

void CommandLine::addSwitch(const char* name, char shortName, bool* out, const char* help)
{
	Option& option = add(name, shortName, nullptr, help);
	option.kind = OptionKind::Switch;
	option.out = out;
}

void CommandLine::addU32(const char* name, char shortName, u32* out, const char* valueName, const char* help)
{
	Option& option = add(name, shortName, valueName, help);
	option.kind = OptionKind::U32;
	option.out = out;
}

void CommandLine::addString(const char* name, char shortName, std::string* out, const char* valueName, const char* help)
{
	Option& option = add(name, shortName, valueName, help);
	option.kind = OptionKind::String;
	option.out = out;
}

void CommandLine::addList(
    const char* name, char shortName, std::vector<std::string>* out, const char* valueName, const char* help)
{
	Option& option = add(name, shortName, valueName, help);
	option.kind = OptionKind::List;
	option.out = out;
}

void CommandLine::addChoice(
    const char* name, char shortName, int* out, std::initializer_list<CommandLineChoice> choices, const char* help)
{
	Option& option = add(name, shortName, nullptr, help);
	option.kind = OptionKind::Choice;
	option.out = out;
	option.choices = choices;
}

void CommandLine::addPositional(const char* name, std::string* out, const char* help)
{
	Option& option = m_positionals.emplace_back();
	option.kind = OptionKind::String;
	option.name = name;
	option.help = help;
	option.out = out;
}

void CommandLine::require(const char* option, const char* prerequisite)
{
	RUSH_ASSERT_MSG(findLong(option, std::strlen(option)), "Unknown command line option");
	RUSH_ASSERT_MSG(findLong(prerequisite, std::strlen(prerequisite)), "Unknown command line option");
	m_requirements.push_back({option, prerequisite});
}

CommandLine::Option* CommandLine::findLong(const char* name, size_t nameLength)
{
	for (Option& option : m_options)
	{
		if (std::strlen(option.name) == nameLength && std::strncmp(option.name, name, nameLength) == 0)
		{
			return &option;
		}
	}
	return nullptr;
}

CommandLine::Option* CommandLine::findShort(char shortName)
{
	for (Option& option : m_options)
	{
		if (option.shortName == shortName)
		{
			return &option;
		}
	}
	return nullptr;
}

bool CommandLine::isSet(const char* name) const
{
	for (const Option& option : m_options)
	{
		if (std::strcmp(option.name, name) == 0)
		{
			return option.set;
		}
	}
	for (const Option& option : m_positionals)
	{
		if (std::strcmp(option.name, name) == 0)
		{
			return option.set;
		}
	}
	RUSH_ASSERT_MSG(false, "Unknown command line option");
	return false;
}

std::optional<int> CommandLine::parse(int argc, char** argv)
{
	if (argc > 0 && argv[0])
	{
		const char* slash = std::strrchr(argv[0], '/');
		const char* backslash = std::strrchr(argv[0], '\\');
		const char* base = slash > backslash ? slash : backslash;
		m_programName = base ? base + 1 : argv[0];
	}

	size_t positionalCount = 0;
	bool optionsEnded = false;

	for (int i = 1; i < argc; ++i)
	{
		const char* arg = argv[i];

		if (optionsEnded || arg[0] != '-' || arg[1] == 0)
		{
			if (positionalCount == m_positionals.size())
			{
				return fail("unexpected argument '%s'", arg);
			}
			Option& positional = m_positionals[positionalCount++];
			*static_cast<std::string*>(positional.out) = arg;
			positional.set = true;
			continue;
		}

		if (std::strcmp(arg, "--") == 0)
		{
			optionsEnded = true;
			continue;
		}

		if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0)
		{
			printUsage();
			return 0;
		}

#if defined(__APPLE__)
		if (isCocoaDefault(arg))
		{
			++i;
			continue;
		}
#endif

		Option*     option = nullptr;
		const char* inlineValue = nullptr;
		if (arg[1] == '-')
		{
			const char* name = arg + 2;
			const char* eq = std::strchr(name, '=');
			option = findLong(name, eq ? size_t(eq - name) : std::strlen(name));
			inlineValue = eq ? eq + 1 : nullptr;
		}
		else if (arg[2] == 0)
		{
			option = findShort(arg[1]);
		}

		if (!option)
		{
			return fail("unknown option '%s'", arg);
		}

		option->set = true;

		if (option->kind == OptionKind::Switch)
		{
			if (inlineValue)
			{
				return fail("option '--%s' takes no value", option->name);
			}
			*static_cast<bool*>(option->out) = true;
			continue;
		}

		const char* value = inlineValue;
		if (!value)
		{
			if (i + 1 >= argc)
			{
				return fail("option '--%s' requires a value", option->name);
			}
			value = argv[++i];
		}

		switch (option->kind)
		{
		case OptionKind::U32:
			if (!parseU32(value, *static_cast<u32*>(option->out)))
			{
				return fail("option '--%s' expects a non-negative integer, got '%s'", option->name, value);
			}
			break;
		case OptionKind::String:
			*static_cast<std::string*>(option->out) = value;
			break;
		case OptionKind::List:
			static_cast<std::vector<std::string>*>(option->out)->push_back(value);
			break;
		case OptionKind::Choice:
		{
			const CommandLineChoice* match = nullptr;
			for (const CommandLineChoice& choice : option->choices)
			{
				if (std::strcmp(choice.name, value) == 0)
				{
					match = &choice;
					break;
				}
			}
			if (!match)
			{
				return fail("option '--%s' got unknown value '%s'", option->name, value);
			}
			*static_cast<int*>(option->out) = match->value;
			break;
		}
		case OptionKind::Switch:
			break;
		}
	}

	for (const Requirement& requirement : m_requirements)
	{
		if (isSet(requirement.option) && !isSet(requirement.prerequisite))
		{
			return fail("option '--%s' requires '--%s'", requirement.option, requirement.prerequisite);
		}
	}

	return std::nullopt;
}

int CommandLine::fail(const char* format, ...) const
{
	char message[512];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	Log::error("%s: %s", m_programName.c_str(), message);
	printUsage();
	return 1;
}

void CommandLine::printUsage() const
{
	std::string synopsis = "Usage: " + m_programName + " [options]";
	for (const Option& positional : m_positionals)
	{
		synopsis += " [";
		synopsis += positional.name;
		synopsis += "]";
	}
	Log::message("%s", synopsis.c_str());

	if (!m_positionals.empty())
	{
		Log::message("Arguments:");
	}
	for (const Option& positional : m_positionals)
	{
		Log::message("  %-28s %s", positional.name, positional.help);
	}

	std::vector<const Option*> options;
	for (const Option& option : m_options)
	{
		options.push_back(&option);
	}

	Option help;
	help.name = "help";
	help.shortName = 'h';
	help.help = "Show this message";
	options.push_back(&help);

	Log::message("Options:");
	for (const Option* option : options)
	{
		std::string flags;
		if (option->shortName)
		{
			flags += '-';
			flags += option->shortName;
			flags += ", ";
		}
		flags += "--";
		flags += option->name;

		if (option->kind == OptionKind::Choice)
		{
			flags += ' ';
			for (size_t i = 0; i < option->choices.size(); ++i)
			{
				if (i != 0)
				{
					flags += '|';
				}
				flags += option->choices[i].name;
			}
		}
		else if (option->valueName)
		{
			flags += ' ';
			flags += option->valueName;
		}

		Log::message("  %-28s %s", flags.c_str(), option->help);
	}
}

}
