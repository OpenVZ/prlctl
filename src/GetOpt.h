///////////////////////////////////////////////////////////////////////////////
///
/// @file GetOpt.h
///
/// Command line parser
///
/// @author igor@
///
/// Copyright (c) 2005-2015 Parallels IP Holdings GmbH
///
/// This file is part of OpenVZ. OpenVZ is free software; you can redistribute
/// it and/or modify it under the terms of the GNU General Public License as
/// published by the Free Software Foundation; either version 2 of the License,
/// or (at your option) any later version.
/// 
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
/// GNU General Public License for more details.
/// 
/// You should have received a copy of the GNU General Public License
/// along with this program; if not, write to the Free Software
/// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
/// 02110-1301, USA.
///
/// Our contact details: Parallels IP Holdings GmbH, Vordergasse 59, 8200
/// Schaffhausen, Switzerland.
///
///////////////////////////////////////////////////////////////////////////////
#ifndef __GETOPT_H__
#define __GETOPT_H__
#include <list>
#include <string>

typedef std::list<std::string> stringList;
#define GETOPTERROR	'?'
#define GETOPTUNKNOWN	-2
#define OPTION_END	{NULL, '\0', OptNoArg , -1}

enum OptionArg {OptNoArg, OptRequireArg};
enum OptionType {OptLong, OptShort, OptUnknown};
struct Option {
	const char *long_name;
	char short_name;
	OptionArg type;
	int id;
};

class GetOptLong
{
private:
	const Option *m_options;
	int m_argc;
	char **m_argv;
	int m_cur_id;
	char *m_cur_short_arg;
	char *m_parsed_arg;

public:
	GetOptLong(int argc, char *argv[], const Option *options,
		int offset = 1);
	/*
	   If an option was successfully found, then returns the Option::id.
	   If all command-line options have been parsed, then returns -1.
	   If encounters an option character that was not in Option,
	   the '?'’is returned. If encounters an option with a missing argument.
	   then the return value depends on the Option::type:
	   if it is ’OptRequireArg, then '?' is returned; otherwise
	   Option::id’is  returned.
	 */
	int parse(std::string &arg);
	const char *get_next()
	{
		if (m_cur_id >= m_argc)
			return NULL;
		return m_argv[m_cur_id++];
	}
	char **get_args()
	{
		return &m_argv[m_cur_id];
	}
	void hide_arg();

private:
	OptionType getOptType(const std::string &opt) const;
	const Option &findOption(const std::string &opt, OptionType type);
	const Option &findOption();

};

#endif // __GETOPT_H__
