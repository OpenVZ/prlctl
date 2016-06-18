/*
 * Copyright (c) 2015 Parallels IP Holdings GmbH
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
 * Our contact details: Parallels IP Holdings GmbH, Vordergasse 59, 8200
 * Schaffhausen, Switzerland.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "PrlVm.h"
#include "Logger.h"
#include "Utils.h"
#include "CmdParam.h"

#ifndef _WIN_
#include <unistd.h>
#endif

#ifdef _LIN_
static int _execvp(const char *path, char *const argv[])
{
	if (!strchr(path, '/')) {
		const char *p = getenv("PATH");

		if (!p)
			p = "/bin:/usr/bin:/sbin";
		for (; p && *p;) {
			char partial[PATH_MAX];
			const char *p2;

			p2 = strchr(p, ':');
			size_t len = 0;
			if (p2)
				len = p2 - p;
			else
				len = strlen(p);

			if (len >= PATH_MAX)
				return prl_err(-1, "PATH variable has too long path.");

			strncpy(partial, p, len);
			partial[len] = 0;

			if (len)
				strcat(partial, "/");
			strcat(partial, path);

			execv(partial, argv);

			if (errno != ENOENT)
				return -1;
			if (p2) {
				p = p2 + 1;
			} else {
				p = 0;
			}
		}
		return -1;
	} else
		return execv(path, argv);
}

int PrlVm::enter()
{
	const char *arg[255];
	int i = 0;

	if (m_VmState != VMS_RUNNING)
		return prl_err(-1, "Failed to enter the %s %s.",
				vmstate2str(m_VmState), get_vm_type_str());

	VncParam vnc = get_vnc_param();

	if (vnc.mode == PRD_DISABLED)
		return prl_err(-1, "Remote Display is disabled for the %s."
			" Use --vnc-mode option to configure Remote Display.",
			get_vm_type_str());

	std::string address = vnc.address + ":" + ui2string(vnc.port);

	arg[i++] = "vncviewer";
	arg[i++] = address.c_str();
	arg[i++] = NULL;
	_execvp(arg[0], (char *const *)arg);

	return prl_err(-1, "enter failed: no VNC client found.");
}
#else
int PrlVm::enter()
{
	return prl_err(-1, "enter is not supported.");
}
#endif
