///////////////////////////////////////////////////////////////////////////////
///
/// @file PrlSnapshot.cpp
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


#include "PrlXml.h"
#include "PrlSnapshot.h"
#include "Logger.h"

#include <string.h>
#include <stdio.h>

std::string SnapshotData::get_info() const
{
	std::string out;

	out += "ID: "; out += id; out += "\n";
	out += "Name: "; out += name; out += "\n";
	out += "Date: "; out += date; out += "\n";
	if (current) {
		out += "Current: yes"; out += "\n";
	}
	out += "State: "; out += state; out += "\n";
	out += "Description: "; out += desc; out += "\n";

	return out;
}

SnapshotData PrlSnapshotTree::parse_entry(const char *str)
{
	SnapshotData data;
	std::string val, tag;

	PrlXml xml(str);
	val = xml.get_attr("guid");
	if (!val.empty())
		data.id = val;
	val = xml.get_attr("state");
	if (!val.empty())
		data.state = val;
	val = xml.get_attr("current");
	if (val == "yes")
		data.current = true;

	if (xml.get_child_value("Name", val))
		data.name = val;
	if (xml.get_child_value("DateTime", val))
		data.date = val;
	if (xml.get_child_value("Description", val))
		data.desc = val;

	prl_log(L_DEBUG, "%s\n", data.get_info().c_str());

	return data;
}

int PrlSnapshotTree::parse(const char *str)
{
	std::string data;
	SnapshotNode *node = 0;
	e_xml_tag_flag tag;

	PrlXml xml(str);
	do {
		tag = xml.find_tag("SavedStateItem", data);
		if (tag == TAG_START) {
			SnapshotData snapshot_data = parse_entry(data.c_str());
			if (!node) {
				/* root node */
				node = new SnapshotNode(0, snapshot_data);
				m_root_tree = node;
			} else
				node = node->add_elem(snapshot_data);
		} else if (tag == TAG_END) {
			if (node)
				node = node->parent();
		}
	} while (tag != TAG_NONE);

	return 0;
}

void PrlSnapshotTree::add_child_to_pool(SnapshotNodePtrList &pool,
		SnapshotNode *node)
{
	for (int i = node->m_siblings.size() - 1; i >= 0; --i)
		pool.push_back(node->m_siblings[i]);

#if 0
	SnapshotNodePtrList::iterator it, eit;

	printf("Pool: ");
	for (it = pool.begin(); it != pool.end(); ++it)
		printf("<%s> ", (*it)->m_data.id.c_str());
	printf("\n");
#endif
}


void PrlSnapshotTree::print_tree()
{
	SnapshotNodePtrList pool;
	std::string line, entry;

	if (!m_root_tree)
		return;
	add_child_to_pool(pool, m_root_tree);
	while (pool.size()) {
		SnapshotNode *node = pool.back();
		pool.pop_back();

		prl_log(L_DEBUG, "processing: %s\n", node->m_data.id.c_str());
		if (node->empty()) {
			/* Walk backward from end to the ROOT and build the tree */
			for (SnapshotNode *it = node; it; it = it->m_parent) {
				 /* do not print root entry */
				if (it == m_root_tree)
					break;
				SnapshotNode *parent = it->parent();
				prl_log(L_DEBUG, "parent: %s\n", it->m_data.id.c_str());
				entry.clear();
				const std::string &id = it->data().id;
				if (it->data().flags) {
					/* entry already shown, just add space */
					if (parent->get_next_child(it))
						entry += "|";
					else if (parent->data().flags)
						entry += " ";
					for (unsigned i = 0; i < id.length(); ++i)
						entry += " ";
					line.insert(0, entry);
				} else {
					/* Draw prefix based on prev entry */
					if (parent->data().flags)
						entry = "\\";
					if (it->data().current)
						entry += "*";
					else
						entry += "_";
					entry += id;

					line.insert(0, entry);
					it->m_data.flags = 1;
				}
			}
			printf("%s\n", line.c_str());
			line.clear();
		} else {
			add_child_to_pool(pool, node);
		}
	}
}

void PrlSnapshotTree::print_list(bool no_hdr)
{
	SnapshotNodePtrList pool;

	if (!no_hdr)
		printf("PARENT_SNAPSHOT_ID                      SNAPSHOT_ID\n");
	if (m_root_tree) {
		add_child_to_pool(pool, m_root_tree);
		while (pool.size()) {
			SnapshotNode *node = pool.back();
			pool.pop_back();

			printf("%38s %c%38s\n",
					node->parent()->data().id.c_str(),
					node->data().current ? '*' : ' ',
					node->data().id.c_str());

			add_child_to_pool(pool, node);
		}
	}
}

void PrlSnapshotTree::print_info(const std::string &id)
{
	SnapshotNodePtrList pool;

	add_child_to_pool(pool, m_root_tree);
	while (pool.size()) {
		SnapshotNode *node = pool.back();
		pool.pop_back();
		if (node->data().id == id) {
			printf("%s\n", node->data().get_info().c_str());
			break;
		}
		add_child_to_pool(pool, node);
	}
}

void PrlSnapshotTree::get_current_id(const char *str, std::string &sid)
{
	std::string data;
	e_xml_tag_flag tag;

	PrlXml xml(str);
	do {
		tag = xml.find_tag("SavedStateItem", data);
		if (tag == TAG_START) {
			SnapshotData snapshot_data = parse_entry(data.c_str());
			if (snapshot_data.current) {
				sid = snapshot_data.id;
				break;
			}
		}
	} while (tag != TAG_NONE);
}

#if 0
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main()
{
	PrlSnapshotTree snap_tree;
	char buf[4096 * 10];

	int fd = open("/tmp/Snapshots.xml", O_RDWR);
	read(fd, buf, sizeof(buf));

	snap_tree.str2tree(buf);
	snap_tree.print();

}
#endif



