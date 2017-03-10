/*
 * @file Logger.h
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

#ifndef __LOGGER_H__
#define __LOGGER_H__

#define L_ERR		-1
#define L_NORMAL	0
#define L_INFO		1
#define L_WARN		2
#define L_DEBUG		3


void prl_log(int level, const char *format, ...);
int prl_set_log_verbose(int verbose);
int prl_set_log_enable(int enable);
int prl_err(int err, const char *format, ...);
int prl_get_log_verbose();

#endif
