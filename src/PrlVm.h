/*
 * @file PrlVm.h
 *
 * VM management
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

#ifndef __PRLVM_H__
#define __PRLVM_H__

#include "PrlTypes.h"
#include "PrlDev.h"
#include "CmdParam.h"

class PrlSrv;
class PrlSharedFolder;


class PrlBootEntry {
public:
	unsigned int m_index;
	bool m_inuse;
	const PrlDev *m_dev;

public:
	PrlBootEntry(unsigned int index, bool inuse, const PrlDev *dev) :
		m_index(index), m_inuse(inuse), m_dev(dev)
	{}
};

typedef PrlList<PrlDev *> PrlDevList;
typedef std::vector<PrlDevNet *> PrlDevNetList;

class PrlOutFormatter;

class PrlVm
{
private:
	PrlSrv &m_srv;
	PRL_HANDLE m_hVm;
	std::string m_uuid;
	std::string m_name;
	std::string m_home;
	unsigned int m_ostype;
	bool m_template;
	std::string m_owner;
	PrlDevList m_DevList;
	PrlDevNetList m_DevNetList;
	VIRTUAL_MACHINE_STATE m_VmState;
	bool m_updated;
	PRL_BOOL m_is_vnc_server_started;
	std::string m_current_password;
	std::string m_ctid;
	bool m_efi_boot;
	bool m_select_boot_dev;
	std::string m_ext_boot_dev;
	str_list_t m_confirmation_list;

public:
	PrlVm(PrlSrv &srv, PRL_HANDLE hVm, const std::string &uuid,
		const std::string &name, unsigned int ostype);
	PRL_VM_TYPE get_vm_type() const;
	PRL_HANDLE get_handle() const { return m_hVm; }
	const PrlSrv &get_srv() const { return m_srv; }
	void set_updated() { m_updated = true; }
	bool is_updated() const { return m_updated; }
	int start(int mode, int mask);
	int mount(int flags);
	int mount_info();
	int umount();
	int change_sid();
	int reset_uptime();
	int auth(const std::string &user_name, const std::string &user_password);
	int stop(const CmdParamData &param);
	int reset();
	int restart();
	int suspend();
	int resume();
	int pause(bool acpi = false);
	int reg_event_callback(PRL_EVENT_HANDLER_PTR fn, void *data);
	void unreg_event_callback(PRL_EVENT_HANDLER_PTR fn, void *data);
	int snapshot_create(const SnapshotParam &param);
	int snapshot_switch_to(const SnapshotParam &param);
	int snapshot_delete(const SnapshotParam &param);
	int snapshot_get_tree(std::string &out);
	int snapshot_list(const CmdParamData &param);
	int problem_report(const CmdParamData &param);
	int screenshot(const CmdParamData &param);
	int console();
	int enter();
	int exec(const CmdParamData &param);
	int set_userpasswd(const std::string &userpasswd, bool crypted);
	int internal_cmd(char **argv);
	PrlDevList &get_devs()
	{
		return m_DevList;
	}
	const PrlDevNetList &get_net_devs() const
	{
		return m_DevNetList;
	}

	int update_state();
	void set_state(VIRTUAL_MACHINE_STATE state) {m_VmState = state;}
	int reg(const std::string &location, PRL_UINT32 nFlags = 0);
	int clone(const std::string &name, const std::string &uuid,
			const std::string &location, unsigned int flags);
	int unreg();
	int destroy(const CmdParamData &param);
	int set_boot_dev(const PrlDev *dev, int bootindex, bool inuse = true) const;
	unsigned int get_cpu_cores() const;
	unsigned get_cpu_sockets() const;
	PRL_VM_ACCELERATION_LEVEL get_cpu_acc_level() const;
	PRL_CPU_MODE get_cpu_mode() const;
	bool is_cpu_hotplug_enabled() const;
	int set_cpu_hotplug(const std::string &value);
	int set_cpu_cores(unsigned value_);
	int set_cpu_sockets(unsigned value_);
	int set_cpuunits(unsigned int cpuunits);
	int get_cpuunits(unsigned int *cpuunits);
	int set_cpumask(const std::string &mask);
	std::string get_cpumask();
	int set_nodemask(const std::string& mask);
	int get_nodemask(std::string& mask);
	int set_cpulimit(PRL_CONST_CPULIMIT_DATA_PTR cpulimit);
	int get_cpulimit(PRL_CPULIMIT_DATA_PTR cpulimit);
	int get_cpulimitmode(unsigned int *limittype);
	int set_distribution(const OsDistribution *dist);
	int set_ioprio(unsigned int ioprio);
	int get_ioprio(unsigned int *ioprio);
	int set_iolimit(unsigned int iolimit);
	int get_iolimit(unsigned int *iolimit);
	int set_iopslimit(unsigned int limit);
	int get_iopslimit(unsigned int *limit);
	unsigned int get_memsize() const;
	int set_memsize(unsigned int num);
	bool is_mem_hotplug_enabled() const;
	int set_mem_hotplug(int value);
	const std::string &get_owner() const { return m_owner; }
	unsigned int get_videosize() const;
	int set_videosize(unsigned int num);
	int get_3d_acceleration() const;
	int set_3d_acceleration(int mode);
	bool is_vertical_sync_enabled() const;
	bool is_high_resolution_enabled() const;
	int set_vertical_sync(int enabled);
	void get_memguarantee(PrlOutFormatter &f);
	int set_memguarantee(const CmdParamData &param);
	int set_desc(const std::string &desc);
	std::string get_desc() const;
	bool is_tools_autoupdate_enabled() const;
	int set_autostart(const std::string &mode);
	std::string get_autostart_info() const;
	int set_autostart_delay(unsigned int delay);
	int set_autostop(const std::string &mode);
	int set_autocompact(int enabled);
	bool get_autocompact();
	std::string get_autostop_info() const;
	int set_startup_view(const std::string &mode);
	std::string get_startup_view_info() const;
	int set_on_crash(const std::string &mode);
	std::string get_on_crash_info() const;
	int set_on_shutdown(const std::string &mode);
	std::string get_on_shutdown_info() const;
	int set_on_window_close(const std::string &mode);
	std::string get_on_window_close_info() const;
	std::string get_system_flags();
	int set_system_flags(const std::string &flags);
	int set_name(const std::string &name);
	FeaturesParam get_features();
	PRL_VM_LOCATION get_location();
	int set_features(const FeaturesParam &features);
	int set_cap(const CapParam &capability);
	int set_ct_resources(const CmdParamData &param);
	std::string get_hostname();
	std::string get_nameservers();
	std::string get_searchdomains();
	int set_net(const CmdParamData &param);
	bool is_vtx_enabled() const;
	bool is_template() const;
	int set(const CmdParamData &param);
	PRL_VM_TOOLS_STATE get_tools_state(std::string &version) const;
	void get_tools_info(PrlOutFormatter &f);
	void get_virt_printers_info(PrlOutFormatter &f);
	void get_usb_info(PrlOutFormatter &f);
	void get_optimization_info(PrlOutFormatter &f);
	void get_security_info(PrlOutFormatter &f);
	void get_expiration_info(PrlOutFormatter &f);
	int set_confirmations_list(const std::map<std::string , bool >& cmd_list);
	int switch_lock_edit_settings(int on_off, const std::string& host_admin);
	int install_tools();
	int migrate(const MigrateParam &param);
	int update();
	int move(const std::string &location);
	int load_def_configuration(const OsDistribution *dist);
	int load_configuration(const std::string &sample);
	int apply_configuration(const std::string &sample);
	VIRTUAL_MACHINE_STATE get_state() const { return m_VmState; };
	const std::string &get_uuid() const { return m_uuid; }
	const std::string &get_id() const { return get_uuid(); }
	const std::string &get_ctid() const { return m_ctid; }
	const std::string &get_name() const { return m_name; }
	const std::string &get_home() const { return m_home; }
	int create_dev(DevType type, const DevInfo &param);
	std::string get_dist() const;
	std::string get_ostemplate() const;
	int get_home_dir(std::string &dir) const;
	int get_dev_info();
	int get_vm_info();
	int get_confirmation_list();
	int get_new_dir(const char *pattern, std::string &new_dir,
		const char *dir = 0) const;
	void clear();
	PrlDevSrv *find_srv_dev(DevType type, const std::string &name) const;
	PrlDev *find_dev(DevType type, unsigned int idx) const;
	PrlDev *find_dev(const std::string &sname) const;
	int get_uptime(PRL_UINT64 *uptime, std::string &start_date);
	int get_real_ip(ip_list_t &ips, unsigned int timeout = JOB_INFINIT_WAIT_TIMEOUT);
	std::string get_netdev_name();
	bool get_ha_enable() const;
	unsigned int get_ha_prio() const;
	void get_high_availability_info(PrlOutFormatter &f);
	void append_net_shaping_info(PrlOutFormatter &f);
	void append_configuration(PrlOutFormatter &f);
	int validate_config(PRL_VM_CONFIG_SECTIONS section) const;
	~PrlVm();
	int set_templates(str_list_t lstTemplates);
	int load_config();
	const char *get_vm_type_str() const;
	const char *get_vm_type_str(PRL_VM_TYPE type_) const;
	int set_template_sign(int template_sign);
	int set_nested_virt(int enabled);
	int reinstall(const CmdParamData &param);
	int monitor();
	std::string get_backup_path() const;
	int set_backup_path(const std::string &path);
	PRL_CHIPSET_TYPE get_chipset_type() const;
	int set_chipset_type(const PRL_CHIPSET_TYPE type_);
	PRL_UINT32 get_chipset_version() const;
	int set_chipset_version(const PRL_UINT32 version_);
	const char *get_chipset_type_str() const;

private:
	PrlDev *new_dev(PRL_HANDLE hDev, DevType type, unsigned int idx);
	int fixup_configuration();
	unsigned int new_dev_idx(DevType type) const;
	std::string get_bootdev_info();
	int set_vnc(const VncParam &param);
	VncParam get_vnc_param();
	void get_vnc_info(PrlOutFormatter &f);
	int get_boot_list(PrlList<PrlBootEntry *> &bootlist,
		unsigned int &max_index) const;
	int add_boot_entry(const PrlBootEntry *entry, unsigned int index) const;
	const PrlBootEntry *find_boot_entry(
		const PrlList<PrlBootEntry *> &bootlist,
		const PrlDev *dev) const;
	void del_boot_entry(PrlList<PrlBootEntry *> &bootlist, const PrlDev *dev);
	int del_boot_list();
	int set_boot_list(const CmdParamData &param);
	int update_owner(PRL_HANDLE hVmInfo);
	int commit_configuration(const CmdParamData &param);
	int set_rate(const rate_list_t &rate);
	int attach_backup_disk(const std::string& id, const std::string& disk,
		const DevInfo *param);
	int attach_backup_disks(const CmdParamData& param);
	int detach_backup_disks(const std::string& id);
	void search_attached_backups(std::list<PrlDevHdd *>& disks);
	int commit_encryption(const CmdParamData &param);
	int get_current_snapshot_id(std::string& output);
};

#endif // __PRLVM_H__
