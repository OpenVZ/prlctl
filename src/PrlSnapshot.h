/*
 * @file PrlSnapshot.h
 *
 * Snapshot management
 *
 * @author igor@
 *
 * Copyright (c) 2005-2017, Parallels International GmbH
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

#ifndef __PRLSNAPSHOT_H__
#define	__PRLSNAPSHOT_H__
#include <boost/property_tree/ptree.hpp>
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
	typedef boost::property_tree::ptree tree_type;

public:
	PrlSnapshotTree() : m_root_tree(0) {}
	int parse(const char *str);
	void print_tree();
	void print_list(bool no_hdr);
	void print_info(const std::string &id);
	std::string get_current_snapshot_id();  // returns id of snapshot we are based now, or empty string if snapshot doesn't exist

private:
	SnapshotNode * parse_entry(const tree_type& entry_, SnapshotNode *parent_);
	void add_child_to_pool(SnapshotNodePtrList &pool,
			SnapshotNode *node);

private:
	SnapshotNode *m_root_tree;
};

#endif // __PRLSNAPSHOT_H__
