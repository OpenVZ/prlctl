/*
 * @file PrlSrv.h
 *
 * Server managenment
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

#ifndef __PRLSRV_H__
#define __PRLSRV_H__
#include <string>
#include "PrlVm.h"
#include "PrlDisp.h"
#include "PrlTypes.h"
#include "PrlOutFormatter.h"

class LoginInfo;
class CmdParamData;
class PrlVm;
class PrlDevSrv;
class PrlBackupTree;

const unsigned int DEFAULT_LOGOFF_TIMEOUT = 60000;

class PrlUser {
public:
	std::string m_name;
	std::string m_uuid;
	std::string m_home;
	unsigned int m_session_count;
	bool m_manage_srv_settings;

public:
	PrlUser(const char *name, const char *uuid, const char *home) :
		m_name(name), m_uuid(uuid), m_home(home),
		m_session_count(0), m_manage_srv_settings(false)
	{}
	const std::string &get_name() const { return m_name; }
	const std::string &get_uuid() const { return m_uuid; }
	const std::string &get_def_home() const { return m_home; }
	unsigned int get_session_count() const { return m_session_count; }
};

class PrlLic
{
public:
	unsigned long long set_fields;
	int m_state;
	std::string m_name;
	std::string m_company;
	std::string m_key;
	std::string m_hwid;
	std::string m_key_number;
	std::string m_expiration_date;
	std::string m_start_date;
	std::string m_update_date;
	std::string m_product;
	unsigned int m_grace_period;
	unsigned int m_cpu_total;
	unsigned int m_max_memory;
	unsigned int m_vms_total;
	bool m_rku_allowed;
	bool m_ha_allowed;
	std::string m_node_hwid;

public:
#define PRL_LIC_UNLIM_VAL               (0xFFFF)
	PrlLic() : set_fields(0),
		m_state(PRL_ERR_LICENSE_NOT_VALID),
		m_grace_period(0),
		m_cpu_total(PRL_LIC_UNLIM_VAL),
		m_max_memory(PRL_LIC_UNLIM_VAL),
		m_vms_total(PRL_LIC_UNLIM_VAL),
		m_rku_allowed(false),
		m_ha_allowed(false)
	{}

	void append_info(PrlOutFormatter &f)
	{
		std::string out;

		f.open("License", true);
		if (PRL_LICENSE_IS_VALID(m_state)) {
			if (m_state == PRL_ERR_LICENSE_GRACED)
				f.add("state", "graced", true, true);
			else
				f.add("state", "valid", true, true);

			if (!m_name.empty())
				f.add("name", m_name, true, true);

			if (!m_company.empty())
				f.add("company", m_company, true, true);

			if (!m_key.empty())
				f.add("key", m_key, true, true);

		} else {
			f.add("state", "not installed", true, true);
		}
		f.close(true);

		f.add("Hardware Id", m_node_hwid, false, true);
	}
};

typedef PrlList<PrlVm *> PrlVmList ;
typedef PrlList<PrlDevSrv *> PrlDevSrvList;
typedef std::list<std::pair<PRL_GUEST_OS_SUPPORT_TYPE, PRL_UINT16> > DistList;

class PrlSrv
{
private:
	PRL_HANDLE m_hSrv;
	PrlHandle m_hSrvConf;
	bool m_logged;
	unsigned int m_logoffTimeout;
	PrlVmList m_VmList;
	PrlDevSrvList m_DevList;
	PrlVNetList m_VNetList;
	PrlPrivNetList m_PrivNetList;
	unsigned int m_cpus;
	std::string m_uuid;
	std::string m_sessionid;
	int	m_run_via_launchd;
	std::string m_product_version;
	std::string m_hostname;
	PrlDisp *m_disp;

public:
	PrlSrv() : m_hSrv(PRL_INVALID_HANDLE),
			   m_logged(false),
			   m_logoffTimeout(DEFAULT_LOGOFF_TIMEOUT),
			   m_cpus(0),
			   m_run_via_launchd(-1)
	{
		m_disp = new PrlDisp(*this);
	}
	PrlDisp *get_disp() { return m_disp; };
	int login(const LoginInfo &login);
	void logoff();
	int run_action(const CmdParamData &param);
	int run_disp_action(const CmdParamData &param);
	int get_new_dir(const char *dir, const char *pattern,
		std::string &new_dir_name) const;
	int get_vm_config(const std::string &id, PrlVm **vm, bool ignore_not_found = false,
		int nFlags = PGVC_SEARCH_BY_UUID | PGVC_SEARCH_BY_NAME );
	int get_vm_config(const CmdParamData &param, PrlVm **vm,
		bool ignore_not_found = false);
	PrlDevSrv *find_dev(DevType type, const std::string &name);
	PrlDevSrv *find_net_dev_by_mac(const std::string &mac);
	PrlDevSrv *find_net_dev_by_idx(unsigned int idx, bool is_virtual);
	const char *get_uuid() const { return m_uuid.c_str(); }
	unsigned int get_cpu_count();
	PRL_HANDLE get_handle() const { return m_hSrv; }
	const char *get_sessionid() const { return m_sessionid.c_str(); }
	unsigned int get_min_security_level() const;
	PRL_HANDLE get_srv_config_handle();
	void append_info(PrlOutFormatter &f);
	int install_license(const std::string &key, const std::string &name,
			const std::string &org);
	int update_license();
	int get_default_vm_location(std::string &location);
	int reg_event_callback(PRL_EVENT_HANDLER_PTR fn, void *data = NULL);
	void unreg_event_callback(PRL_EVENT_HANDLER_PTR fn, void *data = NULL);
	int fill_vnetworks_list(PrlVNetList &list) const;
	int find_vnetwork_handle_by_mac_and_vlan(const std::string &mac,
			unsigned short vlanTag, PrlHandle &hVirtNet);
	int find_vnetwork_handle_by_name(const std::string &name,
			PrlHandle &hVirtNet);
	int find_vnetwork_handle_by_ifname(const std::string &iface,
			PrlHandle &hVirtNet);
	int find_vnetwork_handle(const VNetParam &vnet, PrlHandle &hVirtNet);
	int fill_vnetwork_handle(const VNetParam &vnet, PrlHandle &hVirtNet);
	int fill_vnetwork_nat_rules(const VNetParam &vnet, PrlHandle &hVirtNet);
	void print_boundto_bridged(const PrlHandle *phVirtNet,
			const std::string &netId, PrlOutFormatter &f,
			const VNetParam &vnet);
	void print_boundto_host_only(const PrlHandle *phVirtNet,	
			const std::string &netId,
			PrlOutFormatter &f, const VNetParam &vnet);
	void print_vnetwork_info(const PrlHandle *phVirtNet,
			PrlOutFormatter &f, const VNetParam &vnet);
	void print_nat_info(const PrlHandle& hVirtNet,	PrlOutFormatter &f);
	int vnetwork(const VNetParam &param, bool use_json);
	int vnetwork_list(const VNetParam &vnet, bool use_json);
	int get_ip(const PrlHandle *phPrivNet, ip_list_t &ips);
	int fill_priv_networks_list(PrlPrivNetList &list) const;
	int find_priv_network_handle(const PrivNetParam &privnet, PrlHandle &hPrivNet);
	int fill_priv_network_handle(const PrivNetParam &privnet, PrlHandle &hPrivNet);
	void print_priv_network_info(const PrlHandle *phPrivNet, PrlOutFormatter &f);
	int priv_network(const PrivNetParam &param, bool use_json);
	int appliance_install(const CmdParamData &param);
	int ct_templates(const CtTemplateParam &param, bool use_json);
	int copy_ct_template(const CtTemplateParam &tmpl, const CopyCtTemplateParam &copy_tmpl);
	int monitor();
	void set_logoff_timeout(unsigned int timeout) { m_logoffTimeout = timeout; }
	~PrlSrv();
	int get_backup_disks(const std::string& id, std::list<std::string>& disks);
	int print_dist_info(const CmdParamData &param);

private:
	int get_srv_info();
	int list_vm(const CmdParamData &param);
	int list_user(const CmdParamData &param, bool use_json);
	int set_user(const CmdParamData &param);
	int create_vm(const CmdParamData &param);
	int create_ct(const CmdParamData &param);
	int register_vm(const CmdParamData &param);
	int convert_vm(const CmdParamData &param);
	int do_vm_abackup(const PrlVm& vm, const BackupParam& param);
	int do_vm_backup(const PrlVm& vm, const CmdParamData& param,
			const PrlSrv &storage);
	int backup_vm(const CmdParamData& param);
	int backup_node(const CmdParamData &param);
	int restore_vm(const CmdParamData &param);
	int backup_delete(const CmdParamData &param);
	int backup_list(const CmdParamData &param);
	int do_get_backup_tree(const std::string& id, const std::string& server, int port,
		const std::string& dir, const std::string& session_id, unsigned int flags,
		PrlBackupTree &tree);
	int get_backup_tree(const CmdParamData &param, PrlBackupTree &tree, PrlSrv &storage);
	int get_hw_info(ResType type);
	int get_hw_dev_info();
	int get_hw_dev_info(DevType type);
	int get_user_info(PrlList<PrlUser *> &users);
	int vm_from_result(PrlHandle &hResult, int idx, PrlVm **vm);
	int get_vm_list(PrlList<PrlVm *> &list, unsigned vmtype, unsigned list_flags);
	int update_vm_list(unsigned vmtype, unsigned flags = PGVLF_FILL_AUTOGENERATED);
	int problem_report(const CmdParamData &param);
	int shutdown(bool force, bool suspend_vm_to_pram);
	PrlLic get_lic_info();
	PrlLic & get_verbose_lic_info(PrlLic &lic, PrlHandle &hLicInfo);
	void append_lic_info(PrlOutFormatter &f);
	void append_lic_verbose_info(PrlOutFormatter &f);
	const PrlVm *find_dev_in_use(PrlDevSrv *dev);
	void append_hw_info(PrlOutFormatter &f);
	void append_slave_ifaces(PrlOutFormatter &f, const std::string& netId, bool detailed);
	int print_info(bool is_license_info, bool use_json);
	void clear();
	int status_vm(const CmdParamData &param);
	int print_statistics(const CmdParamData &param, PrlVm *vm = NULL) ;
	int get_server_launch_mode(PrlHandle& hResponse);

	void print_ct_template_info(const PrlHandle *phTmpl, size_t width, PrlOutFormatter &f);
	int fill_ct_templates_list(PrlCtTemplateList &list) const;
	int convert_handle_to_event(PRL_HANDLE h, PRL_HANDLE* phEvent);
	int restart_shaping();
	int get_supported_os_info(DistList& info);
	int get_os_type_info(PRL_HANDLE hOsesMatrix, PRL_UINT8 nOsType, DistList& info);
	void print_dist_list(const DistList& info, PRL_GUEST_OS_SUPPORT_TYPE type);
	std::string load_rsa_public_key();
	std::string get_user_keys_directory();
};

int server_event_handler_monitor(PRL_HANDLE hEvent, void *data);
#endif // __PRLSRV_H__
