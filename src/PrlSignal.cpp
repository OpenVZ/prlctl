/*
 * @file PrlSignal.cpp
 *
 * Signal handling helper function cpp file
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

#include "PrlSignal.h"
#include "Logger.h"
#include <stddef.h>

#ifdef _WIN_

int PrlSig::set_handler(sig_handler handler)
{
	if (!SetConsoleCtrlHandler(handler, TRUE))
		return prl_err(GetLastError(), "SetConsoleCtrlHandler");

	return 0;
}

void PrlSig::restore()
{
	if (!SetConsoleCtrlHandler(NULL, FALSE))
		prl_err(GetLastError(), "SetConsoleCtrlHandler");
}

int PrlSig::ignore(int *signals)
{
	(void)signals;
	if (!SetConsoleCtrlHandler(NULL, TRUE))
		return prl_err(GetLastError(), "SetConsoleCtrlHandler");
	return 0;
}

#else

int PrlSig::set_handler(int sig, sig_handler handler)
{
	struct sigaction sa, old_sa;
	int ret;

	sigaction(sig, NULL, &old_sa);
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = handler;
	sa.sa_flags = 0;
	if ((ret = sigaction(sig, &sa, NULL)))
		return prl_err(ret, "sigaction");

	/* store old state for further restore */
	if (find(sig) == end())
		(*this)[sig] = old_sa;

	return 0;
}

void PrlSig::restore()
{
	const_iterator it = begin(), eit = end();
	for (; it != eit; ++it)
		sigaction(it->first, &it->second, NULL);
	clear();
}

int PrlSig::ignore(int *signals)
{
	for (int *sig = signals; *sig; ++sig)
		set_handler(*sig, SIG_IGN);
	return 0;
}
#endif
