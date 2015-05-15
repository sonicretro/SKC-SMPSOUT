/**
 * SADX Mod Loader
 * INI file parser.
 */

#define _CRT_SECURE_NO_WARNINGS
#if ! defined(_MSC_VER) || _MSC_VER >= 1600
#include "IniFile.hpp"
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#if ! defined(_MSC_VER) || _MSC_VER >= 1600
using std::transform;
using std::string;
using std::wstring;
using std::unordered_map;

/** IniGroup **/

/**
 * Check if the INI group has the specified key.
 * @param key Key.
 * @return True if the key exists; false if not.
 */
bool IniGroup::hasKey(const string &key) const
{
	return (m_data.find(key) != m_data.end());
}

const unordered_map<string, string> *IniGroup::data(void) const
{
	return &m_data;
}

/**
 * Get a string value from the INI group.
 * @param key Key.
 * @param def Default value.
 * @return String value.
 */
string IniGroup::getString(const string &key, const string &def) const
{
	auto iter = m_data.find(key);
	return (iter != m_data.end() ? iter->second : def);
}

/**
 * Get a boolean value from the INI group.
 * @param key Key.
 * @param def Default value.
 * @return Boolean value.
 */
bool IniGroup::getBool(const string &key, bool def) const
{
	auto iter = m_data.find(key);
	if (iter == m_data.end())
		return def;

	string value = iter->second;
	transform(value.begin(), value.end(), value.begin(), ::tolower);
	return (value == "true" || value == "1");
}

/**
 * Get an integer value from the INI group.
 * @param key Key.
 * @param def Default value.
 * @return Integer value.
 */
int IniGroup::getInt(const string &key, int def) const
{
	auto iter = m_data.find(key);
	if (iter == m_data.end())
		return def;

	string value = iter->second;
	return (int)strtol(value.c_str(), nullptr, 10);
}

/**
 * Get a hexadecimal integer value from the INI group.
 * @param key Key.
 * @param def Default value.
 * @return Integer value.
 */
int IniGroup::getHexInt(const string &key, int def) const
{
	auto iter = m_data.find(key);
	if (iter == m_data.end())
		return def;

	string value = iter->second;
	return (int)strtol(value.c_str(), nullptr, 16);
}

/** IniFile **/

IniFile::IniFile(const string &filename)
{
	FILE *f = fopen(filename.c_str(), "r");
	if (!f)
		return;

	load(f);
	fclose(f);
}

IniFile::IniFile(const wstring &filename)
{
	FILE *f = _wfopen(filename.c_str(), L"r");
	if (!f)
		return;

	load(f);
	fclose(f);
}

IniFile::IniFile(FILE *f)
{
	load(f);
}

IniFile::~IniFile()
{
	clear();
}

/**
 * Get an INI group.
 * @param section Section.
 * @return INI group, or nullptr if not found.
 */
const IniGroup *IniFile::getGroup(const string &section) const
{
	auto iter = m_groups.find(section);
	return (iter != m_groups.end() ? iter->second : nullptr);
}

/**
 * Check if the INI file has the specified group.
 * @param section Section.
 * @return True if the section exists; false if not.
 */
bool IniFile::hasGroup(const string &section) const
{
	return (m_groups.find(section) != m_groups.end());
}

/**
 * Check if the INI file has the specified key.
 * @param section Section.
 * @param key Key.
 * @return True if the key exists; false if not.
 */
bool IniFile::hasKey(const string &section, const string &key) const
{
	auto iter = m_groups.find(section);
	if (iter == m_groups.end())
		return false;

	return iter->second->hasKey(key);
}

/**
 * Get a string value from the INI file.
 * @param section Section.
 * @param key Key.
 * @param def Default value.
 * @return String value.
 */
string IniFile::getString(const string &section, const string &key, const string &def) const
{
	const IniGroup *group = getGroup(section);
	if (!group)
		return def;
	return group->getString(key, def);
}

/**
 * Get a boolean value from the INI file.
 * @param section Section.
 * @param key Key.
 * @param def Default value.
 * @return Boolean value.
 */
bool IniFile::getBool(const string &section, const string &key, bool def) const
{
	const IniGroup *group = getGroup(section);
	if (!group)
		return def;
	return group->getBool(key, def);
}

/**
 * Get an integer value from the INI file.
 * @param section Section.
 * @param key Key.
 * @param def Default value.
 * @return Integer value.
 */
int IniFile::getInt(const string &section, const string &key, int def) const
{
	const IniGroup *group = getGroup(section);
	if (!group)
		return def;
	return group->getInt(key, def);
}

/**
 * Get a hexadecimal integer value from the INI file.
 * @param section Section.
 * @param key Key.
 * @param def Default value.
 * @return Integer value.
 */
int IniFile::getHexInt(const string &section, const string &key, int def) const
{
	const IniGroup *group = getGroup(section);
	if (!group)
		return def;
	return group->getHexInt(key, def);
}

std::unordered_map<std::string, IniGroup*>::const_iterator IniFile::begin() const
{
	return m_groups.begin();
}

std::unordered_map<std::string, IniGroup*>::const_iterator IniFile::end() const
{
	return m_groups.end();
}

/**
 * Load an INI file.
 * Internal function; called from the constructor.
 * @param f FILE pointer. (File is not closed after processing.)
 */
void IniFile::load(FILE *f)
{
	clear();
	fseek(f, 0, SEEK_SET);

	// Create an empty group for default settings.
	IniGroup *curGroup = new IniGroup();
	m_groups[""] = curGroup;

	// Process the INI file.
	while (!feof(f))
	{
		char line[1024];
		char *ret = fgets(line, sizeof(line), f);
		if (!ret)
			break;
		const int line_len = (int)strnlen(line, sizeof(line));
		if (line_len == 0)
			continue;

		bool startswithbracket = false;
		int firstequals = -1;
		int endbracket = -1;

		// String can contain escape characters, so
		// we need a string buffer.
		string sb;
		sb.reserve(line_len);

		// Process the line.
		for (int c = 0; c < line_len; c++)
		{
			switch (line[c])
			{
				case '\\': // escape character
					if (c + 1 >= line_len)
					{
						// Backslash at the end of the line.
						goto appendchar;
					}
					c++;
					switch (line[c])
					{
						case 'n': // line feed
							sb += '\n';
							break;
						case 'r': // carriage return
							sb += '\r';
							break;
						default: // literal character
							goto appendchar;
					}
					break;

				case '=':
					if (firstequals == -1)
						firstequals = (int)sb.length();
					goto appendchar;

				case '[':
					if (c == 0)
						startswithbracket = true;
					goto appendchar;

				case ']':
					endbracket = (int)sb.length();
					goto appendchar;

				case ';':	// comment character
				case '\r':	// trailing newline (CRLF)
				case '\n':	// trailing newline (LF)
					// Stop processing this line.
					c = line_len;
					break;

				default:
appendchar:
					// Normal character. Append to the string buffer.
					sb += line[c];
					break;
			}
		}

		// Check the string buffer.
		if (startswithbracket && endbracket != -1)
		{
			// New section.
			string section = sb.substr(1, endbracket - 1);
			auto iter = m_groups.find(section);
			if (iter != m_groups.end())
			{
				// Section already exists.
				// Use the existing section.
				curGroup = iter->second;
			}
			else
			{
				// New section.
				curGroup = new IniGroup();
				m_groups[section] = curGroup;
			}
		}
		else if (!sb.empty())
		{
			// Key/value.
			string key;
			string value;
			if (firstequals > -1)
			{
				key = sb.substr(0, firstequals);
				size_t endpos = key.find_last_not_of(" \t");
				if (string::npos != endpos)
					key = key.substr(0, endpos + 1);	// trim trailing spaces, see Stack Overflow, Question 216823
				value = sb.substr(firstequals + 1);
				size_t startpos  = value.find_first_not_of(" \t");
				if (string::npos != startpos)
					value = value.substr(startpos);	// trim leading spaces, see Stack Overflow, Question 216823
			}
			else
			{
				key = line;
			}

			// Store the value in the current group.
			curGroup->m_data[key] = value;
		}
	}
}

/**
 * Clear the loaded INI file.
 */
void IniFile::clear(void)
{
	for (auto iter = m_groups.begin(); iter != m_groups.end(); ++iter)
	{
		delete iter->second;
	}

	m_groups.clear();
}
#endif