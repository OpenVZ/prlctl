/*
 * @file PrlDisp.h
 *
 * Dispatcher configuration managenment
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

#ifndef __PRLDISP_H__
#define __PRLDISP_H__
#include "PrlTypes.h"
#include "CmdParam.h"
#include "PrlOutFormatter.h"

class PrlSrv;

class PrlDisp {
private:
	PrlSrv &m_srv;
	PRL_HANDLE m_hDisp;
	bool m_updated;

public:
	bool m_mem_limit_auto;
	unsigned int m_mem_limit;
	unsigned int m_mem_limit_max;
	std::string m_home;
	PRL_SECURITY_LEVEL m_min_security_level;
	PRL_BOOL m_allow_mng_settings;
	PRL_BOOL m_cep_mechanism;
	PRL_BOOL m_verbose_log_level;
	PRL_BOOL m_allow_multiple_pmc;
	PRL_BOOL m_log_rotation;
	str_list_t m_confirmation_list;
	PRL_UINT32 m_vm_cpulimit_type;

public:
	PrlDisp(PrlSrv &srv) :
		m_srv(srv), m_hDisp(0), m_updated(false),
		m_mem_limit_auto(false), m_mem_limit(0), m_mem_limit_max(0),
		m_min_security_level(PSL_LOW_SECURITY),
		m_allow_mng_settings(PRL_FALSE),
		m_cep_mechanism(PRL_FALSE),
		m_verbose_log_level(PRL_FALSE),
		m_allow_multiple_pmc(PRL_FALSE),
		m_log_rotation(PRL_FALSE),
		m_vm_cpulimit_type(PRL_VM_CPULIMIT_FULL)
	{}
	void append_info(PrlOutFormatter &f);
	int set(const DispParam &param);
	int list(const DispParam &param);
	int usbassign(const UsbParam &param, bool use_json);
	int set_mem_limit(unsigned int limit);
	int set_vcmmd_policy(const std::string &name);
	int set_dev_assign_mode(const std::string &name, int mode);
	int get_security_level(PRL_SECURITY_LEVEL level,
		std::string &out) const;
	int get_security_level(const std::string &level,
		PRL_SECURITY_LEVEL &out) const;
	int update_info();
	int get_confirmation_list();
	int up_listen_interface(const std::string &iface);
	int is_network_shaping_enabled();
	void get_net_shaping_rate_info(std::ostringstream &os);
	~PrlDisp();

private:
	bool is_updated() const { return m_updated; }
	void set_updated() { m_updated = true; }
	int get_config_handle();
	int set_min_security_level(const std::string &level);
	int set_allow_mng_settings(int allow);
	int switch_cep_mechanism(int on_off);
	int switch_verbose_log_level(int on_off);
	std::string get_backup_path();
	std::string get_backup_mode();
	int set_backup_path(const std::string &path);
	int set_backup_mode(PRL_VM_BACKUP_MODE mode);
	int get_backup_timeout(unsigned int *tmo);
	int set_backup_timeout(unsigned int tmo);
	std::string get_def_backup_storage();
	int set_def_backup_storage(const LoginInfo &server);
	std::string get_backup_tmpdir();
	int set_backup_tmpdir(const std::string &tmpdir, bool is_backup_mode_init);
	int update_offline_service(const OfflineSrvParam &offline_service);
	int list_network_classes_config();
	int update_network_classes_config(const NetworkClassParam &param);
	int get_network_shaping_config(PrlHandle &hShaping);
	int list_network_shaping_config();
	int update_network_shaping_list(const NetworkShapingParam &param,
			PrlHandle &hShaping, bool *pupdated);
	int update_network_shaping_config(const DispParam &param);
	int update_network_bandwidth_list(const NetworkBandwidthParam &param,
                PrlHandle &hShaping, bool *pupdated);
	int switch_log_rotation(int on_off);
	std::string get_vm_guest_cpu_limit_type() const;
	int switch_lock_edit_settings(int on_off, const std::string& host_admin);
	int set_confirmations_list(const std::map<std::string , bool >& cmd_list);

	std::string print_unmaskable_features();
	std::string print_masked_features();
	int set_cpu_features_mask(std::string mask_changes);
	std::string get_cpu_features();
	std::string get_cpu_model();
	int set_vnc_encryption(const std::string &public_key, const std::string &private_key);
	int set_vm_cpulimit_type(int vm_cpulimit_type);
	int get_vcmmd_policy(std::string &policy, int nFlags = 0) const;

	void clear();
};

#endif // __PRLDISP_H__
