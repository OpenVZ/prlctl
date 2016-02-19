#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vim:ts=4:sw=4:noexpandtab
#
# ==================================================================
#
# Copyright (c) 2012-2016 Parallels IP Holdings GmbH. All Rights Reserved
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

