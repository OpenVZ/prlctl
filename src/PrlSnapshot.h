///////////////////////////////////////////////////////////////////////////////
///
/// @file PrlSnapshot.h
///
/// Snapshot management
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

#ifndef __PRLSNAPSHOT_H__
#define	__PRLSNAPSHOT_H__
#include "PrlTypes.h"

struct SnapshotData {
	std::string id;
	std::string date;
	std::string name;
	std::string desc;
	std::string state;
	bool current;
	int flags;

	SnapshotData() : current(false), flags(0) {}
	std::string get_info() const;
};

typedef TNode<SnapshotData> SnapshotNode;
typedef std::vector<SnapshotNode *> SnapshotNodePtrList;

class PrlSnapshotTree {
public:
	PrlSnapshotTree() : m_root_tree(0) {}
	int parse(const char *str);
	void print_tree();
	void print_list(bool no_hdr);
	void print_info(const std::string &id);
	void get_current_id(const char *str, std::string &sid);

private:
	SnapshotData parse_entry(const char *str);
	void add_child_to_pool(SnapshotNodePtrList &pool,
			SnapshotNode *node);

private:
	SnapshotNode *m_root_tree;
};

#endif // __PRLSNAPSHOT_H__
