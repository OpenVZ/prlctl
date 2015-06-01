///////////////////////////////////////////////////////////////////////////////
///
/// @file PrlCleanup.cpp
///
/// PrlCleanup class implementation.
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

#include <stdlib.h>

#include <PrlTypes.h>

#include "Logger.h"
#include "PrlSignal.h"
#include "PrlCleanup.h"
#include "Utils.h"

static PrlCleanup g_cleanup_ctx;
hookList *PrlCleanup::m_hooks = 0;
PrlSig *PrlCleanup::m_sig = 0;

PrlCleanup &get_cleanup_ctx()
{
	return g_cleanup_ctx;
}

const PrlHook *PrlCleanup::register_hook(hook_fn fn, void *data)
{
	if (!m_hooks)
		m_hooks = new hookList;
	m_hooks->push_back(PrlHook(fn, data));
	const PrlHook *h = &m_hooks->back();
	prl_log(L_DEBUG, "PrlCleanup::register_hook: %x", h);
	return h;
}

void PrlCleanup::unregister_hook(const PrlHook *h)
{
	hookList::iterator it = m_hooks->begin(),
		eit = m_hooks->end();
        for (; it != eit; ++it) {
		if (&(*it) == h) {
			prl_log(L_DEBUG, "PrlCleanup::unregister_hook: %x", h);
			m_hooks->erase(it);
			break;
		}
	}
}

void PrlCleanup::register_cancel(PRL_HANDLE h)
{
	get_cleanup_ctx().register_hook(cancel_job, h);
}

void PrlCleanup::unregister_last()
{
	prl_log(L_DEBUG, "PrlCleanup::unregister_last");
	if (!m_hooks->empty())
		m_hooks->pop_back();
}

#ifdef _WIN_
BOOL WINAPI PrlCleanup::run(DWORD sig)
#else
void PrlCleanup::run(int sig)
#endif
{
	prl_log(L_INFO, "Call the cleanup on the %d signal.", sig);
	if (m_hooks) {
		hookList::reverse_iterator it = m_hooks->rbegin(),
			eit = m_hooks->rend();
		for (; it != eit; ++it)
			it->fn(it->data);
		m_hooks->clear();
	}
#ifdef _WIN_
	// mark signal as processed, so runtime will not terminate us in default handler
	return TRUE;
#endif
}

void PrlCleanup::set_cleanup_handler()
{
	if (!m_sig) {
		m_sig = new PrlSig;
		m_sig->set_cleanup_handler(run);
	}
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

void login_cancel_job(void *data)
{
	prl_log(0, "\nCanceling login...");

	call_exit(data);
}

void call_exit(void *data)
{
	(void) data;
	exit(0);
}

