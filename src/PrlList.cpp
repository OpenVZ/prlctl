/*
 * @file PrlList.cpp
 *
 * VM listing
 *
 * @author igor@
 *
 * Copyright (c) 2005-2017, Parallels International GmbH
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
 * Our contact details: Parallels International GmbH, Vordergasse 59, 8200
 * Schaffhausen, Switzerland.
 */

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <vector>
#include <boost/foreach.hpp>

#include "PrlTypes.h"
#include "Utils.h"
#include "PrlSrv.h"
#include "PrlVm.h"
#include "CmdParam.h"
#include "Logger.h"
#include "PrlOutFormatter.h"

#define PVTF_ALL (PVTF_VM | PVTF_CT)
#define PVTF_HIDE (1<<(PACF_MAX+3))

typedef std::list<int> IntList;

struct FieldVm
{
	const char *name;
	const char *hdr;
	const unsigned int type;
	const char *fmt;

	std::string (*get_fn)(PrlVm *vm);
	bool (*sort_fn)(const PrlVm *val1, const PrlVm *val2);

public:
	int find(const char *name) const;
	IntList get_field_order(const char *fields);
	void print_hdr(IntList &order);
	void print(PrlVm *vm, IntList &list, PrlOutFormatter &f);
};


static const char *default_field_order = "uuid,status,ip_configured,type,name";

static const char *default_vzcompat_field_order = "status,ip,name";
static const char *default_vzcompat_name_field_order = "uuid,numproc,status,ip,name";
static const char *default_template_order = "uuid,dist,type,name";
static const char *default_user_order = "name,mng_settings,def_vm_home";
static bool last_field;


static inline bool uuid_sort_fn(const PrlVm *val1, const PrlVm *val2)
{
	return (val1->get_uuid().compare(val2->get_uuid()) < 0);
}

inline bool name_sort_fn(const PrlVm *val1, const PrlVm *val2)
{
	return (val1->get_name().compare(val2->get_name()) < 0);
}

static std::string handle_empty_str(const std::string &in)
{
	if (in.empty())
		return std::string("-");
	return in;
}

static std::string get_id(PrlVm *vm)
{
	return vm->get_uuid();
}

static std::string get_ctid(PrlVm *vm)
{
	return vm->get_ctid();
}

static std::string get_status(PrlVm *vm)
{
	if (vm->get_state() == VMS_UNKNOWN)
		vm->update_state();
	return vmstate2str(vm->get_state());
}

static std::string get_name(PrlVm *vm)
{
	return handle_empty_str(vm->get_name());
}

static std::string get_dist(PrlVm *vm)
{
	return handle_empty_str(vm->get_dist());
}

static std::string get_owner(PrlVm *vm)
{
	return vm->get_owner();
}

static std::string get_system_flags(PrlVm *vm)
{
	return vm->get_system_flags();
}

static std::string get_description(PrlVm *vm)
{
	return handle_empty_str(vm->get_desc());
}

static std::string get_empty(PrlVm *vm)
{
	if (vm)
		return std::string("-");
	else
		return std::string("");
}

static std::string get_hostname(PrlVm *vm)
{
	return handle_empty_str(vm->get_hostname());
}

// Sort ip in the order: IPv4, IPv6, IPv6Link-Local
static ip_list_t SortNetAddresses(ip_list_t &in)
{
	ip_list_t ips, ips6, ips6local;

	for (ip_list_t::const_iterator it = in.begin(), eit = in.end(); it != eit; it++)
	{
		if (it->ip.find(':') != std::string::npos) {
			if (it->ip.compare(0, 5, "FE80:") == 0)
				ips6local.push_back(*it);
			else
				ips6.push_back(*it);
		} else {
			ips.push_back(*it);
		}
	}
	ips.insert(ips.end(), ips6.begin(), ips6.end());
	ips.insert(ips.end(), ips6local.begin(), ips6local.end());

	return ips;
}

static std::string _get_ip(PrlVm *vm, bool print_real)
{
	std::string out;
	bool net_dev_exists = false;
	ip_list_t ips;

	vm->get_vm_info();

	BOOST_FOREACH(PrlDevNet *net, vm->get_net_devs()) {
		ip_list_t _ips;
		if (net != NULL && net->get_ip(_ips) == 0) {
			net_dev_exists = true;
			ips.insert(ips.end(), _ips.begin(), _ips.end());
		}
	}

	if (print_real)
	{
		ip_list_t _ips;
		if (net_dev_exists &&
		    vm->get_state() == VMS_RUNNING &&
		    vm->get_real_ip(_ips, 60 * 1000) == 0)
		{
			ips.clear();
			ips.insert(ips.end(), _ips.begin(), _ips.end());
		}
	}

	ips = SortNetAddresses(ips);

	if (!last_field) {
		if (ips.size()) {
			out = ips.front().ip;
		}
	} else {
		out += ips.to_str(false);
		out += " ";
	}

	return handle_empty_str(out);
}

static std::string get_ip_configured(PrlVm *vm)
{
	return _get_ip(vm, false);
}

static std::string get_ip(PrlVm *vm)
{
	return _get_ip(vm, true);
}

static std::string get_mac(PrlVm *vm)
{
	vm->get_vm_info();

	std::string out;
	BOOST_FOREACH(PrlDevNet *net, vm->get_net_devs()) {
		if (net != NULL) {
			out += net->get_mac();
			out += " ";
		}
	}
	return handle_empty_str(out);
}

static std::string get_netif(PrlVm *vm)
{
	vm->get_vm_info();

	std::string out;
	BOOST_FOREACH(PrlDevNet *net, vm->get_net_devs()) {
		if (net != NULL) {
			out += net->get_name();
			out += " ";
		}
	}
	return handle_empty_str(out);
}

static std::string get_ostemplate(PrlVm *vm)
{
	if (vm->get_vm_type() == PVT_VM)
		return vm->get_dist();
	else
		return vm->get_ostemplate();
}

static std::string get_features(PrlVm *vm)
{
	return feature2str(vm->get_features());
}

static std::string get_location(PrlVm *vm)
{
	return location2str(vm->get_location());
}

static std::string get_iolimit(PrlVm *vm)
{
	unsigned int iolimit = 0;

	vm->get_iolimit(&iolimit);

	return ui2string(iolimit);
}

static std::string get_netdev(PrlVm *vm)
{
	std::string out;

	out = vm->get_netdev_name();

	return handle_empty_str(out);
}

static std::string get_type(PrlVm *vm)
{
	if(vm)
		switch(vm->get_vm_type())
		{
		case PVT_VM:
			return std::string("VM");
			break;
		case PVT_CT:
			return std::string("CT");
			break;
		}
	return std::string("--");
}

static std::string get_ha_enable(PrlVm *vm)
{
	return vm->get_ha_enable() ? std::string("yes") : std::string("no");
}

static std::string get_ha_prio(PrlVm *vm)
{
	return ui2string(vm->get_ha_prio());
}

static inline bool ha_prio_sort_fn(const PrlVm *val1, const PrlVm *val2)
{
	return (val1->get_ha_prio() < val2->get_ha_prio());
}

static FieldVm vm_field_tbl[] = {
{"uuid",	"UUID", PVTF_ALL, "%-39s ", get_id, uuid_sort_fn},
{"id" ,		"UUID", PVTF_ALL | PVTF_HIDE, "%-39s ", get_id, uuid_sort_fn},
{"ctid" ,	"UUID", PVTF_ALL | PVTF_HIDE, "%-39s ", get_id, uuid_sort_fn},
{"veid" ,	"UUID", PVTF_ALL | PVTF_HIDE, "%-39s ", get_id, uuid_sort_fn},
{"envid" ,	"ENVID", PVTF_ALL, "%-10s ", get_ctid, 0},

{"status",	"STATUS", PVTF_ALL, "%-12s ", get_status, 0},
{"name",	"NAME", PVTF_ALL, "%-32s ", get_name, name_sort_fn},
{"dist",	"DIST", PVTF_ALL, "%-15s ", get_dist, 0},
{"owner",	"OWNER", PVTF_ALL, "%-32s ", get_owner, 0},
{"system-flags","SYSTEM_FLAGS", PVTF_VM, "%-32s ", get_system_flags, 0},
{"description",	"DESCRIPTION", PVTF_ALL, "%-32s ", get_description, 0},

{"numproc",	"NPROC", PVTF_CT, "%9s ", get_empty, 0},
{"ip",		"IP_ADDR", PVTF_ALL, "%-15s ", get_ip, 0},
{"ip_configured","IP_ADDR", PVTF_ALL, "%-15s ", get_ip_configured, 0},
{"hostname",	"HOSTNAME", PVTF_ALL, "%-32s ", get_hostname, 0},
{"netif",	"NETIF", PVTF_ALL, "%-16s ", get_netif, 0},
{"mac",		"MAC", PVTF_ALL, "%-36s ", get_mac, 0},
{"ostemplate",	"OSTEMPLATE", PVTF_ALL | PVTF_HIDE, "%-24s ", get_ostemplate, 0},
{"features",	"FEATURES", PVTF_ALL, "%-256s ", get_features, 0},
{"location",	"LOCATION", PVTF_ALL, "%-39s ", get_location, 0},
{"iolimit",	"IOLIMIT", PVTF_ALL, "%-10s ", get_iolimit, 0},
{"netdev",	"NETDEV", PVTF_ALL, "%-14s ", get_netdev, 0},
{"type",	"T", PVTF_ALL, "%-2s ", get_type, 0},
{"ha_enable", "HA_ENABLE", PVTF_ALL, "%-9s ", get_ha_enable, 0},
{"ha_prio", "HA_PRIO", PVTF_ALL, "%-10s ", get_ha_prio, ha_prio_sort_fn},
{"-",		"-", PVTF_ALL, "%-10s ", get_empty, 0},
{0, 0, PVTF_ALL, 0, 0, 0},

};

/* Skip width specification for last column in case string */
static const char *get_fmt(const char *fmt)
{
	if (last_field && strchr(fmt, 's'))
		return "%s";
	return fmt;
}

int FieldVm::find(const char *name) const
{
	unsigned int i;

	for (i = 0; this[i].name; i++)
		if (!strcmp(this[i].name, name))
			return i;
	return -1;
}

IntList FieldVm::get_field_order(const char *fields)
{
	const char *sp, *ep, *p;
	char name[32];
	int id;
	unsigned int nm_len;
	IntList list;

	sp = fields;
	ep = sp + strlen(sp);
	do {
		if ((p = strchr(sp, ',')) == 0)
			p = ep;
		nm_len = p - sp + 1;
		if (nm_len > sizeof(name) - 1) {
			fprintf(stderr, "The %s field name is unknown.\n", sp);
			break;
		}
		strncpy(name, sp, nm_len);
		name[nm_len - 1] = 0;
		sp = p + 1;
		if ((id = find(name)) < 0) {
			if (!g_vzcompat_mode) {
				fprintf(stderr, "Unknown field: %s\n", name);
				list.clear();
				break;
			}
			/* HACK for vzcompat mode: display unknown field */
			id = find("-");
		}
		list.push_back(id);
	} while (sp < ep);

	return list;
}

void FieldVm::print_hdr(IntList &order)
{
	// Print Header
	IntList::const_iterator it = order.begin(),
				eit = order.end();

	last_field = false;
	while (it != eit) {
		int id = *it;

		if (++it == eit)
			last_field = true;
		fprintf(stdout, get_fmt(this[id].fmt), this[id].hdr);
	}
	fprintf(stdout, "\n");
}

void FieldVm::print(PrlVm *vm, IntList &order, PrlOutFormatter &f)
{
	IntList::const_iterator it = order.begin(),
				eit = order.end();
	last_field = false;
	while (it != eit) {
		int id = *it;

		if (++it == eit)
			last_field = true;

		if (!strcmp(this[id].name, "uuid"))
			f.tbl_add_uuid(this[id].name, get_fmt(this[id].fmt),
							this[id].get_fn(vm).c_str());
		else
			f.tbl_add_item(this[id].name, get_fmt(this[id].fmt),
							this[id].get_fn(vm).c_str());
	}
}

int PrlSrv::list_vm(const CmdParamData &param)
{
	PRL_RESULT ret;
	const char *order_str = 0;
	int sort_fld;
	bool sort_rev = false;
	IntList field_order;
	PrlOutFormatter &f = *(get_formatter(param.use_json));

	if (param.list_all_fields) {
		FieldVm *field;
		field = vm_field_tbl;
		while (field->name) {
			if (!(field->type & PVTF_HIDE) && field->type & param.vmtype)
				printf("%-20s %s\n", field->name, field->hdr);
			++field;
		}
		return 0;
	}

	unsigned flags = param.info ? PGVLF_FILL_AUTOGENERATED : 0;
	// sort by name by default
	sort_fld = vm_field_tbl->find("name");
	if (!param.list_field.empty()) {
		order_str = param.list_field.c_str();
		flags |= PGVLF_FILL_AUTOGENERATED;
	} else if (param.tmpl) {
		order_str = default_template_order;
		flags |= PGVLF_GET_ONLY_VM_TEMPLATES;
	} else {
		flags |= PGVLF_GET_STATE_INFO | PGVLF_GET_NET_STATIC_IP_INFO;
		order_str = g_vzcompat_mode ?
				(param.list_name ? default_vzcompat_name_field_order : default_vzcompat_field_order) :
				default_field_order;
	}

	if (!param.info) {
		field_order = vm_field_tbl->get_field_order(order_str);
		if (field_order.empty())
			return 1;
	}

	if (!param.list_sort.empty()) {
		std::string name(param.list_sort);
		if (name[0] == '-') {
			name = param.list_sort.c_str() + 1;
			sort_rev = true;
		}
		sort_fld = vm_field_tbl->find(name.c_str());
		if (sort_fld < 0) {
			fprintf(stderr, "%s is an invalid field name in"
				" this query.\n", name.c_str());
			return 1;
		}
	} else if (!param.list_field.empty() && !field_order.empty()) {
		sort_fld = field_order.front();
	}

	if (!param.list_no_hdr && param.info && !param.use_json)
		fprintf(stdout, "INFO");

	if (!param.list_no_hdr && !param.use_json)
		vm_field_tbl->print_hdr(field_order);

	std::vector<PrlVm *> vm_list;
	if (param.id.empty()) {
		if ((ret = update_vm_list(param.vmtype, flags)))
			return ret;

		/* Copy of VmList to perform sort operation */
		vm_list.insert(vm_list.begin(), m_VmList.begin(), m_VmList.end());
		/* Sort VM list before processing */
		if (sort_fld != -1 && vm_field_tbl[sort_fld].sort_fn != NULL)
			std::sort(vm_list.begin(), vm_list.end(),
					vm_field_tbl[sort_fld].sort_fn);
	} else {
		PrlVm *vm = NULL;

		ret = get_vm_config(param, &vm);
		if (ret)
			return ret;

		vm_list.push_back(vm);
	}

	if (prl_get_log_verbose() == L_NORMAL)
		prl_set_log_enable(0);
	f.open_list();

	unsigned int idx;
	for (unsigned int i = 0; i < vm_list.size(); ++i) {
		if (sort_rev)
			idx = vm_list.size() - i - 1;
		else
			idx = i;

		PrlVm *vm = vm_list[idx];

		bool tmpl = vm->is_template();
		/* Skip all non template if -t specified */
		if (param.tmpl && !tmpl)
			continue;
		/* Do not show templates by default */
		if (!param.tmpl && param.id.empty() && tmpl)
			continue;

		if (!param.info) {
			if (param.id.empty() && !param.list_all && !param.tmpl) {
				vm->update_state();
				if (param.list_stopped) {
					if (vm->get_state() != VMS_STOPPED)
						continue;
				} else if (vm->get_state() != VMS_RUNNING)
					continue;
			}
		}

		f.tbl_row_open();
		if (param.info) {
			vm->append_configuration(f);
		} else {
			vm_field_tbl->print(vm, field_order, f);
		}
		f.tbl_row_close();
	}

	f.close_list();
	fputs(f.get_buffer().c_str(), stdout);
	return 0;
}

struct FieldUser
{
	const char *name;
	const char *hdr;
	const char *fmt;

	void (*print_fn)(PrlOutFormatter &f, const PrlUser *user,
					 const char *hdr, const char *fmt);
	bool (*sort_fn)(const PrlUser *val1, const PrlUser *val2);
public:
	int find(const char *name) const;
	IntList get_field_order(const char *fields);
	void print_hdr(IntList &order);
	void print(PrlOutFormatter &f, const PrlUser *user, IntList &list);

};

static inline bool user_uuid_sort_fn(const PrlUser *val1, const PrlUser *val2)
{
	return (val1->get_uuid().compare(val2->get_uuid()) < 0);
}

inline bool user_name_sort_fn(const PrlUser *val1, const PrlUser *val2)
{
	return (val1->get_name().compare(val2->get_name()) < 0);
}

static void print_user_id(PrlOutFormatter &f, const PrlUser *user,
							const char *hdr, const char *fmt)
{
	f.tbl_add_item(hdr, fmt, user->get_uuid().c_str());
}

static void print_user_name(PrlOutFormatter &f, const PrlUser *user,
							const char *hdr, const char *fmt)
{
	f.tbl_add_item(hdr, fmt, user->get_name().c_str());
}

static void print_user_def_home(PrlOutFormatter &f, const PrlUser *user,
							const char *hdr, const char *fmt)
{
	f.tbl_add_item(hdr, fmt, user->get_def_home().c_str());
}

static void print_user_status(PrlOutFormatter &f, const PrlUser *user,
							const char *hdr, const char *fmt)
{
	f.tbl_add_item(hdr, fmt, ui2string(user->get_session_count()).c_str());
}

static void print_user_allow_mng(PrlOutFormatter &f, const PrlUser *user,
							const char *hdr, const char *fmt)
{
	f.tbl_add_item(hdr, fmt, user->m_manage_srv_settings ? "allow": "deny");
}

static FieldUser user_field_tbl[] = {
{"uuid" ,	"UUID", "%-39s", print_user_id, user_uuid_sort_fn},
{"name",	"NAME", "%-16s", print_user_name, user_name_sort_fn},
{"def_vm_home",	"DEF_VM_HOME", "%-32s", print_user_def_home, 0},
{"sessions",	"SESSIONS", "%-10s", print_user_status, 0},
{"mng_settings","MNG_SETTINGS", "%-13s", print_user_allow_mng, 0},
{0, 0, 0, 0, 0},
};

int FieldUser::find(const char *name) const
{
	unsigned int i;

	for (i = 0; this[i].name; i++)
		if (!strcmp(this[i].name, name))
			return i;
	return -1;
}

IntList FieldUser::get_field_order(const char *fields)
{
	const char *sp, *ep, *p;
	char name[32];
	int id;
	unsigned int nm_len;
	IntList list;

	sp = fields;
	ep = sp + strlen(sp);
	do {
		if ((p = strchr(sp, ',')) == 0)
			p = ep;
		nm_len = p - sp + 1;
		if (nm_len > sizeof(name) - 1) {
			fprintf(stderr, "The %s field name is unknown.\n", sp);
			break;
		}
		strncpy(name, sp, nm_len);
		name[nm_len - 1] = 0;
		sp = p + 1;
		if ((id = find(name)) < 0) {
			fprintf(stderr, "Unknown field: %s\n", name);
			list.clear();
			break;
		}
		list.push_back(id);
	} while (sp < ep);

	return list;
}

void FieldUser::print_hdr(IntList &order)
{
	// Print Header
	IntList::const_iterator it = order.begin(),
				eit = order.end();

	last_field = false;
	while (it != eit) {
		int id = *it;

		if (++it == eit)
			last_field = true;
		fprintf(stdout, get_fmt(this[id].fmt), this[id].hdr);
	}
	fprintf(stdout, "\n");
}

void FieldUser::print(PrlOutFormatter &f, const PrlUser *user, IntList &order)
{
	IntList::const_iterator it = order.begin(),
				eit = order.end();
	last_field = false;
	f.tbl_row_open();
	while (it != eit) {
		int id = *it;

		if (++it == eit)
			last_field = true;
		this[id].print_fn(f, user, this[id].hdr, get_fmt(this[id].fmt));
	}
	f.tbl_row_close();

}

int PrlSrv::list_user(const CmdParamData &param, bool use_json)
{
	PRL_RESULT ret;
	const char *order_str = 0;
	int sort_fld = 2; // sort by name by default
	PrlOutFormatter &f = *(get_formatter(use_json));

	if (!param.list_field.empty()) {
		sort_fld = -1; // sort by --sort or first specified column
		order_str = param.list_field.c_str();
	} else {
		order_str = default_user_order;
	}

	IntList field_order = user_field_tbl->get_field_order(order_str);

	if (field_order.empty())
		return 1;

	if (!param.list_sort.empty()) {
		sort_fld = user_field_tbl->find(param.list_sort.c_str());
		if (sort_fld < 0) {
			fprintf(stderr, "%s is an invalid field name in"
				" this query.\n", param.list_sort.c_str());
			return 1;
		}
	}
	PrlList<PrlUser *> users_list;
        if ((ret = get_user_info(users_list)))
                return ret;
	if (!param.list_no_hdr && f.type == OUT_FORMATTER_PLAIN)
		user_field_tbl->print_hdr(field_order);
	/* Copy of UserList to perform sort operation */
	std::vector<PrlUser *> users(users_list.begin(), users_list.end());
	if (sort_fld < 0) {
                /* In case sort does not specified use first column */
                sort_fld = field_order.front();
	}
	/* Sort User list before processing */
	if (user_field_tbl[sort_fld].sort_fn)
		std::sort(users.begin(), users.end(),
				user_field_tbl[sort_fld].sort_fn);

	std::vector<PrlUser *>::const_iterator it = users.begin(),
						eit = users.end();

	f.open_list();
	for (; it != eit; ++it) {
#if 0
		if (!param.id.empty()) {
			 if (param.id != (*it)->get_uuid() &&
			     param.id != (*it)->get_name())
			{
				continue;
			}
		}
#endif
		user_field_tbl->print(f, *it, field_order);
	}
	f.close_list();

	fprintf(stdout, "%s", f.get_buffer().c_str());
	return 0;
}
