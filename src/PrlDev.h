///////////////////////////////////////////////////////////////////////////////
///
/// @file PrlDev.h
///
/// Devices managenment
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

#ifndef __PRLDEV_H__
#define __PRLDEV_H__

#include <string>
#include <list>

#include "PrlTypes.h"
#include "CmdParam.h"
#include "Utils.h"

DevType str2devtype(const std::string &str);
const char *devtype2str(DevType type);

class PrlSrv;
class PrlVm;

class PrlOutFormatter;

class PrlDevSrv {
public:
	const PrlSrv &m_srv;
	PRL_HANDLE m_hDev;
	DevType m_devType;
	std::string m_name;
	std::string m_id;
	PrlDevSrv *m_parent;
	std::string m_mac;
	PRL_UINT16 m_vlanTag;

public:
	PrlDevSrv(const PrlSrv &srv, PRL_HANDLE hDev, DevType type) :
		m_srv(srv), m_hDev(hDev), m_devType(type), m_parent(0)
	{}
	PRL_HANDLE get_handle() const { return m_hDev; }
	const std::string &get_name();
	const std::string &get_id();
	DevType get_type() const { return m_devType; }
	void set_parent(PrlDevSrv *parent) { m_parent = parent; }
	unsigned int get_idx() const;
	int get_assignment_mode() const;
	int set_assignment_mode(int mode);
	int is_connected() const;
};


class PrlDev {
public:
	const PrlVm &m_vm;
	PRL_HANDLE m_hDev;
	DevType m_devType;
	std::string m_id;
	unsigned int m_idx;
	bool m_updated;

public:
	PrlDev(const PrlVm &vm, PRL_HANDLE hDev, DevType type,
		unsigned int idx);
	void set_updated() { m_updated = true; }
	bool is_updated() const { return m_updated; }
	int set_enable(bool flag);
	bool is_enable() const;
	int set_connect(bool flag);
	PRL_VM_DEV_EMULATION_TYPE get_emu_type() const;
	int set_emu_type(PRL_VM_DEV_EMULATION_TYPE type);
	PRL_MASS_STORAGE_INTERFACE_TYPE get_iface_type() const;
	PRL_CLUSTERED_DEVICE_SUBTYPE get_subtype() const;
	int remove();
	DevType get_type() const { return m_devType; }
	unsigned int get_idx() const { return m_idx; }
	void set_idx(unsigned int idx);
	const std::string &get_id() const { return m_id; }
	const std::string &get_name() const { return get_id(); }
	std::string get_fname() const;
	int set_fname(const std::string& name);
	int set_fname2(const std::string& name);
	int set_sname(const std::string& name);
	std::string get_sname() const;
	int set_position(unsigned int position);
	unsigned int get_position() const;
	virtual int create(const DevInfo &param) = 0;
	virtual int configure(const DevInfo &param) = 0;
	virtual int connect(const std::string &name);
	virtual int disconnect(const std::string &name);
	std::string get_info();
	virtual void append_info(PrlOutFormatter &f) = 0;
	int is_connected() const;
	virtual ~PrlDev()
	{}
};

class PrlDevHdd : public PrlDev {
public:
	PrlDevHdd(const PrlVm &vm, PRL_HANDLE hDev, DevType type,
			unsigned int idx) :
		PrlDev(vm, hDev, type, idx)
	{
	}
	int create(const DevInfo &param);
	int configure(const DevInfo &param);
	void append_info(PrlOutFormatter &f);
	void get_storage_url(StorageUrl& url) const;

	virtual ~PrlDevHdd()
	{}
private:

	enum info_spec {
		INFO_SHOW_TYPE = 1 << 0,
		INFO_SHOW_SIZE = 1 << 1
	};

	void append_info_spec(PrlOutFormatter &f, unsigned int spec);
	int set_iface(const std::string &iface);
	int set_subtype(const std::string &subtype);
	int create_image(const DevInfo &param);
	int resize_image(const DevInfo &param);
	int set_device(const DevInfo &param);
};

class PrlDevCdrom : public PrlDev {
public:
	PrlDevCdrom(const PrlVm &vm, PRL_HANDLE hDev, DevType type,
			unsigned int idx) :
		PrlDev(vm, hDev, type, idx)
	{}
	int create(const DevInfo &param);
	int configure(const DevInfo &param);
	void append_info(PrlOutFormatter &f);

	virtual ~PrlDevCdrom()
	{}

private:
	int set_iface(const std::string &iface);
	int set_subtype(const std::string &subtype);
	int set_device(const DevInfo &param);
};

class PrlDevNet : public PrlDev {
public:
	PrlDevNet(const PrlVm &vm, PRL_HANDLE hDev, DevType type,
			unsigned int idx) :
		PrlDev(vm, hDev, type, idx)
	{
	}
	int create(const DevInfo &param);
	int configure(const DevInfo &param);
	int get_ip(ip_list_t &ips);
	void append_info(PrlOutFormatter &f);
	std::string get_mac();

	std::string get_veth_name() const;
	virtual ~PrlDevNet()
	{}

private:
	int set_mac(const std::string &mac);
	int set_iface(const std::string &iface);
	int find_vnet(const std::string &network_id);
	int set_vnet(const std::string &vnet);
	int set_device(DevMode mode, const std::string &iface,
		const std::string &mac);
	int set_device(const std::string &vnet,
		const std::string &mac);
	int build_ip(const NetParam &net, ip_list_t &ips);
	int set_network(const DevInfo& param);
	int set_firewall(const NetParam &net);
	std::string get_vnetwork();
};

class PrlDevUsb : public PrlDev {
public:
	PrlDevUsb(const PrlVm &vm, PRL_HANDLE hDev, DevType type,
			unsigned int idx) :
		PrlDev(vm, hDev, type, idx)
	{
		/* Usb devisa has no indexes. set name 'usb' */
		m_id = ::devtype2str(type);
	}
	int create(const DevInfo &param);
	int connect(const std::string &name);
	int disconnect(const std::string &name);
	int configure(const DevInfo &param);
	void append_info(PrlOutFormatter &f);

	virtual ~PrlDevUsb()
	{}
};

class PrlDevFdd : public PrlDev {
public:
	PrlDevFdd(const PrlVm &vm, PRL_HANDLE hDev, DevType type,
			unsigned int idx) :
		PrlDev(vm, hDev, type, idx)
	{
	}
	int create(const DevInfo &param);
	int configure(const DevInfo &param);
	void append_info(PrlOutFormatter &f);

	virtual ~PrlDevFdd()
	{}

private:
	int create_image(const DevInfo &param);
	int set_device(const DevInfo &param);
};

class PrlDevSerial : public PrlDev {
public:
	PrlDevSerial(const PrlVm &vm, PRL_HANDLE hDev, DevType type,
			unsigned int idx) :
		PrlDev(vm, hDev, type, idx)
	{
	}
	int create(const DevInfo &param);
	int configure(const DevInfo &param);
	void append_info(PrlOutFormatter &f);

	virtual ~PrlDevSerial()
	{}

private:
	int set_device(const DevInfo &param);
};

class PrlDevParallel : public PrlDev {
public:
	PrlDevParallel(const PrlVm &vm, PRL_HANDLE hDev, DevType type,
			unsigned int idx) :
		PrlDev(vm, hDev, type, idx)
	{
	}
	int create(const DevInfo &param);
	int configure(const DevInfo &param);
	void append_info(PrlOutFormatter &f);

	virtual ~PrlDevParallel()
	{}

private:
	int set_device(const DevInfo &param);
};

class PrlDevSound: public PrlDev {
public:
	PrlDevSound(const PrlVm &vm, PRL_HANDLE hDev, DevType type,
			unsigned int idx) :
		PrlDev(vm, hDev, type, idx)
	{
	}
	int create(const DevInfo &param);
	int configure(const DevInfo &param);
	void append_info(PrlOutFormatter &f);

	virtual ~PrlDevSound()
	{}

private:
	int set_device(const DevInfo &param);
};

class PrlDevGenericPci: public PrlDev {
public:
	PrlDevGenericPci(const PrlVm &vm, PRL_HANDLE hDev, DevType type,
			unsigned int idx) :
		PrlDev(vm, hDev, type, idx)
	{
	}
	int create(const DevInfo &param);
	int configure(const DevInfo &param);
	void append_info(PrlOutFormatter &f);

	virtual ~PrlDevGenericPci()
	{}
private:
	int set_device(const DevInfo &param);

};

#endif
