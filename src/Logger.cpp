/*
 * @file Logger.cpp
 *
 * Logging helper functions
 *
 * @author igor@
 *
 * Copyright (c) 2005-2017, Parallels International GmbH
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

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>


#include "Logger.h"


struct LogParam {
	int enable;             /**< enable/disable logging. */
	int quiet;              /**< skip logging to stdout. */
	int verbose;            /**< Console verbosity. */
	char prog[32];          /**< program name. */
};
static struct LogParam _g_log = {
	1,      /* enable */
	0,      /* quiet */
	0,     /* verbose */
	"",
};

#define LOG_BUF_SIZE    8192

static void logger_ap(int level, int err_no, const char *format, va_list ap)
{
	char buf[LOG_BUF_SIZE];
	unsigned int r;
	int errno_tmp = errno;

	r = vsnprintf(buf, sizeof(buf), format, ap);
	if ((r < sizeof(buf) - 1) && err_no) {
		sprintf(buf + r, ": %s", strerror(err_no));
	}
	if (_g_log.enable) {
		if (!_g_log.quiet && _g_log.verbose >= level) {
			fprintf((level < 0 ? stderr : stdout), "%s\n", buf);
			fflush(level < 0 ? stderr : stdout);
		}
	}
	errno = errno_tmp;
}

void prl_log(int level, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	logger_ap(level, 0, format, ap);
	va_end(ap);
}

int prl_set_log_verbose(int verbose)
{
	int tmp;

	tmp = _g_log.verbose;
	_g_log.verbose = (verbose < -1 ? -1 : verbose);
	return tmp;
}

int prl_get_log_verbose()
{
	return (_g_log.verbose);
}

int prl_set_log_enable(int enable)
{
	int tmp;

	tmp = _g_log.enable;
	_g_log.enable = enable;
	return tmp;
}

int prl_err(int err, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	logger_ap(-1, 0, format, ap);
	va_end(ap);

	return err;
}
