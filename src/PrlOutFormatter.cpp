/*
 * Copyright (c) 2015-2017, Parallels International GmbH
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "Utils.h"
#include "PrlSrv.h"
#include "PrlOutFormatter.h"

#ifdef _WIN_
#define snprintf _snprintf
#endif

std::string PrlOutFormatter::get_buffer()
{
	return out.str();
}

PrlOutFormatterJSON::PrlOutFormatterJSON()
{
	type = OUT_FORMATTER_JSON;
	indent = 0;
	is_first_key = true;
};

void PrlOutFormatterJSON::open_object()
{
	out << "{\n";
}

void PrlOutFormatterJSON::open_list()
{
	out << "[\n";
}

void PrlOutFormatterJSON::close_list()
{
	out << "\n]\n";
}

void PrlOutFormatterJSON::open(const char *key, bool)
{
	if (!is_first_key)
		out << ",\n";
	is_first_key = true;

	for (int i = 0; i < indent + 1; i++)
		out << '\t';
	out << '\"' << key << '\"' << ": {\n";
	indent++;
};

void PrlOutFormatterJSON::open_dev(const char *key)
{
	open(key);
};

void PrlOutFormatterJSON::open_shf(const char *key, bool is_enabled)
{
	open(key);
	add("enabled", is_enabled);
}

void PrlOutFormatterJSON::close(bool)
{
	indent--;

	out << "\n";
	for (int i = 0; i < indent + 1; i++)
		out << '\t';
	out << "}";
	is_first_key = false;
};

void PrlOutFormatterJSON::close_object()
{
	out << "\n}\n";
};

void PrlOutFormatterJSON::add_key(const char *key)
{
	if (!is_first_key)
		out << ",\n";
	else
		is_first_key = false;

	for (int i = 0; i < indent + 1; i++)
		out << '\t';
	out << '\"' << key << '\"' << ": ";
};

void PrlOutFormatterJSON::add(const char *key, const char *value,
								bool, bool, bool)
{
	add_key(key);
	out << '\"' << value << "\"";
};

void PrlOutFormatterJSON::add(const char *key, std::string value,
							  bool, bool, bool)
{
	add(key, value.c_str());
}

void PrlOutFormatterJSON::add(const char *key, int value,
							  const char *suffix, bool, bool)
{
	add_key(key);
	if (strlen(suffix) != 0)
		out << '\"' << value << suffix << '\"';
	else
		out << value;
}

void PrlOutFormatterJSON::add(const char *key, bool value)
{
	add_key(key);
	out << (value ? "true" : "false");
}

void PrlOutFormatterJSON::add_uptime(const char *key,
								unsigned long long uptime, std::string)
{
	add_key(key);
	out << '\"' << uptime << '\"';
}

void PrlOutFormatterJSON::add_isenabled(bool is_enabled)
{
	add("enabled", is_enabled);
}

void PrlOutFormatterJSON::add_host_dev(const char *id, const char *name,
							  const char *type, const char *assignment,
							  const char *used_by)
{
	open(id);
	add("name", name);
	add("type", type);
	if (assignment)
		add("assignment", assignment);
	if (used_by)
		add("used_by", used_by);
	close();
}

void PrlOutFormatterJSON::add_uuid(const char *key, const char *uuid)
{
	std::string uuid_stripped;
	if (*uuid != '\0') {
		uuid_stripped = std::string(uuid + 1);
		uuid_stripped.resize(uuid_stripped.size() - 1);
	}
	add(key, uuid_stripped);
}

void PrlOutFormatterJSON::lic_add(const char *key, const char *value)
{
	add(key, value);
}

void PrlOutFormatterJSON::lic_add(const char *key, std::string value)
{
	add(key, value);
}

void PrlOutFormatterJSON::lic_add(const char *key, int value)
{
	if (value == PRL_LIC_UNLIM_VAL)
		add(key, -1);
	else
		add(key, value);
}

void PrlOutFormatterJSON::tbl_row_open()
{
	if (!is_first_key)
		out << ",\n";
	is_first_key = true;

	for (int i = 0; i < indent + 1; i++)
		out << '\t';
	out << "{\n";
	indent++;
}

void PrlOutFormatterJSON::tbl_row_close()
{
	close();
}

void PrlOutFormatterJSON::tbl_add_item(const char *key, const char *,
										const char *value)
{
	add(key, value);
}

void PrlOutFormatterJSON::tbl_add_uuid(const char *key, const char *,
										const char *value)
{
	add_uuid(key, value);
}

PrlOutFormatterPlain::PrlOutFormatterPlain(const char *tab)
{
	type = OUT_FORMATTER_PLAIN;
	indent = 0;
	this->tab = tab;
};

void PrlOutFormatterPlain::open_object()
{

}

void PrlOutFormatterPlain::open_list()
{

}

void PrlOutFormatterPlain::close_list()
{

}

void PrlOutFormatterPlain::open(const char *key, bool is_inline)
{
	if (is_inline) {
		for (int i = 0; i < indent; i++)
			out << tab;
		out << key;
		if(indent == 0)
			out << ':';
	} else {
		out << key << ":\n";
	}

	indent++;
};

void PrlOutFormatterPlain::open_dev(const char *key)
{
	for (int i = 0; i < indent; i++)
		out << tab;
	out << key;
	indent++;
};

void PrlOutFormatterPlain::open_shf(const char *key, bool is_enabled)
{
	out << key << ": ";
	out << (is_enabled ? "(+)" : "(-)") << "\n";
	indent++;
};

void PrlOutFormatterPlain::close(bool is_inline)
{
	if (is_inline)
		out << "\n";
	indent--;
};

void PrlOutFormatterPlain::close_object()
{

};

void PrlOutFormatterPlain::open_key(const char * key, bool is_inline,
									bool hide_key)
{
	if (is_inline && hide_key) {
		out << ' ';
	} else if(is_inline) {
		out << ' ' << key << '=';
	} else {
		for (int i = 0; i < indent; i++)
			out << tab;
		out << key << ": ";
	}
};

void PrlOutFormatterPlain::close_key(bool is_inline)
{
	if (! is_inline)
		out << '\n';
};

void PrlOutFormatterPlain::add(const char *key, const char *value,
							bool is_inline, bool use_quotes, bool hide_key)
{
	open_key(key, is_inline, hide_key);
	if (use_quotes)
		out << '\'' << value << '\'';
	else
		out << value;
	close_key(is_inline);
};

void PrlOutFormatterPlain::add(const char *key, std::string value,
		bool is_inline, bool use_quotes, bool hide_key)
{
	add(key, value.c_str(), is_inline, use_quotes, hide_key);
}

void PrlOutFormatterPlain::add(const char *key, int value, const char *suffix,
								bool is_inline, bool hide_key)
{
	open_key(key, is_inline, hide_key);
	out << value << suffix;
	close_key(is_inline);
};

void PrlOutFormatterPlain::add(const char *key, bool value)
{
	if (value)
		out << ' ' << key;
};

void PrlOutFormatterPlain::add_uptime(const char *key,
							unsigned long long uptime, std::string start_date)
{
	std::string tmp;

	tmp = uptime2str(uptime);
	tmp += " (since ";
	tmp += start_date;
	tmp += ")";
	add(key, tmp);
};

void PrlOutFormatterPlain::add_isenabled(bool is_enabled)
{
	add("enabled", is_enabled ? "(+)" : "(-)", true, false, true);
};

void PrlOutFormatterPlain::add_host_dev(const char *id, const char *name,
							  const char *type, const char *assignment,
							  const char *used_by)
{
	char buf[1024];
	snprintf(buf, 1023, "%8s  %-40s '%-s'", type, name, id);
	buf[1023] = '\0';
	out << buf;

	if (assignment)
		out << " assignment=" << assignment;
	if (used_by)
		out << " used_by=" << used_by;
	out << "\n";
};

PrlOutFormatter * get_formatter(bool use_json, const char *tab)
{
	if (use_json)
		return new PrlOutFormatterJSON();
	else
		return new PrlOutFormatterPlain(tab);
}

void PrlOutFormatterPlain::add_uuid(const char *key, const char *uuid)
{
	add(key, uuid);
};

void PrlOutFormatterPlain::lic_add(const char *key, const char *value)
{
	out << "\t" << key << "=\"" << value << "\"\n";
}

void PrlOutFormatterPlain::lic_add(const char *key, std::string value)
{
	lic_add(key, value.c_str());
}

void PrlOutFormatterPlain::lic_add(const char *key, int value)
{
	out << "\t" << key << "=";
	if (value == PRL_LIC_UNLIM_VAL)
		out << "\"unlimited\"\n";
	else
		out << value << "\n";
}

void PrlOutFormatterPlain::tbl_row_open()
{

}

void PrlOutFormatterPlain::tbl_row_close()
{
	out << "\n";
}

void PrlOutFormatterPlain::tbl_add_item(const char *, const char *fmt,
											const char *value)
{
	char buf[256];

	buf[255] = '\0';
	snprintf(buf, 255, fmt, value);

	out << buf;
}

void PrlOutFormatterPlain::tbl_add_uuid(const char *, const char *fmt,
											const char *value)
{
	tbl_add_item(NULL, fmt, value);
}
