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

#include "PrlSharedFolder.h"
#include "Utils.h"
#include "Logger.h"
#include "PrlOutFormatter.h"

PrlSharedFolder::~PrlSharedFolder()
{
	if (m_hShf)
		PrlHandle_Free(m_hShf);
}

const std::string &PrlSharedFolder::get_name()
{
	int ret;
	char buf[4096];
	unsigned int len = sizeof(buf);

	if (!m_name.empty())
		return m_name;

	ret = PrlShare_GetName(m_hShf, buf, &len);
	if (ret)
		prl_err(ret, "PrlShare_GetName: %s",
				get_error_str(ret).c_str());
	else
		m_name = buf;

	return m_name;

}

int PrlSharedFolder::set_enable(bool flag)
{
	PRL_RESULT ret;

	ret = PrlShare_SetEnabled(m_hShf, flag);
	if (ret)
		return prl_err(ret, "PrlShare_SetEnabled: %s",
				get_error_str(ret).c_str());
	set_updated();
	return 0;
}

int PrlSharedFolder::set_readonly(bool flag)
{
	PRL_RESULT ret;

	ret = PrlShare_SetReadOnly(m_hShf, flag);
	if (ret)
		return prl_err(ret, "PrlShare_SetReadOnly: %s",
				get_error_str(ret).c_str());
	set_updated();
	return 0;
}

int PrlSharedFolder::configure(const SharedFolderParam &param)
{
	int ret;

	if (param.enable)
		set_enable(true);
	if (param.disable)
		set_enable(false);
	if (!param.mode.empty())
		set_readonly(param.mode == "ro" ? true : false);

	if (!param.path.empty()) {
		ret = PrlShare_SetPath(m_hShf, param.path.c_str());
		if (ret)
			return prl_err(ret, "PrlShare_SetPath: %s",
				get_error_str(ret).c_str());
		set_updated();
	}
	if (!param.desc.empty()) {
		ret = PrlShare_SetDescription(m_hShf, param.desc.c_str());
		if (ret)
			return prl_err(ret, "PrlShare_SetDescription: %s",
				get_error_str(ret).c_str());
		set_updated();
	}

	return 0;
}

int PrlSharedFolder::create(const SharedFolderParam &param)
{
	int ret;

	if (param.mode.empty())
		set_readonly(false);
	ret = PrlShare_SetName(m_hShf, param.name.c_str());
	if (ret)
		return prl_err(ret, "PrlShare_SetName: %s",
			get_error_str(ret).c_str());

	ret = configure(param);
	if (ret)
		return ret;

	prl_log(0, "Creating shared folder: %s", get_info().c_str());

	return 0;
}

int PrlSharedFolder::remove()
{
        PRL_RESULT ret;

        prl_log(0, "Remove shared folder '%s'", get_name().c_str());
        if ((ret = PrlShare_Remove(m_hShf)))
                return prl_err(ret, "PrlShare_Remove: %s",
                                get_error_str(ret).c_str());
        set_updated();
        return 0;
}

std::string PrlSharedFolder::get_info() const
{
	PrlOutFormatterPlain f;
	append_info(f);
	return f.get_buffer();
}

void PrlSharedFolder::append_info(PrlOutFormatter &f) const
{
	int ret;
	PRL_BOOL flag;
	char buf[4096];
	unsigned int len = sizeof(buf);

	ret = PrlShare_GetName(m_hShf, buf, &len);
	if (ret == 0) {
		f.open_dev(buf);
	} else {
		prl_log(L_INFO, "PrlShare_GetName: %s",
			get_error_str(ret).c_str());
	}

	len = sizeof(buf);
	ret = PrlShare_IsEnabled(m_hShf, &flag);
	if (ret == 0) {
		f.add_isenabled(!!flag);
	} else {
		prl_log(L_INFO, "PrlShare_IsEnabled: %s",
			get_error_str(ret).c_str());
	}

	len = sizeof(buf);
	ret = PrlShare_GetPath(m_hShf, buf, &len);
	if (ret == 0) {
		f.add("path", buf, true, true, false);
	} else {
		prl_log(L_INFO, "PrlShare_GetPath: %s",
			get_error_str(ret).c_str());
	}

	ret = PrlShare_IsReadOnly(m_hShf, &flag);
	if (ret == 0) {
		f.add("mode", flag ? "ro" : "rw", true, true, false);
	} else {
		prl_log(L_INFO, "PrlShare_IsReadOnly: %s",
			get_error_str(ret).c_str());
	}
	f.close(true);
}
