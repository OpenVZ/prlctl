/*
 * @file PrlSignal.h
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

#ifndef __PRLSIGNAL_H__
#define __PRLSIGNAL_H__

#ifdef _WIN_

#include <windows.h>

typedef PHANDLER_ROUTINE sig_handler;

class PrlSig
{
public:
	PrlSig() {}
	void set_cleanup_handler(sig_handler handler)
	{
		set_handler(handler);
	}
	int ignore(int *signals);
	void restore();
	~PrlSig() { restore(); }
private:
	int set_handler(sig_handler handler);
};

class PrlSigIgn
{
public:
	PrlSigIgn() {}
	void restore() {}
};

#else // _WIN_

typedef void (* sig_handler) (int sig);

#include <signal.h>
#include <map>

class PrlSig: protected std::map<int, struct sigaction>
{
public:
	PrlSig() {}
	int ignore(int *signals);
	void set_cleanup_handler(sig_handler handler)
	{
		set_handler(SIGINT, handler);
		set_handler(SIGHUP, handler);
		set_handler(SIGTERM, handler);
	}
	void restore();
	~PrlSig() { restore(); }

private:
	int set_handler(int sig, sig_handler handler);

};

class PrlSigIgn : public PrlSig
{
public:
	PrlSigIgn()
	{
		static int s_ign[] = {SIGTTOU, SIGTTIN, SIGTSTP, SIGINT, SIGHUP, 0};
		ignore(s_ign);
	}
	~PrlSigIgn() {}
};
#endif // ! _WIN_
#endif // __PRLSIGNAL_H__
