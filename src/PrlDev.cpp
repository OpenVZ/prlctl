/*
 * @file PrlDev.cpp
 *
 * Devices managenment
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

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <PrlApiDisp.h>
#include <PrlApiDeprecated.h>
#include <PrlApiNet.h>

#include "PrlTypes.h"
#include "PrlDev.h"
#include "PrlVm.h"
#include "PrlSrv.h"
#include "Utils.h"
#include "Logger.h"
#include "CmdParam.h"
#include "PrlCleanup.h"
#include "PrlOutFormatter.h"

static const char* cmd2string(Command cmd)
{
	switch (cmd)
	{
	case Add:
		return "Creating";
	case Set:
		return "Configure";
	case Del:
		return "Delete";
	default:
		return "";
	}
}

const std::string &PrlDevSrv::get_name()
{
	PRL_RESULT ret;
	char buf[4096];
	unsigned len = sizeof(buf);

	if (!m_name.empty())
		return m_name;

	PRL_HANDLE_TYPE type = PHT_ERROR;

	PrlHandle_GetType(m_hDev, &type);
	switch (type) {
	case PHT_HW_HARD_DISK_PARTITION:
		if (!(ret = PrlSrvCfgHddPart_GetName(m_hDev, buf, &len))) {
			if (len > 1) {
				m_name = buf;
			} else {
				unsigned int type;
				if (!PrlSrvCfgHddPart_GetType(m_hDev, &type))
					m_name = partition_type2str(type);
				prl_log(L_DEBUG, "PrlSrvCfgHddPart_GetType: %x",
						type);
			}
		} else
			prl_err(-1, "PrlSrvCfgHddPart_GetName: %s",
				get_error_str(ret).c_str());
		break;
	case PHT_HW_HARD_DISK:
		if (!(ret = PrlSrvCfgHdd_GetDevName(m_hDev, buf, &len)))
			m_name = buf;
		else
			prl_err(-1, "PrlSrvCfgHdd_GetDevName: %s",
				get_error_str(ret).c_str());
		break;
	default:
		if (!(ret = PrlSrvCfgDev_GetName(m_hDev, buf, &len)))
			m_name = buf;
		else
			prl_err(-1, "PrlSrvCfgDev_GetName: %s",
				get_error_str(ret).c_str());
		break;
	}
	return m_name;
}

const std::string &PrlDevSrv::get_id()
{
	PRL_RESULT ret;
	char buf[4096];
	unsigned len = sizeof(buf);

	if (!m_id.empty())
		return m_id;

	PRL_HANDLE_TYPE type = PHT_ERROR;

	PrlHandle_GetType(m_hDev, &type);
	switch (type) {
	case PHT_HW_HARD_DISK_PARTITION:
		if (!(ret = PrlSrvCfgHddPart_GetSysName(m_hDev, buf, &len)))
			m_id = buf;
		else
			prl_err(-1, "PrlSrvCfgHddPart_GetSysName",
				get_error_str(ret).c_str());
		break;
	case PHT_HW_HARD_DISK:
		if (!(ret = PrlSrvCfgHdd_GetDevId(m_hDev, buf, &len)))
			m_id = buf;
		else
			prl_err(-1, "PrlSrvCfgHdd_GetDevName: %s",
				get_error_str(ret).c_str());
		break;
	default:
		if (!(ret = PrlSrvCfgDev_GetId(m_hDev, buf, &len)))
			m_id = buf;
		else
			prl_err(-1, "PrlSrvCfgDev_GetName: %s",
				get_error_str(ret).c_str());
		break;
	}
	return m_id;
}

unsigned int PrlDevSrv::get_idx() const
{
	PRL_RESULT ret;
	unsigned int idx = 0;

	switch (m_devType) {
	case DEV_HDD:
		if ((ret = PrlSrvCfgHdd_GetDiskIndex(m_hDev, &idx)))
			prl_err(-1, "PrlSrvCfgHdd_GetDiskIndex",
				get_error_str(ret).c_str());
		break;
	case DEV_NET:
		if ((ret = PrlSrvCfgNet_GetSysIndex(m_hDev, &idx)))
			prl_err(-1, "PrlSrvCfgNet_GetSysIndex: %s",
				get_error_str(ret).c_str());
		break;
	default:
		break;
	}
	return idx;
}

int PrlDevSrv::get_assignment_mode() const
{
	PRL_GENERIC_DEVICE_STATE state;

	if (m_devType == DEV_GENERIC_PCI) {
		if (PrlSrvCfgDev_GetDeviceState(m_hDev, &state) == 0)
			return (int) state;
	}
	return -1;
}

int PrlDevSrv::set_assignment_mode(int mode)
{
	PRL_RESULT ret;

	if (m_devType == DEV_GENERIC_PCI) {
		if ((ret = PrlSrvCfgDev_SetDeviceState(m_hDev, (PRL_GENERIC_DEVICE_STATE) mode)))
			 return prl_err(ret, "PrlSrvCfgDev_SetDeviceStat: %s",
				get_error_str(ret).c_str());
	}
	return 0;
}

int PrlDevSrv::is_connected() const
{
	PRL_BOOL connected;

	if (PrlSrvCfgDev_IsConnectedToVm(m_hDev, &connected) == 0)
		return (int) connected;

	return -1;
}

/*************************** PrlDev *********************************/

PrlDev::PrlDev(const PrlVm &vm, PRL_HANDLE hDev, DevType type, unsigned int idx) :
		m_vm(vm), m_hDev(hDev), m_devType(type), m_idx(idx),
		m_updated(false)
{
	set_idx(m_idx);
}

void PrlDev::set_idx(unsigned int idx) {
	m_idx = idx;
	m_id = ::devtype2str(m_devType);
	m_id += ui2string(m_idx);
}

int PrlDev::set_enable(bool flag)
{
	PRL_RESULT ret;

	prl_log(L_DEBUG, "PrlDev::set_enable %d", flag);
	if ((ret = PrlVmDev_SetEnabled(m_hDev, flag)))
		return prl_err(ret, "PrlVmDev_SetEnabled: %s",
				get_error_str(ret).c_str());
	set_updated();
	return 0;
}

bool PrlDev::is_enable() const
{
	PRL_RESULT ret;
	PRL_BOOL enable = PRL_FALSE;

	if ((ret = PrlVmDev_IsEnabled(m_hDev, &enable)))
		prl_err(ret, "PrlVmDev_SetEnabled: %s",
				get_error_str(ret).c_str());
	return prl_bool(enable);
}

int PrlDev::set_connect(bool flag)
{
	PRL_RESULT ret;

	prl_log(L_DEBUG, "PrlDev::set_connect: %d", flag);
	if ((ret = PrlVmDev_SetConnected(m_hDev, flag)))
		return prl_err(ret, "PrlVmDev_SetConnected: %s",
				get_error_str(ret).c_str());

	set_updated();
	return 0;
}

PRL_VM_DEV_EMULATION_TYPE PrlDev::get_emu_type() const
{
	PRL_RESULT ret;
	PRL_VM_DEV_EMULATION_TYPE type = PDT_ANY_TYPE;

	if ((ret = PrlVmDev_GetEmulatedType(m_hDev, &type)))
		prl_err(-1, "PrlVmDev_GetEmulatedType: %s",
			get_error_str(ret).c_str());

	return type;
}

int PrlDev::set_emu_type(PRL_VM_DEV_EMULATION_TYPE type)
{
	PRL_RESULT ret;

	if ((ret = PrlVmDev_SetEmulatedType(m_hDev, type)))
		return prl_err(ret, "PrlVmDev_SetEmulatedType: %s",
			get_error_str(ret).c_str());

	set_updated();
	return 0;
}

PRL_MASS_STORAGE_INTERFACE_TYPE PrlDev::get_iface_type() const
{
	PRL_RESULT ret;
	PRL_MASS_STORAGE_INTERFACE_TYPE type;

	if ((ret = PrlVmDev_GetIfaceType(m_hDev, &type)))
		prl_err(ret, "PrlVmDev_GetIfaceType: %s",
			get_error_str(ret).c_str());

	return type;
}

PRL_CLUSTERED_DEVICE_SUBTYPE PrlDev::get_subtype() const
{
	PRL_RESULT ret;
	PRL_CLUSTERED_DEVICE_SUBTYPE type;

	if ((ret = PrlVmDev_GetSubType(m_hDev, &type)))
		prl_err(ret, "PrlVmDev_GetSubType: %s",
			get_error_str(ret).c_str());

	return type;
}

int PrlDev::remove()
{
	PRL_RESULT ret;

	prl_log(0, "Remove the %s device.", get_id().c_str());
	if ((ret = PrlVmDev_Remove(m_hDev)))
		return prl_err(ret, "PrlVmDev_Remove: %s",
				get_error_str(ret).c_str());
	set_updated();
	return 0;
}

std::string PrlDev::get_fname() const
{
	PRL_RESULT ret;
	char buf[4096];
	unsigned int len = sizeof(buf);

	buf[0] = 0;
	if ((ret = PrlVmDev_GetFriendlyName(m_hDev, buf, &len)))
		prl_err(ret, "PrlVmDev_GetFriendlyName: %s",
			get_error_str(ret).c_str());

	return std::string(buf);
}

int PrlDev::set_sname(const std::string& name)
{
	PRL_RESULT ret;

	if ((ret = PrlVmDev_SetSysName(m_hDev, name.c_str())))
		return prl_err(ret, "PrlVmDev_SetSysName: %s",
			get_error_str(ret).c_str());

	return 0;
}

int PrlDev::set_fname2(const std::string& name)
{
	PRL_RESULT ret;

	if ((ret = PrlVmDev_SetFriendlyName(m_hDev, name.c_str())))
		return prl_err(ret, "PrlVmDev_SetFriendlyName: %s",
			get_error_str(ret).c_str());

	return 0;
}

int PrlDev::set_fname(const std::string& name)
{
	int ret;

	if ((ret = set_sname(name)))
		return ret;

	if ((ret = set_fname2(name)))
		return ret;

	return 0;
}

std::string PrlDev::get_sname() const
{
	PRL_RESULT ret;
	char buf[4096];
	unsigned int len = sizeof(buf);

	buf[0] = 0;
	if ((ret = PrlVmDev_GetSysName(m_hDev, buf, &len)))
		prl_err(ret, "PrlVmDev_GetSysName: %s",
			get_error_str(ret).c_str());

	return std::string(buf);
}

int PrlDev::set_position(unsigned int position)
{
	PRL_RESULT ret;

	if ((ret = PrlVmDev_SetStackIndex(m_hDev, position)))
		return prl_err(ret, "PrlVmDev_SetStackIndex: %s",
			get_error_str(ret).c_str());

	set_updated();
	return 0;
}

unsigned int PrlDev::get_position() const
{
	PRL_RESULT ret;
	PRL_UINT32 index = 0;

	if ((ret = PrlVmDev_GetStackIndex(m_hDev, &index)))
		return prl_err(ret, "PrlVmDev_GetStackIndex: %s",
			get_error_str(ret).c_str());
	return index;
}

int PrlDev::connect(const std::string &name)
{
	int ret;

	std::string _name(name);
	std::string err;

	prl_log(0, "Connect device: %s", get_name().c_str());
	PrlHandle hJob(PrlVmDev_Connect(m_hDev));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "Failed to connect device: %s",
			err.c_str());
	prl_log(0, "The device successfully connected");

	return 0;
}

int PrlDev::disconnect(const std::string &name)
{
	int ret;

	std::string _name(name);
	std::string err;

	prl_log(0, "Disconnect device: %s", get_name().c_str());
	PrlHandle hJob(PrlVmDev_Disconnect(m_hDev));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "Failed to disconnect device: %s",
			err.c_str());
	prl_log(0, "The device successfully disconnected");

	return 0;
}

int PrlDev::is_connected() const
{
	PRL_BOOL connected;

	if (PrlVmDev_IsConnected(m_hDev, &connected) == 0)
		return (int) connected;

	return -1;
}

std::string PrlDev::get_info()
{
	PrlOutFormatterPlain f;
	append_info(f);
	return f.get_buffer();
}

/*************************** PrlDevHdd **********************************/

int PrlDevHdd::set_iface(const std::string &iface)
{
	PRL_RESULT ret;
	PRL_MASS_STORAGE_INTERFACE_TYPE type;

	prl_log(L_INFO, "Set the %s interface.", iface.c_str());
	if (iface == "scsi")
		type = PMS_SCSI_DEVICE;
	else if (iface == "ide")
		type = PMS_IDE_DEVICE;
	else if (iface == "virtio")
		type = PMS_VIRTIO_BLOCK_DEVICE;
	else
		return prl_err(-1, "An invalid interface is specified: %s.,"
				" The following interfaces are supported: ide, scsi, virtio.",
				iface.c_str());
	if ((ret = PrlVmDev_SetIfaceType(m_hDev, type)))
		return prl_err(ret, "PrlVmDev_SetIfaceType: %s",
			get_error_str(ret).c_str());

	set_updated();
	return 0;
}

int PrlDevHdd::set_subtype(const std::string &subtype)
{
	PRL_RESULT ret;
	PRL_CLUSTERED_DEVICE_SUBTYPE type;

	prl_log(L_INFO, "Set the %s subtype.", subtype.c_str());
	if (subtype == "virtio-scsi")
		type = PCD_VIRTIO_SCSI;
	else if (subtype == "hyperv")
		type = PCD_HYPER_V_SCSI;
	else
			return prl_err(-1, "Invalid subtype: %s,"
					" supported one: virtio-scsi, hyperv",
					subtype.c_str());
	if ((ret = PrlVmDev_SetSubType(m_hDev, type)))
		return prl_err(ret, "PrlVmDev_SetSubType: %s",
			get_error_str(ret).c_str());

	set_updated();
	return 0;
}

int PrlDevHdd::create_image(const DevInfo &param)
{
	PRL_RESULT ret;
	PRL_HARD_DISK_INTERNAL_FORMAT type = PHD_EXPANDING_HARD_DISK;
	unsigned int size = param.size;

	if ((ret = PrlVmDevHd_SetDiskType(m_hDev, type)))
		return prl_err(ret, "PrlVmDevHd_SetDiskType: %s",
				get_error_str(ret).c_str());
	if (param.split)
		PrlVmDevHd_SetSplitted(m_hDev, true);
	if (size == 0) {
		if (PrlVmDevHd_GetDiskSize(m_hDev, &size) || size == 0)
			size = 64;
	}
	if (param.hdd_block_size) {
		if ((ret = PrlVmDevHd_SetBlockSize(m_hDev, param.hdd_block_size)))
			return prl_err(ret, "PrlVmDevHd_SetBlockSize: %s",
					get_error_str(ret).c_str());
	}

	if (!param.enc_keyid.empty()) {
		ret = set_encryption_keyid(param.enc_keyid);
		if (ret)
			return ret;
	}

	PrlVmDevHd_SetDiskSize(m_hDev, size);

	if (m_vm.get_vm_type() == PVT_VM) {
		prl_log(0, "Creating %s", get_info().c_str());
	} else {
		// See #PSBM-15338
		PrlOutFormatterPlain f;
		append_info_spec(f, 0);
		prl_log(0, "Creating %s", f.get_buffer().c_str());
	}


	std::string err;
	PrlHandle hJob(PrlVmDev_CreateImage(m_hDev, param.recreate, true));
	PrlCleanup::register_cancel(hJob.get_handle());
	if ((ret = get_job_retcode(hJob.get_handle(), err, ~0)))
		prl_err(ret, "PrlVmDev_CreateImage: %s", err.c_str());
	else
		set_updated();
	PrlCleanup::unregister_last();
	return ret;
}

int PrlDevHdd::resize_image(const DevInfo &param)
{
	PRL_RESULT ret;
	PRL_VM_DEV_EMULATION_TYPE type;
	unsigned int size = param.size;

	type = get_emu_type();
	if (type != PDT_USE_IMAGE_FILE && type != PDT_USE_FILE_SYSTEM)
		return prl_err(-1, "Only disk image supported for resize");

	std::string err;
	PRL_UINT32 flags = (param.no_fs_resize ? 0 : PRIF_RESIZE_LAST_PARTITION) |
				(param.offline ? PRIF_RESIZE_OFFLINE : 0);
	prl_log(0, "Resize disk image '%s' up to %d", get_fname().c_str(), size);
	PrlHandle hJob(PrlVmDev_ResizeImage(m_hDev, param.size, flags));
	PrlCleanup::register_cancel(hJob.get_handle());
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to resize: %s", err.c_str());
	PrlCleanup::unregister_last();
	return ret;
}

int PrlDevHdd::set_encryption_keyid(const std::string &keyid)
{
	PrlHandle hEncryption;

	int ret = PrlVmDevHd_GetEncryption(m_hDev, hEncryption.get_ptr());
	if (ret)
		return prl_err(ret, "PrlVmDevHd_GetEncryption: %s",
				get_error_str(ret).c_str());

	ret = PrlVmDevHdEncryption_SetKeyId(hEncryption, keyid.c_str());
	if (ret)
		return prl_err(ret, "PrlVmDevHdEncryption_SetKeyId: %s",
				get_error_str(ret).c_str());

	ret = PrlVmDevHd_SetEncryption(m_hDev, hEncryption);
	if (ret)
		return prl_err(ret, "PrlVmDevHd_SetEncryption: %s",
				get_error_str(ret).c_str());


	return 0;
}

std::string PrlDevHdd::get_encryption_keyid()
{
	PrlHandle hEncryption;
	std::string out;
	PRL_UINT32 len = 0;

	if (PrlVmDevHd_GetEncryption(m_hDev, hEncryption.get_ptr()) == 0 &&
		PrlVmDevHdEncryption_GetKeyId(hEncryption, NULL, &len) == 0 &&
		len > 1)
	{
		out.resize(--len);
		PrlVmDevHdEncryption_GetKeyId(hEncryption, &out[0], &len);
	}

	return out;
}

int PrlDevHdd::set_serial_number(const std::string &serial)
{	
	int ret = PrlVmDevHd_SetSerialNumber(m_hDev, serial.c_str());
	if (ret)
		return prl_err(ret, "PrlVmDevHd_SetSerialNumber: %s",
				get_error_str(ret).c_str());
	return 0;
}

PRL_RESULT PrlDevHdd::apply_encryption(const DevInfo &param)
{
	std::string old_keyid = get_encryption_keyid();
	std::string keyid = param.enc_keyid;

	if (param.cmd == Set) {
		switch (param.enc_action) {
		case ENC_SET:
			if (old_keyid.empty())
				return prl_err(PRL_ERR_INVALID_ARG, "The disk is not encrypted. "
					"To encrypt the disk, specify the --encrypt option.");
			break;

		case ENC_ENCRYPT:
			if (!old_keyid.empty())
				return prl_err(PRL_ERR_INVALID_ARG, "The disk is already encrypted.");
			break;

		case ENC_DECRYPT:
			if (old_keyid.empty())
				return prl_err(PRL_ERR_INVALID_ARG, "The disk is not encrypted.");
			keyid.clear();
			break;

		default:
			return PRL_ERR_INVALID_ARG;
		}
	}

	return set_encryption_keyid(keyid);
}

int PrlDevHdd::set_device(const DevInfo &param)
{
	PRL_RESULT ret;
	PRL_VM_DEV_EMULATION_TYPE type;
	std::string path;
	std::string id;
	bool create = param.recreate;
	int err;

	if (!param.device.empty()) {
		type = PDT_USE_REAL_HDD;
		PrlDevSrv *dev, *dev_part;

		dev = m_vm.find_srv_dev(m_devType, param.device);
		if (dev) {
			id = dev->get_id();
			path = id;
		} else if ((dev_part = m_vm.find_srv_dev(DEV_HDD_PARTITION, param.device))) {
			type = PDT_USE_OTHER;
			dev = dev_part->m_parent;
			path = dev->get_name();
			id = dev->get_id();

			PrlHandle hPart;

			if ((ret = PrlVmDevHd_AddPartition(m_hDev, hPart.get_ptr())))
				return prl_err(-1, "PrlVmDevHd_AddPartition: %s",
					param.device.c_str());
			if ((ret = PrlVmDevHdPart_SetSysName(hPart.get_handle(), dev_part->get_id().c_str())))
				return prl_err(-1, "PrlVmDevHdPart_SetSysName: %s",
					param.device.c_str());
		} else {
			/* Pass device as is */
			path = id = param.device;
		}
	} else if (param.image) {
		type = PDT_USE_IMAGE_FILE;
		id = path = param.image.get();
	} else if (!param.storage_url.empty()) {
		if ((ret = PrlVmDevHd_SetStorageURL(m_hDev, param.storage_url.c_str())))
			return prl_err(ret, "PrlVmDevHd_SetStorageURL(%s): %s",
				param.storage_url.c_str(), get_error_str(ret).c_str());
		/* disk names and type will be initialized in the dispatcher task */
		type = PDT_ANY_TYPE;
		path = id = param.storage_url;
	} else if (param.size && param.cmd == Set) {
		if ((ret = resize_image(param)))
			return ret;
		return 0;
	} else {
		/* Skip to create new image on the set action */
		if (param.cmd == Set) {
			 if (param.enc_action != ENC_NONE)
				return apply_encryption(param);
			return 0;
		}

		type = PDT_USE_IMAGE_FILE;
		/* generate hdd image name */
		if ((ret = m_vm.get_new_dir("harddisk%d.hdd", path)))
			return ret;
		create = true;
		id = path;
	}

	if ((ret = PrlVmDev_SetImagePath(m_hDev, path.c_str())))
		return prl_err(ret, "PrlVmDev_SetImagePath: %s",
				get_error_str(ret).c_str());
	if ((ret = set_emu_type(type)))
		return ret;
	if ((ret = set_fname2(path)))
		return ret;
	if ((ret = set_sname(id)))
		return ret;

	if (type == PDT_USE_IMAGE_FILE && create) {
		if ((ret = create_image(param)))
			return ret;
	} else {
		PrlOutFormatterPlain f;
		append_info_spec(f, 0);
		prl_log(0, "Creating %s", f.get_buffer().c_str());
	}

	if ((err = m_vm.validate_config(PVC_HARD_DISK)))
		return err;

	set_updated();
	return 0;
}

int PrlDevHdd::create(const DevInfo &param)
{
	int ret = 0;

	if ((ret = configure(param)))
		return ret;
	/* add to the end of the boot order */
	if (m_vm.get_vm_type() == PVT_VM && param.storage_url.empty() &&
	    (ret = m_vm.set_boot_dev(this, -1, param.storage_url.empty())))
		return ret;

	return 0;
}

int PrlDevHdd::configure(const DevInfo &param)
{
	int ret;

	if (param.enable)
		set_enable(true);
	if (param.disable)
		set_enable(false);
	if (param.connect)
		set_connect(true);
	if (param.disconnect)
		set_connect(false);

	if (!param.iface.empty()) {
		if ((ret = set_iface(param.iface)))
			return ret;
	}
	if (!param.subtype.empty()) {
		if ((ret = set_subtype(param.subtype)))
			return ret;
	}
	if (param.position >= 0) {
		if ((ret = set_position((unsigned int)param.position)))
			return ret;
	}
	if (param.passthr != -1) {
		ret = PrlVmDev_SetPassthrough(m_hDev, (PRL_BOOL) param.passthr);
		if (ret)
			return prl_err(ret, "PrlVmDev_SetPassthrough: %s",
				get_error_str(ret).c_str());
	}

	if (param.mnt) {
		if (m_vm.get_vm_type() == PVT_VM)
			return prl_err(1, "Assigning a mount point to virtual"
				" machines is not supported");
		if ((ret = PrlVmDevHd_SetMountPoint(m_hDev, param.mnt.get().c_str())))
			return prl_err(ret, "PrlVmDev_SetMountPoint: %s",
				get_error_str(ret).c_str());
		set_updated();
	}

	if (param.autocompact != -1) {
		if ((ret = PrlVmDevHd_SetAutoCompressEnabled(m_hDev, (PRL_BOOL) param.autocompact)))
			return prl_err(ret, "PrlVmDevHd_SetAutoCompressEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}

	if (param.autocompact != -1) {
		if ((ret = PrlVmDevHd_SetAutoCompressEnabled(m_hDev, (PRL_BOOL) param.autocompact)))
			return prl_err(ret, "PrlVmDevHd_SetAutoCompressEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}

	if (!param.serial_number.empty()) {
		if ((ret = set_serial_number(param.serial_number)))
			return ret;
	}

	if ((ret = set_device(param)))
		return ret;

	return 0;
}

void PrlDevHdd::append_info(PrlOutFormatter &f)
{
	append_info_spec(f, INFO_SHOW_TYPE | INFO_SHOW_SIZE);
}

void PrlDevHdd::append_info_spec(PrlOutFormatter &f, unsigned int spec)
{
	char buf[4096];
	unsigned int len;

	std::string tmp;

	f.open_dev(get_id().c_str());
	f.add_isenabled(is_enable());

	switch ( get_iface_type() ) {
		case PMS_IDE_DEVICE:
			tmp = "ide"; break;
		case PMS_SATA_DEVICE:
			tmp = "sata"; break;
		case PMS_SCSI_DEVICE:
			tmp = "scsi"; break;
		case PMS_VIRTIO_BLOCK_DEVICE:
			tmp = "virtio";
			break;
		default:
			break;

	}
	if (!tmp.empty()) {
		tmp += ":";
		tmp += ui2string(get_position());
		f.add("port", tmp, true, false, true);
	}

	StorageUrl url;
	get_storage_url(url);

	PRL_VM_DEV_EMULATION_TYPE type = get_emu_type();
	if (type == PDT_USE_IMAGE_FILE || !url.isEmpty())
		f.add("image", get_fname(), true, true);
	else
		f.add("real", get_fname(), true, true);

	if (!url.isEmpty()) {
		if (url.getSchema() == "backup") {
			f.add("backup", url.getBackupId(), true, true);
			f.add("disk", url.getDiskName(), true, true);
		} else
			f.add("url", url.getUrl(), true, true);
	}

	if (type == PDT_USE_IMAGE_FILE) {
		PRL_HARD_DISK_INTERNAL_FORMAT type;
		unsigned int size;

		if ((spec & INFO_SHOW_TYPE) &&
				!PrlVmDevHd_GetDiskType(m_hDev, &type))
			f.add("type", (type == PHD_PLAIN_HARD_DISK)? "plain" : "expanded",
					true, true);
		if ((spec & INFO_SHOW_SIZE) &&
				!PrlVmDevHd_GetDiskSize(m_hDev, &size) && size)
			f.add("size", size, "Mb", true, true);
	} else {
		unsigned int count = 0;
		std::string out;

		PrlVmDevHd_GetPartitionsCount(m_hDev, &count);
		for (unsigned int i = 0; i < count; ++i) {
			PrlHandle hPart;
			tmp = "<";

			len = sizeof(buf);
			if (!PrlVmDevHd_GetPartition(m_hDev, i, hPart.get_ptr()) &&
			    !PrlVmDevHdPart_GetSysName(hPart.get_handle(), buf, &len))
			{
				if (i)
					tmp += " ";
				tmp += buf;
			}
			tmp += ">";
		}
		if (count)
			f.add("device", tmp, true);
	}

	len = sizeof(buf);
	if (PrlVmDevHd_GetMountPoint(m_hDev, buf, &len) == 0 && len > 1)
		f.add("mnt", buf, true);

	if (!is_connected())
		f.add("state", "disconnected", true);

	if (get_iface_type() == PMS_SCSI_DEVICE)
	{
		switch(get_subtype())
		{
		case PCD_VIRTIO_SCSI:
			tmp = "virtio-scsi";
			break;
		case PCD_HYPER_V_SCSI:
			tmp = "hyperv";
			break;
		default:
			tmp = "unknown";
		}
		f.add("subtype", tmp, true);
	}

	PRL_BOOL enabled;
	if (PrlVmDevHd_IsAutoCompressEnabled(m_hDev, &enabled) == 0 && !enabled)
		f.add("autocompact", "off", true);

	if (is_full_info_mode()) {
		len = sizeof(buf);
		if (PrlVmDevHd_GetSerialNumber(m_hDev, buf, &len) == 0 && len > 1)
			f.add("serial", buf, true);
	}

	std::string key = get_encryption_keyid();
	if (!key.empty())
		f.add("keyid", key, true, true);

	f.close(true);
};

void PrlDevHdd::get_storage_url(StorageUrl& url) const
{
	PRL_RESULT ret;
	char buf[BUFSIZ];
	unsigned int len = sizeof(buf);

	buf[0] = 0;
	if ((ret = PrlVmDevHd_GetStorageURL(m_hDev, buf, &len)))
		prl_err(ret, "PrlVmDevHd_GetStorageURL: %s",
			get_error_str(ret).c_str());
	url = StorageUrl(buf);
}

/*************************** PrlDevCdrom **********************************/
int PrlDevCdrom::create(const DevInfo &param)
{
	PRL_RESULT ret = PRL_ERR_SUCCESS;

	if (param.device.empty() && param.image == boost::none)
		return prl_err(-1, "The device type is not specified."
			" Use either the --device or --image option.");

	if ((ret = configure(param)))
		return ret;
	/* add to the end of the boot order */
	if ((ret = m_vm.set_boot_dev(this, -1)))
		return ret;

	return 0;
}

int PrlDevCdrom::set_iface(const std::string &iface)
{
	PRL_RESULT ret;
	PRL_MASS_STORAGE_INTERFACE_TYPE type;

	prl_log(L_INFO, "Set the %s interface.", iface.c_str());
	if (iface == "scsi")
		type = PMS_SCSI_DEVICE;
	else if (iface == "ide")
		type = PMS_IDE_DEVICE;
	else
			return prl_err(-1, "Invalid interface: %s,"
					" supported one: ide, scsi",
					iface.c_str());
	if ((ret = PrlVmDev_SetIfaceType(m_hDev, type)))
		return prl_err(ret, "PrlVmDev_SetIfaceType: %s",
			get_error_str(ret).c_str());

	set_updated();
	return 0;
}

int PrlDevCdrom::set_subtype(const std::string &subtype)
{
	PRL_RESULT ret;
	PRL_CLUSTERED_DEVICE_SUBTYPE type;

	prl_log(L_INFO, "Set the %s subtype.", subtype.c_str());
	if (subtype == "virtio-scsi")
		type = PCD_VIRTIO_SCSI;
	else if (subtype == "hyperv")
		type = PCD_HYPER_V_SCSI;
	else
			return prl_err(-1, "Invalid subtype: %s,"
					" supported one: virtio-scsi, hyperv",
					subtype.c_str());
	if ((ret = PrlVmDev_SetSubType(m_hDev, type)))
		return prl_err(ret, "PrlVmDev_SetSubType: %s",
			get_error_str(ret).c_str());

	set_updated();
	return 0;
}

int PrlDevCdrom::set_device(const DevInfo &param)
{
	PRL_RESULT ret;
	PRL_VM_DEV_EMULATION_TYPE type;
	std::string path;

	if (!param.device.empty()) {
		type = PDT_USE_REAL_DEVICE;
		PrlDevSrv *dev = m_vm.find_srv_dev(DEV_CDROM, param.device);
		if (!dev)
			return prl_err(-1, "Unknown device: %s",
				param.device.c_str());
		path = dev->get_name();
	} else if (param.image) {
		type = PDT_USE_IMAGE_FILE;
		path = param.image.get();
	} else {
		return 0;
	}
	if (!path.empty() || param.image) {
		if ((ret = PrlVmDev_SetImagePath(m_hDev, path.c_str())))
			return prl_err(ret, "PrlVmDev_SetImagePath: %s",
					get_error_str(ret).c_str());
		if ((ret = set_emu_type(type)))
			return ret;
		if ((ret = set_fname(path)))
			return ret;
	}
	prl_log(0, "Creating %s", get_info().c_str());

	set_updated();
	return 0;
}

int PrlDevCdrom::configure(const DevInfo &param)
{
	PRL_RESULT ret;

	if (param.enable)
		set_enable(true);
	if (param.disable)
		set_enable(false);
	if (param.connect)
		set_connect(true);
	if (param.disconnect)
		set_connect(false);

	if (!param.iface.empty())
		if ((ret = set_iface(param.iface)))
			return ret;
	if (!param.subtype.empty())
		if ((ret = set_subtype(param.subtype)))
			return ret;
	if (param.position >= 0)
		if ((ret = set_position((unsigned int)param.position)))
			return ret;
	if (param.passthr != -1) {
		if ((ret  = PrlVmDev_SetPassthrough(m_hDev, (PRL_BOOL) param.passthr)))
			return ret;
	}
	if ((ret = set_device(param)))
		return ret;

	return 0;
}

void PrlDevCdrom::append_info(PrlOutFormatter &f)
{
	std::string tmp;

	f.open_dev(get_id().c_str());
	f.add_isenabled(is_enable());

	switch ( get_iface_type() ) {
		case PMS_IDE_DEVICE:
			tmp = "ide"; break;
		case PMS_SATA_DEVICE:
			tmp = "sata"; break;
		case PMS_VIRTIO_BLOCK_DEVICE:
			tmp = "virtio";
			break;
		default:
			tmp = "scsi";
	}

	tmp += ":";
	tmp += ui2string(get_position());
	f.add("port", tmp, true, false, true);

	PRL_VM_DEV_EMULATION_TYPE type = get_emu_type();
	if (type == PDT_USE_IMAGE_FILE)
		f.add("image", get_fname(), true, true);
	else
		f.add("real", get_fname(), true, true);

	if (!is_connected())
		f.add("state", "disconnected", true);

	if (get_iface_type() == PMS_SCSI_DEVICE)
	{
		switch(get_subtype())
		{
		case PCD_VIRTIO_SCSI:
			tmp = "virtio-scsi";
			break;
		case PCD_HYPER_V_SCSI:
			tmp = "hyperv";
			break;
		default:
			tmp = "unknown";
		}
		f.add("subtype", tmp, true);
	}

	PRL_BOOL enabled;
	if (PrlVmDevHd_IsAutoCompressEnabled(m_hDev, &enabled) == 0 && !enabled)
		f.add("autocompact", "off", true);

	f.close(true);
};

/*************************** PrlDevNet **********************************/
int PrlDevNet::set_mac(const std::string &mac)
{
	PRL_RESULT ret;

	if (mac.empty())
		return 0;
	prl_log(L_DEBUG, "PrlDevNet::set_mac: %s", mac.c_str());
	if (mac == "auto") {
		if ((ret = PrlVmDevNet_GenerateMacAddr(m_hDev)))
			return prl_err(ret, "PrlVmDevNet_GenerateMacAddr: %s",
				get_error_str(ret).c_str());
	} else {
		if ((ret = PrlVmDevNet_SetMacAddress(m_hDev, mac.c_str())))
			return prl_err(ret, "PrlVmDevNet_SetMacAddress: %s",
				get_error_str(ret).c_str());
	}
	set_updated();
	return 0;

}

int PrlDevNet::find_vnet(const std::string &network_id)
{
	int ret;
	PrlVNetList vnetlist;

	if (m_vm.get_srv().fill_vnetworks_list(vnetlist))
		return -1;

	char buf[1024];
	PRL_UINT32 len;
	PrlVNetList::const_iterator it = vnetlist.begin();
	for (; it != vnetlist.end(); ++it) {
		len = sizeof(buf);
		ret = PrlVirtNet_GetNetworkId((*it)->get_handle(),
				buf, &len);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlVirtNet_GetNetworkId"
					" failed: %s", get_error_str(ret).c_str());
			continue;
		}
		if (!strncmp(network_id.c_str(), buf, sizeof(buf)))
			return 1;
	}
	return 0;
}

int PrlDevNet::set_vnet(const std::string &network_id)
{
	PRL_RESULT ret;
	PRL_NET_ADAPTER_EMULATED_TYPE type = PNA_BRIDGED_ETHERNET;

	if (!find_vnet(network_id))
		prl_log(0, "Warning: The virtual network '%s' is not configured",
			network_id.c_str());
	if ((ret = PrlVmDevNet_SetVirtualNetworkId(m_hDev, network_id.c_str())))
		return prl_err(ret, "PrlVmDevNet_SetVirtualNetworkId: %s",
				get_error_str(ret).c_str());
	if ((ret = set_emu_type((PRL_VM_DEV_EMULATION_TYPE)type)))
		return ret;
	set_updated();
	return 0;
}

std::string PrlDevNet::get_vnetwork()
{
	char buf[1024];
	unsigned int len =  sizeof(buf);
	std::string vnet;

	if (PrlVmDevNet_GetVirtualNetworkId(m_hDev, buf, &len) == 0)
		vnet = buf;

	return vnet;
}

int PrlDevNet::set_device(DevMode mode, const std::string &iface,
	const std::string &mac)
{
	PRL_RESULT ret;
	PRL_NET_ADAPTER_EMULATED_TYPE type;

	if (mode == DEV_TYPE_NET_HOST) {
		type = PNA_HOST_ONLY;
	} else if (mode == DEV_TYPE_NET_BRIDGED) {
		type = PNA_BRIDGED_ETHERNET;
	} else if (mode == DEV_TYPE_NET_SHARED) {
		type = PNA_SHARED;
	} else if (mode == DEV_TYPE_NET_ROUTED) {
		type = PNA_ROUTED;
	} else if (mode == DEV_TYPE_NET_BRIDGE) {
		type = PNA_BRIDGE;
		PrlVmDevNet_SetVirtualNetworkId(m_hDev, iface.c_str());
	} else if (mode == DEV_TYPE_NET_DEVICE) {
		type = PNA_DIRECT_ASSIGN;
		PrlDevSrv *dev = m_vm.find_srv_dev(DEV_GENERIC_PCI, iface);
		if (!dev)
			return prl_err(-1, "Unknown device: %s",
					iface.c_str());
		if ((ret = PrlVmDev_SetSysName(m_hDev, dev->get_id().c_str())))
			return prl_err(ret, "PrlVmDev_SetSysName: %s",
					get_error_str(ret).c_str());
		if ((ret = PrlVmDev_SetFriendlyName(m_hDev, dev->get_name().c_str())))
			return prl_err(ret, "PrlVmDev_SetFriendlyName: %s",
					get_error_str(ret).c_str());
	} else if (mode == DEV_TYPE_NONE) {
		if (iface.empty() && mac.empty())
			return 0;
	} else {
		return prl_err(-1, "An incorrect network type is specified.");
	}
	if (mode != DEV_TYPE_NONE) {
		if (mode != DEV_TYPE_NET_DEVICE && mode != DEV_TYPE_NET_ROUTED &&
				mode != DEV_TYPE_NET_BRIDGE)
		{
			if (get_vnetwork().length() != 0)
				return prl_err(-1, "To specify the virtual network,"
					" use the --network option instead of --type.");
		}
		if ((ret = set_emu_type((PRL_VM_DEV_EMULATION_TYPE)type)))
			return ret;
	}
	if (!mac.empty()) {
		if ((ret = set_mac(mac)))
			return ret;
	}

	return 0;
}

int PrlDevNet::set_device(const std::string &vnet,
		const std::string &mac)
{
	PRL_RESULT ret;

	if (!vnet.empty() && (ret = set_vnet(vnet)))
		return ret;

	if (!mac.empty()) {
		if ((ret = set_mac(mac)))
			return ret;
	}

	return 0;
}

int PrlDevNet::get_ip(ip_list_t &ips)
{
	PRL_RESULT ret;
	PrlHandle h;

	if ((ret = PrlVmDevNet_GetNetAddresses(m_hDev, h.get_ptr())))
		return prl_err(ret, "PrlVmDevNet_GetNetAddresses: %s", get_error_str(ret).c_str());

	PRL_UINT32 count;
	PrlStrList_GetItemsCount(h.get_handle(), &count);
	for (unsigned int i = 0; i < count; i++) {
		char buf[128];
		unsigned int len = sizeof(buf);

		PrlStrList_GetItem(h.get_handle(), i, buf, &len);
		std::string tmp(buf);
		normalize_ip(tmp);
		ips.add(tmp);
	}

	return 0;
}

int PrlDevNet::build_ip(const NetParam &net, ip_list_t &ips)
{
	if (net.set_ip) {
		ips = net.ip;
		return 0;
	}
	ip_list_t cur_ips;
	if (!net.delall_ip)
		get_ip(cur_ips);
	for (ip_list_t::const_iterator it = cur_ips.begin(), eit = cur_ips.end(); it != eit; it++) {
		if (!net.ip_del.find(*it) && !net.ip.find(*it)) {
			// IP without mask means this IP with any mask
			if (!(*it).mask.empty()) {
				struct ip_addr tmp(*it);
				tmp.mask = "";
				if (net.ip_del.find(tmp) || net.ip.find(tmp))
					continue;
			}
			ips.push_back(*it);
		}
	}
	for (ip_list_t::const_iterator it = net.ip.begin(), eit = net.ip.end(); it != eit; it++)
		ips.push_back(*it);

	return 0;
}

int PrlDevNet::set_firewall(const NetParam &net)
{
	int ret;

	if (net.fw_enable != -1) {
		if ((ret = PrlVmDevNet_SetFirewallEnabled(m_hDev, (PRL_BOOL) net.fw_enable)))
			return prl_err(ret, "PrlVmDevNet_SetFirewallEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (net.fw_policy != -1) {
		if (net.fw_direction == -1) {
			ret = PrlVmDevNet_SetFirewallDefaultPolicy(m_hDev,
				PFD_INCOMING, (PRL_FIREWALL_POLICY)net.fw_policy);
			if (!ret)
				ret = PrlVmDevNet_SetFirewallDefaultPolicy(m_hDev,
					PFD_OUTGOING, (PRL_FIREWALL_POLICY)net.fw_policy);
		} else {
			ret = PrlVmDevNet_SetFirewallDefaultPolicy(m_hDev,
				(PRL_FIREWALL_DIRECTION) net.fw_direction,
				(PRL_FIREWALL_POLICY) net.fw_policy);
		}
		if (ret)
			return prl_err(ret, "PrlVmDevNet_SetFirewallDefaultPolicy: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (!net.fw_rules.empty()) {
		PrlHandle hList;

		PrlApi_CreateHandlesList(hList.get_ptr());

		for (fw_rule_list_t::const_iterator it = net.fw_rules.begin(),
				eit = net.fw_rules.end(); it != eit; it++) {
			PrlHandle hRule;

			if ((ret = PrlFirewallRule_Create(hRule.get_ptr())))
				return prl_err(-1, "PrlFirewallRule_SetLocalPort: %s",
						get_error_str(ret).c_str());

			if (!it->proto.empty()) {
				if ((ret = PrlFirewallRule_SetProtocol(hRule, it->proto.c_str())))
					return prl_err(-1, "PrlFirewallRule_SetProtocol: %s",
							get_error_str(ret).c_str());
			}
			if (it->local_port != 0) {
				if ((ret = PrlFirewallRule_SetLocalPort(hRule, it->local_port)))
					return prl_err(-1, "PrlFirewallRule_SetLocalPort: %s",
							get_error_str(ret).c_str());
			}
			if (!it->local_ip.empty()) {
				if ((ret = PrlFirewallRule_SetLocalNetAddress(hRule, it->local_ip.c_str())))
					return prl_err(-1, "PrlFirewallRule_SetLocalNetAddress: %s",
							get_error_str(ret).c_str());
			}
			if (it->remote_port != 0) {
				if ((ret = PrlFirewallRule_SetRemotePort(hRule, it->remote_port)))
					return prl_err(-1, "PrlFirewallRule_SetRemotePort: %s",
							get_error_str(ret).c_str());
			}
			if (!it->remote_ip.empty()) {
				if ((ret = PrlFirewallRule_SetRemoteNetAddress(hRule, it->remote_ip.c_str())))
					return prl_err(-1, "PrlFirewallRule_SetRemoteNetAddress: %s",
							get_error_str(ret).c_str());
			}

			if ((ret = PrlHndlList_AddItem(hList.get_handle(), hRule.get_handle())))
				return prl_err(-1, "PrlHndlList_AddItem: %s",
					get_error_str(ret).c_str());
		}
		ret = PrlVmDevNet_SetFirewallRuleList(m_hDev,
				(PRL_FIREWALL_DIRECTION) net.fw_direction, hList);
		if (ret)
			return prl_err(-1, "PrlVmDevNet_SetFirewallRuleList: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	return 0;
}

static bool is_ipv6(const struct ip_addr &ip)
{
	std::string::size_type pos = ip.ip.find_first_of(":");
	if (pos != std::string::npos)
		return true;
	return false;
}

int PrlDevNet::set_network(const DevInfo& param)
{
	int ret;
	const NetParam& net = param.net;
	int updated = 0;
	std::string gw;

	PRL_NET_ADAPTER_EMULATED_TYPE type = (PRL_NET_ADAPTER_EMULATED_TYPE)get_emu_type();
	if (type == PNA_ROUTED) {
		if ((net.dhcp != -1)  || (net.dhcp6 != -1))
			return prl_err(1, "This adapter is in the routed mode."
				" Configuration via DHCP is not supported.");
		if (!net.gw.empty() || !net.gw6.empty())
			return prl_err(1, "This adapter is in the routed mode."
				" Configuration of the default gateway is not supported.");

	}
	if (!net.nameserver.empty()) {
		PrlHandle hArgs;

		str_list2handle(net.nameserver, hArgs);

		if ((ret = PrlVmDevNet_SetDnsServers(m_hDev, hArgs.get_handle())))
			return prl_err(ret, "PrlVmDevNet_SetDnsServers: %s",
					get_error_str(ret).c_str());
		updated++;
	}
	if (net.searchdomain) {
		PrlHandle hArgs;

		str_list2handle(net.searchdomain.get(), hArgs);

		if ((ret = PrlVmDevNet_SetSearchDomains(m_hDev, hArgs.get_handle())))
			return prl_err(ret, "PrlVmDevNet_SetSearchDomains: %s",
					get_error_str(ret).c_str());
		updated++;
	}
	if (!net.ip.empty() || !net.ip_del.empty() || net.delall_ip) {
		ip_list_t ips;
		build_ip(net, ips);

		PrlHandle h;
		PrlApi_CreateStringsList(h.get_ptr());

		bool ipv6_found = false;
		bool ipv4_found = false;
		for (ip_list_t::const_iterator it = ips.begin(), eit = ips.end(); it != eit; ++it)
		{
			if (is_ipv6(*it))
				ipv6_found = true;
			else
				ipv4_found = true;
			PrlStrList_AddItem(h.get_handle(), it->to_str().c_str());
		}

		if ((ret = PrlVmDevNet_SetNetAddresses(m_hDev, h.get_handle())))
		{
			if (ret == PRL_ERR_INVALID_ARG)
				return prl_err(ret, "The following IP addresses are invalid: '%s'",
					ips.to_str().c_str());
			else
				return prl_err(ret, "PrlVmDevNet_SetNetAddresses: %s",
					get_error_str(ret).c_str());
		}
		/* switch to DHCP automatically */
		if ((ret = PrlVmDevNet_SetConfigureWithDhcp(m_hDev,
					(!ipv4_found) ? PRL_TRUE : PRL_FALSE)))
			return prl_err(ret, "PrlVmDevNet_SetConfigureWithDhcp: %s",
				get_error_str(ret).c_str());
		if (ipv6_found)
		{
			if ((ret = PrlVmDevNet_SetConfigureWithDhcpIPv6(m_hDev, PRL_FALSE)))
				return prl_err(ret, "PrlVmDevNet_SetConfigureWithDhcp: %s",
				get_error_str(ret).c_str());
		}
		updated++;
	}
	if (net.dhcp != -1) {
		if ((ret = PrlVmDevNet_SetConfigureWithDhcp(m_hDev, (PRL_BOOL) net.dhcp)))
			return prl_err(ret, "PrlVmDevNet_SetConfigureWithDhcp: %s",
					get_error_str(ret).c_str());
		updated++;
	}
	if (net.dhcp6 != -1) {
		if ((ret = PrlVmDevNet_SetConfigureWithDhcpIPv6(m_hDev, (PRL_BOOL) net.dhcp6)))
			return prl_err(ret, "PrlVmDevNet_SetConfigureWithDhcpIPv6: %s",
					get_error_str(ret).c_str());
		updated++;
	}
	gw = net.gw;
	if (!gw.empty()) {
		if (gw == "x")
			gw = "";
		if ((ret = PrlVmDevNet_SetDefaultGateway(m_hDev, gw.c_str()))) {
			if (ret == PRL_ERR_INVALID_ARG)
				return prl_err(ret, "The following gateway is invalid: '%s'",
					gw.c_str());
			else
				return prl_err(ret, "PrlVmDevNet_SetDefaultGateway(%s): %s",
					gw.c_str(), get_error_str(ret).c_str());
		}
		updated++;
	}
	gw = net.gw6;
	if (!gw.empty()) {
		if (gw == "x")
			gw = "";
		if ((ret = PrlVmDevNet_SetDefaultGatewayIPv6(m_hDev, gw.c_str()))) {
			if (ret == PRL_ERR_INVALID_ARG)
				return prl_err(ret, "The following gateway is invalid: '%s'",
					gw.c_str());
			else
				return prl_err(ret, "PrlVmDevNet_SetDefaultGatewayIPv6(%s): %s",
					gw.c_str(), get_error_str(ret).c_str());
			}
		updated++;
	}

	if (updated || net.configure != -1) {
		PRL_BOOL enable = PRL_TRUE;

		if (net.configure != -1)
			enable = (PRL_BOOL) net.configure;
		else {
			PRL_BOOL auto_apply = PRL_FALSE;
			if ((ret = PrlVmDevNet_IsAutoApply(m_hDev, &auto_apply)))
				return prl_err(ret, "PrlVmDevNet_GetAutoApply: %s", get_error_str(ret).c_str());

			if (!auto_apply)
				prl_log(0, "Enable automatic reconfiguration for this network adapter.");
		}

		if ((ret = PrlVmDevNet_SetAutoApply(m_hDev, enable)))
			return prl_err(ret, "PrlVmDevNet_SetAutoApply: %s", get_error_str(ret).c_str());
		updated++;
	}

	if (net.mac_filter != -1) {
	    if ((ret = PrlVmDevNet_SetPktFilterPreventMacSpoof(m_hDev, (PRL_BOOL) net.mac_filter)))
		    return prl_err(ret, "PrlVmDevNet_SetPktFilterPreventMacSpoof: %s",
				    get_error_str(ret).c_str());
		updated++;
	}

	if (net.prevent_promisc != -1) {
	    if ((ret = PrlVmDevNet_SetPktFilterPreventPromisc(m_hDev, (PRL_BOOL) net.prevent_promisc)))
		    return prl_err(ret, "PrlVmDevNet_SetPktFilterPreventPromisc: %s",
				    get_error_str(ret).c_str());
		updated++;
	}

	if (net.ip_filter != -1) {
	    if ((ret = PrlVmDevNet_SetPktFilterPreventIpSpoof(m_hDev, (PRL_BOOL) net.ip_filter)))
		    return prl_err(ret, "PrlVmDevNet_SetPktFilterPreventIpSpoof: %s",
				    get_error_str(ret).c_str());
		updated++;
	}

	if (updated || param.cmd == Add)
	{
		m_vm.validate_config(PVC_NETWORK_ADAPTER);
		if (updated)
			set_updated();
	}

	return 0;
}

int PrlDevNet::create(const DevInfo &param)
{
	int ret;

	if (!param.disable)
		set_enable(true);
	if (param.mode == DEV_TYPE_NONE && param.device.empty()) {
		/* assign to Bridged network if not specified */
		std::string vnet;
		if (param.vnetwork.empty())
			vnet = "Bridged";
		else
			vnet = param.vnetwork;
		if ((ret = set_device(vnet, param.mac)))
			return ret;
	} else if (param.mode == DEV_TYPE_NET_BRIDGED && param.iface.empty()) {
		/* Set iface == 'default' for Bridged mode if not specified */
		if ((ret = PrlVmDevNet_SetBoundAdapterIndex(m_hDev, -1)))
			return prl_err(ret, "PrlVmDevNet_SetBoundAdapterIndex: %s",
					get_error_str(ret).c_str());
	}
	if (m_vm.get_vm_type() == PVT_CT && !param.net.ifname.empty()) {
		int idx;

		if (sscanf(param.net.ifname.c_str(), "%*[^0-9]%d", &idx) != 1)
			return prl_err(-1, "Unable to get index from '%s'",
					param.net.ifname.c_str());
		if ((ret = PrlVmDev_SetIndex(m_hDev, idx)))
			return prl_err(ret, "PrlVmDev_SetIndex: %s",
					get_error_str(ret).c_str());

		if ((ret = PrlVmDev_SetSysName(m_hDev, param.net.ifname.c_str())))
			return prl_err(ret, "PrlVmDev_SetSysName: %s",
					get_error_str(ret).c_str());
		set_idx(idx);
	}
	if ((ret = configure(param)))
		return ret;
	/* add to the end of the boot order */
	if ((ret = m_vm.set_boot_dev(this, -1)))
		return ret;

	return 0;
}

int PrlDevNet::configure(const DevInfo &param)
{
	PRL_RESULT ret;

	prl_log(L_INFO, "Configure %s.", get_id().c_str());
	if (param.enable)
		set_enable(true);
	if (param.disable)
		set_enable(false);
	if (param.connect)
		set_connect(true);
	if (param.disconnect)
		set_connect(false);
	if (param.net.adapter_type != PNT_UNDEFINED) {
		if ((ret = PrlVmDevNet_SetAdapterType(m_hDev,
					param.net.adapter_type)))
		    return prl_err(ret, "PrlVmDevNet_SetAdapterType: %s",
				    get_error_str(ret).c_str());
		set_updated();
	}

	if (!param.device.empty()) {
		if ((ret = set_device(DEV_TYPE_NET_DEVICE, param.device, std::string())))
			return ret;
	} else {
		if (param.mode != DEV_TYPE_NONE)
			ret = set_device(param.mode, param.iface, param.mac);
		else
			ret = set_device(param.vnetwork, param.mac);
		if (ret)
			return ret;
	}
	if ((ret = set_network(param)))
		return ret;
	if ((ret = set_firewall(param.net)))
		return ret;
	if (is_updated())
		prl_log(0, "%s %s",
			cmd2string(param.cmd),
			get_info().c_str());

	return 0;
}

std::string PrlDevNet::get_mac()
{
	char buf[20];
	unsigned int len;
	std::string out;

	len = sizeof(buf);
	if (!PrlVmDevNet_GetMacAddress(m_hDev, buf, &len)) {
		out += buf[0];
		for (unsigned int i = 1; buf[i] != 0; i++) {
			if (!(i % 2))
				out += ":";
			out += buf[i];
		}
	}

	return out;
}

std::string PrlDevNet::get_veth_name() const
{
	char buf[64] = "";
	PRL_RESULT ret;
	unsigned int l = sizeof(buf);

	if ((get_id() != VENET0_ID) &&
			PRL_FAILED(ret = PrlVmDevNet_GetHostInterfaceName(m_hDev, buf, &l))) {
		prl_log(L_WARN, "PrlVmDevNet_GetHostInterfaceName:%s",
			get_error_str(ret).c_str());
	}

	return std::string(buf);
}

std::string PrlDevNet::get_name() const
{
	return (get_id() == VENET0_ID ? "" : get_id());
}

void PrlDevNet::append_info(PrlOutFormatter &f)
{

	std::string out;
	char buf[4096];
	unsigned int len;
	PRL_BOOL bEnabled;
	ip_list_t ips;

	PRL_NET_ADAPTER_EMULATED_TYPE type = (PRL_NET_ADAPTER_EMULATED_TYPE)get_emu_type();

#ifdef _LIN_
	PRL_VM_TYPE vm_type = m_vm.get_vm_type();
	if (vm_type == PVT_CT && type == PNA_ROUTED) {
		f.open_dev(VENET0_STR);
		f.add_isenabled(true);
		f.add("type", "routed", true, true);
		get_ip(ips);
		if (!ips.empty())
			f.add("ips", ips.to_str(), true, true);
		f.close(true);
		return;
	}
#endif
	f.open_dev(get_id().c_str());
	f.add_isenabled(is_enable());

	std::string vnet = get_vnetwork();
#ifdef _LIN_
	f.add("dev", get_veth_name(), true, true);
	std::string ifname = get_sname();
	if (!ifname.empty())
		f.add("ifname", ifname.c_str(), true, true);
#endif

	if (type == PNA_BRIDGE) {
		f.add("type", "bridge", true);
	} else if (type == PNA_BRIDGED_ETHERNET) {
		f.add("network", vnet, true, true);
	} else {
		if (type == PNA_ROUTED) {
			f.add("type", "routed", true);
		} else if (   type == PNA_HOST_ONLY
				 || type == PNA_BRIDGED_ETHERNET)
		{
			int idx;
			len = sizeof(buf);

			f.add("type", type == PNA_BRIDGED_ETHERNET ? "bridged" : "host", true);

			/* Woraround for bug #112691.
			   get index first to track "Default Adapter"
			 */
			if (   ! PrlVmDevNet_GetBoundAdapterIndex(m_hDev, &idx)
				&& ! PrlVmDevNet_GetBoundAdapterName(m_hDev, buf, &len))
				f.add("iface", (type == PNA_BRIDGED_ETHERNET && idx == -1) ? "default" : buf, true, true);
		} else if (type == PNA_SHARED)
			f.add("type", "shared", true);
		else if ( type == PNA_DIRECT_ASSIGN) {
			f.add("type", "direct", true);
			f.add("device", get_sname(), true, true);
		} else
			f.add("type", "unknown", true);
	}

	if (type != PNA_DIRECT_ASSIGN) {
		len = sizeof(buf);
		if (!PrlVmDevNet_GetMacAddress(m_hDev, buf, &len))
			f.add("mac", buf, true);
	}
	PRL_VM_NET_ADAPTER_TYPE nAdapterType;
	if (PrlVmDevNet_GetAdapterType(m_hDev, &nAdapterType) == 0 &&
		nAdapterType != PNT_UNDEFINED)
	{
		if (nAdapterType == PNT_RTL)
			f.add("card", "rtl", true);
		else if (nAdapterType == PNT_E1000)
			f.add("card", "e1000", true);
		else if (nAdapterType == PNT_HYPERV)
			f.add("card", "hyperv", true);
		else if (nAdapterType == PNT_E1000E)
			f.add("card", "e1000e", true);
		else if (nAdapterType == PNT_VIRTIO)
			f.add("card", "virtio", true);
		else
			f.add("card", "unknown", true);
	}

	if (is_full_info_mode()) {
		if (PrlVmDevNet_IsPktFilterPreventPromisc(m_hDev, &bEnabled) == 0)
			f.add("preventpromisc", bEnabled ? "on" : "off", true);

		if (PrlVmDevNet_IsPktFilterPreventMacSpoof(m_hDev, &bEnabled) == 0)
			f.add("mac_filter", bEnabled ? "on" : "off", true);

		if (PrlVmDevNet_IsPktFilterPreventIpSpoof(m_hDev, &bEnabled) == 0)
			f.add("ip_filter", bEnabled ? "on" : "off", true);

		PrlHandle h;

		if (PrlVmDevNet_GetDnsServers(m_hDev, h.get_ptr()) == 0)
			f.add("nameservers", handle2str(h), true);

		if (PrlVmDevNet_GetSearchDomains(m_hDev, h.get_ptr()) == 0)
			f.add("searchdomains", handle2str(h), true);
	}

	get_ip(ips);
	if (!ips.empty())
		f.add("ips", ips.to_str(), true, true);

	if (type != PNA_ROUTED) {
		if (PrlVmDevNet_IsConfigureWithDhcp(m_hDev,  &bEnabled) == 0) {
			if (bEnabled)
				f.add("dhcp", "yes", true, true);
		}
		if (PrlVmDevNet_IsConfigureWithDhcpIPv6(m_hDev,  &bEnabled) == 0) {
			if (bEnabled)
				f.add("dhcp6", "yes", true, true);
		}
		len = sizeof(buf);
		if (PrlVmDevNet_GetDefaultGateway(m_hDev, buf, &len ) == 0) {
			if (len > 1)
				f.add("gw", buf, true, true);
		}
		len = sizeof(buf);
		if (PrlVmDevNet_GetDefaultGatewayIPv6(m_hDev, buf, &len ) == 0) {
			if (len > 1)
				f.add("gw6", buf, true, true);
		}
	}

	if (!is_connected())
		f.add("state", "disconnected", true);

	f.close(true);
};

/************************ PrlDevUsb *************************************/

int PrlDevUsb::create(const DevInfo &param)
{
	const PrlDev *dev = m_vm.find_dev(m_devType, 0);

	if (dev && dev != this)
		return prl_err(-1, "The USB device already exists.");
	prl_log(0, "Creating %s.", get_id().c_str());
	if (!param.disable)
		set_enable(true);

	return configure(param);
}


int PrlDevUsb::connect(const std::string &name)
{
	int ret;
	PrlDevSrv *dev;

	if (!(dev = m_vm.find_srv_dev(m_devType, name)))
		return prl_err(-1, "The device '%s' is not found",
			name.c_str());
	if ((ret = set_fname(dev->get_id())))
		return ret;

	std::string err;

	prl_log(0, "Connect device: %s", name.c_str());
	PrlHandle hJob(PrlVmDev_Connect(m_hDev));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "Failed to connect device: %s",
			err.c_str());
	prl_log(0, "The device successfully connected");

	return 0;
}

int PrlDevUsb::disconnect(const std::string &name)
{
	int ret;
	PrlDevSrv *dev;

	if (!(dev = m_vm.find_srv_dev(m_devType, name)))
		return prl_err(-1, "The device '%s' is not found",
			name.c_str());
	if ((ret = set_fname(dev->get_id())))
		return ret;

	std::string err;

	prl_log(0, "Disconnect device: %s", name.c_str());
	PrlHandle hJob(PrlVmDev_Disconnect(m_hDev));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "Failed to disconnect device: %s",
			err.c_str());
	prl_log(0, "The device successfully disconnected");

	return 0;
}

int PrlDevUsb::configure(const DevInfo &param)
{
	if (param.enable)
		set_enable(true);
	if (param.disable)
		set_enable(false);
	return 0;
}

void PrlDevUsb::append_info(PrlOutFormatter &f)
{
	f.open_dev(get_id().c_str());
	f.add_isenabled(is_enable());

	if (!is_connected())
		f.add("state", "disconnected", true);

	f.close(true);
};
/******************** PrlDevFdd ****************************/
int PrlDevFdd::create_image(const DevInfo &param)
{
	PRL_RESULT ret;
	std::string err;

	(void) param;
	prl_log(0, "Create the floppy image file...");
	PrlHandle hJob(PrlVmDev_CreateImage(m_hDev, true, true));
	if ((ret = get_job_retcode(hJob.get_handle(), err, ~0)))
		return prl_err(ret, "PrlVmDev_CreateImage: %s", err.c_str());

	set_updated();
	return 0;
}

int PrlDevFdd::set_device(const DevInfo &param)
{
	PRL_RESULT ret;
	PRL_VM_DEV_EMULATION_TYPE type;
	std::string path;

	if (!param.device.empty()) {
		type = PDT_USE_REAL_DEVICE;
		PrlDevSrv *dev = m_vm.find_srv_dev(DEV_FDD, param.device);
		if (!dev)
			return prl_err(-1, "Unknown device: %s",
				param.device.c_str());
		path = dev->get_name();
	} else if (param.image) {
		type = PDT_USE_IMAGE_FILE;
		path = param.image.get();
	} else {
		/* Skip to create new image on the set action */
		if (param.cmd == Set)
			return 0;
		type = PDT_USE_IMAGE_FILE;
		path = "floppy.fdd";
	}
	if ((ret = PrlVmDev_SetImagePath(m_hDev, path.c_str())))
		return prl_err(ret, "PrlVmDev_SetImagePath: %s",
				get_error_str(ret).c_str());

	if ((ret = set_emu_type(type)))
		return ret;
	if ((ret = set_fname(path)))
		return ret;

	prl_log(0, "Creating %s.", get_info().c_str());

	if (type == PDT_USE_IMAGE_FILE && param.recreate)
		if ((ret = create_image(param)))
			return ret;

	set_updated();
	return 0;
}

int PrlDevFdd::create(const DevInfo &param)
{
	int ret = 0;
	const PrlDev *dev = m_vm.find_dev(m_devType, 0);

	/* Fixme: allow single (fdd0) floppy */
	if (dev && dev != this)
		return prl_err(-1, "The fdd0 device already exists.");
	if (!param.disable)
		set_enable(true);
	if ((ret = configure(param)))
		return ret;
	/* add to the end of the boot order */
	if ((ret = m_vm.set_boot_dev(this, -1)))
		return ret;

	return 0;
}

int PrlDevFdd::configure(const DevInfo &param)
{
	int ret;

	if (param.enable)
		set_enable(true);
	if (param.disable)
		set_enable(false);
	if (param.connect)
		set_connect(true);
	if (param.disconnect)
		set_connect(false);

	if ((ret = set_device(param)))
		return ret;
	/* add to the end of the boot order */
	if ((ret = m_vm.set_boot_dev(this, -1)))
		return ret;

	return 0;
}

void PrlDevFdd::append_info(PrlOutFormatter &f)
{
	f.open_dev(get_id().c_str());
	f.add_isenabled(is_enable());

	PRL_VM_DEV_EMULATION_TYPE type = get_emu_type();
	const char *str_type;
	str_type = (type == PDT_USE_REAL_DEVICE) ? "real" : "image";
	f.add(str_type, get_fname(), true, true);

	if (!is_connected())
		f.add("state", "disconnected", true);

	f.close(true);

};

/**************************** PrlDevSerial **********************/
int PrlDevSerial::set_device(const DevInfo &param)
{
	PRL_RESULT ret;
	PRL_VM_DEV_EMULATION_TYPE type;
	PRL_SERIAL_PORT_SOCKET_OPERATION_MODE mode = PSP_SERIAL_SOCKET_SERVER;
	std::string path;

	if (!param.device.empty()) {
		type = PDT_USE_REAL_DEVICE;
		PrlDevSrv *dev = m_vm.find_srv_dev(DEV_SERIAL, param.device);
		if (!dev)
			return prl_err(-1, "Unknown device: %s",
				param.device.c_str());
		path = dev->get_name();
	} else if (!param.output.empty()) {
		type = PDT_USE_OUTPUT_FILE;
		path = param.output;
	} else if (!param.socket.empty()) {
		type = PDT_USE_SERIAL_PORT_SOCKET_MODE;
		if (param.socket_mode != -1)
			mode = (PRL_SERIAL_PORT_SOCKET_OPERATION_MODE)param.socket_mode;
		path = param.socket;
	} else if (!param.socket_tcp.empty()) {
		type = PDT_USE_SERIAL_PORT_TCP_MODE;
		if (param.socket_mode != -1)
			mode = (PRL_SERIAL_PORT_SOCKET_OPERATION_MODE)param.socket_mode;
		path = param.socket_tcp;
	} else if (!param.socket_udp.empty()) {
		type = PDT_USE_SERIAL_PORT_UDP_MODE;
		path = param.socket_udp;
	} else if (param.socket_mode != -1) {
		type = get_emu_type();
		if (type != PDT_USE_SERIAL_PORT_SOCKET_MODE &&
				type != PDT_USE_SERIAL_PORT_TCP_MODE)
			return prl_err(-1, "The serial port type is not sutable with socket-mode");

		mode = (PRL_SERIAL_PORT_SOCKET_OPERATION_MODE)param.socket_mode;
	} else {
		return 0;
	}
	if ((ret = set_emu_type(type)))
		return ret;
	if (!path.empty() && (ret = set_fname(path)))
		return ret;
	if (type == PDT_USE_SERIAL_PORT_SOCKET_MODE ||
			type == PDT_USE_SERIAL_PORT_TCP_MODE) {
		if ((ret = PrlVmDevSerial_SetSocketMode(m_hDev, mode)))
			return prl_err(ret, "PrlVmDevSerial_SetSocketMode: %s",
					param.device.c_str());
	}

	prl_log(0, "Creating %s", get_info().c_str());
	set_updated();
	return 0;
}

int PrlDevSerial::create(const DevInfo &param)
{
	if (param.device.empty() &&
	    param.output.empty() &&
	    param.socket.empty() &&
	    param.socket_tcp.empty() &&
	    param.socket_udp.empty())
		return prl_err(-1, "The serial port type is not specified."
			" Use one of the following options: --device, --output, or --socket.");

	if (!param.disable)
		set_enable(true);

	return configure(param);
}

int PrlDevSerial::configure(const DevInfo &param)
{
	PRL_RESULT ret;

	if (param.enable)
		set_enable(true);
	if (param.disable)
		set_enable(false);
	if (param.connect)
		set_connect(true);
	if (param.disconnect)
		set_connect(false);
	if ((ret = set_device(param)))
		return ret;

	return 0;
}

void PrlDevSerial::append_info(PrlOutFormatter &f)
{
	f.open_dev(get_id().c_str());
	f.add_isenabled(is_enable());

	PRL_VM_DEV_EMULATION_TYPE type = get_emu_type();
	const char *str_type;
	switch (type)
	{
	case PDT_USE_REAL_DEVICE:
		str_type = "real";
		break;
	case PDT_USE_OUTPUT_FILE:
		str_type = "output";
		break;
	case PDT_USE_SERIAL_PORT_SOCKET_MODE:
		str_type = "socket";
		break;
	case PDT_USE_SERIAL_PORT_TCP_MODE:
		str_type = "tcp";
		break;
	case PDT_USE_SERIAL_PORT_UDP_MODE:
		str_type = "udp";
		break;
	default:
		str_type = "unknown";
	}

	f.add(str_type, get_fname(), true, true);

	if (type == PDT_USE_SERIAL_PORT_SOCKET_MODE ||
			type == PDT_USE_SERIAL_PORT_TCP_MODE) {
		PRL_SERIAL_PORT_SOCKET_OPERATION_MODE mode = PSP_SERIAL_SOCKET_SERVER;
		PrlVmDevSerial_GetSocketMode(m_hDev, &mode);
		switch(mode) {
		case PSP_SERIAL_SOCKET_SERVER:
			f.add("mode", "server", true);
			break;
		case PSP_SERIAL_SOCKET_CLIENT:
			f.add("mode", "client", true);
			break;
		}
	}

	if (!is_connected())
		f.add("state", "disconnected", true);

	f.close(true);
};

/************************** PrlDevParallel ***********************************/
int PrlDevParallel::set_device(const DevInfo &param)
{
	PRL_RESULT ret;
	PRL_VM_DEV_EMULATION_TYPE type;
	std::string path;

	if (!param.device.empty()) {
		type = PDT_USE_REAL_DEVICE;
		PrlDevSrv *dev = m_vm.find_srv_dev(DEV_PARALLEL,
								param.device);
		if (!dev)
			return prl_err(-1, "Unknown device: %s",
				param.device.c_str());
		path = dev->get_name();
	} else if (!param.output.empty()) {
		type = PDT_USE_OUTPUT_FILE;
		path = param.output;
	} else {
		return 0;
	}
	if ((ret = set_emu_type(type)))
		return ret;
	if ((ret = set_fname(path)))
		return ret;

	prl_log(0, "Creating %s.", get_info().c_str());
	set_updated();
	return 0;
}

int PrlDevParallel::create(const DevInfo &param)
{
	if (param.device.empty() && param.output.empty())
		return prl_err(-1, "The parallel port type is not specified."
			" Use either the --device or --output option.");
	if (!param.disable)
		set_enable(true);

	return configure(param);
}

int PrlDevParallel::configure(const DevInfo &param)
{
	PRL_RESULT ret;

	if (param.enable)
		set_enable(true);
	if (param.disable)
		set_enable(false);
	if (param.connect)
		set_connect(true);
	if (param.disconnect)
		set_connect(false);
	if ((ret = set_device(param)))
		return ret;

	return 0;
}

void PrlDevParallel::append_info(PrlOutFormatter &f)
{
	f.open_dev(get_id().c_str());
	f.add_isenabled(is_enable());

	PRL_VM_DEV_EMULATION_TYPE type = get_emu_type();
	const char * str_type;
	str_type = (type == PDT_USE_REAL_DEVICE) ? "real" : "output";
	f.add(str_type, get_fname(), true, true);

	if (!is_connected())
		f.add("state", "disconnected", true);

	f.close(true);
};

/**************************** PrlDevSound ***************************/
int PrlDevSound::set_device(const DevInfo &param)
{
	PRL_RESULT ret;
	PrlDevSrv *dev;

	if (!param.output.empty()) {
		if (!(dev = m_vm.find_srv_dev(DEV_SOUND, param.output)))
			return prl_err(-1, "Unknown device: %s",
				param.device.c_str());
		ret = PrlVmDevSound_SetOutputDev(m_hDev, dev->get_name().c_str());
		if (ret)
			return prl_err(ret, "PrlVmDevSound_SetOutputDev: %s",
				get_error_str(ret).c_str());
	}
	if (!param.mixer.empty()) {
		if (!(dev = m_vm.find_srv_dev(DEV_SOUND, param.mixer)))
			return prl_err(-1, "Unknown device: %s",
				param.device.c_str());
		ret = PrlVmDevSound_SetMixerDev(m_hDev, dev->get_name().c_str());
		if (ret)
			return prl_err(ret, "PrlVmDevSound_SettMixerDev: %s",
				get_error_str(ret).c_str());
	}
	if (param.output.empty() && param.mixer.empty())
		return 0;
	if ((ret = set_emu_type(PDT_USE_REAL_DEVICE)))
		return ret;

	prl_log(0, "Creating %s.", get_info().c_str());
	set_updated();
	return 0;
}

int PrlDevSound::create(const DevInfo &param)
{
	const PrlDev *dev = m_vm.find_dev(m_devType, 0);

	if (dev && dev != this)
		return prl_err(-1, "The sound0 device already exists.");
	if (!param.disable)
		set_enable(true);

	return configure(param);
}

int PrlDevSound::configure(const DevInfo &param)
{
	PRL_RESULT ret;

	if (param.enable)
		set_enable(true);
	if (param.disable)
		set_enable(false);
	if (param.connect)
		set_connect(true);
	if (param.disconnect)
		set_connect(false);
	if ((ret = set_device(param)))
		return ret;

	return 0;
}

void PrlDevSound::append_info(PrlOutFormatter &f)
{
	f.open_dev(get_id().c_str());
	f.add_isenabled(is_enable());

	char buf[512];
	unsigned len = sizeof(buf);
	if (!PrlVmDevSound_GetOutputDev(m_hDev, buf, &len))
		f.add("output", buf, true, true);

	len = sizeof(buf);
	if (!PrlVmDevSound_GetMixerDev(m_hDev, buf, &len))
		f.add("mixer", buf, true, true);

	if (!is_connected())
		f.add("state", "disconnected", true);

	f.close(true);
};

/********************** PrlDevGenericPci ***********************************/
int PrlDevGenericPci::set_device(const DevInfo &param)
{
	PRL_RESULT ret;
	PRL_VM_DEV_EMULATION_TYPE type;
	PRL_DEVICE_TYPE dtype;
	PrlDevSrv *dev;

	if (param.device.empty())
		return 0;
	if (!(dev = m_vm.find_srv_dev(DEV_GENERIC_PCI, param.device)))
		return prl_err(-1, "Unknown pci device: %s",
			param.device.c_str());

	if ((ret = PrlVmDev_SetSysName(m_hDev, dev->get_id().c_str())))
		return prl_err(ret, "PrlVmDev_SetSysName: %s",
			get_error_str(ret).c_str());
	if ((ret = PrlVmDev_SetFriendlyName(m_hDev, dev->get_name().c_str())))
		return prl_err(ret, "PrlVmDev_SetFriendlyName: %s",
			get_error_str(ret).c_str());

	if ((ret = PrlVmDev_GetType(m_hDev, &dtype)))
		return prl_err(ret, "PrlVmDev_GetType: %s",
			get_error_str(ret).c_str());
	if (dtype == PDE_GENERIC_NETWORK_ADAPTER)
		type = PDT_USE_DIRECT_ASSIGN;
	else
		type = PDT_USE_REAL_DEVICE;
	if ((ret = set_emu_type(type)))
		return ret;

	prl_log(0, "Creating %s.", get_info().c_str());
	set_updated();
	return 0;
}

int PrlDevGenericPci::create(const DevInfo &param)
{
	if (param.device.empty())
		return prl_err(-1, "The --device parameter is not specified.");

	if (!param.disable)
		set_enable(true);

	return configure(param);
}

int PrlDevGenericPci::configure(const DevInfo &param)
{
	int ret;

	if (param.enable)
		set_enable(true);
	if (param.disable)
		set_enable(false);
	if ((ret = set_device(param)))
		return ret;

	return 0;
}

void PrlDevGenericPci::append_info(PrlOutFormatter &f)
{
	f.open_dev(get_id().c_str());
	f.add_isenabled(is_enable());

	f.add("friendly_name", get_fname(), true, true, true);
	f.add("sys_name", get_sname(), true, true, true);

	if (!is_connected())
		f.add("state", "disconnected", true);

	f.close(true);

};

/*******************************************************************/
DevType str2devtype(const std::string &str)
{
	if (str == "hdd")
		return DEV_HDD;
	else if (str == "hdd-part")
		return DEV_HDD;
	else if (str == "cdrom")
		return DEV_CDROM;
	else if (str == "fdd")
		return DEV_FDD;
	else if (str == "net")
		return DEV_NET;
	else if (str == "usb")
		return DEV_USB;
	else if (str == "serial")
		return DEV_SERIAL;
	else if (str == "parallel")
		return DEV_PARALLEL;
	else if (str == "sound")
		return DEV_SOUND;
	else if (str == "pci")
		return DEV_GENERIC_PCI;
	else
		return DEV_NONE;
}

const char *devtype2str(DevType type)
{
	switch (type) {
	case DEV_HDD:
		return "hdd";
	case DEV_HDD_PARTITION:
		return "hdd-part";
	case DEV_CDROM:
		return "cdrom";
	case DEV_FDD:
		return "fdd";
	case DEV_NET:
		return "net";
	case DEV_USB:
		return "usb";
	case DEV_SERIAL:
		return "serial";
	case DEV_PARALLEL:
		return "parallel";
	case DEV_SOUND:
		return "sound";
	case DEV_GENERIC_PCI:
		return "pci";
	case DEV_NONE:
		return "unknown";
	}
	return "unknown";
}
