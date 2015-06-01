///////////////////////////////////////////////////////////////////////////////
///
/// @file Utils.cpp
///
/// Miscellaneous helper functions
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

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <algorithm>
#include <map>

#ifndef _WIN_
#include <termios.h>
#include <unistd.h>
#include <pwd.h>
#else
#include <windows.h>
#include <time.h>
#include <conio.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#define snprintf _snprintf
#define strtoull _strtoui64
#endif
#include <signal.h>
#include <errno.h>

#include <fstream>
#include <sstream>
#include <assert.h>
#include <stdarg.h>

#include <PrlErrorsValues.h>

// #include "Interfaces/ParallelsDomModel.h"
#define EVT_PARAM_OP_RC								"op_rc"
#define EVT_PARAM_PROGRESS_STAGE					"progress_stage"

#include <PrlOses.h>
#include <PrlApiDeprecated.h>
#include <PrlApiDisp.h>

#include "Utils.h"
#include "CmdParam.h"
#include "PrlDev.h"
#include "Logger.h"
#include "PrlCleanup.h"

static volatile int signo;
extern bool g_problem_report_cmd;
extern const char *capnames[NUMCAP];
PRL_UINT32 g_nJobTimeout = JOB_INFINIT_WAIT_TIMEOUT;

/**
 * Load parallels SDK library symbols.
 */
int init_sdk_lib()
{
#ifdef DYN_API_WRAP
	if (!SdkWrap_LoadLibFromStdPaths(prl_get_log_verbose() > L_NORMAL))
	{
		fprintf(stderr, "Failed to load SDK library\n");
		return -1;
	}
#endif

	// Disable logging output to console
	PrlApi_SwitchConsoleLogging(0);

	//Install Parallels crash handler
	PrlApi_InitCrashHandler(0);

	PRL_UINT32 nFlags = 0;
	nFlags |= g_problem_report_cmd ? PAIF_USE_GRAPHIC_MODE : 0;

	PRL_RESULT ret;
	ret = PrlApi_InitEx(PARALLELS_API_VER,
			PAM_SERVER,
			nFlags,
			0);
	if (PRL_FAILED(ret)) {
		fprintf(stderr, "PrlApi_Init returned the following error: (%s)\n",
				get_error_str(ret).c_str());
		return -1;
	}

	PrlApi_SetLogPath(NULL);

	return 0;
}

void deinit_sdk_lib()
{
#ifdef DYN_API_WRAP
	if (SdkWrap_IsLoaded())
	{
#endif
		PrlApi_Deinit();
#ifdef DYN_API_WRAP
		SdkWrap_Unload();
	}
#endif
	fflush(stdout);
	fflush(stderr);
}

static int problem_report_cb(PRL_HANDLE hEvent, void *)
{
	PrlHandle h(hEvent);
	PRL_HANDLE_TYPE type;
	int ret;

	if ((ret = PrlHandle_GetType(h.get_handle(), &type))) {
		prl_log(L_DEBUG, "PrlHandle_GetType: %s",
				get_error_str(ret).c_str());
		return ret;
	}
	if (type == PHT_EVENT) {
		PRL_EVENT_TYPE evt_type;

		if ((ret = PrlEvent_GetType(h.get_handle(), &evt_type))) {
			prl_log(L_DEBUG, "PrlEvent_GetType: %s",
					get_error_str(ret).c_str());
			return ret;
		}
		if (evt_type == PET_DSP_EVT_JOB_PROGRESS_CHANGED) {

			PrlHandle hPrm;
			ret = PrlEvent_GetParam(h.get_handle(), 0, hPrm.get_ptr());
			if (PRL_FAILED(ret)) {
				prl_log(L_DEBUG, "PrlEvent_GetParam %s",
						get_error_str(ret).c_str());
				return ret;
			}

			PRL_UINT32 val;
			ret = PrlEvtPrm_ToUint32(hPrm.get_handle(), &val);
			if (PRL_FAILED(ret)) {
				prl_log(L_DEBUG, "PrlEvtPrm_ToUint32 %s",
						get_error_str(ret).c_str());
				return ret;
			}
			print_procent(val);
		}
	}
	return 0;
}

/* Parse audentification line in format:
   [proto://][[user][:passwd]@]server[:port]
*/
static int parse_url(const std::string &str, LoginInfo &login)
{
	std::string url;
	std::string user;
	std::string::size_type len;
	std::string::size_type pos;

	/* skip proto */
	pos = str.find("://");
	if (pos != std::string::npos)
		url = str.substr(pos + 3);
	else
		url = str;
	len = url.length();
	pos = url.find_first_of("@");
	if (pos != std::string::npos) {
		user = url.substr(0, pos);
		pos++;
	} else {
		pos = 0;
	}
	/* server:port */
	std::string::size_type port_pos = url.find_first_of(":", pos);
	std::string::size_type server_len = 0;
	if (port_pos != std::string::npos) {
		std::string::size_type port_len = len - port_pos - 1;
		if (port_len != 0) {
			std::string port = url.substr(port_pos + 1, port_len);
			login.port = atoi(port.c_str());
			server_len = port_pos - pos;
		}
	} else {
		server_len = len - pos;
	}
	if (server_len <= 0) // server is not specified
		return -1;
	login.server = url.substr(pos, server_len);

	/* user:passwd */
	std::string::size_type user_pos = user.find_last_of(":");
	if (user_pos != std::string::npos) {
		login.user = user.substr(0, user_pos);
		login.get_passwd_buf() = user.substr(user_pos + 1);
	} else {
		login.user = user;
	}

	return 0;
}

int send_problem_report(const PrlHandle &hProblemReport, const ProblemReportParam &param)
{
	PRL_RESULT ret;
	PRL_UINT32 resultCount = 0;
	PrlHandle hResult;
	bool bUseProxy = !param.dont_use_proxy, ok = false;
	std::string sHost, sUser, sPassword;
	int nPort = 0;
	LoginInfo url;
	unsigned int timeout = JOB_INFINIT_WAIT_TIMEOUT == g_nJobTimeout ? JOB_WAIT_TIMEOUT : g_nJobTimeout;

	if (!param.proxy_settings.empty()) {
		parse_url(param.proxy_settings, url);
		sHost = url.server;
		sUser = url.user;
		sPassword = url.get_passwd_from_stack(ok);
		nPort = url.port;
	}

	PrlHandle hJob(PrlReport_Send(hProblemReport.get_handle(),
		bUseProxy,
		(sHost.empty() ? 0 : sHost.c_str()),
		nPort,
		(sUser.empty() ? 0 : sUser.c_str()),
		(sPassword.empty() ? 0 : sPassword.c_str()),
		timeout,
		0, problem_report_cb, 0));

	const PrlHook *h = get_cleanup_ctx().register_hook(cancel_job, hJob.get_handle());
	ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &resultCount, timeout);
	get_cleanup_ctx().unregister_hook(h);
	if (ret == 0) {
		char data[128];
		PRL_UINT32 len = sizeof(data);
		if ((ret = PrlResult_GetParamAsString(hResult.get_handle(), data, &len)))
			return prl_err(-1, "PrlResult_GetParamAsString: %s",
					get_error_str(ret).c_str());
		else
			prl_log(0, "\nThe problem report was successfully sent with id: %d", atoi(data));
	} else {
		prl_log(L_ERR, "\nFailed to send problem report: %s",
				get_error_str(ret).c_str());
	}
	return ret;
}

int send_problem_report_on_stdout(const PrlHandle &hProblemReport)
{
	int ret = 0;
	PRL_PROBLEM_REPORT_SCHEME nReportScheme = PRS_NEW_PACKED;
	if ((ret = PrlReport_GetScheme(hProblemReport.get_handle(), &nReportScheme)))
		return prl_err(-1, "Failed to get problem report scheme: %s",
			get_error_str(ret).c_str());

	if (PRS_OLD_XML_BASED == nReportScheme)
	{
		unsigned int len = 0;
		if ((ret = PrlReport_AsString(hProblemReport.get_handle(), 0, &len)))
			return prl_err(-1, "PrlReport_AsString: %s",
				get_error_str(ret).c_str());
		char *data = (char *) malloc(len + 1);
		if (data == NULL)
			return prl_err(-1, "Unable to allocate %d bytes", len);
		if ((ret = PrlReport_AsString(hProblemReport.get_handle(), data, &len)))
			return prl_err(-1, "PrlReport_AsString: %s",
				get_error_str(ret).c_str());
		fprintf(stdout, "%s\n", data);
		free(data);
	}
	else
	{
		PRL_UINT32 nBufferSize = 0;
		if ((ret = PrlReport_GetData(hProblemReport.get_handle(), 0, &nBufferSize)))
			return prl_err(-1, "PrlReport_GetData: %s",
				get_error_str(ret).c_str());

		if (nBufferSize == 0)
			return prl_err(-1, "An empty problem report received");

		void *data = malloc(nBufferSize);
		if (data == NULL)
			return prl_err(-1, "Unable to allocate %d bytes", nBufferSize);
		if ((ret = PrlReport_GetData(hProblemReport.get_handle(), data, &nBufferSize)))
			return prl_err(-1, "PrlEvtPrm_GetBuffer: %s",
				get_error_str(ret).c_str());


#ifdef _WIN_
		//Switch stdout to the binary mode - https://bugzilla.sw.ru/show_bug.cgi?id=462039
		int nRetCode = _setmode(_fileno(stdout), _O_BINARY);
		(void *)nRetCode;
		assert(-1 != nRetCode);
#endif
		ret = fwrite(data, nBufferSize, 1, stdout);
		free(data);
	}
	return 0;
}

int assembly_problem_report(const PrlHandle &hProblemReport, const ProblemReportParam &param, PRL_UINT32 flags)
{
	PrlHandle hResult;
	PRL_UINT32 resultCount = 0;
	int ret = 0;
	if ((ret = PrlReport_SetUserName(hProblemReport.get_handle(), param.user_name.c_str())) != 0)
				prl_log(L_ERR, "Failed to set user name: %s", get_error_str(ret).c_str());
	if ((ret = PrlReport_SetUserEmail(hProblemReport.get_handle(), param.user_email.c_str())) != 0)
				prl_log(L_ERR, "Failed to set user E-mail: %s", get_error_str(ret).c_str());
	if ((ret = PrlReport_SetDescription(hProblemReport.get_handle(), param.description.c_str())) != 0)
				prl_log(L_ERR, "Failed to set description: %s", get_error_str(ret).c_str());
	PrlHandle hJob(PrlReport_Assembly(hProblemReport.get_handle(), PPRF_ADD_CLIENT_PART | (param.stand_alone ? PPRF_ADD_SERVER_PART : 0) | flags));
	return get_job_result(hJob.get_handle(), hResult.get_ptr(), &resultCount);
}

PrlBase* PrlBase::free()
{
    PrlHandle_Free(m_handle) ;
    m_handle = PRL_INVALID_HANDLE ;
    return this ;
}

static void get_op_rc(PrlHandle &hEvent, PRL_RESULT &retcode)
{
	PRL_UINT32 i, nCount = 0, len, rc;

	PrlEvent_GetParamsCount(hEvent.get_handle(), &nCount);
	for (i = 0; i < nCount; i++) {
		char buf[100];
		PrlHandle hParam;

		PrlEvent_GetParam(hEvent.get_handle(), i, hParam.get_ptr());
		len = sizeof(buf);
		PrlEvtPrm_GetName(hParam.get_handle(), buf, &len);

		if (!strncmp(buf, EVT_PARAM_OP_RC, sizeof(EVT_PARAM_OP_RC) -1) &&
				PrlEvtPrm_ToUint32(hParam.get_handle(), &rc) == 0)
		{
			retcode = (PRL_RESULT)rc;
			break;
		}
	}
}

PRL_RESULT get_job_retcode(PRL_HANDLE hJob, std::string &err,
	unsigned int timeout)
{
	PRL_RESULT ret, retcode;

	err.clear();
	if ((ret = PrlJob_Wait(hJob, (JOB_INFINIT_WAIT_TIMEOUT == timeout ? g_nJobTimeout : timeout)))) {
		err = "PrlJob_Wait: " + get_error_str(ret);
		return ret;
	}
	if ((ret = PrlJob_GetRetCode(hJob, &retcode))) {
		err = "PrlJob_GetRetCode: " +
			get_error_str(ret);
		return ret;
	}
	if (retcode) {
		/* In case error get error message from the job,
		   if failed get from the retcode.
		*/
		PrlHandle hErr;
		if ((ret = PrlJob_GetError(hJob, hErr.get_ptr()))) {
			err = "PrlJob_GetError: " + get_error_str(ret);
			return ret;
		}

		if (get_result_error_string(hErr.get_handle(), err))
			err = get_error_str(retcode);

		/* #PSBM-27689 report vzctl specific error code */
		if (retcode == PRL_ERR_VZCTL_OPERATION_FAILED)
			get_op_rc(hErr, retcode);

	}

	return retcode;
}

PRL_RESULT get_job_result(PRL_HANDLE hJob, PRL_HANDLE_PTR hResult,
		PRL_UINT32_PTR resultCount, unsigned int timeout)
{
	PRL_RESULT ret;
	std::string err;

	if ((ret = get_job_retcode(hJob, err, timeout)))
		return prl_err(ret, "%s", err.c_str());
	if ((ret = PrlJob_GetResult(hJob, hResult)))
		return prl_err(ret, "PrlJob_GetResult returned the following error:"
				" %s [%d]",
				get_error_str(ret).c_str(), ret);
	if ((ret = PrlResult_GetParamsCount(*hResult, resultCount)))
		return prl_err(ret, "PrlResult_GetParamsCount returned the following"
			" error: %s [%d]",
			get_error_str(ret).c_str(), ret);
	return ret;
}

int get_result_as_string(PRL_HANDLE hResult, std::string &out, bool xml)
{
	PRL_RESULT ret;
	PRL_UINT32 len, count;
	char *buf;

	len = 0;
	if ((ret = PrlResult_GetParamsCount(hResult, &count)))
		return prl_err(-1, "PrlResult_GetParamsCount: %s",
				get_error_str(ret).c_str());
	if (count == 0)
		return 0;

	if ((ret = PrlResult_GetParamAsString(hResult, 0, &len)))
		return prl_err(-1, "PrlResult_GetParamAsString: %s",
				get_error_str(ret).c_str());

	if ((buf = (char*) malloc(len + 1)) == NULL)
		return prl_err(-1, "Unable to allocate %d bytes", len);

	if ((ret = PrlResult_GetParamAsString(hResult, buf, &len))) {
		free(buf);
		return prl_err(-1, "PrlResult_GetParamAsString: %s",
				get_error_str(ret).c_str());
	}

	if (xml) {
		char *src, *dst;
		int c;

		src = dst = buf;
		while (*src++) {
			/* translate &#xd; -> \r */
			if (src[0] == '&' && src[1] == '#' && src[2] == 'x') {
				if (sscanf(src + 3, "%x", &c) == 1) {
					*dst = (char) c;
					while (*src && *src != ';') src++;
				} else {
					*dst = *src;
				}
			} else {
				*dst = *src;
			}
			dst++;
		}
		*dst = 0;
	}

	out = buf;
	free(buf);
	return 0;
}

PRL_RESULT get_job_result_object(PRL_HANDLE hJob, PRL_HANDLE_PTR hObject, unsigned int timeout)
{
	PrlHandle hResult;
	PRL_RESULT ret;
	PRL_UINT32 resultCount ;
	ret = get_job_result(hJob, hResult.get_ptr(), &resultCount, timeout);
	if (PRL_FAILED(ret))
		return ret;

	if ((ret = PrlResult_GetParam(hResult.get_handle(), hObject)))
		return prl_err(ret, "PrlResult_GetParams: %s",
				get_error_str(ret).c_str(), ret);
	return ret;
}

/** Retruns string representation of error code */
std::string get_error_str(int nErrCode)
{
	char sErrorBuf[4096];
	PRL_UINT32 nErrorBufLength = sizeof(sErrorBuf);
	PRL_RESULT ret;
	std::string result;

	// get first part of error
	ret = PrlApi_GetResultDescription(nErrCode, PRL_TRUE, PRL_FALSE,
			sErrorBuf, &nErrorBufLength);
	if (PRL_SUCCEEDED(ret))
		result = sErrorBuf;

	// get second part of error
	nErrorBufLength = sizeof(sErrorBuf);
	ret = PrlApi_GetResultDescription(nErrCode, PRL_FALSE, PRL_FALSE,
		sErrorBuf, &nErrorBufLength);
	if (PRL_SUCCEEDED(ret)) {
		if (!(result == sErrorBuf)) {
			result += " ";
			result += std::string(sErrorBuf);
		}
	}

	return result;
}

PRL_RESULT get_result_error_string(PRL_HANDLE hResult, std::string &err)
{
	//PrlHandle h(hResult);
	char buf[1024], buf2[1024];
	unsigned int len;
	PRL_RESULT ret, retcode;
	char codebuf[64];
	std::string codestr;

	err = "";
	if ((ret = PrlEvent_GetErrCode(hResult, &retcode)))
	{
		prl_log(L_DEBUG, "PrlEvent_GetErrCode: %s",
			get_error_str(ret).c_str());
		return ret;
	}
	if (snprintf(codebuf, sizeof(codebuf)-1, "Error code: %d", retcode) > 0)
		codestr = codebuf;

	len = sizeof(buf);
	if ((ret = PrlEvent_GetErrString(hResult, PRL_TRUE, PRL_FALSE, buf, &len )))
	{
		prl_log(L_DEBUG, "PrlEvent_GetErrString: %s",
			    get_error_str(ret).c_str());
		err = codestr;
		return ret;
	}

	len = sizeof(buf2);
	if ((ret = PrlEvent_GetErrString(hResult, PRL_FALSE, PRL_FALSE, buf2, &len )))
	{
	    prl_log(L_DEBUG, "PrlEvent_GetErrString: %s",
			get_error_str(ret).c_str());
	    err = codestr;
	    return ret;
	}

	err = buf;
	if (strcmp(buf, buf2)) {
		err += " ";
		err += buf2;
	}

	return ret;
}


using namespace std;

/* Parse audentification line in format:
   user[[:passwd]@server[:port]]
*/
int parse_auth(const std::string &auth, LoginInfo &login,
		char *hide_passwd)
{
	std::string user;
	string::size_type port_pos = string::npos;
	string::size_type server_len;
	string::size_type len = auth.length();
	string::size_type pos = auth.find_last_of("@");

	if (pos == string::npos) {
		user = "root";
		pos = 0;
	} else {
		user = auth.substr(0, pos);
		pos++;
	}

	bool ipv6 = false;
	unsigned int count = 0;
	for (unsigned int i = pos; i < len; i++)
		if (auth[i] == ':')
			count++;
	if (count > 1)
		ipv6 = true;

	/* server:port */
	if (auth[pos] == '[') {
		string::size_type ip6_pos = auth.find_last_of("]");
		if (ip6_pos == string::npos)
			return -1;
		port_pos = auth.find_first_of(":", ip6_pos);
		pos++;
		len--;
	} else {
		if (!ipv6)
			port_pos = auth.find_first_of(":", pos);
	}
	if (port_pos != string::npos) {
		string::size_type port_len = auth.length() - port_pos - 1;
		if (port_len == 0)
			return -1;
		std::string port = auth.substr(port_pos + 1, port_len);
		login.port = atoi(port.c_str());
		server_len = port_pos - pos;
	} else
		server_len = len - pos;
	if (server_len <= 1) // server is not specified
		return -1;
	login.server = auth.substr(pos, server_len);

	/* user:passwd */
	string::size_type user_pos = user.find_last_of(":");
	if (user_pos != string::npos) {
		login.user = user.substr(0, user_pos);
		login.get_passwd_buf() = user.substr(user_pos + 1);
#ifdef _LIN_
		if (hide_passwd != NULL &&
			user_pos < strlen(hide_passwd))
		{
			int len, n;

			len = strlen(hide_passwd + user_pos + 1);
			n = user.length() - (user_pos + 1);
			if (n > len)
				n = len;
			memset(hide_passwd + user_pos + 1, '*', n);
		}
#else
		(void)hide_passwd;
#endif
	} else {
		login.user = user;
	}

	if (login.user.empty())
		return -1;
	if (login.server.empty())
		login.server = "127.0.0.1";
	return 0;
}


int parse_userpw(const std::string &userpw, std::string &user, std::string &pw)
{
	string::size_type user_pos = userpw.find_first_of(":");
	if (user_pos == string::npos) {
		user = userpw;
		return 0;
	}

	user = userpw.substr(0, user_pos);
	pw = userpw.substr(user_pos + 1);

	return 0;
}

std::string parse_mac(const char *mac)
{
	char buf[3];
	char *endptr;
	const char *sp = mac;
	const char *ep = mac + strlen(mac);

	if (!strcmp(mac, "auto"))
		return std::string("auto");

	std::string out;
	while (sp < ep) {
		long val;

		buf[0] = *sp++;
		buf[1] = *sp++;
		if (sp > ep)
			return std::string();

		buf[2] = '\0';
		val = strtol(buf, &endptr, 16);
		(void)val;
		if (*endptr != '\0')
			return std::string();

		out += buf;
		/* skip ':' to convert to the SDK mac representation
		   00:18:F3:F0:0D:A0 -> 0018F3F00DA0
		*/
		if (*sp == ':')
			sp++;
	}
	if (out.length() != 12)
		return std::string();
	return out;
}

const char *vmstate2str(VIRTUAL_MACHINE_STATE nVmState)
{
	switch (nVmState) {
	case VMS_STOPPED: return "stopped";
	case VMS_STARTING: return "starting";
	case VMS_RESTORING: return "restoring";
	case VMS_RUNNING: return "running";
	case VMS_PAUSED: return "paused";
	case VMS_RESETTING: return "resetting";
	case VMS_PAUSING: return "pausing";
	case VMS_SUSPENDING: return "suspending";
	case VMS_STOPPING: return "stopping";
	case VMS_COMPACTING: return "compacting";
	case VMS_SUSPENDED: return "suspended";
	case VMS_SNAPSHOTING: return "snapshoting";
	case VMS_CONTINUING: return "continuing";
	case VMS_MIGRATING: return "migrating";
	case VMS_DELETING_STATE: return "del_snapshot";
	case VMS_RESUMING: return "resuming";
	case VMS_SUSPENDING_SYNC: return "syncing";
	case VMS_UNKNOWN: return "invalid";
	case VMS_RECONNECTING: return "reconnecting";
	case VMS_MOUNTED: return "mounted";
	}
	return "unknown";
}

int str2dev_assign_mode(const std::string &str)
{
	if (str == "host")
		return AM_HOST;
	else if (str == "vm")
		return AM_VM;
	else
		return AM_NONE;
}

const char *dev_assign_mode2str(int mode)
{
	switch (mode) {
	case AM_HOST: return "host";
	case AM_VM: return "vm";
	}
	return "-";
}

#ifndef _WIN_
static void term_printf(const char *format, ...)
{
	FILE *f = fopen("/dev/tty", "w");
	if (!f) {
		prl_err(-1, "Failed to open /dev/tty: %m (%d)", errno);
		return;
	}

	va_list ap;
	va_start(ap, format);
	vfprintf(f, format, ap);
	va_end(ap);

	fclose(f);
}

static void handler(int sig)
{
	signo = sig;
}

static int read_passwd_helper(std::string &passwd)
{
	struct termios term, old_term;
	struct sigaction sa, old_sa_int, old_sa_hup;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = handler;
	/* Fixme: should be also handled:
	   SIGQUIT, SIGTERM, SIGTSTP, SIGTTIN, SIGTTOU
	 */
	sigaction(SIGINT, &sa, &old_sa_int);
	sigaction(SIGHUP, &sa, &old_sa_hup);
	signo = 0;
	// turn off echo
	if (tcgetattr(STDIN_FILENO, &old_term) == 0) {
		memcpy(&term, &old_term, sizeof(term));
		term.c_lflag &= ~(ECHO | ECHONL);
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
	}
	char c;
	while ( std::cin.good() && ( c = std::cin.get() ) != '\n' )
		passwd += c;
	if (!(term.c_lflag & ECHO))
		term_printf("\n");
	// restore terminal
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term);
	sigaction(SIGINT, &old_sa_int, NULL);
	sigaction(SIGHUP, &old_sa_hup, NULL);

	return (signo != 0);
}

static bool is_terminal_input()
{
	return !!isatty(STDIN_FILENO);
}

#else

static int WindowsTerminal_getConsoleMode(DWORD *mode)
{
	HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);

	if (hConsole == INVALID_HANDLE_VALUE)
		return -1;
	if (!GetConsoleMode(hConsole, mode))
		return -1;
	return 0;
}

static int WindowsTerminal_setConsoleMode(DWORD mode)
{
	HANDLE hConsole = GetStdHandle (STD_INPUT_HANDLE);

	if (hConsole == INVALID_HANDLE_VALUE)
		return -1;
	SetConsoleMode(hConsole, mode);
	return 0;
}

static int read_passwd_helper(std::string &passwd)
{
    HANDLE hInputDevice = GetStdHandle (STD_INPUT_HANDLE);

    DWORD deviceType =  GetFileType(hInputDevice);
    DWORD mode;

    if (deviceType == FILE_TYPE_CHAR)
    {
        if (WindowsTerminal_getConsoleMode(&mode))
            return -1;
        WindowsTerminal_setConsoleMode(mode & ~ENABLE_ECHO_INPUT);
    };


    std::cin >> passwd;

    if (deviceType == FILE_TYPE_CHAR)
        WindowsTerminal_setConsoleMode(mode);

    fputc('\n', stderr);

	return 0;
}

static void term_printf(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	/* TODO: implement proper printing to terminal instead of stderr */
	vfprintf(stderr, format, ap);
	va_end(ap);
}

static bool is_terminal_input()
{
	/* TODO: implement this */
	return true;
}

#endif

int read_passwd(const std::string &name, const std::string &server,
		std::string &passwd)
{
	term_printf("%s@%s's password: ", name.c_str(), server.c_str());
	return read_passwd_helper(passwd);
}

int read_passwd_for_vm(std::string &passwd, const std::string &vm_name, const std::string &prompt)
{
	if (!vm_name.empty())
		term_printf("Virtual machine '%s' is encrypted - password required to continue operation\n",
			vm_name.c_str());
	if (prompt.empty())
		term_printf("Please enter password: ");
	else
		term_printf("%s", prompt.c_str());
	return read_passwd_helper(passwd);
}

int read_passwd(std::string &passwd)
{
	return read_passwd_for_vm(passwd, "", "");
}

int read_passwd(std::string &passwd, const std::string &prompt)
{
	/* if we have a terminal behind stdin - prompt user to enter the password */
	if (is_terminal_input()) {
		if (prompt.empty())
			term_printf("Please enter password: ");
		else
			term_printf("%s", prompt.c_str());
		return read_passwd_helper(passwd);
	}
	/* read from stdin if no terminal available */
	std::getline(std::cin, passwd);
	return 0;
}

int file2str(const char *filename, std::string &out)
{
	char buffer[4096];
	std::ostringstream to;
	std::ifstream from(filename);

	if (!from)
		return prl_err(-1, "Failed to open %s", filename);
	while (from.read(buffer, sizeof(buffer))
			|| from.gcount() > 0)
		if (!to.write(buffer, from.gcount()))
			return prl_err(-1, "Failed to write %d bytes",
				from.gcount());
	out = to.str();
	return 0;
}


static struct PrlTypeMap {
	PRL_HANDLE_TYPE hType;
	PRL_DEVICE_TYPE dType;
	DevType type;
} prl_type_map[] = {
{PHT_VIRTUAL_DEV_HARD_DISK,	PDE_HARD_DISK,		DEV_HDD},
{PHT_VIRTUAL_DEV_NET_ADAPTER,	PDE_GENERIC_NETWORK_ADAPTER, DEV_NET},
{PHT_VIRTUAL_DEV_OPTICAL_DISK,	PDE_OPTICAL_DISK,	DEV_CDROM},
{PHT_VIRTUAL_DEV_FLOPPY,	PDE_FLOPPY_DISK,	DEV_FDD},
{PHT_VIRTUAL_DEV_USB_DEVICE,	PDE_USB_DEVICE,		DEV_USB},
{PHT_VIRTUAL_DEV_SERIAL_PORT,	PDE_SERIAL_PORT,	DEV_SERIAL},
{PHT_VIRTUAL_DEV_PARALLEL_PORT, PDE_PARALLEL_PORT,	DEV_PARALLEL},
{PHT_VIRTUAL_DEV_SOUND,		PDE_SOUND_DEVICE,	DEV_SOUND},
{PHT_VIRTUAL_DEV_GENERIC_PCI,	PDE_GENERIC_PCI_DEVICE, DEV_GENERIC_PCI},
{PHT_VIRTUAL_DEV_DISPLAY,      PDE_PCI_VIDEO_ADAPTER,  DEV_GENERIC_PCI},
};

DevType handle2type(PRL_HANDLE_TYPE type)
{
	unsigned int i;

	for (i = 0; i < sizeof(prl_type_map)/sizeof(prl_type_map[0]); ++i)
		if (prl_type_map[i].hType == type)
			return prl_type_map[i].type;
	return DEV_NONE;
}

DevType prl_dev_type2type(PRL_DEVICE_TYPE type)
{
	unsigned int i;

	for (i = 0; i < sizeof(prl_type_map)/sizeof(prl_type_map[0]); ++i)
		if (prl_type_map[i].dType == type)
			return prl_type_map[i].type;
	return DEV_NONE;
}

PRL_HANDLE_TYPE type2handle(DevType type)
{
	unsigned int i;

	for (i = 0; i < sizeof(prl_type_map)/sizeof(prl_type_map[0]); ++i)
		if (prl_type_map[i].type == type)
			return prl_type_map[i].hType;
	assert(0);
	return PHT_ERROR;
}

PRL_DEVICE_TYPE type2prl_dev_type(DevType type)
{
	unsigned int i;

	for (i = 0; i < sizeof(prl_type_map)/sizeof(prl_type_map[0]); ++i)
		if (prl_type_map[i].type == type)
			return prl_type_map[i].dType;
	assert(0);
	return PDE_MAX;
}

static OsDistribution def_dist_map [] = {
        {"windows", PVS_GUEST_TYPE_WINDOWS, PVS_GUEST_VER_WIN_2008},
		{"win", PVS_GUEST_TYPE_WINDOWS, PVS_GUEST_VER_WIN_2008},
        {"linux", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_CENTOS},
		{"lin", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_CENTOS},
	{"macos", PVS_GUEST_TYPE_MACOS, PVS_GUEST_VER_MACOS_UNIVERSAL},
        {"freebsd", PVS_GUEST_TYPE_FREEBSD, PVS_GUEST_VER_BSD_5X},
        {"os2", PVS_GUEST_TYPE_OS2, PVS_GUEST_VER_OS2_WARP4},
        {"msdos", PVS_GUEST_TYPE_MSDOS, PVS_GUEST_VER_DOS_MS622},
        {"netware", PVS_GUEST_TYPE_NETWARE, PVS_GUEST_VER_NET_5X},
        {"solaris", PVS_GUEST_TYPE_SOLARIS, PVS_GUEST_VER_SOL_10},
        {"other", PVS_GUEST_TYPE_OTHER, PVS_GUEST_VER_OTH_OTHER}
};

const OsDistribution *get_def_dist(const std::string &name)
{
        unsigned int i;

        for (i = 0; i < sizeof(def_dist_map)/sizeof(*def_dist_map); ++i)
                if (name == def_dist_map[i].name)
                        return &def_dist_map[i];
        return 0;
}

static OsDistribution dist_map[] = {
	/* Windosws */

	{"win-2000", PVS_GUEST_TYPE_WINDOWS, PVS_GUEST_VER_WIN_2K},
	{"win-xp", PVS_GUEST_TYPE_WINDOWS, PVS_GUEST_VER_WIN_XP},
	{"win-2003", PVS_GUEST_TYPE_WINDOWS, PVS_GUEST_VER_WIN_2003},
	{"win-vista", PVS_GUEST_TYPE_WINDOWS, PVS_GUEST_VER_WIN_VISTA},
	{"win-2008", PVS_GUEST_TYPE_WINDOWS, PVS_GUEST_VER_WIN_2008},
	{"win-7", PVS_GUEST_TYPE_WINDOWS, PVS_GUEST_VER_WIN_WINDOWS7},
	{"win-8", PVS_GUEST_TYPE_WINDOWS, PVS_GUEST_VER_WIN_WINDOWS8},
	{"win-2012", PVS_GUEST_TYPE_WINDOWS, PVS_GUEST_VER_WIN_2012},
	{"win-8.1", PVS_GUEST_TYPE_WINDOWS, PVS_GUEST_VER_WIN_WINDOWS8_1},
	{"win", PVS_GUEST_TYPE_WINDOWS, PVS_GUEST_VER_WIN_OTHER},

	/* Linux */
	{"rhel", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_REDHAT},
	{"rhel7", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_REDHAT_7},
	{"suse", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_SUSE},
	{"debian", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_DEBIAN},
	{"fedora-core", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_FEDORA},
		{"fc", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_FEDORA},
	{"xandros", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_XANDROS},
	{"ubuntu", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_UBUNTU},
	{"mandriva", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_MANDRAKE},
		{"mandrake", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_MANDRAKE},
	{"centos", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_CENTOS},
	{"centos7", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_CENTOS_7},
	{"psbm", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_PSBM},
	{"redhat", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_RH_LEGACY},
	{"opensuse", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_OPENSUSE},
	{"linux-2.4", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_KRNL_24},
	{"linux-2.6", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_KRNL_26},
	{"linux", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_OTHER},
	{"mageia", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_MAGEIA},
	{"mint", PVS_GUEST_TYPE_LINUX, PVS_GUEST_VER_LIN_MINT},

	/* MACOS */
	{"macosx", PVS_GUEST_TYPE_MACOS, PVS_GUEST_VER_MACOS_UNIVERSAL},
	/* Mac OS X 10.4 Tiger Server */
	{"tiger", PVS_GUEST_TYPE_MACOS, PVS_GUEST_VER_MACOS_TIGER},
		{"macos-10.4", PVS_GUEST_TYPE_MACOS, PVS_GUEST_VER_MACOS_TIGER},
	/* Mac OS X 10.5 Leopard Server" */
	{"leopard", PVS_GUEST_TYPE_MACOS, PVS_GUEST_VER_MACOS_LEOPARD},
		{"macos-10.5", PVS_GUEST_TYPE_MACOS, PVS_GUEST_VER_MACOS_LEOPARD},
	{"snowleopard", PVS_GUEST_TYPE_MACOS, PVS_GUEST_VER_MACOS_SNOW_LEOPARD},
		{"macos-10.6", PVS_GUEST_TYPE_MACOS, PVS_GUEST_VER_MACOS_SNOW_LEOPARD},
	/* FreeBSD */
	{"freebsd-4", PVS_GUEST_TYPE_FREEBSD, PVS_GUEST_VER_BSD_4X},
	{"freebsd-5", PVS_GUEST_TYPE_FREEBSD, PVS_GUEST_VER_BSD_5X},
	{"freebsd-6", PVS_GUEST_TYPE_FREEBSD, PVS_GUEST_VER_BSD_6X},
	{"freebsd-7", PVS_GUEST_TYPE_FREEBSD, PVS_GUEST_VER_BSD_7X},
	{"freebsd-8", PVS_GUEST_TYPE_FREEBSD, PVS_GUEST_VER_BSD_8X},
	{"freebsd", PVS_GUEST_TYPE_FREEBSD, PVS_GUEST_VER_BSD_OTHER},

	/* Chrome */
	{"chrome-1", PVS_GUEST_TYPE_CHROMEOS, PVS_GUEST_VER_CHROMEOS_1x},
	{"chrome", PVS_GUEST_TYPE_CHROMEOS, PVS_GUEST_VER_CHROMEOS_OTHER},

};

const OsDistribution *get_dist(const std::string &name)
{
	unsigned int i;

	for (i = 0; i < sizeof(dist_map)/sizeof(*dist_map); ++i)
		if (name == dist_map[i].name)
			return &dist_map[i];
	return 0;
}

const char *get_dist_by_id(unsigned int id)
{
	unsigned int i;

	for (i = 0; i < sizeof(dist_map)/sizeof(*dist_map); ++i)
		if (id == dist_map[i].ver)
			return dist_map[i].name;
	return "";
}

void print_dist(bool def)
{
	unsigned int i;
	const OsDistribution *map = def ? def_dist_map : dist_map;
	unsigned int len = def ?
		sizeof(def_dist_map) / sizeof(*def_dist_map) :
			sizeof(dist_map) / sizeof(*dist_map);

	printf("\n");
	for (i = 0; i < len; ++i) {
		printf("%-16s%s", map[i].name, (i+1)%4?"\t":"\n");
	}
	printf("\n");
}

static int get_mul(char c, unsigned long long *m)
{
	*m = 1;
	switch (c) {
	case 'T':
	case 't':
		*m *= 1024;
	case 'G':
	case 'g':
		*m *= 1024;
	case 'M':
	case 'm':
		*m *= 1024;
	case 'K':
	case 'k':
		*m *= 1024;
	case 'B':
	case 'b':
		break;
	default:
		return -1;
	}
	return 0;
}

/* This function parse string in form xxx[GMKPB]:yyy[GMKPB]
 * If :yyy is omitted, it is set to xxx.
 */
int parse_two_longs_N(const char *str, unsigned long *barrier,
		unsigned long *limit, int div, int def_div)
{
	unsigned long long n;
	char *tail;
	unsigned long long tmp;
	int ret = 0;

	errno = 0;
	tmp = strtoull(str, &tail, 10);
	if (errno == ERANGE)
		return 1;
	if (!strncmp(str, "unlimited", 9)) {
		tmp = LONG_MAX;
		tail = (char *)str + 9;
	} else if (str == tail) {
		return 1;
	} else if (*tail != ':' && *tail != '\0') {
		if (get_mul(*tail, &n))
			return 1;
		tmp = tmp * n / div;
		tail++;
	} else {
		tmp /= def_div;
	}
	if (tmp > LONG_MAX)
		tmp = LONG_MAX;
	*barrier = (unsigned long)tmp;
	if (*tail == ':') {
		str = ++tail;
		errno = 0;
		tmp = strtoull(str, &tail, 10);
		if (errno == ERANGE)
			return 1;
		if (!strcmp(str, "unlimited")) {
			tmp = LONG_MAX;
		} else if (str == tail) {
			return 1;
		} else if (*tail != '\0') {
			if (get_mul(*tail, &n))
				return 1;
			tmp = tmp * n / div;
		} else {
			tmp /= def_div;
		}
		if (tmp > LONG_MAX)
			tmp = LONG_MAX;
		*limit = (unsigned long)tmp;
	} else if (*tail == '\0'){
		*limit = *barrier;
	} else
		return 1;
	return ret;
}

/* This function parse string in form xxx[GMKB
   return val in bytes.
 */
int parse_ui_x(const char *str, unsigned int *val, bool bDefInMb)
{
	unsigned long long tmp;
	unsigned long long n;
	char *tail;

	errno = 0;
	tmp = strtoul(str, (char **)&tail, 10);
	if (errno == ERANGE)
		return 1;
	if (*tail != '\0') {
		if (get_mul(*tail, &n) || tail[1] != '\0')
			return 1;
		if (bDefInMb)
			tmp = tmp * n /1024 / 1024;
		else
			tmp *= n;
	} else {
		if (!bDefInMb)
			tmp *= 1024 * 1024;
	}
	*val = (unsigned int) tmp;
	return 0;
}

int parse_ui(const char *str, unsigned int *val)
{
	unsigned long tmp;
	char *tail;

	errno = 0;
	tmp = strtoul(str, (char **)&tail, 10);
	if (*tail != '\0' || errno == ERANGE || tmp > UINT_MAX)
		return 1;
	*val = (unsigned int) tmp;
	return 0;
}

int parse_ui_unlim(const char *str, unsigned int *val)
{
	char *tail;

	if (strncmp(str, "unlimited", 9))
		return parse_ui(str, val);

	tail = (char *)str + 9;
	if (*tail != '\0')
		return 1;
	*val = (unsigned int)-1;
	return 0;
}

int parse_cpulimit(const char *str, PRL_CPULIMIT_DATA *param)
{
        char *tail;
        unsigned long val;

        errno = 0;
        val = (int)strtoul(str, (char **)&tail, 10);
	if (*tail == '%' && *(tail + 1) == '\0') {
		param->value = val;
		param->type = PRL_CPULIMIT_PERCENTS;
	} else if (!strcmp(tail, "m") || !strcmp(tail, "mhz")) {
		param->value = val;
		param->type = PRL_CPULIMIT_MHZ;
	} else if (*tail == '\0') {
		param->value = val;
		param->type = PRL_CPULIMIT_PERCENTS;
	} else
		return -1;

	return 0;
}

std::string ui2string(unsigned int val)
{
	char buf[64];

	sprintf(buf, "%u", val);

	return std::string (buf);
}

std::string uptime2str(PRL_UINT64 uptime)
{
	std::string out;
	unsigned int days, hours, min, secs;

	days  = (unsigned int)(uptime / (60 * 60 * 24));
	min = (unsigned int)(uptime / 60);
	hours = min / 60;
	hours = hours % 24;
	min = min % 60;
	PRL_UINT64 ull60 = 60;
	secs = (unsigned int)(uptime - (ull60*min + ull60*ull60*hours + ull60*ull60*24*days));

	if (days) {
		out += ui2string(days);
		out += " day";
		if (days >= 1)
			out += "s";
		out += " ";
	}
	out += hours < 10 ? "0" + ui2string(hours) : ui2string(hours);
	out += ":";
	out += min < 10 ? "0" + ui2string(min) : ui2string(min);
	out += ":";
	out += secs < 10 ? "0" + ui2string(secs) : ui2string(secs);

	return out;
}

const char *prl_basename(const char *name)
{
	const char *p = NULL;

	if ((p = strrchr(name, '/')))
		name = ++p;
#ifdef _WIN_
	if ((p = strrchr(name, '\\')))
		name = ++p;
#endif

	return name;
}

str_list_t split(const std::string &str, const char *delim, bool delim_once)
{
	str_list_t output;
	std::string::size_type sp, ep;

	sp = str.find_first_not_of(delim, 0);
	if (sp != string::npos) {
		bool end = false;

		while (!end) {
			ep = str.find_first_of(delim, sp);
			if (ep == string::npos) {
				ep = str.find_last_not_of(delim) + 1;
				end = true;
			}
			else if (delim_once && output.size() > 0) {
				ep = str.length();
				end = true;
			}

			output.push_back(string(str, sp, ep - sp));
			sp = str.find_first_not_of(delim, ep);
			if (sp == string::npos)
				break;
		}
	}

	return output;
}

/*
 0  Empty           1e  Hidden W95 FAT1 80  Old Minix       be  Solaris boot
 1  FAT12           24  NEC DOS         81  Minix / old Lin bf  Solaris
 2  XENIX root      39  Plan 9          82  Linux swap / So c1  DRDOS/sec (FAT-
 3  XENIX usr       3c  PartitionMagic  83  Linux           c4  DRDOS/sec (FAT-
 4  FAT16 <32M      40  Venix 80286     84  OS/2 hidden C:  c6  DRDOS/sec (FAT-
 5  Extended        41  PPC PReP Boot   85  Linux extended  c7  Syrinx
 6  FAT16           42  SFS             86  NTFS volume set da  Non-FS data
 7  HPFS/NTFS       4d  QNX4.x          87  NTFS volume set db  CP/M / CTOS / .
 8  AIX             4e  QNX4.x 2nd part 88  Linux plaintext de  Dell Utility
 9  AIX bootable    4f  QNX4.x 3rd part 8e  Linux LVM       df  BootIt
 a  OS/2 Boot Manag 50  OnTrack DM      93  Amoeba          e1  DOS access
 b  W95 FAT32       51  OnTrack DM6 Aux 94  Amoeba BBT      e3  DOS R/O
 c  W95 FAT32 (LBA) 52  CP/M            9f  BSD/OS          e4  SpeedStor
 e  W95 FAT16 (LBA) 53  OnTrack DM6 Aux a0  IBM Thinkpad hi eb  BeOS fs
 f  W95 Ext'd (LBA) 54  OnTrackDM6      a5  FreeBSD         ee  EFI GPT
10  OPUS            55  EZ-Drive        a6  OpenBSD         ef  EFI (FAT-12/16/
11  Hidden FAT12    56  Golden Bow      a7  NeXTSTEP        f0  Linux/PA-RISC b
12  Compaq diagnost 5c  Priam Edisk     a8  Darwin UFS      f1  SpeedStor
14  Hidden FAT16 <3 61  SpeedStor       a9  NetBSD          f4  SpeedStor
16  Hidden FAT16    63  GNU HURD or Sys ab  Darwin boot     f2  DOS secondary
17  Hidden HPFS/NTF 64  Novell Netware  b7  BSDI fs         fd  Linux raid auto
18  AST SmartSleep  65  Novell Netware  b8  BSDI swap       fe  LANstep
1b  Hidden W95 FAT3 70  DiskSecure Mult bb  Boot Wizard hid ff  BBT
1c  Hidden W95 FAT3 75  PC/IX
*/

const char *partition_type2str(unsigned int type)
{
	switch (type) {
	case 0x82:
		return "Linux swap";
	case 0x83:
		return "Linux";
	case 0xb:
	case 0xc:
		return "FAT32";
	case 0x7:
		return "NTFS";
	case 0xa8:
		return "Mac OS-X";
	default:
		return "";
	}
}

struct FeaturesMap {
	int type;
	unsigned long id;
	const char *name;
} features_map[] = {
	{PVT_VM, FT_AutoCaptureReleaseMouse,	"auto_capture_release_mouse"},
	{PVT_VM, FT_ShareClipboard,		"share_clipboard"},
	{PVT_VM, FT_TimeSynchronization,	"time_synchronization"},
	{PVT_VM, FT_TimeSyncSmartMode,		"time_sync_smart_mode"},
	{PVT_VM, FT_SharedProfile,		"shared_profile"},
	{PVT_VM, FT_UseDesktop,			"use_desktop"},
	{PVT_VM, FT_UseDocuments,		"use_documents"},
	{PVT_VM, FT_UsePictures,		"use_pictures"},
	{PVT_VM, FT_UseMusic,			"use_music"},
	{PVT_VM, FT_TimeSyncInterval,		"time_sync_interval"},
	{PVT_VM, FT_SmartGuard,			"smart_guard"},
	{PVT_VM, FT_SmartGuardNotify,		"smart_guard_notify"},
	{PVT_VM, FT_SmartGuardInterval,		"smart_guard_interval"},
	{PVT_VM, FT_SmartGuardMaxSnapshots,	"smart_guard_max_snapshots"},
	{PVT_VM, FT_SmartMount,			"smart_mount"},
	{PVT_VM, FT_SmartMountRemovableDrives,	"smart_mount_external_disks"},
	{PVT_VM, FT_SmartMountDVDs,		"smart_mount_external_dvds"},
	{PVT_VM, FT_SmartMountNetworkShares,	"smart_mount_network_shares"},

	{PVT_CT, PCF_FEATURE_SYSFS,		"sysfs"},
	{PVT_CT, PCF_FEATURE_NFS,		"nfs"},
	{PVT_CT, PCF_FEATURE_SIT,		"sit"},
	{PVT_CT, PCF_FEATURE_IPIP,		"ipip"},
	{PVT_CT, PCF_FEATURE_PPP,		"ppp"},
	{PVT_CT, PCF_FEATURE_IPGRE,		"ipgre"},
	{PVT_CT, PCF_FEATURE_BRIDGE,		"bridge"},
	{PVT_CT, PCF_FEATURE_NFSD,		"nfsd"}
};

unsigned long feature2id(const std::string &name, int &type)
{
	unsigned i;

	for (i = 0; i < sizeof(features_map)/sizeof(*features_map); ++i)
		if (name == features_map[i].name) {
			type = features_map[i].type;
			return features_map[i].id;
		}
	return 0;
}

std::string location2str(PRL_VM_LOCATION location)
{
	switch (location)
	{
		case PVL_LOCAL_FS: return "local";
		case PVL_REMOTE_FS: return "remote";
		case PVL_USB_DRIVE: return "USB drive";
		case PVL_FIREWIRE_DRIVE: return "FireWire drive";
		default: return "unknown";
	}
}

std::string feature2str(const FeaturesParam &feature)
{
	unsigned i;
	std::string out;

	for (i = 0; i < sizeof(features_map)/sizeof(*features_map); ++i) {
		if (feature.type != features_map[i].type)
			continue;
		if (feature.known & features_map[i].id) {
			out += features_map[i].name;
			out += ":";
			switch (features_map[i].id) {
			case FT_TimeSyncInterval:
				out += ui2string(feature.time_sync_interval);
				break;
			case FT_SmartGuardInterval:
				out += ui2string(feature.smart_guard_interval);
				break;
			case FT_SmartGuardMaxSnapshots:
				out += ui2string(feature.smart_guard_max_snapshots);
				break;
			default:
				out += feature.mask & features_map[i].id ? "on" : "off";
				break;
			}
			out += " ";
		}
	}
	return out;
}

int str_list2handle(const str_list_t &list, PrlHandle &h)
{
	PrlApi_CreateStringsList(h.get_ptr());

	str_list_t::const_iterator it;
	for (it = list.begin(); it != list.end(); ++it)
		PrlStrList_AddItem(h.get_handle(), it->c_str());

	return 0;
}

std::string handle2str(PrlHandle &h)
{
	char buf[1024];
	PRL_UINT32 count = 0;
	std::string out;

	PrlStrList_GetItemsCount(h, &count);

	for (unsigned int i = 0; i < count; i++) {
		unsigned int len = sizeof(buf);

		PrlStrList_GetItem(h, i, buf, &len);
		if (i > 0)
			out += " ";
		out += buf;
	}
	return out;
}

int str2on_off(const std::string &val)
{
	if (val == "on" || val == "yes")
		return 1;
	else if (val == "off" || val == "no")
		return 0;
	return -1;
}

void print_procent(unsigned int procent, std::string message)
{
	if(message.empty())
		fprintf(stdout, "\rOperation progress %s%2d%%",
				procent == 100?"   ":"...",
				procent);
	else
		fprintf(stdout, "\r%s %s%2d%%",
				message.c_str(),
				procent == 100?"   ":"...",
				procent);

	if (procent == 100)
		fprintf(stdout, "\n");

        fflush(stdout);
}

int get_progress(PRL_HANDLE h, PRL_UINT32 &progress, std::string &stage)
{
	PRL_RESULT ret;

	PrlHandle hPrm;
	ret = PrlEvent_GetParam(h, 0, hPrm.get_ptr());
	if (PRL_FAILED(ret)) {
		prl_log(L_DEBUG, "PrlEvent_GetParam %s",
				get_error_str(ret).c_str());
		return -1;
	}

	ret = PrlEvtPrm_ToUint32(hPrm.get_handle(), &progress);
	if (PRL_FAILED(ret)) {
		prl_log(L_DEBUG, "PrlEvtPrm_ToUint32 %s",
				get_error_str(ret).c_str());
		return -1;
	}

	PrlHandle hEvt;
	if (PrlEvent_GetParamByName(h, EVT_PARAM_PROGRESS_STAGE, hEvt.get_ptr()) == 0) {
		char buf[4096];
		PRL_UINT32 len = sizeof(buf);

		if (PrlEvtPrm_ToString(hEvt, buf, &len) == 0)
			stage = buf;
	}
	return 0;
}

std::string get_dev_name(PRL_DEVICE_TYPE devType, int devNum)
{
	std::ostringstream os;
	switch(devType)
	{
	case PDE_FLOPPY_DISK:
		os << "floppy";
		break;
	case PDE_VIRTUAL_SNAPSHOT_DEVICE:
		os << "snapshots";
		break;
	case PDE_HARD_DISK:
		os << "hard disk " << devNum;
		break;
	default:
		os << "type " << devType << " " << devNum;
		break;
	}
	return os.str();
}

std::string get_dev_from_handle(PrlHandle &h)
{
	PrlHandle hDevType, hDevNum;
	PRL_DEVICE_TYPE devType;
	int iDev = 0;

	if (PRL_FAILED(PrlEvent_GetParam( h.get_handle(), 0, hDevType.get_ptr() )))
			return std::string();

	if( PRL_FAILED(PrlEvtPrm_ToInt32( hDevType.get_handle(), (PRL_INT32_PTR)&devType )) )
		return std::string();

	if( PRL_SUCCEEDED(PrlEvent_GetParam( h.get_handle(), 1, hDevNum.get_ptr() )))
		(void)PrlEvtPrm_ToInt32( hDevNum.get_handle(), &iDev );

	return get_dev_name( devType, iDev );
}

void print_progress(PRL_HANDLE h, std::string message)
{
	PRL_UINT32 progress;
	std::string stage;

	if (get_progress(h, progress, stage) == 0)
		print_procent(progress, message);
}

void print_vz_progress(PRL_HANDLE h)
{
	PRL_UINT32 progress;
	std::string stage;

	if (get_progress(h, progress, stage))
		return;
	if (progress == 100)
		return;

	fprintf(stdout, "%s\n", stage.c_str());
}

#define UUID_LEN			36
#define NORMALIZED_UUID_LEN (UUID_LEN + 2)

bool is_uuid(const std::string &str)
{
	const char *s;
	/* fbcdf284-5345-416b-a589-7b5fcaa87673 */
	switch (str.length()) {
	case UUID_LEN:
		s = str.c_str();
		break;
	case NORMALIZED_UUID_LEN:
		if ((str[0] != '{') || (str[NORMALIZED_UUID_LEN - 1] != '}'))
			return false;
		s = str.c_str() + 1;
		break;
	default:
		return false;
	}

	for (unsigned int i = 0; i < UUID_LEN; i++) {
		if ((i == 8) || (i == 13) || (i == 18) || (i == 23)) {
			if (s[i] != '-' )
				return false;
		} else if (!isxdigit(s[i])) {
			return false;
		}
	}
	return true;
}

int normalize_uuid(const std::string &str, std::string &out)
{
	if (str.length() != UUID_LEN && str.length() != NORMALIZED_UUID_LEN)
		return -1;

	if (!is_uuid(str))
		return -1;

	if (str[0] != '{')
		out = std::string("{") + str + std::string("}");
	else
		out = str;

	return 0;
}

int get_error(int action, PRL_RESULT res)
{
	if (action == VmStopAction) {
		switch (res) {
		case PRL_ERR_DISP_VM_IS_NOT_STARTED:
			return 0;
		default:
			break;
		}
	}
	return res;
}

static time_t _timegm(struct tm *tm)
{
	time_t ret = 0;
	char *tz;

	tz = getenv("TZ");
#ifdef _WIN_
#define tzset _tzset
	_putenv("TZ=UTC");
	tzset();
	ret = mktime(tm);
	if (tz) {
		char tzstr[256];
                snprintf(tzstr, sizeof(tzstr), "TZ=%s", tz);
                tzstr[sizeof(tzstr)-1] = 0;
		_putenv(tzstr);
	} else
		_putenv("TZ=");
#else
	setenv("TZ", "UTC", 1);
	tzset();
	ret = mktime(tm);
	if (tz)
		setenv("TZ", tz, 1);
	else
		unsetenv("TZ");
#endif
	tzset();

	return ret;
}

std::string convert_time(const char *date)
{
	struct tm t, *pt;
	time_t tmp;
	char buf[32];

	memset(&t, 0, sizeof(struct tm));
	if (sscanf(date, "%4d-%02d-%02d %02d:%02d:%02d", &t.tm_year,
		&t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec) != 6)
		return date;
	t.tm_year -= 1900;
	t.tm_mon -= 1;
	tmp = _timegm(&t);
	pt = localtime(&tmp);
	snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d:%02d",
		pt->tm_mon + 1, pt->tm_mday, pt->tm_year + 1900,
		pt->tm_hour, pt->tm_min, pt->tm_sec);

	return std::string(buf);
}

std::string get_veth_name(PRL_VM_TYPE type, unsigned int envid, unsigned int id)
{
	char buf[64];

	if (type == PVT_CT)
		snprintf(buf, sizeof(buf), "veth%d.%d", envid, id);
	else
		snprintf(buf, sizeof(buf), "vme%08x.%d", envid, id);

	return std::string(buf);
}


static bool s_full_info_mode = false;
void set_full_info_mode()
{
	s_full_info_mode = true;
}
bool is_full_info_mode()
{
	return s_full_info_mode;
}

/* IPv6 can be presented in a number of formats - this function is to convert
 * them to some predefined, for future search and compare */
void normalize_ip(std::string &val)
{
	std::string::size_type pos = val.find_first_of(":");
	if (pos == std::string::npos)
		// IPv4, do nothing
		return;

	// IPv6

	// capitalize characters
	std::transform(val.begin(), val.end(), val.begin(), ::toupper);

	// check '::' instance - we need to expand if present
	pos = val.find("::");
	if (pos != std::string::npos) {
		// count ':' chars
		int count = std::count(val.begin(), val.end(), ':');
		if (count > 7)
			// invalid ip, do nothing
			return;
		std::string tmp = ":0";
		while (7 - count++)
			tmp += ":0";
		tmp += ":";
		val.replace(pos, 2, tmp);
	}
}

std::string capability2str(const CapParam &capability)
{
	std::string output;

	for (unsigned int pos = 0, val = 1; pos < NUMCAP; ++pos, val = 1 << pos)
	{
		if (capability.mask_on & val || capability.mask_off & val)
		{
			if (!output.empty())
				output += ",";

			output += capnames[pos];

			if (capability.mask_on & val )
				output += ":on";
			else
				output += ":off";
		}
	}

	return output;
}

namespace Netfilter
{
static Mode mode_map[] = {
	Mode("disabled", PCNM_DISABLED),
	Mode("stateless", PCNM_STATELESS),
	Mode("stateful", PCNM_STATEFUL),
	Mode("full", PCNM_FULL),
};

Mode fromId(PRL_NETFILTER_MODE src) {
	for (unsigned i = 0; i < sizeof(mode_map)/sizeof(mode_map[0]); ++i)
	{
		if (mode_map[i].id == src)
			return mode_map[i];
	}

	return Mode();
}

Mode fromString(const std::string& src) {
	std::string m(src);
	std::transform(m.begin(), m.end(), m.begin(), ::tolower);

	for (unsigned i = 0; i < sizeof(mode_map)/sizeof(mode_map[0]); ++i)
	{
		if (mode_map[i].name == src)
			return mode_map[i];
	}

	return Mode();
}

} // namespace Netfilter

const char *prl_ct_resource2str(PRL_CT_RESOURCE id)
{
	switch(id) {
	case PCR_SWAPPAGES:
		return "swappages";
	case PCR_QUOTAUGIDLIMIT:
		return "quotaugidlimit";
	default:
		break;
	}
	return "unknown";
}

int prlerr2exitcode(PRL_RESULT result)
{
	if (result == 0)
		return 0;

	switch (result) {
	case PRL_ERR_VM_IN_FROZEN_STATE:
		return 254;
	case PRL_ERR_CANT_CONNECT_TO_DISPATCHER:
		return 253;
	default:
		break;
	}

	return (result < 0 || result > 255) ? 255 : result;
}

void xplatform_sleep(unsigned uiMsec)
{
#ifdef _WIN_
	::Sleep(uiMsec);
#elif (defined (_LIN_))
	usleep(uiMsec * 1000);
#endif
}

int parse_adv_security_mode(const char *str, int *val)
{
	if (!strncmp(str, "off", 3))
		*val = PMAA_NO_ADVANCED_AUTH_NEEDED;
	else if (!strncmp(str, "auth", 4))
		*val = PMAA_USE_SYSTEM_CREDENTIALS;
	else
		return -1;

	return 0;
}

int edit_allow_command_list(PRL_HANDLE hList, const std::vector< std::pair<PRL_ALLOWED_VM_COMMAND, bool > >& vCmds)
{
	PRL_UINT32 nCount;
	int ret = PrlOpTypeList_GetItemsCount(hList, &nCount);
	if (ret)
		return prl_err(ret, "PrlOpTypeList_GetItemsCount: %s",
				get_error_str(ret).c_str());

	std::map<PRL_ALLOWED_VM_COMMAND, PRL_UINT32 > mCmds;
	for(PRL_UINT32 i = 0; i < nCount; i++)
	{
		PRL_ALLOWED_VM_COMMAND cmd;
		ret = PrlOpTypeList_GetItem(hList, i, &cmd);
		if (ret)
			return prl_err(ret, "PrlOpTypeList_GetItem: %s",
					get_error_str(ret).c_str());
		mCmds.insert(std::pair<PRL_ALLOWED_VM_COMMAND, PRL_UINT32 >(cmd, i));
	}

	std::set<PRL_UINT32 > sDelIdx;
	for(unsigned int j = 0; j < vCmds.size(); j++)
	{
		bool bAdd = vCmds[j].second;
		PRL_ALLOWED_VM_COMMAND cmd = vCmds[j].first;

		if (bAdd)
		{
			if (mCmds.find(cmd) != mCmds.end())
				continue;

			ret = PrlOpTypeList_AddItem(hList, &cmd);
			if (ret)
				return prl_err(ret, "PrlOpTypeList_AddItem: %s",
						get_error_str(ret).c_str());
		}
		else
		{
			std::map<PRL_ALLOWED_VM_COMMAND, PRL_UINT32 >::iterator it = mCmds.find(cmd);
			if (it == mCmds.end())
				continue;
			sDelIdx.insert(it->second);
		}
	}

	for(std::set<PRL_UINT32 >::reverse_iterator rit = sDelIdx.rbegin();
		rit != sDelIdx.rend();
		rit++)
	{
		ret = PrlOpTypeList_RemoveItem(hList, *rit);
		if (ret)
			return prl_err(ret, "PrlOpTypeList_AddItem: %s",
					get_error_str(ret).c_str());
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// struct StorageUrl

const std::string StorageUrl::m_schema("backup://");

StorageUrl::StorageUrl(const std::string& backup_id, const std::string& diskname)
	: m_url(m_schema + "/" + backup_id + "/" + diskname)
{
}

std::string StorageUrl::getBackupId() const
{
	str_list_t parts = split();
	if (parts.size() < 2)
		return std::string();
	// skip disk name, which is the last element
	parts.pop_back();
	return parts.back();
}

std::string StorageUrl::getDiskName() const
{
	str_list_t parts = split();
	if (parts.size() < 2)
		return std::string();
	return parts.back();
}

str_list_t StorageUrl::split() const
{
	return ::split(m_url.substr(m_schema.size()), "/");
}

//////////////////////////////////////////////////////////////////////////////////
// cpufeatures


// size == 0 will mark end in array of items
// name == NULL will mark reserved items
struct RegisterDescriptionItem {
	const char *name;
	int size;
};

#define FLAG_ITEM(name) { name, 1 }
#define TERMINATOR_ITEM { NULL, 0 }
#define RESERVED_FLAG { NULL, 1 }
#define RESERVED_AREA(min, max) { NULL, (max) - (min) + 1}

static const RegisterDescriptionItem s_cpuid_00000001_ECX_items[] = {
	FLAG_ITEM("pni"),
	FLAG_ITEM("pclmulqdq"),
	FLAG_ITEM("dtes64"),
	FLAG_ITEM("monitor"),
	FLAG_ITEM("ds_cpl"),
	FLAG_ITEM("vmx"),
	FLAG_ITEM("smx"),
	FLAG_ITEM("est"),
	FLAG_ITEM("tm2"),
	FLAG_ITEM("ssse3"),
	FLAG_ITEM("cid"),
	RESERVED_FLAG,
	FLAG_ITEM("fma"),
	FLAG_ITEM("cx16"),
	FLAG_ITEM("xtpr"),
	FLAG_ITEM("pdcm"),
	RESERVED_FLAG,
	FLAG_ITEM("pcid"),
	FLAG_ITEM("dca"),
	FLAG_ITEM("sse4_1"),
	FLAG_ITEM("sse4_2"),
	FLAG_ITEM("x2apic"),
	FLAG_ITEM("movbe"),
	FLAG_ITEM("popcnt"),
	FLAG_ITEM("tsc_deadline_timer"),
	FLAG_ITEM("aes"),
	FLAG_ITEM("xsave"),
	FLAG_ITEM("osxsave"),
	FLAG_ITEM("avx"),
	FLAG_ITEM("f16c"),
	FLAG_ITEM("rdrand"),
	FLAG_ITEM("hypervisor"),
	TERMINATOR_ITEM
};

static const RegisterDescriptionItem s_cpuid_00000001_EDX_items[] = {
	FLAG_ITEM("fpu"),
	FLAG_ITEM("vme"),
	FLAG_ITEM("de"),
	FLAG_ITEM("pse"),
	FLAG_ITEM("tsc"),
	FLAG_ITEM("msr"),
	FLAG_ITEM("pae"),
	FLAG_ITEM("mce"),
	FLAG_ITEM("cx8"),
	FLAG_ITEM("apic"),
	RESERVED_FLAG,
	FLAG_ITEM("sep"),
	FLAG_ITEM("mtrr"),
	FLAG_ITEM("pge"),
	FLAG_ITEM("mca"),
	FLAG_ITEM("cmov"),
	FLAG_ITEM("pat"),
	FLAG_ITEM("pse36"),
	FLAG_ITEM("pn"),
	FLAG_ITEM("clflush"),
	RESERVED_FLAG,
	FLAG_ITEM("dts"),
	FLAG_ITEM("acpi"),
	FLAG_ITEM("mmx"),
	FLAG_ITEM("fxsr"),
	FLAG_ITEM("sse"),
	FLAG_ITEM("sse2"),
	FLAG_ITEM("ss"),
	FLAG_ITEM("ht"),
	FLAG_ITEM("tm"),
	FLAG_ITEM("ia64"),
	FLAG_ITEM("pbe"),
	TERMINATOR_ITEM
};

static const RegisterDescriptionItem s_cpuid_80000001_ECX_items[] = {
	FLAG_ITEM("lahf_lm"),
	FLAG_ITEM("cmp_legacy"),
	FLAG_ITEM("svm"),
	FLAG_ITEM("extapic"),
	FLAG_ITEM("cr8_legacy"),
	FLAG_ITEM("abm"),
	FLAG_ITEM("sse4a"),
	FLAG_ITEM("misalignsse"),
	FLAG_ITEM("3dnowprefetch"),
	FLAG_ITEM("osvw"),
	FLAG_ITEM("ibs"),
	FLAG_ITEM("xop"),
	FLAG_ITEM("skinit"),
	FLAG_ITEM("wdt"),
	RESERVED_FLAG,
	FLAG_ITEM("lwp"),
	FLAG_ITEM("fma4"),
	FLAG_ITEM("tce"),
	RESERVED_FLAG,
	FLAG_ITEM("nodeid_msr"),
	RESERVED_FLAG,
	FLAG_ITEM("tbm"),
	FLAG_ITEM("topoext"),
	FLAG_ITEM("perfctr_core"),
	FLAG_ITEM("perfctr_nb"),
	RESERVED_FLAG,
	FLAG_ITEM("dbx"),
	FLAG_ITEM("perftsc"),
	FLAG_ITEM("perfctr_l2"),
	RESERVED_AREA(29, 31),
	TERMINATOR_ITEM
};

static const RegisterDescriptionItem s_cpuid_80000001_EDX_items[] = {
	FLAG_ITEM("fpu:DUP"),
	FLAG_ITEM("vme:DUP"),
	FLAG_ITEM("de:DUP"),
	FLAG_ITEM("pse:DUP"),
	FLAG_ITEM("tsc:DUP"),
	FLAG_ITEM("msr:DUP"),
	FLAG_ITEM("pae:DUP"),
	FLAG_ITEM("mce:DUP"),
	FLAG_ITEM("cx8:DUP"),
	FLAG_ITEM("apic:DUP"),
	RESERVED_FLAG,
	FLAG_ITEM("syscall"),
	FLAG_ITEM("mtrr:DUP"),
	FLAG_ITEM("pge:DUP"),
	FLAG_ITEM("mca:DUP"),
	FLAG_ITEM("cmov:DUP"),
	FLAG_ITEM("pat:DUP"),
	FLAG_ITEM("pse36:DUP"),
	RESERVED_FLAG,
	FLAG_ITEM("mp"),
	FLAG_ITEM("nx"),
	RESERVED_FLAG,
	FLAG_ITEM("mmxext"),
	FLAG_ITEM("mmx:DUP"),
	FLAG_ITEM("fxsr:DUP"),
	FLAG_ITEM("fxsr_opt"),
	FLAG_ITEM("pdpe1gb"),
	FLAG_ITEM("rdtscp"),
	RESERVED_FLAG,
	FLAG_ITEM("lm"),
	FLAG_ITEM("3dnowext"),
	FLAG_ITEM("3dnow"),
	TERMINATOR_ITEM
};

static const RegisterDescriptionItem s_cpuid_0000000D_01_EAX_items[] = {
	FLAG_ITEM("xsaveopt"),
	FLAG_ITEM("xsavec"),
	FLAG_ITEM("xgetbv1"),
	FLAG_ITEM("xsaves"),
	RESERVED_AREA(4, 31),
	TERMINATOR_ITEM
};

static const RegisterDescriptionItem s_cpuid_80000007_EDX_items[] = {
	FLAG_ITEM("ts"),
	FLAG_ITEM("fid"),
	FLAG_ITEM("vid"),
	FLAG_ITEM("ttp"),
	FLAG_ITEM("tm87"),
	FLAG_ITEM("stc"),
	FLAG_ITEM("mul100"),
	FLAG_ITEM("hwps"),
	FLAG_ITEM("itsc"),
	FLAG_ITEM("cpb"),
	FLAG_ITEM("efro"),
	FLAG_ITEM("pfi"),
	FLAG_ITEM("pa"),
	RESERVED_AREA(13, 31),
	TERMINATOR_ITEM
};

static const RegisterDescriptionItem s_cpuid_00000007_EBX_items[] = {
	FLAG_ITEM("fsgsbase"),
	FLAG_ITEM("tsc_adjust"),
	FLAG_ITEM("sgx"),
	FLAG_ITEM("bmi1"),
	FLAG_ITEM("hle"),
	FLAG_ITEM("avx2"),
	RESERVED_FLAG,
	FLAG_ITEM("smep"),
	FLAG_ITEM("bmi2"),
	FLAG_ITEM("erms"),
	FLAG_ITEM("invpcid"),
	FLAG_ITEM("rtm"),
	FLAG_ITEM("pqm"),
	FLAG_ITEM("depfpp"),
	FLAG_ITEM("mpx"),
	FLAG_ITEM("pqe"),
	FLAG_ITEM("avx512f"),
	FLAG_ITEM("avx512dq"),
	FLAG_ITEM("rdseed"),
	FLAG_ITEM("adx"),
	FLAG_ITEM("smap"),
	RESERVED_FLAG,
	RESERVED_FLAG,
	FLAG_ITEM("cflushopt"),
	RESERVED_FLAG,
	FLAG_ITEM("pt"),
	FLAG_ITEM("avx512pf"),
	FLAG_ITEM("avx512er"),
	FLAG_ITEM("avx512cd"),
	FLAG_ITEM("sha"),
	FLAG_ITEM("avx512bw"),
	FLAG_ITEM("avx512vl"),
	TERMINATOR_ITEM
};

static const RegisterDescriptionItem s_cpuid_80000008_EAX_items[] = {
	{ "physical_address_bits", 8 },
	{ "virtual_address_bits", 8 },
	RESERVED_AREA(16, 31),
	TERMINATOR_ITEM
};

static const RegisterDescriptionItem * const s_register_descriptions[PCFE_MAX] = {
	s_cpuid_00000001_EDX_items,
	s_cpuid_00000001_ECX_items,
	s_cpuid_00000007_EBX_items,
	s_cpuid_80000001_ECX_items,
	s_cpuid_80000001_EDX_items,
	s_cpuid_80000007_EDX_items,
	s_cpuid_80000008_EAX_items,
	s_cpuid_0000000D_01_EAX_items
};

class BitFieldAccessor {
public:
	explicit BitFieldAccessor(PRL_UINT32 store, unsigned int begin, unsigned int size) :
		m_store(store),
		m_begin(begin),
		m_size(size)
	{
	}

	bool isCorrect(PRL_UINT32 value) const
	{
		return (value & mask()) == value;
	}

	BitFieldAccessor &set(PRL_UINT32 value)
	{
		// clear place
		m_store &= ~(mask() << m_begin);
		// set in place
		m_store |= value << m_begin;
		return *this;
	}

	PRL_UINT32 get() const
	{
		return (m_store >> m_begin) & mask();
	}

	PRL_UINT32 getStore() const
	{
		return m_store;
	}

private:

	PRL_UINT32 mask() const
	{
		return (1UL << m_size) - 1;
	}

	PRL_UINT32 m_store;
	unsigned int m_begin;
	unsigned int m_size;
};

struct FeatureDescription {
	explicit FeatureDescription(const char *a_name, PRL_CPU_FEATURES_EX a_reg,
			unsigned int a_begin, unsigned int a_size) :
		name(a_name),
		reg(a_reg),
		begin(a_begin),
		size(a_size)
	{
	}

	bool isBit() const
	{
		return size == 1;
	}

	BitFieldAccessor getAccessor(PRL_UINT32 store) const
	{
		return BitFieldAccessor(store, begin, size);
	}

	const char *name;
	PRL_CPU_FEATURES_EX reg;
	unsigned int begin;
	unsigned int size;

};

class PlainFeaturesPrinter {
public:
	explicit PlainFeaturesPrinter(std::ostringstream &os, const CpuFeatures &features) :
		m_os(&os),
		m_features(&features)
	{
	}

	void operator()(const FeatureDescription &f)
	{
		unsigned int v = m_features->getItem(f);
		if (!f.name) {
			if (v)
				*m_os << "RESERVED ";
		} else if (!f.isBit()) {
			*m_os << f.name << "=" << v << " ";
		} else if (v) {
			*m_os << f.name << " ";
		}
	}
private:
	std::ostringstream *m_os;
	const CpuFeatures *m_features;
};

class UnmaskableFeaturesPrinter {
public:
	explicit UnmaskableFeaturesPrinter(std::ostringstream &os,
			const CpuFeatures &features, const CpuFeatures &maskCaps) :
		m_os(&os),
		m_features(&features),
		m_maskCaps(&maskCaps)
	{
	}

	void operator()(const FeatureDescription &f) const
	{
		if (!f.name)
			return;
		if (m_features->getItem(f) > m_maskCaps->getItem(f))
			*m_os << f.name << " ";
	}
private:
	std::ostringstream *m_os;
	const CpuFeatures *m_features;
	const CpuFeatures *m_maskCaps;
};

class MaskedFeaturesPrinter {
public:
	explicit MaskedFeaturesPrinter(std::ostringstream &os,
			const CpuFeatures &features, const CpuFeatures &mask) :
		m_os(&os),
		m_features(&features),
		m_mask(&mask)
	{
	}

	void operator()(const FeatureDescription &f) const
	{
		if (!f.name)
			return;
		unsigned int fv = m_features->getItem(f);
		unsigned int mv = m_mask->getItem(f);
		if (mv < fv) {
			*m_os << f.name;
			if (!f.isBit())
				*m_os << "=" << mv;
			*m_os << " ";
		}
	}
private:
	std::ostringstream *m_os;
	const CpuFeatures *m_features;
	const CpuFeatures *m_mask;
};

class FeatureNameCompare {
public:
	explicit FeatureNameCompare(const char *name) :
		m_name(name)
	{
	}

	bool operator()(const FeatureDescription &f) const
	{
		return f.name && strcmp(f.name, m_name) == 0;
	}
private:
	const char *m_name;
};

class FeatureDescriptions {
public:
	FeatureDescriptions()
	{
		int i, b;

		for (i = 0; i < PCFE_MAX; ++i) {
			const RegisterDescriptionItem *it = s_register_descriptions[i];
			b = 0;
			while (it->size) {
				m_descriptions.push_back(
					FeatureDescription(
						it->name, (PRL_CPU_FEATURES_EX)i,
						b, it->size));
				b += it->size;
				++it;
			}
		}
	}

	template <typename functor>
	void forEach(functor f) const
	{
		std::for_each(m_descriptions.begin(), m_descriptions.end(), f);
	}

	const FeatureDescription *findByName(const char *name) const
	{
		std::vector<FeatureDescription>::const_iterator i;
		i = std::find_if(m_descriptions.begin(), m_descriptions.end(), FeatureNameCompare(name));
		return i == m_descriptions.end() ? NULL : &*i;
	}

private:
	std::vector<FeatureDescription> m_descriptions;
};

static const class FeatureDescriptions s_feature_descriptions;

std::string CpuFeatures::print() const
{
	std::ostringstream os;
	s_feature_descriptions.forEach(PlainFeaturesPrinter(os, m_handle));
	return os.str();
}

std::string print_features_unmaskable(const CpuFeatures &features, const CpuFeatures &maskCaps)
{
	std::ostringstream os;
	s_feature_descriptions.forEach(UnmaskableFeaturesPrinter(os, features, maskCaps));
	return os.str();
}

std::string print_features_masked(const CpuFeatures &features, const CpuFeatures &mask)
{
	std::ostringstream os;
	s_feature_descriptions.forEach(MaskedFeaturesPrinter(os, features, mask));
	return os.str();
}

bool CpuFeatures::setItem(const char *name, PRL_UINT32 value)
{
	const FeatureDescription *d = s_feature_descriptions.findByName(name);
	if (!d)
		return false;

	BitFieldAccessor a = d->getAccessor(get(d->reg));
	if (!a.isCorrect(value))
		return false;

	set(d->reg, a.set(value).getStore());

	return true;
}

PRL_UINT32 CpuFeatures::getItem(const FeatureDescription &d) const
{
	return d.getAccessor(get(d.reg)).get();
}

static std::bitset<32> create_dup_flags()
{
	std::bitset<32> dup_flags;
	const char *dup_names[] = { "fpu", "vme", "de", "pse", "tsc", "msr", "pae", "mce", "cx8",
			"apic", "mtrr", "pge", "mca", "cmov", "pat", "pse36", "mmx", "fxsr" };
	int i;
	const int dup_num = sizeof(dup_names)/sizeof(char*);
	for (i = 0; i < dup_num; ++i) {
		const FeatureDescription *f = s_feature_descriptions.findByName(dup_names[i]);
		if (!f) {
			std::cerr << "Can not find feature description '" << dup_names[i] << "'";
			continue;
		}
		dup_flags.set(f->begin);
	}

	return dup_flags;
}

const std::bitset<32> CpuFeatures::s_dupFlags = create_dup_flags();

void CpuFeatures::removeDups()
{
	PRL_UINT32 v = get(PCFE_EXT_80000001_EDX);
	v &= ~s_dupFlags.to_ulong();
	set(PCFE_EXT_80000001_EDX, v);
}

void CpuFeatures::setDups()
{
	PRL_UINT32 v0 = get(PCFE_FEATURES);
	PRL_UINT32 v8 = get(PCFE_EXT_80000001_EDX);
	// clear old values in dup positions
	v8 &= ~s_dupFlags.to_ulong();
	// set new values in dup positions
	v8 |= v0 & s_dupFlags.to_ulong();
	set(PCFE_EXT_80000001_EDX, v8);
}

PRL_UINT32 CpuFeatures::get(PRL_CPU_FEATURES_EX reg) const
{
	PRL_UINT32 v = 0;
	PrlCpuFeatures_GetValue(m_handle, reg, &v);
	return v;
}

void CpuFeatures::set(PRL_CPU_FEATURES_EX reg, PRL_UINT32 v)
{
	PrlCpuFeatures_SetValue(m_handle, reg, v);
}
