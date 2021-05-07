/*
 * @file PrlSrvCtl.cpp
 *
 * main() for prlsrvctl utility
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

#include "PrlSrv.h"
#include "CmdParam.h"
#include "Utils.h"
#include "PrlCleanup.h"

int main(int argc, char **argv)
{
	int ret = 1;

	fprintf(stderr, "WARNING: You are using a deprecated CLI component "
			"that will be dropped in the next major release. "
			"Please use virsh instead\n");
	cmdParam cmd;
	PrlSrv *srv = new PrlSrv();
	PrlCleanup::set_cleanup_handler();
	CmdParamData param = cmd.get_disp(argc, argv);
	if (param.action != InvalidAction)
	{
		if (init_sdk_lib())
			return 1;

		ret = srv->run_disp_action(param);
	}

	PrlCleanup::join();

	delete srv;

	deinit_sdk_lib();

	return prlerr2exitcode(ret);
}
