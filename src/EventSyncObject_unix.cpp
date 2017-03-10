/*
 * @file EventSyncObject_unix.cpp
 *
 * Autoreseting event synch object
 *
 * @author sdmitry
 *
 * Copyright (c) 2007-2017, Parallels International GmbH
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

#include "EventSyncObject.h"

#include <errno.h>

#if defined(_LIN_)
#include <sys/time.h>
#endif


CEventSyncObject::CEventSyncObject()
{
	m_bSignalled = false;
	pthread_mutex_init(&m_evMutex, NULL);
	pthread_cond_init(&m_evCond, NULL);
}


CEventSyncObject::~CEventSyncObject()
{
	pthread_mutex_destroy(&m_evMutex);
	pthread_cond_destroy(&m_evCond);
}

void CEventSyncObject::Signal()
{
	pthread_mutex_lock(&m_evMutex);
	if( !m_bSignalled )
	{
		m_bSignalled = true;
		pthread_cond_broadcast(&m_evCond);
	}
	pthread_mutex_unlock(&m_evMutex);
}


void CEventSyncObject::Reset()
{
	pthread_mutex_lock(&m_evMutex);
	m_bSignalled = false;
	pthread_mutex_unlock(&m_evMutex);
}


bool CEventSyncObject::Wait(int msecsWaitTime)
{
	pthread_mutex_lock(&m_evMutex);

	if( m_bSignalled )
	{
		m_bSignalled = false;
		pthread_mutex_unlock(&m_evMutex);
		return true;
	}

	bool bRes;
	if( msecsWaitTime >= 0 )
	{
#if defined(_MAC_)
		struct timespec tsDelay;
		tsDelay.tv_nsec = (msecsWaitTime % 1000) * 1000000;
        tsDelay.tv_sec = msecsWaitTime/1000;
		int res = pthread_cond_timedwait_relative_np(&m_evCond, &m_evMutex, &tsDelay);
#else
        struct timeval tv;
        gettimeofday(&tv, 0);

        timespec ti;
        ti.tv_nsec = (tv.tv_usec + (msecsWaitTime % 1000) * 1000) * 1000;
        ti.tv_sec = tv.tv_sec + (msecsWaitTime / 1000) + (ti.tv_nsec / 1000000000);
        ti.tv_nsec %= 1000000000;

        int res = pthread_cond_timedwait(&m_evCond, &m_evMutex, &ti);
#endif
		bRes = (res != ETIMEDOUT);
	}
	else
	{
		bRes = true;
		pthread_cond_wait(&m_evCond, &m_evMutex);
	}

	m_bSignalled = false;

	pthread_mutex_unlock(&m_evMutex);

	return bRes;
}
