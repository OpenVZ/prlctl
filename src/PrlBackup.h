/*
 * @file PrlBackup.h
 *
 * Backup management
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

#ifndef __PRLBACKUP_H__
#define	__PRLBACKUP_H__
#include <boost/property_tree/ptree.hpp>
#include "PrlTypes.h"

struct BackupParam;
class PrlSrv;

struct BackupDisk
{
	std::string name;
	std::string orig_path;
	std::string size;
};
typedef std::list<BackupDisk> BackupDiskList;

struct BackupData
{
	std::string uuid;
	std::string date;
	std::string host;
	std::string owner;
	std::string size;
	std::string type;
	std::string desc;
	std::string srv_uuid;

	BackupDiskList lst_disks;
};
typedef std::list<BackupData> BackupList;

struct FullBackup
{
	struct BackupData data;
	BackupList lst_backups;
};

typedef std::list<FullBackup> FullBackupList;

struct VmBackupData {
	std::string uuid;
	std::string name;

	FullBackupList lst_backups;

	void print_data(const BackupParam &param, PrlSrv &srv) const;
};

class PrlBackupTree {
	typedef boost::property_tree::ptree tree_type;

public:
	PrlBackupTree() {}
	~PrlBackupTree();
	int parse(const char *str);
	void parse_data(const tree_type& item_, BackupData &backup);
	VmBackupData *parse_entry(const tree_type& entry_);
	void print_list(const BackupParam &param, bool no_hdr, PrlSrv &srv);
	const BackupData *find_backup_data(const std::string &id, std::string &vmid) const;
	std::string get_vm_uuid_by_id(const std::string &pid) const;
	int get_disks_by_id(const std::string& id, std::list<std::string>& disks) const;
	void parse_disk(const tree_type& disk_, BackupDisk &disk);
	void parse_disk_list(const tree_type& disks_, BackupDiskList &list);

private:
	typedef std::list<VmBackupData *> VmBackupDataList;
	VmBackupDataList m_tree;
};

#endif
