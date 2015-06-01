///////////////////////////////////////////////////////////////////////////////
///
/// @file PrlXml.h
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

#ifndef __PRLXML_H__
#define	__PRLXML_H__
#include "PrlTypes.h"

enum e_xml_tag_flag {
	TAG_START,
	TAG_END,
	TAG_NONE,
};

class PrlXml {
public:
	PrlXml(const char *data) : m_data(data), m_cur_pos(data) {}
	bool get_child_value(const char *name, std::string &val, bool next = false);
	e_xml_tag_flag find_tag(const char *name, std::string &data);
	std::string get_attr(const char *attr);

private:
	const char *m_data;
	const char *m_cur_pos;
};

#endif
