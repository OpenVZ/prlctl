/*
 * @file GetOpt.cpp
 *
 * Command line parser
 *
 * @author igor@
 *
 * Copyright (c) 2005-2017, Parallels International GmbH
 *
 * This file is part of OpenVZ. OpenVZ is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Our contact details: Parallels International GmbH, Vordergasse 59, 8200
 * Schaffhausen, Switzerland.
 */

#include <stdio.h>
#include <string.h>
#include "GetOpt.h"

static Option unknown_option = {NULL, '\0',OptNoArg , -1};


GetOptLong::GetOptLong(int argc, char *argv[], const Option *options,
		int offset) :
	m_options(options), m_argc(argc), m_argv(argv), m_cur_short_arg(NULL), m_parsed_arg(NULL)
{
	m_cur_id = offset;
}

const Option &GetOptLong::findOption(const std::string &opt,
		OptionType type)
{
	for (std::size_t i = 0; m_options[i].long_name; ++i) {
		switch (type) {
		case OptLong:
			if (m_options[i].long_name == opt)
				return m_options[i];
			break;
		case OptShort:
			if (opt[0] == m_options[i].short_name)
				return m_options[i];
			break;
		case OptUnknown:
		default:
			break;
		}
	}
	return unknown_option;
}

OptionType GetOptLong::getOptType(const std::string &opt) const
{
	if (opt[0] != '-')
		return OptUnknown;
	else if (opt[1] == '-')
		return OptLong;
	else
		return OptShort;
}

const Option &GetOptLong::findOption()
{
	OptionType type;
	std::string name;

	if (m_cur_short_arg && *(m_cur_short_arg + 1) != '\0') {
		/* process short syntax -abc */
		type = OptShort;
		name = *(++m_cur_short_arg);
	} else {
		char *arg = m_argv[m_cur_id];
		m_cur_short_arg = NULL;
		type = getOptType(arg);
		if (type == OptLong) {
			/* skip '--' and '='  --name=val */
			name = arg;
			std::size_t equal_pos = name.find('=');
			std::size_t opt_len;
			if (equal_pos != std::string::npos)
				opt_len = equal_pos - 2;
			else
				opt_len = name.length() - 2;
			name = name.substr(2, opt_len);
		} else if (type == OptShort) {
			// skip '-'
			name = ++arg;
			if (strlen(arg) != 1) {
				const Option &opt = findOption(name, OptShort);
				/* In case multiple option specified '-abc' split them to
				   '-a' '-bc' or '-a' 'bc'
				   depends of argument type
				*/
				if (&opt == &unknown_option)
					return unknown_option;
				m_cur_short_arg = arg;
				return opt;
			}
		} else {
			return unknown_option;
		}
	}
	return findOption(name, type);
}

int GetOptLong::parse(std::string &arg)
{

	if (m_cur_id >= m_argc)
		return -1;

	char *cur_arg = m_argv[m_cur_id];
	const Option &opt = findOption();

	if (&opt == &unknown_option)
		return GETOPTUNKNOWN;
	char *equal_p = strchr(cur_arg, '=');
	m_parsed_arg = NULL;
	switch (opt.type) {
	case OptRequireArg: {
		if (m_cur_short_arg != NULL) {
			if (m_cur_short_arg[1] == '\0') {
				m_cur_id++;
				if (m_cur_id >= m_argc) {
					fprintf(stderr, "The `%s' option requires an argument.\n",
							m_cur_short_arg);
					return GETOPTERROR;
				}
				/* case: -a b */
				arg = m_parsed_arg = m_argv[m_cur_id];
			} else
				/* case -ab */
				arg = m_parsed_arg = m_cur_short_arg + 1;
			m_cur_short_arg = NULL;
		} else {
			if (equal_p != NULL) {
				/* case: --a=b */
				arg = equal_p + 1;
			} else if (m_cur_id + 1 >= m_argc) {
				fprintf(stderr, "The `%s' option requires an argument.\n",
						cur_arg);
				return GETOPTERROR;
			} else {
				/* case: --a b */
				arg = m_parsed_arg = m_argv[++m_cur_id];
			}
		}
		m_cur_id++;
		break;
	}
	case OptNoArg:
	default:
		if (m_cur_short_arg == NULL || *(m_cur_short_arg + 1)== '\0') {
			m_cur_short_arg = NULL;
			m_cur_id++;
		}
		arg = "";
		break;
	}
	return opt.id;
}

void GetOptLong::hide_arg()
{
#ifdef _LIN_
	if (m_parsed_arg != NULL)
		memset(m_parsed_arg, 0, strlen(m_parsed_arg));
#endif
}
