#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vim:ts=4:sw=4:noexpandtab
#
# ==================================================================
#
# Copyright (c) 2012-2017, Parallels International GmbH. All Rights Reserved.
# Copyright (c) 2017-2019 Virtuozzo International GmbH. All rights reserved.
#
# This file is part of OpenVZ. OpenVZ is free software; you can redistribute
# it and/or modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#
# Our contact details: Virtuozzo International GmbH, Vordergasse 59, 8200
# Schaffhausen, Switzerland.
#
# local login for pmigrate
#
# ==================================================================
#

import sys
import prlsdkapi

prlsdkapi.init_server_sdk()

max_wait_timeout = 10*1000 #tmo 10 seconds

try:
    server = prlsdkapi.Server()

    job = server.login_local()
    result = job.wait( max_wait_timeout )
    login_response = result.get_param()

    # The preferences info is obtained as a prlsdkapi.DispConfig object.
    result = server.get_common_prefs().wait()
    disp_config = result.get_param()

    print 'sessionid=%s' % login_response.get_session_uuid()
    print 'securitylevel=%d' % disp_config.get_min_security_level()
    sys.stdout.flush()

    # wait reply from source side
    raw_input ("")
    job = server.logoff()
    job.wait( max_wait_timeout )
except Exception,e :
    print 'error caught: e=%s' % e
    sys.exit(1)
except prlsdkapi.PrlSDKError, e:
    print "Error: %s" % e
    sys.exit(1)

sys.exit(0)

