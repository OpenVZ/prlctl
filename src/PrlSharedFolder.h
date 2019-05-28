/*
 * Copyright (c) 2015-2017, Parallels International GmbH
 * Copyright (c) 2017-2019 Virtuozzo International GmbH. All rights reserved.
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
 * Our contact details: Virtuozzo International GmbH, Vordergasse 59, 8200
 * Schaffhausen, Switzerland.
 */

#ifndef __PRLSHAREDFOLDER_H__
#define __PRLSHAREDFOLDER_H__

#include "PrlTypes.h"
#include "CmdParam.h"

class PrlOutFormatter;

class PrlSharedFolder {
private:
	std::string m_name;
	PRL_HANDLE m_hShf;
	bool m_updated;
public:
	PrlSharedFolder(PRL_HANDLE h) : m_hShf(h), m_updated(false) {}
	~PrlSharedFolder();
	const std::string &get_name();
	const std::string &get_id() { return get_name(); }
	void set_updated() { m_updated = true; }
	bool is_updated() const { return m_updated; }
	int set_enable(bool flag);
	int set_readonly(bool flag);
	int configure(const SharedFolderParam &param);
	int create(const SharedFolderParam &param);
	int remove();
	std::string get_info() const;
	void append_info(PrlOutFormatter &f) const;
};

#endif // __PRLSHAREDFOLDER_H__
