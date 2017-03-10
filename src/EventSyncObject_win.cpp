/*
 * @file EventSyncObject_win.cpp
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

CEventSyncObject::CEventSyncObject()
{
	m_hEvent = ::CreateEvent( NULL, FALSE, FALSE, NULL );
	if( NULL == m_hEvent )
	{
		throw std::domain_error( "::CreateEvent() failed" );
	}
}


CEventSyncObject::~CEventSyncObject()
{
	if( NULL != m_hEvent )
		::CloseHandle( m_hEvent );
}


bool CEventSyncObject::Wait( int msecsWaitTime )
{
	DWORD waitTime = (msecsWaitTime >= 0) ? msecsWaitTime : INFINITE;
	return (WAIT_OBJECT_0 == ::WaitForSingleObject( m_hEvent, waitTime ));
}


void CEventSyncObject::Signal()
{
	::SetEvent( m_hEvent );
}


void CEventSyncObject::Reset()
{
	::ResetEvent( m_hEvent );
}
