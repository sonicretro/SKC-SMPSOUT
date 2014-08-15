/**
 * SADX Mod Loader
 * INI file parser.
 */

#ifndef INIFILE_H
#define INIFILE_H

#include <cstdio>
#include <string>
#include <unordered_map>

class IniFile;

/**
 * Individual INI group.
 */
class IniGroup
{
	public:
		bool hasKey(const std::string &key) const;

		const std::unordered_map<std::string, std::string> *data(void) const;

		std::string getString(const std::string &key, const std::string &def = "") const;
		bool getBool(const std::string &key, bool def = false) const;
		int getInt(const std::string &key, int def = 0) const;

	protected:
		friend class IniFile;

		/**
		 * INI section data.
		 * - Key: Key name. (UTF-8)
		 * - Value: Value. (UTF-8)
		 */
		std::unordered_map<std::string, std::string> m_data;
};

/**
 * INI file.
 * Contains multiple INI groups.
 */
class IniFile
{
	public:
		IniFile(const std::string &filename);
		IniFile(const std::wstring &filename);
		IniFile(FILE *f);
		~IniFile();

		const IniGroup *getGroup(const std::string &section) const;

		bool hasGroup(const std::string &section) const;
		bool hasKey(const std::string &section, const std::string &key) const;

		std::string getString(const std::string &section, const std::string &key, const std::string &def = "") const;
		bool getBool(const std::string &section, const std::string &key, bool def = false) const;
		int getInt(const std::string &section, const std::string &key, int def = 0) const;

	protected:
		void load(FILE *f);
		void clear(void);

		/**
		 * INI groups.
		 * - Key: Section name. (UTF-8)
		 * - Value: IniGroup.
		 */
		std::unordered_map<std::string, IniGroup*> m_groups;
};

#endif /* INIFILE_H */
