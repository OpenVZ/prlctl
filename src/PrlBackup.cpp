/*
 * Copyright (c) 2015 Parallels IP Holdings GmbH
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
 * Our contact details: Parallels IP Holdings GmbH, Vordergasse 59, 8200
 * Schaffhausen, Switzerland.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <PrlApiDisp.h>

#include "PrlBackup.h"
#include "CmdParam.h"
#include "PrlSrv.h"
#include "Utils.h"
#include "Logger.h"
#include "PrlCleanup.h"
#include "PrlXml.h"

static int backup_event_handler(PRL_HANDLE hEvent, void *data)
{
	PrlHandle h(hEvent);
	PRL_HANDLE_TYPE type;
	int ret;
	char *vm_uuid = (char *) data;

	if ((ret = PrlHandle_GetType(h.get_handle(), &type))) {
		prl_log(L_DEBUG, "PrlHandle_GetType: %s",
				get_error_str(ret).c_str());
		return ret;
	}

	if (type == PHT_EVENT) {
		PRL_EVENT_TYPE evt_type;

		if ((ret = PrlEvent_GetType(h.get_handle(), &evt_type))) {
			prl_log(L_DEBUG, "PrlEvent_GetType: %s",
					get_error_str(ret).c_str());
			return ret;
		}

		if (vm_uuid != NULL) {
			char uuid[256] = "";
			unsigned int len;

			// get VM uuid
			len = sizeof(uuid);
			ret = PrlEvent_GetIssuerId(h.get_handle(), uuid, &len);
			if (PRL_FAILED(ret)) {
				prl_log(L_DEBUG, "PrlEvent_GetIssuerId %s",
						get_error_str(ret).c_str());
				return ret;
			}

			if (strcmp(uuid, vm_uuid))
				return 0;
		}

		prl_log(L_DEBUG, "EVENT type=%d", evt_type);
		if (	(evt_type == PET_DSP_EVT_BACKUP_PROGRESS_CHANGED) ||
			(evt_type == PET_DSP_EVT_RESTORE_PROGRESS_CHANGED))
		{
			PRL_RESULT ret;

			PrlHandle hPrm;
			ret = PrlEvent_GetParamByName(hEvent, "progress_changed", hPrm.get_ptr());
			if (PRL_FAILED(ret))
				return 0;
			PRL_UINT32 progress = 0;
			PrlEvtPrm_ToUint32(hPrm.get_handle(), &progress);

			PRL_UINT32 idx = 0;
			ret = PrlEvent_GetParamByName(hEvent, "device_index", hPrm.get_ptr());
			if (PRL_SUCCEEDED(ret))
				PrlEvtPrm_ToUint32(hPrm.get_handle(), &idx);
			else
				prl_log(L_DEBUG, "PrlEvent_GetParamByName %s",
						get_error_str(ret).c_str());

			fprintf(stdout, "\r%s hdd.%d progress: %2d%%",
					(evt_type == PET_DSP_EVT_BACKUP_PROGRESS_CHANGED) ? "backup" : "restore",
					idx, progress);
			if (progress == 100)
				fprintf(stdout, "\n");
			fflush(stdout);
		} else if (evt_type == PET_DSP_EVT_BACKUP_STARTED) {
			prl_log(L_INFO, "Backup started.");
		} else if (evt_type == PET_DSP_EVT_CREATE_BACKUP_FINISHED) {
			prl_log(L_INFO, "Backup finished.");
		} else if (evt_type == PET_DSP_EVT_BACKUP_CANCELLED) {
			prl_log(L_INFO, "Backup cancelled!");
		} else if (evt_type == PET_DSP_EVT_VM_MESSAGE) {
			std::string err;
			get_result_error_string(hEvent, err);
			fprintf(stdout, "%s\n", err.c_str());
		}
	}
	return 0;
}

void PrlSrv::handle_job_err(PRL_HANDLE hJob, PRL_RESULT ret)
{
	if (ret != PRL_ERR_TIMEOUT)
		return;

	prl_log(0, "Operation timeout. Cancelling job.");
	PRL_HANDLE hCancel = PrlJob_Cancel(hJob);
	if (hCancel == PRL_INVALID_HANDLE) {
		prl_log(0, "Failed to cancel job.") ;
		return;
	}
	std::string err;
	if ((ret = get_job_retcode(hCancel, err, 60 * 1000)))
		prl_err(ret, "Failed to cancel job: %s.", err.c_str());
	else
		prl_log(0, "Job cancelled.");
	PrlHandle_Free(hCancel);
}

int PrlSrv::backup_vm(const CmdParamData &param)
{
	PRL_RESULT ret;
	const BackupParam& bparam = param.backup;
	const char *sessionid = "";
	const char *server = "";
	int port = 0;
	unsigned int security_level;
	PrlSrv storage;
	char vm_uuid[256];

	security_level = get_min_security_level();
	if (!bparam.storage.server.empty()) {
		if ((ret = storage.login(bparam.storage)))
			return ret;
		sessionid = storage.get_sessionid();
		port = bparam.storage.port;
		server = bparam.storage.server.c_str();

		if (security_level < storage.get_min_security_level())
			security_level = storage.get_min_security_level();
	}

	PrlVm *vm = NULL;
	ret = get_vm_config(param, &vm);
	if (ret != 0)
		return ret;
	if (vm == NULL)
		return prl_err(-1, "The %s virtual machine does not exist.",
				param.id.c_str());

	if (security_level < param.security_level)
		security_level = param.security_level;

	prl_log(L_DEBUG, "security_level=%d", security_level);
	prl_log(0, "Backing up the %s %s",
			vm->get_vm_type_str(), vm->get_name().c_str());
	PRL_HANDLE hBackup;
	hBackup = PrlSrv_CreateVmBackup(
		m_hSrv,
		vm->get_id().c_str(),
		server,
		port,
		sessionid,
		param.desc.c_str(),
		bparam.flags | security_level,
		0,
		PRL_TRUE);

	strcpy(vm_uuid, vm->get_id().c_str());
	reg_event_callback(backup_event_handler, vm_uuid);
	const PrlHook *h = get_cleanup_ctx().register_hook(cancel_job, hBackup);
	std::string err;
	if ((ret = get_job_retcode(hBackup, err))) {
		handle_job_err(hBackup, ret);
		prl_err(ret, "Failed to backed up the %s: %s",
				 vm->get_vm_type_str(), err.c_str());
	} else {
		PrlHandle hResult;
		PRL_UINT32 count = 0;
		int rc;
		std::string backup_id;

		if ((rc = PrlJob_GetResult(hBackup, hResult.get_ptr())) == 0 &&
		    (rc = PrlResult_GetParamsCount(hResult.get_handle(), &count)) == 0)
		{
			prl_log(L_DEBUG, "resultCount: %d", count);
			/*
			 * 1 - vm_id
			 * 2 - backup_id
			 */
			if (count >= 2) {
				PrlHandle phParam;

				rc = PrlResult_GetParamByIndex(hResult.get_handle(), 1, phParam.get_ptr());
				if (rc == 0) {
					char buf[128];
					PRL_UINT32 len = sizeof(buf);
					rc = PrlBackupResult_GetBackupUuid(phParam.get_handle(), buf, &len);
					if (rc == 0)
						backup_id = buf;
				}
			}
		}
		prl_log(0, "The %s has been successfully backed up with backup id %s.",
				 vm->get_vm_type_str(), backup_id.c_str());
	}
	get_cleanup_ctx().unregister_hook(h);

	PrlHandle_Free(hBackup);

	return ret;
}

int PrlSrv::restore_vm(const CmdParamData &param)
{
	PRL_RESULT ret = PRL_ERR_UNINITIALIZED;
	const BackupParam& bparam = param.backup;
	const char *sessionid = "";
	const char *server = "";
	int port = 0;
	unsigned int security_level, flags;
	PrlSrv storage;
	std::string vm_id = param.id;
	const char *type = "VM";

	flags = bparam.flags;
	security_level = get_min_security_level();
	if (!bparam.storage.server.empty()) {
		if ((ret = storage.login(bparam.storage)))
			return ret;
		sessionid = storage.get_sessionid();
		port = bparam.storage.port;
		server = bparam.storage.server.c_str();
		if (security_level < storage.get_min_security_level())
			security_level = storage.get_min_security_level();
	}

	if (security_level < param.security_level)
		security_level = param.security_level;
	prl_log(L_DEBUG, "security_level=%d", security_level);

	if (!param.id.empty()) {
		PrlVm *vm = NULL;
		ret = get_vm_config(param, &vm, is_uuid(param.id));
		if (ret != 0)
			return ret;
		if (vm != NULL) {
			vm_id = vm->get_uuid();
			type = vm->get_vm_type_str();
		}
		prl_log(0, "Restore the %s %s", type, vm_id.c_str());
	} else if (!bparam.id.empty()) {
		PrlBackupTree tree;
		ret = get_backup_tree(param, tree, storage);
		if (ret == 0)
			vm_id = tree.get_vm_uuid_by_id(bparam.id);

		prl_log(0, "Restore the backup id=%s uuid=%s",
				vm_id.c_str(), bparam.id.c_str());
	} else
		return prl_err(1, "VM ID is not specified.");

	if (!bparam.name.empty())
		flags |= PBT_RESTORE_TO_COPY;

	PRL_HANDLE hRestore;
	hRestore = PrlSrv_RestoreVmBackup(
		m_hSrv,
		vm_id.c_str(),
		bparam.id.c_str(),
		server,
		port,
		sessionid,
		bparam.vm_location.c_str(),
		bparam.name.c_str(),
		flags | security_level,
		0,
		false);

	reg_event_callback(backup_event_handler, (void *) vm_id.c_str());
	const PrlHook *h = get_cleanup_ctx().register_hook(cancel_job, hRestore);
	std::string err;
	if ((ret = get_job_retcode(hRestore, err))) {
		handle_job_err(hRestore, ret);
		prl_err(ret, "Failed to restore the %s: %s",
				type,  err.c_str());
	} else {
		prl_log(0, "The %s has been restored.",	type);
	}
	unreg_event_callback(backup_event_handler);
	get_cleanup_ctx().unregister_hook(h);

	return ret;
}

int PrlSrv::backup_delete(const CmdParamData &param)
{
	PRL_RESULT ret = PRL_ERR_UNINITIALIZED;
	const BackupParam& bparam = param.backup;
	const char *sessionid = "";
	const char *server = "";
	int port = 0;
	unsigned int security_level;
	PrlSrv storage;
	std::string vm_id = param.id;
	const char *type = "";

	security_level = get_min_security_level();
	if (!bparam.storage.server.empty()) {
		if ((ret = storage.login(bparam.storage)))
			return ret;
		sessionid = storage.get_sessionid();
		port = bparam.storage.port;
		server = bparam.storage.server.c_str();
		if (security_level < storage.get_min_security_level())
			security_level = storage.get_min_security_level();
	}

	if (security_level < param.security_level)
		security_level = param.security_level;
	prl_log(L_DEBUG, "security_level=%d", security_level);

	if (!param.id.empty()) {
		PrlVm *vm = NULL;
		ret = get_vm_config(param, &vm, is_uuid(param.id));
		if (ret)
			return ret;
		if (vm != NULL) {
			vm_id = vm->get_uuid();
			type = vm->get_vm_type_str();
		}
		prl_log(0, "Delete the %s backup", type);
	} else if (!bparam.id.empty()) {
		type = bparam.id.c_str();
		prl_log(0, "Delete the backup %s", bparam.id.c_str());
	} else
		return prl_err(1, "VM ID is not specified.");

	PRL_HANDLE hRemove;
	hRemove = PrlSrv_RemoveVmBackup(
		m_hSrv,
		vm_id.c_str(),
		bparam.id.c_str(),
		server,
		port,
		sessionid,
		bparam.flags | security_level,
		0,
		PRL_TRUE);

	reg_event_callback(backup_event_handler, (void *) vm_id.c_str());
	const PrlHook *h = get_cleanup_ctx().register_hook(cancel_job, hRemove);
	std::string err;
	if ((ret = get_job_retcode(hRemove, err))) {
		handle_job_err(hRemove, ret);
		prl_err(ret, "Failed to delete the backup: %s", err.c_str());
	} else {
		prl_log(0, "The %s backup have been successfully removed.",
				type);
	}
	unreg_event_callback(backup_event_handler);
	get_cleanup_ctx().unregister_hook(h);

	return ret;
}

int PrlSrv::backup_list(const CmdParamData &param)
{
	int ret;
	const BackupParam& bparam = param.backup;
	PrlBackupTree tree;
	PrlSrv storage;

	if (!bparam.storage.server.empty()) {
		if ((ret = storage.login(bparam.storage)))
			return ret;
	}

	ret = get_backup_tree(param, tree, storage);
	if (ret)
		return ret;
	if (param.backup.list_local_vm)
		update_vm_list(param.vmtype);

	tree.print_list(param.backup, param.list_no_hdr, *this);

	return 0;
}

int PrlSrv::do_get_backup_tree(const std::string& id,
	const std::string& server,
	int port,
	const std::string& session_id,
	unsigned int flags,
	PrlBackupTree &tree)
{
	PrlHandle hJob(PrlSrv_GetBackupTree(
			m_hSrv,
			id.c_str(),
			server.c_str(),
			port,
			session_id.c_str(),
			flags,
			0,
			true));

	const PrlHook *h = get_cleanup_ctx().register_hook(cancel_job, hJob.get_handle());

	PRL_RESULT ret;
	PRL_UINT32 resultCount;
        PrlHandle hResult;
        if ((ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &resultCount)))
                return prl_err(-1, "Failed to get the backup tree: %s",
                        get_error_str(ret).c_str());
	get_cleanup_ctx().unregister_hook(h);
	std::string out;
	if ((ret = get_result_as_string(hResult.get_handle(), out)))
		return ret;
	prl_log(L_DEBUG, "\n%s\n", out.c_str());
	tree.parse(out.c_str());
	return ret;
}

int PrlSrv::get_backup_tree(const CmdParamData &param, PrlBackupTree &tree, PrlSrv &storage)
{
	PRL_RESULT ret;
	const BackupParam& bparam = param.backup;
	std::string sessionid;
	std::string server;
	int port = 0;
	unsigned int flags = bparam.flags;
	unsigned int security_level;

	security_level = get_min_security_level();
	if (!bparam.storage.server.empty()) {
		sessionid = storage.get_sessionid();
		port = bparam.storage.port;
		server = bparam.storage.server;
		if (security_level < storage.get_min_security_level())
			security_level = storage.get_min_security_level();
	}

	if (security_level < param.security_level)
		security_level = param.security_level;

	prl_log(L_DEBUG, "security_level=%d", security_level);

	std::string id = param.id;
	if (!id.empty()) {
		PrlVm *vm = NULL;
		ret = get_vm_config(param, &vm, is_uuid(id));
		if (ret != 0)
			return ret;
		if (vm != NULL)
			id = vm->get_uuid();
		prl_log(L_DEBUG, "filter: %s", id.c_str());
	}

	flags |= security_level;
	if (param.vmtype & PVTF_VM)
		flags |= PBT_VM;
	if (param.vmtype & PVTF_CT)
		flags |= PBT_CT;

	return do_get_backup_tree(id, server, port, sessionid, flags, tree);
}

int PrlSrv::get_backup_disks(const std::string& id, std::list<std::string>& disks)
{
	PrlBackupTree tree;
	/* XXX: using the localhost, since we cannot handle backups located on a remote server yet */
	int ret = do_get_backup_tree(id, "", 0, get_sessionid(), PBT_VM | PBT_CT | PBT_BACKUP_ID, tree);
	if (ret)
		return ret;
	ret = tree.get_disks_by_id(id, disks);
	if (ret)
		return prl_err(-1, "Backup '%s' was not found on localhost.\n"
			"Please note that only local backups can be attached.", id.c_str());
	return 0;
}

void PrlBackupTree::parse_disk(const char *str, BackupDisk &disk)
{
	std::string val;
	PrlXml xml(str);

	if (xml.get_child_value("Name", val))
		disk.name = val;
	if (xml.get_child_value("OriginalPath", val))
		disk.orig_path = val;
	if (xml.get_child_value("Size", val))
		disk.size = val;
}

void PrlBackupTree::parse_disk_list(const char *str, BackupDiskList &list)
{
	PrlXml xml(str);
	e_xml_tag_flag tag;
	do {
		std::string tag_str;
		tag = xml.find_tag("BackupDisk", tag_str);
		if (tag == TAG_START) {
			BackupDisk disk;
			parse_disk(tag_str.c_str(), disk);
			list.push_back(disk);
		}
	} while (tag != TAG_NONE);
}

void PrlBackupTree::parse_data(const char *str, BackupData &backup)
{
	std::string val;
	PrlXml xml(str);

	if (xml.get_child_value("Id", val))
		backup.uuid = val;
	if (xml.get_child_value("Host", val))
		backup.host = val;
	if (xml.get_child_value("Creator", val))
		backup.owner = val;
	if (xml.get_child_value("DateTime", val))
		backup.date = val;
	if (xml.get_child_value("Size", val))
		backup.size = val;
	if (xml.get_child_value("Type", val))
		backup.type = val;
	if (xml.get_child_value("Description", val))
		backup.desc = val;
	if (xml.get_child_value("ServerUuid", val))
		backup.srv_uuid = val;

	std::string list_str;
	e_xml_tag_flag tag = xml.find_tag("BackupDisks", list_str);
	if (tag == TAG_START)
		parse_disk_list(list_str.c_str(), backup.lst_disks);
}

VmBackupData *PrlBackupTree::parse_entry(const char *str)
{
	std::string val;
	e_xml_tag_flag tag;

	PrlXml xml(str);

	VmBackupData *vm_backup = new VmBackupData();
	if (xml.get_child_value("Uuid", val))
		vm_backup->uuid = val;
	if (xml.get_child_value("Name", val))
		vm_backup->name = val;

	do {
		std::string tag_str;
		tag = xml.find_tag("BackupItem", tag_str);
		if (tag == TAG_START) {
			FullBackup full_backup;

			parse_data(tag_str.c_str(), full_backup.data);

			e_xml_tag_flag partial_tag;

			PrlXml xml(tag_str.c_str());
			do {
				std::string tag_str;
				partial_tag = xml.find_tag("PartialBackupItem", tag_str);
				if (partial_tag == TAG_START) {
					BackupData partial_backup;

					parse_data(tag_str.c_str(), partial_backup);
					full_backup.lst_backups.push_back(partial_backup);
				}
			} while (partial_tag != TAG_NONE);

			vm_backup->lst_backups.push_back(full_backup);
		}
	} while (tag != TAG_NONE);

	return vm_backup;
}

int PrlBackupTree::parse(const char *str)
{
	std::string data;
	e_xml_tag_flag tag;

	PrlXml xml(str);
	do {
		tag = xml.find_tag("VmItem", data);
		if (tag == TAG_START) {
			VmBackupData *vm_backup;

			vm_backup = parse_entry(data.c_str());
			if (vm_backup != NULL)
				m_tree.push_back(vm_backup);
		}
	} while (tag != TAG_NONE);

	return 0;
}

#define LIST_STD_FMT		"%38s %-38s %-20s %-20s %4s %10s\n"

void PrlBackupTree::print_list(const BackupParam &param, bool no_hdr, PrlSrv &srv)
{
	if (!param.list_full && !no_hdr)
		printf(LIST_STD_FMT, "ID", "Backup_ID", "Node", "Date", "Type", "Size");

	for (VmBackupDataList::const_iterator it = m_tree.begin(), eit = m_tree.end(); it != eit; ++it)
	{
		(*it)->print_data(param, srv);
	}
}

const BackupData *PrlBackupTree::find_backup_data(const std::string &id,
		std::string &vmid) const
{
	for (VmBackupDataList::const_iterator it = m_tree.begin(), eit = m_tree.end(); it != eit; ++it)
	{
		vmid = (*it)->uuid;
		/* For each full backup */
		for (FullBackupList::const_iterator it_full = (*it)->lst_backups.begin(),
				eit_full = (*it)->lst_backups.end();
				it_full != eit_full; ++it_full)
		{
			if (it_full->data.uuid == id)
				return &it_full->data;

			/* for each Inc backup */
			for (BackupList::const_iterator it_inc = it_full->lst_backups.begin(),
					eit_inc = it_full->lst_backups.end();
					it_inc != eit_inc; ++it_inc)
			{
				if (it_inc->uuid == id)
					return &(*it_inc);
			}
		}
	}
	return NULL;
}

std::string PrlBackupTree::get_vm_uuid_by_id(const std::string &id) const
{
	std::string vmid;

	if (find_backup_data(id, vmid) != NULL)
		return vmid;

	return std::string();
}

int PrlBackupTree::get_disks_by_id(const std::string& id, std::list<std::string>& disks) const
{
	std::string vmid;
	const BackupData *backup = find_backup_data(id, vmid);
	if (!backup)
		return -1;
	for (BackupDiskList::const_iterator it = backup->lst_disks.begin();
		it != backup->lst_disks.end(); ++it)
	{
		disks.push_back(it->name);
	}
	return 0;
}

PrlBackupTree::~PrlBackupTree()
{
	for (VmBackupDataList::const_iterator it = m_tree.begin(),
		eit = m_tree.end();
		it != eit; ++it)
	{
		delete (*it);
	}
}

static void print_backup_data(const std::string &uuid, const std::string &name, const BackupData &data, bool full)
{
	std::string time_str;

	time_str = convert_time(data.date.c_str());
	if (time_str.empty())
		time_str = data.date;
	if (full) {
		printf("VM_UUID: %s\n", uuid.c_str());
		printf("Name: %s\n", name.c_str());
		printf("Host: %s\n", data.host.c_str());
		printf("Owner: %s\n", data.owner.c_str());
		printf("Backup_ID: %s\n", data.uuid.c_str());
		printf("Date: %s\n", time_str.c_str());
		printf("Type: %s\n", data.type.c_str());
		printf("Size: %s\n", data.size.c_str());
		printf("Description: %s\n", data.desc.c_str());
		printf("ServerUuid: %s\n", data.srv_uuid.c_str());
		printf("DiskList:\n");
		for (BackupDiskList::const_iterator i = data.lst_disks.begin();
			i != data.lst_disks.end(); ++i) {
			printf("\tName: %s\n", i->name.c_str());
			printf("\tOriginalPath: %s\n", i->orig_path.c_str());
			printf("\tSize: %s\n", i->size.c_str());
		}
		printf("\n");
	} else {
		printf(LIST_STD_FMT,
				uuid.c_str(),
				data.uuid.c_str(),
				data.host.c_str(),
				time_str.c_str(),
				data.type.c_str(),
				data.size.c_str());
	}
}

void VmBackupData::print_data(const BackupParam &param, PrlSrv &srv) const
{
	/* For each full backup */
	for (FullBackupList::const_iterator it_full = lst_backups.begin(),
			eit_full = lst_backups.end();
			it_full != eit_full; ++it_full)
	{
		if (param.list_local_vm) {
			PrlVm *vm = NULL;

			srv.get_vm_config(uuid, &vm, true);
			// Vm exists or were backed up from the server
			if (vm == NULL &&
			    it_full->data.srv_uuid != srv.get_uuid() )
				continue;
		}

		print_backup_data(uuid, name, it_full->data, param.list_full);
		/* for each Inc backup */
		for (BackupList::const_iterator it_inc = it_full->lst_backups.begin(),
				eit_inc = it_full->lst_backups.end();
				it_inc != eit_inc; ++it_inc)
		{
			print_backup_data(uuid, name, *it_inc, param.list_full);
		}
	}
}
