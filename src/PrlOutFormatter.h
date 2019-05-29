/*
 * Copyright (c) 2015-2017, Parallels International GmbH
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

#ifndef __PRL_OUT_FORMATTER_H__
#define __PRL_OUT_FORMATTER_H__

#include <string>
#include <sstream>

enum OutFormatterType {
	OUT_FORMATTER_PLAIN,
	OUT_FORMATTER_JSON,
};

class PrlOutFormatter {
protected:
	std::stringstream out;

public:
	OutFormatterType type;

	PrlOutFormatter() {};
	virtual ~PrlOutFormatter() {};

	virtual void open_object() = 0;
	virtual void close_object() = 0;
	virtual void open_list() = 0;
	virtual void close_list() = 0;

	virtual void open(const char *key, bool is_inline = false) = 0;
	virtual void open_dev(const char *key) = 0;
	virtual void open_shf(const char *key, bool is_enabled) = 0;
	virtual void close(bool is_inline = false) = 0;

	virtual void add(const char *key, const char *value,
					 bool is_inline = false, bool use_quotes = false,
					 bool hide_key = false) = 0;
	virtual void add(const char *key, std::string value,
					 bool is_inline = false, bool use_quotes = false,
					 bool hide_key = false) = 0;
	virtual void add(const char *key, int value, const char *suffix = "",
					 bool is_inline = false, bool hide_key = false) = 0;
	virtual void add(const char *key, bool value) = 0;
	virtual void add_uptime(const char *key, unsigned long long uptime,
							std::string start_date) = 0;
	virtual void add_isenabled(bool is_enabled) = 0;
	virtual void add_host_dev(const char *id, const char *name,
							  const char *type, const char *assignment,
							  const char *used_by) = 0;
	virtual void add_uuid(const char *key, const char *uuid) = 0;
	virtual void lic_add(const char *key, const char *value) = 0;
	virtual void lic_add(const char *key, std::string value) = 0;
	virtual void lic_add(const char *key, int value) = 0;

	virtual void tbl_row_open() = 0;
	virtual void tbl_row_close() = 0;
	virtual void tbl_add_item(const char *key, const char *fmt,
								const char *value) = 0;
	virtual void tbl_add_uuid(const char *key, const char *fmt,
								const char *value) = 0;

	std::string get_buffer();
};

class PrlOutFormatterJSON : public PrlOutFormatter {
private:
	int indent;
	bool is_first_key;

public:
	PrlOutFormatterJSON();
	virtual void open_object();
	virtual void close_object();
	virtual void open_list();
	virtual void close_list();

	virtual void open(const char *key, bool is_inline = false);
	virtual void open_dev(const char *key);
	virtual void open_shf(const char *key, bool is_enabled);
	virtual void close(bool is_inline = false);
private:
	virtual void add_key(const char *key);
public:
	virtual void add(const char *key, const char *value,
					 bool is_inline = false, bool use_quotes = false,
					 bool hide_key = false);
	virtual void add(const char *key, std::string value,
					 bool is_inline = false, bool use_quotes = false,
					 bool hide_key = false);
	virtual void add(const char *key, int value, const char *suffix = "",
					 bool is_inline = false, bool hide_key = false);
	virtual void add(const char *key, bool value);
	virtual void add_uptime(const char *key, unsigned long long uptime,
							std::string start_date);
	virtual void add_isenabled(bool is_enabled);
	virtual void add_host_dev(const char *id, const char *name,
							  const char *type, const char *assignment,
							  const char *used_by);
	virtual void add_uuid(const char *key, const char *uuid);
	virtual void lic_add(const char *key, const char *value);
	virtual void lic_add(const char *key, std::string value);
	virtual void lic_add(const char *key, int value);

	virtual void tbl_row_open();
	virtual void tbl_row_close();
	virtual void tbl_add_item(const char *key, const char *,
								const char *value);
	virtual void tbl_add_uuid(const char *key, const char *,
								const char *value);
};

class PrlOutFormatterPlain : public PrlOutFormatter {
private:
	int indent;
	const char *tab;

public:
	PrlOutFormatterPlain(const char *tab = "  ");
	virtual void open_object();
	virtual void close_object();
	virtual void open_list();
	virtual void close_list();

	virtual void open(const char *key, bool is_inline = false);
	virtual void open_dev(const char *key);
	virtual void open_shf(const char *key, bool is_enabled);
	virtual void close(bool is_inline = false);
	void open_key(const char * key, bool is_inline, bool hide_key);
	void close_key(bool is_inline);
	virtual void add(const char *key, const char *value,
					 bool is_inline = false, bool use_quotes = false,
					 bool hide_key = false);
	virtual void add(const char *key, std::string value,
					 bool is_inline = false, bool use_quotes = false,
					 bool hide_key = false);
	virtual void add(const char *key, int value, const char *suffix = "",
					 bool is_inline = false, bool hide_key = false);
	virtual void add(const char *key, bool value);
	virtual void add_uptime(const char *key, unsigned long long uptime,
							std::string start_date);
	virtual void add_isenabled(bool is_enabled);
	virtual void add_host_dev(const char *id, const char *name,
							  const char *type, const char *assignment,
							  const char *used_by);
	virtual void add_uuid(const char *key, const char *uuid);
	virtual void lic_add(const char *key, const char *value);
	virtual void lic_add(const char *key, std::string value);
	virtual void lic_add(const char *key, int value);

	virtual void tbl_row_open();
	virtual void tbl_row_close();
	virtual void tbl_add_item(const char *key, const char *fmt,
								const char *value);
	virtual void tbl_add_uuid(const char *key, const char *fmt,
								const char *value);
};

PrlOutFormatter * get_formatter(bool use_json, const char *tab = "  ");

#endif //__PRL_OUT_FORMATTER_H__
