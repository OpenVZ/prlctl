/*
 * @file EventSyncObject.h
 *
 * Autoreseting event synch object
 *
 * @author sdmitry
 *
 * Copyright (c) 2007-2017, Parallels International GmbH
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

#ifndef EventSyncObject_h__
#define EventSyncObject_h__

#if defined(_WIN_)
#include <windows.h>
#endif

#if defined(_MAC_) || defined(_LIN_)
#include <pthread.h>
#endif

#include <stdexcept>

/// @class CEventSyncObject
/// Event Object.
/// Initially nonsignalled
/// Auto resetting event (each successfull wait resets object)
class CEventSyncObject
{
public:
    CEventSyncObject();
    ~CEventSyncObject();
    /// Wait for event for msecsWaitTime.
    /// If <0 specified, will wait forever.
    /// @return true if event is signalled, false on timeout
    bool Wait(int msecsWaitTime = -1);

    /// Signals an event.
    void Signal();

    /// Reset event
    void Reset();

#if defined(_WIN_)
    HANDLE Handle()
    {
        return m_hEvent;
    }
#endif


private:

#if defined(_WIN_)
    HANDLE m_hEvent;
#elif defined(_MAC_) || defined(_LIN_)
    volatile bool           m_bSignalled;

    pthread_mutex_t m_evMutex;
    pthread_cond_t  m_evCond;
#else
#error "System macro is not defined"
#endif

};

#endif // EventSyncObject_h__
