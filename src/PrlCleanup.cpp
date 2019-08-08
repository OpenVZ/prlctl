/*
 * @file PrlCleanup.cpp
 *
 * PrlCleanup class implementation
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

#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include <PrlTypes.h>

#include "Logger.h"
#include "PrlSignal.h"
#include "PrlCleanup.h"
#include "Utils.h"

static PrlCleanup g_cleanup_ctx;
hookList *PrlCleanup::m_hooks = 0;
pthread_t PrlCleanup::m_th = 0;
pthread_mutex_t PrlCleanup::m_mutex = PTHREAD_MUTEX_INITIALIZER;
int PrlCleanup::m_fd[2] = {-1, -1};
PrlSig *PrlCleanup::m_sig = 0;

PrlCleanup &get_cleanup_ctx()
{
	return g_cleanup_ctx;
}

const PrlHook *PrlCleanup::register_hook(hook_fn fn, void *data)
{
	pthread_mutex_lock(&m_mutex);

	if (!m_hooks)
		m_hooks = new hookList;
	m_hooks->push_back(PrlHook(fn, data));
	const PrlHook *h = &m_hooks->back();

	pthread_mutex_unlock(&m_mutex);

	prl_log(L_DEBUG, "PrlCleanup::register_hook: %x", h);
	return h;
}

void PrlCleanup::unregister_hook(const PrlHook *h)
{
	pthread_mutex_lock(&m_mutex);

	hookList::iterator it = m_hooks->begin(),
		eit = m_hooks->end();
        for (; it != eit; ++it) {
		if (&(*it) == h) {
			prl_log(L_DEBUG, "PrlCleanup::unregister_hook: %x", h);
			m_hooks->erase(it);
			break;
		}
	}

	pthread_mutex_unlock(&m_mutex);
}

void PrlCleanup::register_cancel(PRL_HANDLE h)
{
	get_cleanup_ctx().register_hook(cancel_job, h);
}

void PrlCleanup::unregister_last()
{
	prl_log(L_DEBUG, "PrlCleanup::unregister_last");

	pthread_mutex_lock(&m_mutex);

	if (!m_hooks->empty())
		m_hooks->pop_back();

	pthread_mutex_unlock(&m_mutex);
}

void PrlCleanup::do_cleanup()
{
	pthread_mutex_lock(&m_mutex);

	if (m_hooks) {
		hookList::reverse_iterator it = m_hooks->rbegin(),
			eit = m_hooks->rend();
		for (; it != eit; ++it)
			it->fn(it->data);
	}

	pthread_mutex_unlock(&m_mutex);
}
void *PrlCleanup::monitor(void *data)
{
	int fd = *(int *) data;
	int n;

	while (read(fd, &n, sizeof(n)) == sizeof(n)) {
		prl_log(L_INFO, "Call the cleanup on %d signal", n);
		do_cleanup();
	}

	return NULL;
}

void PrlCleanup::join()
{
	if (m_fd[0] != -1) {
		close(m_fd[0]);
		m_fd[0] = -1;
	}
	if (m_fd[1] != -1) {
		close(m_fd[1]);
		m_fd[1] = -1;
	}
	if (m_th) {
		void *res;
		pthread_join(m_th, &res);
		m_th = 0;
	}
}

void PrlCleanup::run(int sig)
{
	if (write(m_fd[1], &sig, sizeof(sig)) == -1)
		prl_err(1, "write(fd=%d): %m", m_fd[0]);
}

int PrlCleanup::set_cleanup_handler()
{
	if (!m_sig) {
		m_sig = new PrlSig;
		m_sig->set_cleanup_handler(run);
	}

	if (pipe(m_fd) < 0)
		return prl_err(1, "socketpair :%m");

	if (pthread_create(&PrlCleanup::m_th, NULL, PrlCleanup::monitor, &m_fd[0]))
		return prl_err(1, "pthread_create: %m");

	return 0;
}

void cancel_job(void *data)
{
	prl_log(0, "\nCanceling the job...");
	PrlHandle hJob(PrlJob_Cancel((PRL_HANDLE) data));
}

void migrate_cancel_job(void *data)
{
	prl_log(0, "\nCanceling the migration...");
	PrlHandle hJob(PrlVm_MigrateCancel((PRL_HANDLE) data));
}

void cancel_session(void *data)
{
	prl_log(0, "\nCanceling the session...");
	PrlHandle hJob(PrlVmGuest_Logout((PRL_HANDLE) data, 0));
}

void call_exit(void *data)
{
	(void) data;
	exit(0);
}
