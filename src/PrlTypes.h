///////////////////////////////////////////////////////////////////////////////
///
/// @file PrlTypes.h
///
/// Types & enumerators
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
#ifndef __PRLTYPES_h__
#define __PRLTYPES_h__
#include <list>
#include <vector>
#include <string>
#include <limits.h>
#include <stdlib.h>
#include <PrlTypes.h>
#include <PrlEnums.h>
#include <PrlErrors.h>
#include <PrlApi.h>
#define VENET0_STR	"venet0"
#define VENET0_ID	"net4294967295"

#define JOB_INFINIT_WAIT_TIMEOUT UINT_MAX //default infinit job wait timeout

enum ResType {
	RES_NONE,
	RES_HW_CPUS,
};

enum DevType {
	DEV_NONE,
	DEV_HDD,
	DEV_HDD_PARTITION,
	DEV_CDROM,
	DEV_FDD,
	DEV_NET,
	DEV_USB,
	DEV_SERIAL,
	DEV_PARALLEL,
	DEV_SOUND,
	DEV_GENERIC_PCI,
};

enum DevMode {
	DEV_TYPE_NONE,
	DEV_TYPE_HDD_EXPAND,
	DEV_TYPE_HDD_PLAIN,
	DEV_TYPE_NET_HOST,
	DEV_TYPE_NET_SHARED,
	DEV_TYPE_NET_BRIDGED,
	DEV_TYPE_NET_DEVICE,
	DEV_TYPE_NET_ROUTED,
};

struct OsDistribution {
	const char *name;
	unsigned int type;
	unsigned int ver;
};


/* Helper conrainer to store list of PrlDev* PrlVm* */
template <typename T>
class PrlList : public std::list<T>
{
public:
	PrlList()
	{}
	T add(T elem)
	{
		std::list<T>::push_back(elem);
		return std::list<T>::back();
	}
	void del()
	{
		typename std::list<T>::iterator
				it = std::list<T>::begin(),
				eit = std::list<T>::end();

		for (; it != eit; ++it)
			delete *it;
		std::list<T>::clear();
	}
	T find(const std::string &id) const
	{
		typename std::list<T>::const_iterator
				it = std::list<T>::begin(),
				eit = std::list<T>::end();

		for (; it != eit; ++it)
			if ((*it)->get_id() == id ||
			    (*it)->get_name() == id)
				return *it;
		return 0;
	}
	T find(DevType type, unsigned int idx) const
	{
		typename std::list<T>::const_iterator
					it = std::list<T>::begin(),
					eit = std::list<T>::end();

		for (; it != eit; ++it)
			if ((*it)->m_devType == type && (*it)->m_idx == idx)
				return *it;
		return 0;
	}
	T find(DevType type) const
	{
		typename std::list<T>::const_iterator
					it = std::list<T>::begin(),
					eit = std::list<T>::end();

		for (; it != eit; ++it)
			if ((*it)->m_devType == type)
				return *it;
		return 0;
	}
};

template <typename T>
class TNode {
public:
	TNode() : m_parent(0) {}
	TNode(TNode *parent) : m_parent(parent) {}
	TNode(TNode *parent, const T &data) :
		m_parent(parent), m_data(data)
	{}
	TNode *add_elem(const T &data)
	{
		TNode *elem = new TNode(this, data);
		m_siblings.push_back(elem);
		return elem;
	}
	void add_child(TNode *child_)
	{
		m_siblings.push_back(child_);
	}
	const TNode *parent() const { return m_parent ? m_parent : this; }
	TNode *parent() { return m_parent ? m_parent : this; }
	void destroy()
	{
		typename std::vector<TNode *>::iterator it = m_siblings.begin(),
						eit = m_siblings.end();
		for (; it != eit; ++it) {
			TNode *node = *it;
			node->destroy();
			delete node;
		}
		m_siblings.clear();
	}
	void setData(const T& data_)
	{
		m_data = data_;
	}
	const T &data() const { return m_data; }
	~TNode() { destroy(); }
	bool empty() { return m_siblings.empty(); }
	TNode *get_next_child(TNode *child)
	{
		TNode *node = 0;
		typename std::vector<TNode *>::iterator it = m_siblings.begin(),
						eit = m_siblings.end();
		for (; it != eit; ++it) {
			if (*it == child) {
				++it;
				if (it != eit)
				node = *it;
				break;
			}
		}
		return node;
	}

public:
	TNode *m_parent;
	std::vector<TNode *> m_siblings;
	T m_data;
};

class PrlBase {
private:
	PRL_HANDLE m_handle;

	PrlBase* free();
	PrlBase* release() { return valid() ? free() : this; }
public:
	PrlBase():m_handle(PRL_INVALID_HANDLE) {}
	PrlBase(PRL_HANDLE m_handle):m_handle(m_handle) {}
	~PrlBase() { release(); }

	bool valid() const { return m_handle != PRL_INVALID_HANDLE; }
	PRL_HANDLE get_handle() const { return m_handle; }
	PRL_HANDLE release_handle()
	{
		PRL_HANDLE ret = m_handle;
		m_handle = PRL_INVALID_HANDLE;
		return ret;
	}
	PRL_HANDLE* get_ptr() { return &release()->m_handle; }
	operator PRL_HANDLE () const
	{
		return (m_handle) ;
	}
};
typedef PrlBase PrlHandle;

static inline bool prl_bool(PRL_BOOL b)
{
	return b ? true : false;
}

static inline bool prl_bool(unsigned long b)
{
	return b ? true : false;
}

class str_list_t : public std::list<std::string>
{
public:
	std::string to_str()
	{
		std::string out;

		for (const_iterator it = begin(), eit = end(); it != eit; it++) {
			out += *it;
			out += " ";
		}
		return out;
	}
};

struct ip_addr {
	std::string ip;
	std::string mask;
	std::string ip6_mask;

	ip_addr() {}
	ip_addr(const std::string &str)
	{
		std::string::size_type pos = str.find_first_of("/");
		if (pos != std::string::npos) {
			ip = str.substr(0, pos);
			mask = str.substr(pos + 1);
			try_to_ip6_mask();
		} else
			ip = str;
	}
	std::string to_str(bool full = true) const
	{
		std::string out;
		out += ip;
		if (full && !mask.empty()) {
			out += "/";
			out += mask;
		}
		return out;
	}

private:
	void try_to_ip6_mask()
	{
		if (ip.find_first_of(":") == std::string::npos)
			return;

		ip6_mask = mask;
		if (mask.find_first_of(":") != std::string::npos)
			return;

		int bits = atoi(ip6_mask.c_str());
		if (bits < 0 || bits > 128)
			return;

		ip6_mask = "0000:0000:0000:0000:0000:0000:0000:0000";
		std::string::iterator it = ip6_mask.begin();
		for(int i = 0; i < bits / 4; i++, ++it)
		{
			if (*it == ':') ++it;
			*it = 'F';
		}
		if (*it == ':') ++it;

		if (it == ip6_mask.end())
			return;
		const char* c = "08CE";
		*it = c[bits % 4];
	}
};

class ip_list_t : public std::list<struct ip_addr>
{
public:
	void add(const std::string &ip)
	{
		push_back(ip_addr(ip));
	}
	bool find(const struct ip_addr &data) const
	{
		for (const_iterator it = begin(), eit = end(); it != eit; it++) {
			if (it->ip == data.ip)
				return true;
		}
		return false;
	}
	std::string to_str(bool full = true) const
	{
		std::string out;

		for (const_iterator it = begin(), eit = end(); it != eit; it++) {
			out += it->to_str(full);
			out += " ";
		}
		return out;
	}
};

struct rate_data {
	unsigned int classid;
	unsigned int rate;
};

class rate_list_t : public std::list<struct rate_data>
{
public:
	void add(struct rate_data &rate)
	{
		push_back(rate);
	}
};

typedef PrlList<PrlHandle *> PrlVNetList;
typedef PrlList<PrlHandle *> PrlPrivNetList;

struct iscsi_target {
	std::string portal;
	std::string target_name;
	unsigned int lun;
};

class iscsi_target_list_t : public std::list<struct iscsi_target>
{
public:
	void add(struct iscsi_target &target)
	{
		push_back(target);
	}
};

struct fw_rule_s {
	std::string proto;
	unsigned int src_port;
	std::string src_ip;
	unsigned int dst_port;
	std::string dst_ip;

	fw_rule_s() : src_port(0), dst_port(0)
	{}
};

class fw_rule_list_t : public std::list<struct fw_rule_s>
{
public:
	void add(struct fw_rule_s &entry)
	{
		push_back(entry);
	}
};

struct nat_rule_s {
	std::string name;
	std::string redir_entry; // ip or vm
	int in_port;
	int redir_port;

	nat_rule_s() : in_port(0), redir_port(0)
	{}
};

class nat_rule_list_t : public std::list<struct nat_rule_s>
{
public:
	void add(struct nat_rule_s& entry)
	{
		push_back(entry);
	}
};

struct CtResource {

	CtResource(PRL_CT_RESOURCE _id, unsigned long _barrier, unsigned long _limit) :
		id(_id), barrier(_barrier), limit(_limit)
	{}

	PRL_CT_RESOURCE id;
	unsigned long barrier;
	unsigned long limit;
};

class ct_resource_list_t : public std::list<struct CtResource>
{
public:

	void add(PRL_CT_RESOURCE id, unsigned long barrier, unsigned long limit)
	{
		struct CtResource entry(id, barrier, limit);

		for (iterator it = begin(), eit = end(); it != eit; it++) {
			if (it->id == id) {
				it->barrier = barrier;
				it->limit = limit;
				return;
			}
		}

		push_back(entry);
	}

	void add(PRL_CT_RESOURCE id, unsigned int limit)
	{
		unsigned long b = limit;
		unsigned long l = limit;

		add(id, b, l);
	}
};

extern bool g_vzcompat_mode;

typedef PrlList<PrlHandle *> PrlCtTemplateList;

#endif
