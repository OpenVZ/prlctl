/*
 * @file CmdParam.h
 *
 * Processing prlctl command line options
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

#ifndef __CMDPARAM_H__
#define __CMDPARAM_H__
#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include "PrlTypes.h"
#include "GetOpt.h"
#include <PrlIOStructs.h>
#include <boost/optional.hpp>

typedef PRL_UINT32 cap_t;

enum Action {
	VmCreateAction,
	VmStartAction,
	VmStopAction,
	VmMountAction,
	VmUmountAction,
	VmSuspendAction,
	VmResumeAction,
	VmPauseAction,
	VmListAction,
	VmDestroyAction,
	VmRegisterAction,
	VmUnregisterAction,
	VmCloneAction,
	VmSetAction,
	VmResetAction,
	VmRestartAction,
	VmInstallToolsAction,
	VmCaptureAction,
	VmMigrateAction,
	VmMoveAction,
	VmBackupAction,
	VmRestoreAction,
	VmBackupListAction,
	VmBackupDeleteAction,
	VmPerfStatsAction,
	VmProblemReportAction,
	VmEnterAction,
	VmConsoleAction,
	VmExecAction,
	VmChangeSidAction,
	VmResetUptimeAction,
	VmUpdateVmAction,

	SrvShutdownAction,
	SrvHwInfoAction,
	SrvUsrInfoAction,
	SrvUsrListAction,
	SrvUsrSetAction,
	SrvInstallLicenseAction,
	SrvDeferredLicenseAction,
	SrvUpdateLicenseAction,
	SrvPerfStatsAction,
	SrvProblemReportAction,
	SrvVNetAction,
	SrvPrivNetAction,
	SrvInstApplianceAction,
	SrvCtTemplateAction,
	SrvCopyCtTemplateAction,
	SrvMonitorAction,
	SrvUpdateHostRegInfoAction,
	SrvPrepareForUninstallAction,
	SrvBackupNodeAction,
	SrvShapingRestartAction,

	DispSetAction,
	DispListAction,
	DispUpListeningInterfaceAction,
	DispStartNatDetectAction,
	DispUsbAction,

	VmSnapshot_Create,
	VmSnapshot_SwitchTo,
	VmSnapshot_Delete,
	VmSnapshot_List,

	VmAuthAction,
	VmStatusAction,
	VmInternalCmd,

	VmConvertAction,
	VmReinstallAction,
	VmMonitorAction,

	InvalidAction
};

enum Command {
	None,
	Add,
	Del,
	Set,
	Connect,
	Disconnect,
	BootOrder,
};

class LoginInfo {
public:
	std::string user;
	std::string server;
	int port;

	LoginInfo() : port(0) {};

	std::string &get_passwd_buf() {
		passwds_stack.push_back( std::string() );
		return passwds_stack.back();
	}

	std::string get_passwd_from_stack(bool &ok) const {
		std::string passwd;
		ok = false;
		if (passwds_stack.size()) {
			passwd = passwds_stack.front();
			ok = true;
			passwds_stack.erase(passwds_stack.begin());
		}
		return passwd;
	}

private:
	mutable std::vector<std::string> passwds_stack;
};

struct NetParam {
	std::string ifname;
	str_list_t searchdomain;
	str_list_t nameserver;
	ip_list_t ip;
	ip_list_t ip_del;
	std::string gw;
	std::string gw6;
	int dhcp;
	int dhcp6;
	bool delall_ip;
	bool set_ip;
	int configure;
	int configure_iponly;
	int ip_filter;
	int prevent_promisc;
	int mac_filter;
	int fw_enable;
	int fw_policy;
	int fw_direction;
	fw_rule_list_t fw_rules;

	PRL_VM_NET_ADAPTER_TYPE adapter_type;

	NetParam() : dhcp(-1), dhcp6(-1), delall_ip(false), set_ip(false),
		configure(-1),
		configure_iponly(-1),
		ip_filter(-1),
		prevent_promisc(-1),
		mac_filter(-1),
		fw_enable(-1),
		fw_policy(-1),
		fw_direction(-1),
		adapter_type(PNT_UNDEFINED)
	{}

	bool is_updated() const
	{
	    if (ip_filter != -1 ||
		prevent_promisc != -1 ||
		mac_filter != -1 ||
		configure != -1 ||
		configure_iponly != -1 ||
		dhcp != -1 ||
		dhcp6 != -1 ||
		!gw.empty() ||
		!gw6.empty() ||
		!ip.empty() ||
		!ip_del.empty() ||
		delall_ip ||
		!searchdomain.empty() ||
		!nameserver.empty() ||
		!ifname.empty()	||
		adapter_type != PNT_UNDEFINED ||
		fw_enable != -1 || fw_policy != -1)
		return true;

	    return false;
	}
};

enum {
	AM_NONE		= -1,
	AM_HOST		= PGS_CONNECTED_TO_HOST,
	AM_VM		= PGS_CONNECTED_TO_VM,
};

enum EncryptionAction {
	ENC_NONE,
	ENC_SET,
	ENC_ENCRYPT,
	ENC_DECRYPT,
};

class DevInfo {
public:
	Command cmd;
	bool enable;
	bool disable;
	bool connect;
	bool disconnect;
	bool recreate;

	bool split;
	DevMode mode;
	int passthr;
	unsigned int size;
	int position;
	DevType type;
	std::string device;
	boost::optional<std::string> image;
	boost::optional<std::string> mnt;
	std::string name;
	std::string iface;
	std::string subtype;
	std::string mac;
	std::string vnetwork;
	std::string output;
	std::string socket;
	std::string socket_tcp;
	std::string socket_udp;
	int socket_mode;
	std::string mixer;
	NetParam net;
	unsigned int hdd_block_size;
	int no_fs_resize;
	int autocompact;
	bool offline;
	std::string storage_url;
	std::string serial_number;
	std::string enc_keyid;
	unsigned int enc_flags;
	EncryptionAction enc_action;

public:
	DevInfo() :
		cmd(None),
		enable(false),
		disable(false),
		connect(false),
		disconnect(false),
		recreate(false),
		split(false),
		mode(DEV_TYPE_NONE),
		passthr(-1),
		size(0),
		position(-1),
		type(DEV_NONE),
		socket_mode(-1),
		hdd_block_size(0),
		no_fs_resize(0),
		autocompact(-1),
		offline(false),
		enc_flags(0),
		enc_action(ENC_NONE)
	{}

	bool is_updated() const
	{
		if (	connect ||
			disconnect ||
			recreate ||
			split ||
			mode != DEV_TYPE_NONE ||
			passthr != -1 ||
			size != 0 ||
			position != -1 ||
			image ||
			//!name.empty() ||
			!iface.empty() ||
			!mac.empty() ||
			!vnetwork.empty() ||
			!output.empty() ||
			!socket.empty() ||
			!mixer.empty() ||
			autocompact != -1 ||
			!storage_url.empty() ||
			!serial_number.empty())
			return true;

		return false;
	}
};

class SharedFolderParam {
public:
	Command cmd;
	std::string name;
	std::string path;
	std::string desc;
	std::string mode;
	bool enable;
	bool disable;

public:
	SharedFolderParam() : cmd(None), enable(true), disable(false)
	{}
};

class UserParam {
public:
	std::string def_vm_home;
	int mng_settings;

public:
	UserParam() :
		mng_settings(-1)
	{}
};

struct OfflineSrvParam {
	std::string name;
	int port;
	bool used_by_default;
	bool del;

	OfflineSrvParam():
		port(-1),
		used_by_default(false),
		del(false)
	{}
};

struct NetworkClassParam {
	std::string net;
	unsigned int net_class;
	int del;

	NetworkClassParam():
		net_class(0),
		del(0)
	{}
};

struct NetworkShapingParam {
	std::string dev;
	unsigned int net_class;
	unsigned int totalrate;
	unsigned int rate;
	int del;
	int enable;

	NetworkShapingParam():
		net_class(0),
		totalrate(0),
		rate(0),
		del(0),
		enable(-1)
	{}
};

struct NetworkBandwidthParam {
	std::string dev;
	unsigned int bandwidth;
	int del;

	NetworkBandwidthParam():
		bandwidth(0),
		del(0)
	{}
};

class DispParam {
public:
	unsigned int mem_limit;
	int allow_mng_settings;
	std::string min_security_level;
	bool force;
	bool suspend_vm_to_pram;
	std::string device;
	int assign_mode;
	int cep_mechanism;
	int cluster_mode;
	std::string backup_path;
	unsigned int backup_timeout;
	bool change_backup_settings;
	LoginInfo def_backup_storage;
	std::string listen_interface;
	int verbose_log_level;
	bool info_license;
	OfflineSrvParam offline_service;
	NetworkBandwidthParam network_bandwidth;
	NetworkShapingParam network_shaping;
	NetworkClassParam network_class;
	int log_rotation;
	std::string cpu_features_mask_changes;
	boost::optional<std::string> backup_tmpdir;
	int adv_security_mode;
	int allow_attach_screenshots;
	int lock_edit_settings;
	std::string host_admin;
	std::map<std::string , bool > cmd_require_pwd_list;
	bool set_vnc_encryption;
	std::string vnc_public_key;
	std::string vnc_private_key;
	int vm_cpulimit_type;
	std::string vcmmd_policy;

public:
	DispParam() :
		mem_limit(0),
		allow_mng_settings(-1),
		force(false),
		suspend_vm_to_pram(false),
		assign_mode(AM_NONE),
		cep_mechanism(-1),
		backup_timeout(0),
		change_backup_settings(false),
		verbose_log_level(-1),
		info_license(false),
		log_rotation(-1),
		adv_security_mode(-1),
		allow_attach_screenshots(-1),
		lock_edit_settings(-1),
		set_vnc_encryption(false),
		vm_cpulimit_type(-1)
	{}
};

struct XmlRpcParam {
	std::string manager_url;
	std::string user_login;
	std::string data_key;
	int data_version;
	std::string user_data;
	std::string query;
	bool use_json;
	bool action_provided;
	std::string icon_path;
	std::set<std::string> hashes_list;

	XmlRpcParam() :
		data_version(0),
		use_json(false),
		action_provided(false)
	{}
};

class MigrateParam {
public:
	std::string dst_id;
	LoginInfo dst;
	std::string vm_location;
	std::string sessionid;
	unsigned int security_level;
	unsigned int flags;
	str_list_t ssh_opts;

public:
	MigrateParam():
		security_level(0),
		flags(0) {}
};

struct BackupParam {
	std::string id;
	std::string name;
	std::string vm_location;
	std::string path;
	LoginInfo login;
	LoginInfo storage;
	unsigned int flags;
	bool list_full;
	bool list_local_vm;
	bool abackup;
	std::string dst;
	std::string uuid;

	BackupParam() : flags(0), list_full(false), list_local_vm(false), abackup(false) {}
};

struct SnapshotParam {
	std::string id;
	std::string name;
	std::string desc;
	bool tree;
	bool wait;
	bool del_with_children;
	bool skip_resume;

	SnapshotParam()
		: tree(false),
		  wait(false),
		  del_with_children(false),
		  skip_resume(false)
	{}
};

struct StatisticsParam {
	std::string filter ;
	bool        loop ;

	StatisticsParam():loop(false) {}
};

struct ProblemReportParam {
	bool send ;
	bool full ;
	std::string proxy_settings ;
	bool dont_use_proxy ;
	bool stand_alone;
	std::string user_name;
	std::string user_email;
	std::string description;

	ProblemReportParam():send(false), full(false), dont_use_proxy(false), stand_alone(false) {}
};

class PrlOutFormatter;
struct VncParam {
	int		mode;
	PRL_UINT32	port;
	PRL_UINT32	ws_port; // read-only
	std::string	address;
	std::string	passwd;
	int		nopasswd;

	VncParam() : mode(-1), port(0), ws_port(0), nopasswd(0) {}
	std::string get_info() const;
	void append_info(PrlOutFormatter &f) const;
	int set_mode(const std::string &str);
	std::string mode2str() const;
};

enum {
	FT_AutoCaptureReleaseMouse	= 0x00001,
	FT_ShareClipboard		= 0x00002,
	FT_TimeSynchronization		= 0x00004,
	FT_TimeSyncSmartMode		= 0x00008,
	FT_SharedProfile		= 0x00010,
	FT_UseDesktop			= 0x00020,
	FT_UseDocuments			= 0x00040,
	FT_UsePictures			= 0x00080,
	FT_UseMusic			= 0x00100,
	FT_TimeSyncInterval		= 0x00200,
	FT_SmartGuard			= 0x00400,
	FT_SmartGuardNotify		= 0x00800,
	FT_SmartGuardInterval		= 0x01000,
	FT_SmartGuardMaxSnapshots	= 0x02000,
	FT_SmartMount			= 0x04000,
	FT_SmartMountRemovableDrives	= 0x08000,
	FT_SmartMountDVDs		= 0x10000,
	FT_SmartMountNetworkShares	= 0x20000,
};

struct FeaturesParam {
	unsigned long mask;
	unsigned long known;
	unsigned int time_sync_interval;
	unsigned int smart_guard_interval;
	unsigned int smart_guard_max_snapshots;
	int type;

	FeaturesParam() : mask(0), known(0), time_sync_interval(0),
		smart_guard_interval(0), smart_guard_max_snapshots(0),
		type(-1)
	{}
};

#define NUMCAP 33

struct CapParam {
	cap_t mask_on;
	cap_t mask_off;

	CapParam() : mask_on(0), mask_off(0)
	{}

	bool empty() const
	{
		return (mask_on == 0 && mask_off == 0);
	}

	void set_mask_on(unsigned int flag)
	{
		mask_on |= (1 << flag);
	}

	void set_mask_off(unsigned int flag)
	{
		mask_off |= (1 << flag);
	}
};

namespace Netfilter
{

struct Mode
{

	Mode() : name("not set"), id(PCNM_NOT_SET) {
	}

	Mode(const std::string& name_, PRL_NETFILTER_MODE id_) : name(name_), id(id_) {
	}

	bool isValid() const {
		return id != PCNM_NOT_SET && name != "not set";
	}

	std::string name;
	PRL_NETFILTER_MODE id;
};

} //namespace Netfilter

struct VNetParam {
	enum Command {
		Add,
		Set,
		Del,
		List,
		Info,
	};

	unsigned cmd;
	std::string vnet;
	std::string description;
	std::string ip_scope_start;
	std::string ip_scope_end;
	std::string ip6_scope_start;
	std::string ip6_scope_end;
	nat_rule_list_t nat_tcp_add_rules;
	nat_rule_list_t nat_udp_add_rules;
	str_list_t	nat_tcp_del_rules;
	str_list_t	nat_udp_del_rules;
	struct ip_addr host_ip;
	struct ip_addr host_ip6;
	int dhcp_enabled;
	int dhcp6_enabled;
	std::string dhcp_ip;
	std::string dhcp_ip6;
	std::string ifname;
	int type;
	bool is_shared;
	std::string mac;
	bool no_slave;
public:
	VNetParam() :
		dhcp_enabled(-1),
		dhcp6_enabled(-1),
		type(-1),
		is_shared(false),
		no_slave(false)
	{}
	bool detailed() const { return cmd == VNetParam::Info; }
};

struct PrivNetParam {
	enum Command {
		Add,
		Set,
		Del,
		List,
	};

	unsigned cmd;
	std::string name;
	ip_list_t ip;
	ip_list_t ip_del;
	int is_global;
public:
	PrivNetParam() : is_global(-1) {}
};

struct UsbParam {
	enum Command {
		List,
		Set,
		Delete
	};

	unsigned cmd;
	std::string name;
	std::string id;
};

struct CtTemplateParam {
	enum Command {
		List,
		Remove,
	};

	unsigned cmd;
	std::string name;
	std::string os_name;
public:
	CtTemplateParam() : cmd(-1) {}
};

class CopyCtTemplateParam {
public:
	LoginInfo dst;
	bool force;
	unsigned int security_level;

public:
	CopyCtTemplateParam() : force(false), security_level(0)  {}
};

struct PluginParam {
	enum Command {
		List,
		Refresh,
	};

	unsigned cmd;
public:
	PluginParam() : cmd(-1) {}
};

struct ExpirationParam {

	int enabled;
	std::string date;
	std::string time_server;
	int time_check;
	int offline_time;
	std::string note;
	bool note_edit;

public:
	ExpirationParam()
		: enabled(-1)
		, time_check(-1)
		, offline_time(-1)
		, note_edit(false)
	{}
};

struct DistListParam {
	bool print;
	bool error;

public:
	DistListParam() : print(false), error(false)
	{}
};

class CmdParamData {
public:
	std::string id;
	std::string original_id;
	Action action;
	LoginInfo login;
	bool fast;
	/* stop param */
	bool use_acpi;
	bool force;
	bool noforce;
	/* create param */
	std::string config_sample;
	std::string vm_location;
	const OsDistribution *dist;
	std::string ostemplate;
	unsigned int reinstall_opts;
	bool nohdd;
	unsigned int hdd_block_size;
	bool lion_recovery;
	/* register param */
	bool preserve_uuid;
	bool preserve_src_uuid;
	bool ignore_ha_cluster;
	/* clone param */
	std::string new_name;
	bool tmpl;
	unsigned int clone_flags;
	/* device options */
	DevInfo dev;
	unsigned int commit_flags;
	/* dispatcher options*/
	DispParam disp;
	/* user managenment options */
	UserParam user;
	/* virtual networks options */
	VNetParam vnet;
	/* IP private networks options */
	PrivNetParam privnet;
	/* Usb permanent assignement */
	UsbParam usb;
	/* container templates options */
	CtTemplateParam ct_tmpl;
	/* list of application templates */
	str_list_t app_templates;
	/* container templates copy options */
	CopyCtTemplateParam copy_ct_tmpl;
	/* plugin options */
	PluginParam plugin;
	ct_resource_list_t ct_resource;

	/* list param */
	std::string list_field;
	bool list_no_hdr;
	bool list_all;
	bool list_stopped;
	bool list_name;
	bool info;
	bool use_json;
	bool list_all_fields;
	std::string list_sort;

	/* VM param */
	boost::optional<unsigned> cpu_cores;
	boost::optional<unsigned> cpu_sockets;
	std::string cpu_hotplug;
	unsigned int cpuunits;
	PRL_CPULIMIT_DATA cpulimit;
	unsigned int ioprio;
	std::string cpumask;
	std::string nodemask;
	unsigned int iolimit;
	unsigned int iopslimit;
	unsigned int memsize;
	unsigned int videosize;
	int v3d_accelerate;
	int vertical_sync;
	int high_resolution;
	int mem_hotplug;
	PRL_MEMGUARANTEE_DATA memguarantee;
	bool memguarantee_set;
	boost::optional<std::string > desc;
	std::string uuid;
	std::string autostart;
	unsigned int autostart_delay;
	std::string autostop;
	std::string file;
	std::string startup_view;
	std::string on_crash;
	std::string on_shutdown;
	std::string on_window_close;
	std::string name;
	int is_template;
	int smart_mouse_optimize;
	int sticky_mouse;
	int keyboard_optimize;
	int sync_host_printers;
	int sync_default_printer;
	int auto_share_camera;
	int auto_share_bluetooth;
	int support_usb30;
	int efi_boot;
	bool restrict_editing;
	int select_boot_dev;
	std::string ext_boot_dev;
	int winsystray_in_macmenu;
	int auto_switch_fullscreen;
	int disable_aero;
	int hide_min_windows;
	int pwd_to_exit_fullscreen;
	int pwd_to_change_vm_state;
	int pwd_to_manage_snapshots;
	int pwd_to_change_guest_pwd;
	int lock_on_suspend;
	int isolate_vm;
	int smart_guard;
	int sg_notify_before_create;
	unsigned int sg_interval;
	unsigned int sg_max_snapshots;
	int faster_vm;
	int adaptive_hypervisor;
	int auto_compress;
	int nested_virt;
	int pmu_virt;
	int longer_battery_life;
	int battery_status;

	/* License options */
	std::string key;
	std::string company;

	/* Snapshot options */
	SnapshotParam snapshot;

	/* Migrate options */
	MigrateParam migrate;

	/* Statistics options */
	StatisticsParam statistics;

	/* Problem report options */
	ProblemReportParam problem_report;

	std::string system_flags;

	//Auth cmd params
	std::string user_name;
	std::string user_password;

	VncParam vnc;
	FeaturesParam features;
	CapParam cap;
	Netfilter::Mode netfilter;
	str_list_t nameserver;
	str_list_t searchdomain;
	std::string hostname;
	str_list_t off_srv;
	std::string userpasswd;
	bool crypted;
	int off_man;
	char **argv;
	BackupParam backup;
	unsigned int security_level;

	//Use default answers mech sign
	int use_default_answers;
	int tools_autoupdate;
	int smart_mount;
	bool batch;
	rate_list_t rate;
	int ratebound;
	unsigned vmtype;

	// dry_run mech
	bool dry_run;
	int mnt_opts;
	int mnt_info;
	int start_opts;
	int apply_iponly;

	/* XML RPC options*/
	XmlRpcParam xmlrpc;

	/* High Availability Cluster */
	int ha_enable;
	long long ha_prio;
	ExpirationParam expiration;

	int template_sign;
	int autocompact;

	Command backup_cmd;
	std::string backup_id;
	std::string backup_disk;

	bool exec_in_shell;
	DistListParam dist_list;

public:
	CmdParamData() :
		action(InvalidAction),
		login(LoginInfo()),
		fast(false),
		use_acpi(false),
		force(false),
		noforce(false),
		dist(0),
		reinstall_opts(0),
		nohdd(false),
		hdd_block_size(0),
		lion_recovery(false),
		preserve_uuid(false),
		preserve_src_uuid(true),
		ignore_ha_cluster(false),
		tmpl(false),
		clone_flags( PCVF_DETACH_EXTERNAL_VIRTUAL_HDD),
		commit_flags(PVCF_WAIT_FOR_APPLY | PVCF_DETACH_HDD_BUNDLE),
		list_no_hdr(false),
		list_all(false),
		list_stopped(false),
		list_name(false),
		info(false),
		use_json(false),
		list_all_fields(false),
		cpuunits(0),
		ioprio((unsigned int) -1),
		iolimit((unsigned int) -1),
		iopslimit((unsigned int) -1),
		memsize(0),
		videosize(0),
		v3d_accelerate(-1),
		vertical_sync(-1),
		high_resolution(-1),
		mem_hotplug(-1),
		memguarantee_set(false),
		autostart_delay(-1),
		is_template(-1),
		smart_mouse_optimize(-1),
		sticky_mouse(-1),
		keyboard_optimize(-1),
		sync_host_printers(-1),
		sync_default_printer(-1),
		auto_share_camera(-1),
		auto_share_bluetooth(-1),
		support_usb30(-1),
		efi_boot(-1),
		restrict_editing(false),
		select_boot_dev(-1),
		winsystray_in_macmenu(-1),
		auto_switch_fullscreen(-1),
		disable_aero(-1),
		hide_min_windows(-1),
		pwd_to_exit_fullscreen(-1),
		pwd_to_change_vm_state(-1),
		pwd_to_manage_snapshots(-1),
		pwd_to_change_guest_pwd(-1),
		lock_on_suspend(-1),
		isolate_vm(-1),
		smart_guard(-1),
		sg_notify_before_create(-1),
		sg_interval(0),
		sg_max_snapshots(0),
		faster_vm(-1),
		adaptive_hypervisor(-1),
		auto_compress(-1),
		nested_virt(-1),
		pmu_virt(-1),
		longer_battery_life(-1),
		battery_status(-1),
		crypted(false),
		off_man(-1),
		argv(0),
		security_level(0),
		use_default_answers(-1),
		tools_autoupdate(-1),
		smart_mount(-1),
		batch(false),
		ratebound(-1),
		vmtype(PVTF_VM | PVTF_CT),
		dry_run(false),
		mnt_opts(0),
		mnt_info(0),
		start_opts(0),
		apply_iponly(-1),
		ha_enable(-1),
		ha_prio(-1),
		template_sign(-1),
		autocompact(-1),
		backup_cmd(None),
		exec_in_shell(true)
	{
		cpulimit.value = 0;
		cpulimit.type = (PRL_CPULIMIT_TYPE)0;
		memguarantee.value = 0;
		memguarantee.type = PRL_MEMGUARANTEE_AUTO;
	}
	int check_consistence(int id) const;
	bool get_realpath(std::string &path, bool check = true);
	bool is_valid();
	int set_cpu_hotplug(const std::string &str);
	int set_autostart(const std::string &str);
	int set_autostop(const std::string &str);
	int set_dev_mode(const std::string &str);
	int set_features(const std::string &str);
	int set_cap(const std::string &str);
	bool set_net_param(NetParam &param);
	void set_device_del_flags(unsigned int flags);

private:
	bool is_dev_valid() const;
	bool validate_encryption() const;
};

struct Option;
class cmdParam
{
public:
	cmdParam() {};
	CmdParamData get_vm(int argc, char **argv);
	CmdParamData get_disp(int argc, char **argv);

private:
	CmdParamData get_param(int argc, char **argv, Action action,
		const Option *options, int offset);
	CmdParamData get_disp_param(int argc, char **argv, Action action,
		const Option *options, int offset);
	CmdParamData get_migrate_param(int argc, char **argv, Action action,
		const Option *options, int offset);
	CmdParamData get_backup_param(int argc, char **argv, Action action,
		const Option *options, int offset);
	CmdParamData get_backup_delete_param(int argc, char **argv, Action action,
		const Option *options, int offset);
	CmdParamData get_restore_param(int argc, char **argv, Action action,
		const Option *options, int offset);
	CmdParamData get_backup_list_param(int argc, char **argv, Action action,
		const Option *options, int offset);
	CmdParamData get_statistics_param(int argc, char **argv, Action action,
		const Option *options, int offset) ;
	CmdParamData get_problem_report_param(int argc, char **argv,
		Action action, const Option *options, int offset) ;
	CmdParamData get_vnet_param(int argc, char **argv, unsigned cmd,
		int offset);
	CmdParamData get_privnet_param(int argc, char **argv, unsigned cmd,
		int offset);
	CmdParamData parse_vnet_args(int argc, char **argv, int offset);
	CmdParamData parse_tc_args(int argc, char **argv, int i);
	CmdParamData parse_privnet_args(int argc, char **argv, int offset);
	CmdParamData parse_usb_args(int argc, char **argv, int offset);
	CmdParamData parse_ct_template_args(int argc, char **argv, int i);
	CmdParamData parse_monitor_args(int argc, char **argv);
	CmdParamData get_xmlrpc_param(int argc, char **argv, Action action,
		const Option *options, int offset);
	CmdParamData parse_backup_node_args(int argc, char **argv,
		const Option *options, int offset);
	int parse_memguarantee(const char *value, CmdParamData &param);
	CmdParamData parse_backup_args(int argc, char **argv, Action action,
		const Option *options, int offset);
};

enum cmdOptions {
	CMD_DUMMY = 255, /* skip reserved ids */
	CMD_VZCOMPAT,
	CMD_FAST,
	CMD_USE_ACPI,
	CMD_NOFORCE,
	CMD_VERBOSE,
	CMD_TIMEOUT,
	CMD_LOGIN,
	CMD_PRESERVE_UUID,
	CMD_REGENERATE_SRC_UUID,
	CMD_LIST_FIELD,
	CMD_LIST_ALL,
	CMD_LIST,
	CMD_LIST_NO_HDR,
	CMD_LIST_SORT,
	CMD_LIST_STOPPED,
	CMD_LIST_NAME,
	CMD_LIST_ALL_FIELDS,

	CMD_CONFIG,
	CMD_LOCATION,
	CMD_OSTYPE,
	CMD_OSTEMPLATE,
	CMD_DIST,
	CMD_NAME,
	CMD_UUID,
	CMD_INFO,
	CMD_USE_JSON,
	CMD_INFO_FULL,

	CMD_DEVICE_ADD,
	CMD_NETIF_ADD,
	CMD_DEVICE_SET,
	CMD_DEVICE_DEL,
	CMD_NETIF_DEL,
	CMD_BOOTORDER,
	CMD_DEVICE_DISCONNECT,
	CMD_DEVICE_CONNECT,
	CMD_DEVICE,
	CMD_IMAGE,
	CMD_MNT,
	CMD_RECREATE,
	CMD_DEV_TYPE,
	CMD_ADAPTER_TYPE,
	CMD_SIZE,
	CMD_NO_FS_RESIZE,
	CMD_OFFLINE,
	CMD_SPLIT,
	CMD_ENABLE,
	CMD_DISABLE,
	CMD_CONNECT,
	CMD_DISCONNECT,
	CMD_CONNECT_DEV,
	CMD_DISCONNECT_DEV,
	CMD_IFACE,
	CMD_SUBTYPE,
	CMD_VNETWORK,
	CMD_PASSTHR,
	CMD_POSITION,
	CMD_CPUS,
	CMD_CPU_SOCKETS,
	CMD_CPU_HOTPLUG,
	CMD_CPUUNITS,
	CMD_CPULIMIT,
	CMD_CPUMASK,
	CMD_NODEMASK,
	CMD_IOPRIO,
	CMD_IOLIMIT,
	CMD_IOPSLIMIT,
	CMD_MEMSIZE,
	CMD_VIDEOSIZE,
	CMD_3D_ACCELERATE,
	CMD_VERTICAL_SYNC,
	CMD_HIGH_RESOLUTION,
	CMD_MEM_HOTPLUG,
	CMD_MEMQUOTA,
	CMD_MEMGUARANTEE,
	CMD_OUTPUT,
	CMD_SOCKET,
	CMD_SOCKET_TCP,
	CMD_SOCKET_UDP,
	CMD_SOCKET_MODE,
	CMD_MIXER,
	CMD_DESC,
	CMD_VM_NAME,
	CMD_VM_RENAME_EXT_DISKS,
	CMD_TEMPLATE,
	CMD_PASSWD,
	CMD_AUTOSTART,
	CMD_AUTOSTART_DELAY,
	CMD_AUTOSTOP,
	CMD_STARTUP_VIEW,
	CMD_ON_CRASH,
	CMD_ON_SHUTDOWN,
	CMD_ON_WINDOW_CLOSE,
	CMD_UNDO_DISKS,
	CMD_TMPL,
	CMD_CHANGE_SID,
	CMD_LINKED_CLONE,
	CMD_DETACH_EXTERNAL_HDD,
	CMD_ONLINE_CLONE,
	CMD_MAC,
	CMD_FILE,
	CMD_BATCH,
	CMD_KEY,
	CMD_COMPANY,
	CMD_MEMORY_LIMIT,
	CMD_SMART_MOUSE_OPTIMIZE,
	CMD_STICKY_MOUSE,
	CMD_KEYBOARD_OPTIMIZE,
	CMD_SYNC_HOST_PRINTERS,
	CMD_SYNC_DEFAULT_PRINTER,
	CMD_AUTO_SHARE_CAMERA,
	CMD_AUTO_SHARE_BLUETOOTH,
	CMD_SUPPORT_USB30,
	CMD_EFI_BOOT,
	CMD_SELECT_BOOT_DEV,
	CMD_EXT_BOOT_DEV,

	CMD_USER_DEF_VM_HOME,
	CMD_USER_MNG_SETTINGS,
	CMD_MIN_SECURITY_LEVEL,

	CMD_SNAPSHOT_ID,
	CMD_SNAPSHOT_NAME,
	CMD_SNAPSHOT_DESC,
	CMD_SNAPSHOT_LIST_TREE,
	CMD_SNAPSHOT_WAIT,
	CMD_SNAPSHOT_CHILDREN,
	CMD_SNAPSHOT_SKIP_RESUME,

	CMD_FORCE,
	CMD_SUSPEND_VM_TO_PRAM,
	CMD_SESSIONID,
	CMD_SECURITY_LEVEL,
	CMD_LOOP,
	CMD_PERF_FILTER,
	CMD_FLAGS,

	CMD_FASTER_VM,
	CMD_ADAPTIVE_HYPERVISOR,
	CMD_DISABLE_WINLOGO,
	CMD_AUTO_COMPRESS,
	CMD_NESTED_VIRT,
	CMD_PMU_VIRT,
	CMD_LONGER_BATTERY_LIFE,
	CMD_BATTERY_STATUS,

	CMD_WINSYSTRAY_IN_MACMENU,
	CMD_AUTO_SWITCH_FULLSCREEN,
	CMD_DISABLE_AERO,
	CMD_HIDE_MIN_WINDOWS,
	CMD_LOCK_ON_SUSPEND,
	CMD_ISOLATE_VM,
	CMD_SMART_GUARD,
	CMD_SG_NOTIFY_BEFORE_CREATE,
	CMD_SG_INTERVAL,
	CMD_SG_MAX_SNAPSHOTS,
	CMD_EXPIRATION,

	CMD_VNC_MODE,
	CMD_VNC_PORT,
	CMD_VNC_PASSWD,
	CMD_VNC_NOPASSWD,
	CMD_VNC_ADDRESS,

	CMD_HELP,

	CMD_USERNAME,
	CMD_USERPASSWORD,

	CMD_FEATURES,

	CMD_SET_CAP,

	CMD_IFNAME,
	CMD_SEARCHDOMAIN,
	CMD_HOSTNAME,
	CMD_NAMESERVER,
	CMD_IP_SET,
	CMD_IP_ADD,
	CMD_IP_DEL,
	CMD_GW,
	CMD_GW6,
	CMD_DHCP,
	CMD_DHCP6,
	CMD_CONFIGURE,
	CMD_APPLY_IPONLY,
	CMD_IP_FILTER,
	CMD_MAC_FILTER,
	CMD_PREVENT_PROMISC,
	CMD_OFF_MAN,
	CMD_OFF_SRV,
	CMD_USERPASSWD,
	CMD_HOST_ADMIN,
	CMD_CRYPTED,

	CMD_NETFILTER,

	CMD_ASSIGNMENT,

	CMD_BACKUP_FULL,
	CMD_BACKUP_LIST_FULL,
	CMD_BACKUP_INC,
	CMD_BACKUP_DIFF,
	CMD_BACKUP_STORAGE,
	CMD_BACKUP_ID,
	CMD_BACKUP_PATH,
	CMD_BACKUP_TMPDIR,
	CMD_UNCOMPRESSED,
	CMD_NO_REVERSED_DELTA,
	CMD_BACKUP_TIMEOUT,
	CMD_BACKUP_KEEP_CHAIN,
	CMD_VM_ID,
	CMD_VM_CPULIMIT_TYPE,

	CMD_SEND_PROBLEM_REPORT,
	CMD_DUMP_PROBLEM_REPORT,
	CMD_DUMP_FULL_PROBLEM_REPORT,
	CMD_USE_PROXY,
	CMD_DONT_USE_PROXY,
	CMD_CREATE_PROBLEM_REPORT_WITHOUT_SERVER,
	CMD_PROBLEM_REPORT_USER_NAME,
	CMD_PROBLEM_REPORT_USER_EMAIL,
	CMD_PROBLEM_REPORT_DESCRIPTION,

	CMD_VNET_DESCRIPTION,
	CMD_VNET_IFACE,
	CMD_VNET_NEW_NAME,
	CMD_VNET_TYPE,
	CMD_VNET_MAC,
	CMD_VNET_IP_SCOPE_START,
	CMD_VNET_IP_SCOPE_END,
	CMD_VNET_IP6_SCOPE_START,
	CMD_VNET_IP6_SCOPE_END,
	CMD_VNET_NAT_TCP_ADD,
	CMD_VNET_NAT_UDP_ADD,
	CMD_VNET_NAT_TCP_DEL,
	CMD_VNET_NAT_UDP_DEL,
	CMD_VNET_HOST_IP,
	CMD_VNET_HOST_IP6,
	CMD_VNET_DHCP_ENABLED,
	CMD_VNET_DHCP6_ENABLED,
	CMD_VNET_DHCP_IP,
	CMD_VNET_DHCP_IP6,
	CMD_VNET_NO_SLAVE,

	CMD_PRIVNET_IPADD,
	CMD_PRIVNET_IPDEL,
	CMD_PRIVNET_GLOBAL,

	CMD_USE_DEFAULT_ANSWERS,
	CMD_CEP_MECH_SETTINGS,
	CMD_VERBOSE_LOG_LEVEL,
	CMD_TOOLS_AUTOUPDATE,
	CMD_LIST_LOCAL_VM,
	CMD_SMART_MOUNT,
	CMD_LOG_ROTATION,
	CMD_ALLOW_ATTACH_SCREENSHOTS,
	CMD_REQUIRE_PWD,
	CMD_LOCK_EDIT_SETTINGS,
	CMD_ADVANCED_SECURITY_MODE,

	CMD_INFO_LICENSE,
	CMD_UPDATE_OFFLINE_SERVICE,
	CMD_DEL_OFFLINE_SERVICE,
	CMD_ADD_NETWORK_CLASS,
	CMD_DEL_NETWORK_CLASS,
	CMD_ADD_SHAPING_ENTRY,
	CMD_DEL_SHAPING_ENTRY,
	CMD_SHAPING_ENABLE,
	CMD_ADD_BANDWIDTH_ENTRY,
	CMD_DEL_BANDWIDTH_ENTRY,

	CMD_RATE,
	CMD_RATEBOUND,
	CMD_VMTYPE,

	CMD_LISTEN_INTERFACE,

	CMD_ISCSI_TARGET,

	CMD_APPTEMPLATE,

	CMD_USB_LIST,
	CMD_USB_SET,
	CMD_USB_DELETE,

	CMD_CLONE_MODE,
	CMD_REMOVE_BUNDLE,
	CMD_SWITCH_TEMPLATE,
	CMD_IGNORE_EXISTING_BUNDLE,
	CMD_MNT_OPT,
	CMD_MNT_INFO,
	CMD_FW,
	CMD_FW_POLICY,
	CMD_FW_DIRECTION,
	CMD_FW_RULE,
	CMD_NO_HDD,
	CMD_LION_RECOVERY,
	CMD_HDD_BLOCK_SIZE,
	CMD_CPU_FEATURES_MASK,
	CMD_IGNORE_HA_CLUSTER,

	CMD_DETACH_HDD,
	CMD_DESTROY_HDD,
	CMD_DESTROY_HDD_FORCE,
	CMD_SWAPPAGES,
	CMD_SWAP,
	CMD_QUOTAUGIDLIMIT,
	CMD_WAIT,

	CMD_XMLRPC_MNG_URL,
	CMD_XMLRPC_USER_LOGIN,
	CMD_XMLRPC_PUD_DATAKEY,
	CMD_XMLRPC_PUD_DATAVERSION,
	CMD_XMLRPC_PUD_USERDATA,
	CMD_XMLRPC_GUD_QUERY,
	CMD_XMLRPC_USE_JSON,
	CMD_XMLRPC_PI_ICONPATH,
	CMD_XMLRPC_VIC_ICON_HASH,

	CMD_HA_ENABLE,
	CMD_HA_PRIO,

	CMD_SSH_OPTS,
	CMD_SET_RESTRICT_EDITING,
	CMD_VNC_PUBLIC_KEY,
	CMD_VNC_PRIVATE_KEY,
	CMD_TEMPLATE_SIGN,
	CMD_AUTOCOMPACT,
	CMD_ATTACH_BACKUP_ID,
	CMD_ATTACH_BACKUP_DISK,
	CMD_DETACH_BACKUP_ID,

	CMD_EXEC_NO_SHELL,

	CMD_NO_TUNNEL,
	CMD_SERIAL_NUMBER,

	CMD_ENC_KEYID,
	CMD_ENC_ENCRYPT,
	CMD_ENC_DECRYPT,
	CMD_ENC_REENCRYPT,
	CMD_ENC_NOWIPE,

	CMD_VCMMD_POLICY,
	CMD_SKIP_BACKIP,
	CMD_SKIP_SCRIPTS,
	CMD_RESET_PWDB,
	CMD_RESTORE_LIVE,
	CMD_ABACKUP, 
};

#endif // __CMDPARAM_H__

