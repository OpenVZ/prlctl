/*
 * @file PrlCleanup.h
 *
 * This header file contains the PrlCleanup class definition.
 * Static instance of PrlCleanup could be used to call registered hooks
 * to rollback transaction on cacelation.
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

#ifndef __PRLCLEANUP_H__
#include <list>
#include "PrlSignal.h"

typedef void (* hook_fn) (void *data);

struct PrlHook {
	hook_fn fn;
	void *data;
public:
	PrlHook(hook_fn _fn, void *_data) :
		fn(_fn), data(_data)
	{}
};

typedef std::list<PrlHook> hookList;
class PrlCleanup
{
private:
	static PrlSig *m_sig;
	static hookList *m_hooks;
	static pthread_t m_th;
	static pthread_mutex_t m_mutex;
	static int m_fd[2];
public:
	PrlCleanup() {}
	const PrlHook *register_hook(hook_fn fn, void *data);
	static void unregister_hook(const PrlHook *h);
	static void register_cancel(PRL_HANDLE h);
	static void unregister_last();
	static void *monitor(void *);
	static void do_cleanup();
	static void join();
	static void run(int sig);
	static int set_cleanup_handler();
};

void cancel_job(void *data);
void migrate_cancel_job(void *data);
void cancel_session(void *data);
void login_cancel_job(void *data);
void call_exit(void *data);
PrlCleanup &get_cleanup_ctx();
#endif // __PRLCLEANUP_H__
