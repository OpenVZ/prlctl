/*
 * @file Utils.h
 *
 * Miscellaneous helper functions
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

#ifndef __UTILS_H__
#define __UTILS_H__
#include <string>
#include <vector>
#include <bitset>
#include "PrlTypes.h"
#include "CmdParam.h"

#ifdef _WIN_
#define strdup _strdup
#endif

enum {UUID_LEN = 36, NORMALIZED_UUID_LEN = UUID_LEN + 2};
#define JOB_WAIT_TIMEOUT 600 * 1000//Custom job wait timeout for specific operations
//Global default wait job operation completion timeout
extern PRL_UINT32 g_nJobTimeout;

#define PRL_EXTRACT_STRING_VALUE_HELPER(result_string, handle, sdk_method, nResultCode)\
	{\
		char _string_value_extracting_buf[4096];\
		PRL_UINT32 nStringValueExtractingBufSize = sizeof(_string_value_extracting_buf);\
		nResultCode = sdk_method(handle, _string_value_extracting_buf, &nStringValueExtractingBufSize);\
		if (PRL_SUCCEEDED(nResultCode))\
			result_string = _string_value_extracting_buf;\
	}

#define PRL_EXTRACT_STRING_VALUE_BY_INDEX_HELPER(result_string, handle, sdk_method, nIndex, nResultCode)\
	{\
		char _string_value_extracting_buf[4096];\
		PRL_UINT32 nStringValueExtractingBufSize = sizeof(_string_value_extracting_buf);\
		nResultCode = sdk_method(handle, nIndex, _string_value_extracting_buf, &nStringValueExtractingBufSize);\
		if (PRL_SUCCEEDED(nResultCode))\
			result_string = _string_value_extracting_buf;\
	}

class LoginInfo;
struct FeaturesParam;

int init_sdk_lib();
void deinit_sdk_lib();
int send_problem_report(const PrlHandle &hProblemReport, const ProblemReportParam &param);
int send_problem_report_on_stdout(const PrlHandle &hProblemReport);
int assembly_problem_report(const PrlHandle &hProblemReport, const ProblemReportParam &param, PRL_UINT32 flags);
PRL_RESULT get_job_retcode(PRL_HANDLE hJob, std::string &err,
	unsigned int timeout = JOB_INFINIT_WAIT_TIMEOUT);
inline PRL_RESULT get_job_retcode_predefined(PRL_HANDLE hJob, std::string &err)
{
	bool s = (JOB_INFINIT_WAIT_TIMEOUT == g_nJobTimeout);
	return get_job_retcode(hJob, err, s*60000 + (false == s)*g_nJobTimeout);
}
PRL_RESULT get_job_result(PRL_HANDLE hJob, PRL_HANDLE_PTR hResult,
	PRL_UINT32_PTR resultCount, unsigned int timeout = JOB_INFINIT_WAIT_TIMEOUT);
PRL_RESULT get_job_result_object(PRL_HANDLE hJob, PRL_HANDLE_PTR hObject,
                                 unsigned int timeout = JOB_INFINIT_WAIT_TIMEOUT);
int get_result_as_string(PRL_HANDLE hResult, std::string &out, bool xml = false);
std::string get_error_str(int nErrCode);
PRL_RESULT get_result_error_string(PRL_HANDLE hResult, std::string &err);
std::string get_details(PRL_HANDLE hJob);
void handle_job_err(PRL_HANDLE hJob, PRL_RESULT ret);
int parse_auth(const std::string &auth, LoginInfo &login, char *hide_passwd = NULL);
int parse_userpw(const std::string &userpw, std::string &user, std::string &pw);
std::string parse_mac(const char *mac);
const char *vmstate2str(VIRTUAL_MACHINE_STATE nVmState);
int str2dev_assign_mode(const std::string &str);
boost::optional<PRL_VM_BACKUP_MODE> str2backup_mode(const std::string& str);
const char *dev_assign_mode2str(int mode);
int read_passwd(const std::string &name, const std::string &server,
	std::string &passwd);
int read_passwd(std::string &passwd, const std::string &prompt);
int file2str(const char *filename, std::string &out);
DevType handle2type(PRL_HANDLE_TYPE type);
DevType prl_dev_type2type(PRL_DEVICE_TYPE type);
PRL_HANDLE_TYPE type2handle(DevType type);
PRL_DEVICE_TYPE type2prl_dev_type(DevType type);
const OsDistribution *get_def_dist(const std::string &name);
const OsDistribution *get_dist(const std::string &name);
const char *get_dist_by_id(unsigned int id);
int parse_two_longs_N(const char *str, unsigned long *barrier,
		unsigned long *limit, int div, int def_div);
int parse_ui_x(const char *str, unsigned int *val, bool bInMb = true);
int parse_ui(const char *str, unsigned int *val);
int parse_ui_unlim(const char *str, unsigned int *val);
std::string ui2string(unsigned int val);
std::string uptime2str(PRL_UINT64 uptime);
const char *prl_basename(const char *name);
void print_dist(bool ostype);
str_list_t split(const std::string &str,  const char *delim ="\t ", bool delim_once = false);
const char *partition_type2str(unsigned int type);
unsigned long feature2id(const std::string &name, int &type);
std::string location2str(PRL_VM_LOCATION location);
std::string feature2str(const FeaturesParam &feature);
int str_list2handle(const str_list_t &list, PrlHandle &h);
std::string handle2str(PrlHandle &h);
int str2on_off(const std::string &val);
void print_procent(unsigned int procent, std::string message = std::string());
void print_vz_progress(PRL_HANDLE h);
int get_progress(PRL_HANDLE h, PRL_UINT32 &progress, std::string &stage);
std::string get_dev_name(PRL_DEVICE_TYPE devType, int devNum);
std::string get_dev_from_handle(PrlHandle &h);
void print_progress(PRL_HANDLE h, std::string message = std::string());
bool is_uuid(const std::string &str);
int normalize_uuid(const std::string &str, std::string &out);
int get_error(int action, PRL_RESULT res);
std::string convert_time(const char *date);
int parse_cpulimit(const char *str, PRL_CPULIMIT_DATA *param);
void set_full_info_mode();
bool is_full_info_mode();
void normalize_ip(std::string &val);
std::string capability2str(const CapParam &capability);

struct FeatureDescription;

class CpuFeatures {
public:
	CpuFeatures(PRL_HANDLE handle) :
		m_handle(handle)
	{
	}

	~CpuFeatures()
	{
		PrlHandle_Free(m_handle);
	}

	PRL_UINT32 getItem(const FeatureDescription &d) const;
	bool setItem(const char *name, PRL_UINT32 value);
	void removeDups();
	void setDups();
	std::string print() const;

	PRL_HANDLE getHandle() const
	{
		return m_handle;
	}
private:
	CpuFeatures(const CpuFeatures&);
	CpuFeatures &operator=(const CpuFeatures&);

	PRL_UINT32 get(PRL_CPU_FEATURES_EX reg) const;
	void set(PRL_CPU_FEATURES_EX reg, PRL_UINT32 v);

	static const std::bitset<32> s_dupFlags;

	PRL_HANDLE m_handle;
};

std::string print_features_masked(const CpuFeatures &features, const CpuFeatures &mask);
std::string print_features_unmaskable(const CpuFeatures &features, const CpuFeatures &maskCaps);

const char *prl_ct_resource2str(PRL_CT_RESOURCE id);
int prlerr2exitcode(PRL_RESULT result);
void xplatform_sleep(unsigned uiMsec);
int parse_adv_security_mode(const char *str, int *val);
const char * adv_security_mode_to_str(PRL_MOBILE_ADVANCED_AUTH_MODE val);
int edit_allow_command_list(PRL_HANDLE hList, const std::vector< std::pair<PRL_ALLOWED_VM_COMMAND, bool > >& vCmds);

struct StorageUrl
{
	StorageUrl()
	{
	}

	explicit StorageUrl(const std::string& url) : m_url(url)
	{
	}

	bool isEmpty() const
	{
		return m_url.empty();
	}

	const std::string& getUrl() const
	{
		return m_url;
	}

	StorageUrl(const std::string& backup_id, const std::string& diskname);
	std::string getBackupId() const;
	std::string getDiskName() const;
	std::string getSchema() const;

private:
	str_list_t split() const;

	static const std::string m_schema;
	std::string m_url;
};

bool check_address(const std::string& address_);

#endif // __UTILS_H__
