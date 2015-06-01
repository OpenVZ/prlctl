///////////////////////////////////////////////////////////////////////////////
///
/// @file PrlXml.cpp
///
/// XML parser helper
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


#include "PrlXml.h"
#include "Logger.h"

#include <string.h>


bool PrlXml::get_child_value(const char *name, std::string &val, bool next)
{
	const char *sp, *ep;
	std::string tag;
	bool cdata = false;

	tag = "<"; tag += name; tag += ">";
	if (!(sp = strstr(m_data, tag.c_str())))
		return false;
	sp += tag.length();
	if (!strncmp(sp, "<![CDATA[", 9)) {
		sp += 9;
		cdata = true;
	}
	tag = "</"; tag += name; tag += ">";
	if (!(ep = strstr(sp, tag.c_str())))
		return false;
	if (cdata && !strncmp(ep - 3, "]]>", 3))
		ep -= 3;

	val.assign(sp, ep - sp);

	if (next)
		m_data = sp;

	return true;
}

e_xml_tag_flag PrlXml::find_tag(const char *name, std::string &data)
{
	const char *sp;
	unsigned int len = strlen(name);

	while ((sp = strstr(m_cur_pos, name))) {
		m_cur_pos = sp + len;
		if (*(sp-1) == '<' &&
		    (m_cur_pos[0] == '>' || m_cur_pos[0] == ' ')) {
			/* the data block is between
			   <name> ... <name>
				   or
			   <name> ... </name>
			*/
			m_cur_pos++;
			const char *ep = m_cur_pos;
			while ((ep = strstr(ep, name))) {
				if (*(ep-1) == '<' ||
				    (*(ep-2) == '<' && *(ep -1) == '/'))
					break;
				ep++;

			}
			if (!ep)
				return TAG_NONE;

			data.assign(m_cur_pos, ep - m_cur_pos);
			m_cur_pos = ep;

			return TAG_START;

		} else if (*(sp-2) ==  '<' && *(sp-1) == '/')
			return TAG_END;
	}
	return TAG_NONE;
}

std::string PrlXml::get_attr(const char *attr)
{
	const char *sp, *ep;
	std::string tag;

	tag = attr; tag += "=";

	if (!(sp = strstr(m_data, tag.c_str())))
		return "";
	sp += tag.length();
	if (sp[0] == '\"')
		++sp;
	if (!(ep = strstr(sp, "\"")))
		return "";
	std::string data;
	data.assign(sp, ep - sp);
	return data;
}
