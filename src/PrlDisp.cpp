/*
 * @file PrlDisp.cpp
 *
 * Dispatcher configuration managenment
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

#include <stdio.h>

#include <PrlApiDisp.h>
#include <PrlApiNet.h>

#include "PrlDisp.h"
#include "PrlDev.h"
#include "PrlSrv.h"
#include "CmdParam.h"
#include "Utils.h"
#include "PrlOutFormatter.h"
#include "Logger.h"

#ifdef _WIN_
#define snprintf _snprintf
#endif

int PrlDisp::get_config_handle()
{
	PRL_RESULT ret;
	PRL_UINT32 resultCount = 0;
	PrlHandle hResult;

	if (m_hDisp)
		return 0;

	PrlHandle hJob(PrlSrv_GetCommonPrefs(m_srv.get_handle()));
	if ((ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &resultCount)))
		return prl_err(ret, "PrlSrv_GetCommonPrefs: %s",
			get_error_str(ret).c_str());
	if ((ret = PrlResult_GetParam(hResult.get_handle(), &m_hDisp)))
		return prl_err(ret, "PrlSrv_GetCommonPrefs PrlResult_GetParam: %s",
			get_error_str(ret).c_str());

	return 0;
}

int PrlDisp::update_info()
{
	PRL_RESULT ret;

	if ((ret = get_config_handle()))
		return ret;
	PrlHandle hUsr;
	unsigned int len;
	char buf[4096];
	PrlHandle hJob(PrlSrv_GetUserProfile(m_srv.get_handle()));
	if (!get_job_result_object(hJob.get_handle(), hUsr.get_ptr())) {
		len = sizeof(buf);
		if (!PrlUsrCfg_GetDefaultVmFolder(hUsr.get_handle(), buf, &len))
			m_home = buf;
               if (m_home.empty()) {
		       len = sizeof(buf);
		       if (!PrlUsrCfg_GetVmDirUuid(hUsr.get_handle(), buf, &len))
			       m_home = buf;
	       }
	}

	PRL_BOOL flag;
	if (!PrlDispCfg_IsAdjustMemAuto(m_hDisp, &flag))
		m_mem_limit_auto = prl_bool(flag);
	else
		prl_log(L_DEBUG, "PrlDispCfg_IsAdjustMemAuto: %s",
			get_error_str(ret).c_str());
	if (PrlDispCfg_GetReservedMemLimit(m_hDisp, &m_mem_limit))
		prl_log(L_DEBUG, "PrlDispCfg_GetReservedMemLimit: %s",
			get_error_str(ret).c_str());
	if (PrlDispCfg_GetMaxReservMemLimit(m_hDisp, &m_mem_limit_max))
		prl_log(L_DEBUG, "PrlDispCfg_GetMaxReservMemLimit: %s",
			get_error_str(ret).c_str());
	if (PrlDispCfg_GetMinSecurityLevel(m_hDisp, &m_min_security_level))
		 prl_log(L_DEBUG, "PrlDispCfg_GetMinSecurityLevel: %s",
			get_error_str(ret).c_str());
	if (PrlDispCfg_CanChangeDefaultSettings(m_hDisp, &m_allow_mng_settings))
		prl_log(L_DEBUG, "PrlDispCfg_CanChangeDefaultSettings: %s",
			get_error_str(ret).c_str());
	if (PrlDispCfg_IsAllowMultiplePMC(m_hDisp, &m_allow_multiple_pmc))
		prl_log(L_DEBUG, "PrlDispCfg_IsAllowMultiplePMC: %s",
			get_error_str(ret).c_str());
	if (PrlDispCfg_IsSendStatisticReport(m_hDisp, &m_cep_mechanism))
		prl_log(L_DEBUG, "PrlDispCfg_IsSendStatisticReport: %s",
			get_error_str(ret).c_str());
	if (PrlDispCfg_IsVerboseLogEnabled(m_hDisp, &m_verbose_log_level))
		prl_log(L_DEBUG, "PrlDispCfg_IsVerboseLogEnabled: %s",
			get_error_str(ret).c_str());
	if (PrlDispCfg_IsLogRotationEnabled(m_hDisp, &m_log_rotation))
		prl_log(L_DEBUG, "PrlDispCfg_IsLogRotationEnabled: %s",
			get_error_str(ret).c_str());
	if (PrlDispCfg_GetVmCpuLimitType(m_hDisp, &m_vm_cpulimit_type)) {
		prl_log(L_DEBUG, "PrlDispCfg_GetVmCpuLimitType: %s",
			get_error_str(ret).c_str());
	}

	get_confirmation_list();

	return 0;
}

int PrlDisp::get_confirmation_list()
{
	PRL_HANDLE hList;
	PRL_RESULT ret = PrlDispCfg_GetConfirmationsList(m_hDisp, &hList);
	if (ret)
	{
		prl_log(L_DEBUG, "PrlDispCfg_GetConfirmationsList: %s",
				get_error_str(ret).c_str());
		return ret;
	}

	PRL_UINT32 nCount;
	ret = PrlOpTypeList_GetItemsCount(hList, &nCount);
	if (ret)
	{
		prl_log(L_DEBUG, "PrlOpTypeList_GetItemsCount: %s",
				get_error_str(ret).c_str());
		return ret;
	}

	for(PRL_UINT32 i = 0; i < nCount; i++)
	{
		PRL_ALLOWED_VM_COMMAND cmd;
		ret = PrlOpTypeList_GetItem(hList, i, &cmd);
		if (ret)
		{
			prl_log(L_DEBUG, "PrlOpTypeList_GetItem: %s",
					get_error_str(ret).c_str());
			return ret;
		}

		if (cmd == PAR_VM_CREATE_ACCESS)
			m_confirmation_list.push_back("create-vm");
		if (cmd == PAR_VM_REGISTER_ACCESS)
			m_confirmation_list.push_back("add-vm");
		if (cmd == PAR_VM_DELETE_ACCESS)
			m_confirmation_list.push_back("remove-vm");
		if (cmd == PAR_VM_CLONE_ACCESS)
			m_confirmation_list.push_back("clone-vm");
	}

	return 0;
}

std::string PrlDisp::get_vm_guest_cpu_limit_type() const
{
	switch (m_vm_cpulimit_type)
	{
		case PRL_VM_CPULIMIT_FULL:  return "full";
		case PRL_VM_CPULIMIT_GUEST: return "guest";
		default:                    return "unknown";
	}
}

int PrlDisp::set_min_security_level(const std::string &level)
{
	PRL_RESULT ret;
	PRL_SECURITY_LEVEL prl_level;

	if ((ret = get_security_level(level, prl_level)))
		return prl_err(ret, "An invalid parameter: %s", level.c_str());
	if ((ret = get_config_handle()))
		return ret;
	if ((ret = PrlDispCfg_SetMinSecurityLevel(m_hDisp, prl_level)))
		return prl_err(ret, "PrlDispCfg_SetMinSecurityLevel: %s",
			get_error_str(ret).c_str());
	set_updated();

	return 0;
}

int PrlDisp::get_security_level(PRL_SECURITY_LEVEL level,
		std::string &out) const
{
	switch (level) {
	case PSL_LOW_SECURITY:
		out = "low";
		break;
	case PSL_NORMAL_SECURITY:
		out = "normal";
		break;
	case PSL_HIGH_SECURITY:
		out = "high";
		break;
	}
	return 0;
}

int PrlDisp::get_security_level(const std::string &level,
		 PRL_SECURITY_LEVEL &out) const
{
	if (level == "low")
		out = PSL_LOW_SECURITY;
	else if (level == "normal")
		out = PSL_NORMAL_SECURITY;
	else if (level == "high")
		out = PSL_HIGH_SECURITY;
	else
		return -1;
	return 0;
}

int PrlDisp::set_allow_mng_settings(int allow)
{
	PRL_RESULT ret;

	if ((ret = get_config_handle()))
		return ret;

	if ((ret = PrlDispCfg_SetCanChangeDefaultSettings(m_hDisp,
						allow ? PRL_TRUE :PRL_FALSE)))
		return prl_err(ret, "PrlDispCfg_SetCanChangeDefaultSettings: %s",
			get_error_str(ret).c_str());
	set_updated();

	return 0;
}

int PrlDisp::switch_cep_mechanism(int on_off)
{
	PRL_RESULT ret;

	if ((ret = get_config_handle()))
		return ret;

	if ((ret = PrlDispCfg_SetSendStatisticReport(m_hDisp,
						on_off ? PRL_TRUE : PRL_FALSE)))
		return prl_err(ret, "PrlDispCfg_SetSendStatisticReport: %s",
			get_error_str(ret).c_str());
	set_updated();

	return 0;
}

int PrlDisp::switch_verbose_log_level(int on_off)
{
	PRL_RESULT ret;

	if ((ret = get_config_handle()))
		return ret;

	if ((ret = PrlDispCfg_SetVerboseLogEnabled(m_hDisp, on_off ? PRL_TRUE : PRL_FALSE)))
		return prl_err(ret, "PrlDispCfg_SetVerboseLogEnabled: %s",
			get_error_str(ret).c_str());
	set_updated();

	return 0;
}

int PrlDisp::switch_log_rotation(int on_off)
{
	PRL_RESULT ret;

	if ((ret = get_config_handle()))
		return ret;

	if ((ret = PrlDispCfg_SetLogRotationEnabled(m_hDisp, on_off ? PRL_TRUE : PRL_FALSE)))
		return prl_err(ret, "PrlDispCfg_SetLogRotationEnabled: %s",
			get_error_str(ret).c_str());
	set_updated();

	return 0;
}

int PrlDisp::set_confirmations_list(const std::map<std::string , bool >& cmd_list)
{
	std::vector< std::pair<PRL_ALLOWED_VM_COMMAND, bool > > vCmds;

	std::map<std::string , bool >::const_iterator it;
	for(it = cmd_list.begin(); it != cmd_list.end(); ++it)
	{
		if ((*it).first == "create-vm")
			vCmds.push_back(std::make_pair( PAR_VM_CREATE_ACCESS, (*it).second ));
		if ((*it).first == "add-vm")
			vCmds.push_back(std::make_pair( PAR_VM_REGISTER_ACCESS, (*it).second ));
		if ((*it).first == "remove-vm")
			vCmds.push_back(std::make_pair( PAR_VM_DELETE_ACCESS, (*it).second ));
		if ((*it).first == "clone-vm")
			vCmds.push_back(std::make_pair( PAR_VM_CLONE_ACCESS, (*it).second ));
	}

	PRL_HANDLE hList = PRL_INVALID_HANDLE;
	PRL_RESULT ret = PrlDispCfg_GetConfirmationsList(m_hDisp, &hList);
	if (ret)
		return prl_err(ret, "PrlDispCfg_GetConfirmationsList: %s",
				get_error_str(ret).c_str());

	ret = edit_allow_command_list(hList, vCmds);
	if (ret)
		return ret;

	ret = PrlDispCfg_SetConfirmationsList(m_hDisp, hList);
	if (ret)
		return prl_err(ret, "PrlDispCfg_SetConfirmationsList: %s",
				get_error_str(ret).c_str());

	set_updated();

	return 0;
}

void PrlDisp::append_info(PrlOutFormatter &f)
{
	std::string out;

	if (update_info())
		return;

	f.add("VM home", m_home);

	f.open("Vcmmd policy", true);
	{
		std::string p;
		get_vcmmd_policy(p, VCMMD_COMMAND_RUNTIME);
		f.add("runtime", p, true, false, true);
		get_vcmmd_policy(p);
		f.add("config", p, true);
	}
	f.close(true);

	std::string x;
	get_security_level(m_min_security_level, x);
	f.add("Minimal security level", x);

	f.add("Manage settings for new users",
		  m_allow_mng_settings ? "allow" : "deny");
	std::string str = get_def_backup_storage();
	if (!str.empty())
		f.add("Default backup storage", str);

	f.add("Backup path", get_backup_path());
	f.add("Backup temporary directory", get_backup_tmpdir());

	f.add("Backup mode", get_backup_mode());

	unsigned int tmo;
	get_backup_timeout(&tmo);
	f.add("Backup timeout", tmo, "");

	f.add("Traffic shaping", is_network_shaping_enabled() ? "on" : "off");
	if (is_full_info_mode()) {
		f.add("CPU model", get_cpu_model());
		f.add("CPU features", get_cpu_features());
		f.add("VM guest CPU limit type", get_vm_guest_cpu_limit_type());
		f.add("CPU unmaskable features", print_unmaskable_features());
		f.add("CPU masked features", print_masked_features());
	}

	f.add("VNC Clipboard:", get_vnc_clipboard() ? "on" : "off");
	f.add("Verbose log", m_verbose_log_level ? "on" : "off");
}

std::string PrlDisp::get_cpu_model()
{
	PRL_RESULT ret;
	char buf[256] = "";
	PRL_UINT32 bufsize = sizeof(buf);

	if ((ret = PrlSrvCfg_GetCpuModel(m_srv.get_srv_config_handle(), buf, &bufsize)))
		prl_err(ret, "PrlDispCfg_GetCpuFeaturesMask: %s", get_error_str(ret).c_str());

	return buf;
}

std::string PrlDisp::print_masked_features()
{
	PRL_RESULT ret;
	PRL_HANDLE hFeatures, hMask;

	if (PRL_FAILED(ret = PrlSrvCfg_GetCpuFeaturesEx(m_srv.get_srv_config_handle(), &hFeatures))) {
		prl_err(ret, "PrlSrvCfg_GetCpuFeatures: %s", get_error_str(ret).c_str());
		return std::string();
	}
	CpuFeatures features(hFeatures);
	if (PRL_FAILED(ret = PrlDispCfg_GetCpuFeaturesMaskEx(m_hDisp, &hMask))) {
		prl_err(ret, "PrlDispCfg_GetCpuFeaturesMaskEx: %s", get_error_str(ret).c_str());
		return std::string();
	}
	CpuFeatures mask(hMask);

	features.removeDups();
	return print_features_masked(features, mask);
}

std::string PrlDisp::print_unmaskable_features()
{
	PRL_RESULT ret;
	PRL_HANDLE hFeatures, hMaskCaps;

	if (PRL_FAILED(ret = PrlSrvCfg_GetCpuFeaturesEx(m_srv.get_srv_config_handle(), &hFeatures))) {
		prl_err(ret, "PrlSrvCfg_GetCpuFeatures: %s", get_error_str(ret).c_str());
		return std::string();
	}
	CpuFeatures features(hFeatures);
	if (PRL_FAILED(ret = PrlSrvCfg_GetCpuFeaturesMaskingCapabilities(
					m_srv.get_srv_config_handle(), &hMaskCaps))) {
		prl_err(ret, "PrlSrvCfg_GetCpuFeaturesMaskingCapabilities: %s", get_error_str(ret).c_str());
		return std::string();
	}
	CpuFeatures mask_caps(hMaskCaps);

	features.removeDups();
	return print_features_unmaskable(features, mask_caps);
}

bool PrlDisp::is_cpu_masking_support()
{
	PRL_UINT32 count = 0;
	PrlHandle hResult;

	PrlHandle hJob(PrlSrv_GetCpuMaskSupport(m_srv.get_handle(), 0));

	if ( PRL_ERR_SUCCESS == get_job_result(hJob.get_handle(), hResult.get_ptr(), &count))
		return true;

	return false;
}

int PrlDisp::set_cpu_features_mask(std::string mask_changes)
{
	PRL_RESULT ret;
	PRL_HANDLE hMask;

	if ((ret = get_config_handle()))
		return ret;

	if (PRL_FAILED(ret = PrlDispCfg_GetCpuFeaturesMaskEx(m_hDisp, &hMask)))
		return prl_err(ret, "PrlDispCfg_GetCpuFeaturesMaskEx: %s", get_error_str(ret).c_str());

	CpuFeatures mask(hMask);
	str_list_t names = split(mask_changes, ",");
	for (str_list_t::const_iterator it = names.begin(); it != names.end(); ++it) {
		const char *name = it->c_str();
		if (name[0] == '-') {
			if (!mask.setItem(&name[1], 0))
				goto syntax_error;
		} else if (name[0] == '+') {
			if (!mask.setItem(&name[1], 1))
				goto syntax_error;
		} else {
			str_list_t keyval = split(*it, "=");
			name = keyval.front().c_str();
			if (keyval.size() > 2)
				goto syntax_error;
			else if (keyval.size() == 2) {
				unsigned int v = atoi(keyval.back().c_str());
				if (!mask.setItem(name, v))
					goto syntax_error;
			} else {
				if (!mask.setItem(name, 1))
					goto syntax_error;
			}
		}
	}

	if(!is_cpu_masking_support())
	{
		fprintf(stderr, "WARNING: Node's processor does not support CPU feature 'masking'. "
						"So it will only work for virtual machines, but not for containers.\n");
	}

	mask.setDups();
	if ((ret = PrlDispCfg_SetCpuFeaturesMaskEx(m_hDisp, mask.getHandle())))
		return prl_err(ret, "PrlDispCfg_SetCpuFeaturesMask: %s",
			get_error_str(ret).c_str());

	set_updated();

	return 0;

syntax_error:

	return prl_err(PRL_ERR_FAILURE, "Incorrect CPU feature mask syntax: %s\n", mask_changes.c_str());
}

std::string PrlDisp::get_cpu_features()
{
	PRL_RESULT ret;
	PRL_HANDLE hFeatures;

	if (PRL_FAILED(ret = PrlSrvCfg_GetCpuFeaturesEx(m_srv.get_srv_config_handle(), &hFeatures))) {
		prl_err(ret, "PrlSrvCfg_GetCpuFeatures: %s", get_error_str(ret).c_str());
		return std::string();
	}
	CpuFeatures features(hFeatures);
	features.removeDups();
	return features.print();
}

int PrlDisp::set_mem_limit(unsigned int limit)
{
	PRL_RESULT ret;
	PRL_BOOL auto_limit = PRL_FALSE;

	if (limit != UINT_MAX) {
		prl_log(0, "Set memory limit: %dMb", limit);
		if ((ret = PrlDispCfg_SetReservedMemLimit(m_hDisp, limit)))
			return prl_err(ret, "PrlDispCfg_GetReservedMemLimit: %s",
				get_error_str(ret).c_str());
	} else {
		prl_log(0, "Set memory limit: auto");
		auto_limit = PRL_TRUE;
	}
	if ((ret = PrlDispCfg_SetAdjustMemAuto(m_hDisp, auto_limit)))
		return prl_err(ret, "PrlDispCfg_IsAdjustMemAuto: %s",
			get_error_str(ret).c_str());
	set_updated();

	return 0;
}

int PrlDisp::set_vcmmd_policy(const std::string &name)
{
	std::string err;
	PRL_RESULT ret;

	PrlHandle hGetJob(PrlSrv_GetVcmmdConfig(m_srv.get_handle(), 0));

	PRL_UINT32 resultCount = 0;
	PrlHandle hResult;
	if ((ret = get_job_result(hGetJob.get_handle(), hResult.get_ptr(), &resultCount)))
	{
		return prl_err(ret, "PrlSrv_GetVcmmdConfig: %s",
				get_error_str(ret).c_str());
	}

	PrlHandle hVcmmd;
	if ((ret = PrlResult_GetParam(hResult.get_handle(), hVcmmd.get_ptr())))
	{
		return prl_err(ret, "PrlSrv_GetVcmmdConfig PrlResult_GetParam: %s",
				get_error_str(ret).c_str());
	}

	if ((ret = PrlVcmmdConfig_SetPolicy(hVcmmd.get_handle(), name.c_str())))
	{
		return prl_err(ret, "PrlVcmmdConfig_SetPolicy: %s",
				get_error_str(ret).c_str());
	}

	PrlHandle hSetJob(PrlSrv_SetVcmmdConfig(m_srv.get_handle(), hVcmmd.get_handle(), 0));
	if ((ret = get_job_retcode(hSetJob.get_handle(), err)))
	{
		if (ret == PRL_ERR_RUNNING_VM_OR_CT)
			fprintf(stderr, "WARNING: %s\n", err.c_str());
		else
			return prl_err(ret, "Failed to set vcmmd config: %s", err.c_str());
	}


	return 0;
}

int PrlDisp::get_vcmmd_policy(std::string &policy, int nFlags) const
{
	std::string err;
	PRL_RESULT ret;
	PrlHandle hJob(PrlSrv_GetVcmmdConfig(m_srv.get_handle(), nFlags));

	PRL_UINT32 resultCount = 0;
	PrlHandle hResult;
	if ((ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &resultCount)))
	{
		return prl_err(ret, "PrlSrv_GetVcmmdConfig: %s",
				get_error_str(ret).c_str());
	}

	PrlHandle hVcmmd;
	if ((ret = PrlResult_GetParam(hResult.get_handle(), hVcmmd.get_ptr())))
	{
		return prl_err(ret, "PrlSrv_GetVcmmdConfig PrlResult_GetParam: %s",
				get_error_str(ret).c_str());
	}

	char config[256];
	PRL_UINT32 size = sizeof(config);
	if ((ret = PrlVcmmdConfig_GetPolicy(hVcmmd.get_handle(), config, &size)))
	{
		return prl_err(ret, "PrlVcmmdConfig_GetPolicy: %s",
				get_error_str(ret).c_str());
	}
	policy = std::string(config);
	return 0;
}

int PrlDisp::set_dev_assign_mode(const std::string &name, int mode)
{
	PRL_RESULT ret, retcode;
	PrlHandle hResult;
	PRL_UINT32 resultCount = 0;
	int nResultCode = 0;

	PrlDevSrv *dev;
	if ((dev = m_srv.find_dev(DEV_GENERIC_PCI, name)) == NULL)
		return prl_err(-1, "Unknown pci device %s", name.c_str());
	if ((ret = dev->set_assignment_mode(mode)))
		return ret;
	prl_log(0, "Set '%s' assignment=%s", name.c_str(), dev_assign_mode2str(mode));

	std::string err;
	PrlHandle hList;
	PrlApi_CreateHandlesList(hList.get_ptr());
	PrlHndlList_AddItem(hList.get_handle(), dev->m_hDev);
	PrlHandle hJob(PrlSrv_ConfigureGenericPci(m_srv.get_handle(), hList.get_handle(), 0));
	if ((ret = PrlJob_Wait(hJob.get_handle(), g_nJobTimeout)))
		return prl_err(ret, "Unable to configure generic PCI devices: %s",
			get_error_str(ret).c_str());
	if ((ret = PrlJob_GetRetCode(hJob.get_handle(), &retcode)))
		return prl_err(ret, "PrlJob_GetRetCode: %s",
			get_error_str(ret).c_str());
	if (retcode) {
		if ((ret = PrlJob_GetResult(hJob.get_handle(), hResult.get_ptr())))
			return prl_err(ret, "PrlJob_GetResult: %s [%d]",
				get_error_str(ret).c_str(), ret);

		if ((ret = PrlResult_GetParamsCount(hResult.get_handle(), &resultCount)))
			return prl_err(ret, "PrlResult_GetParamsCount  %s [%d]",
				get_error_str(ret).c_str(), ret);

		prl_log(L_DEBUG, "resultCount: %d", resultCount);
		//We guess here that error code just notiies about necessary to reboot host
		nResultCode = 2;
		for (unsigned int i = 0; i < resultCount; i++) {
			PrlHandle hEvent;
			char buf[4096];
			unsigned int len = sizeof(buf);

			ret = PrlResult_GetParamByIndex(hResult.get_handle(), i, hEvent.get_ptr());
			if (ret)
				break;
			if (!PrlEvent_GetErrCode(hEvent.get_handle(), &ret) &&
			    !PrlEvent_GetErrString(hEvent.get_handle(), PRL_FALSE, PRL_FALSE, buf, &len))
			{
				err += buf;
			}
			if (PRL_ERR_VTD_HOOK_AFTER_INSTALL_NEED_REBOOT != ret &&
				PRL_ERR_VTD_HOOK_AFTER_REVERT_NEED_REBOOT != ret)
			{
				//Have some error not related to host reboot - notify about major error from prlctl
				nResultCode = 1;
			}
		}
		prl_err(retcode, "Some errors occured on generic PCI devices configuration: %s",
			resultCount == 0 ?
				get_error_str(retcode).c_str() : err.c_str());
	}

	return nResultCode;
}

std::string PrlDisp::get_backup_path()
{
	int ret;
	unsigned int len;
	char buf[4096];

	len = sizeof(buf);
	ret = PrlDispCfg_GetDefaultBackupDirectory(m_hDisp, buf, &len);
	if (ret == 0)
		return std::string(buf);

	return std::string();
}

int PrlDisp::set_backup_path(const std::string &path)
{
	int ret;

	if ((ret = PrlDispCfg_SetDefaultBackupDirectory(m_hDisp, path.c_str())))
		return prl_err(ret, "PrlResult_GetParamsCount  %s [%d]",
				get_error_str(ret).c_str(), ret);

	set_updated();
	return 0;
}

int PrlDisp::set_backup_mode(PRL_VM_BACKUP_MODE mode)
{
	int ret;

	if ((ret = PrlDispCfg_SetBackupMode(m_hDisp, mode)))
		return prl_err(ret, "PrlDispCfg_SetBackupMode %s [%d]",
					   get_error_str(ret).c_str(), ret);
	set_updated();
	return 0;
}

std::string PrlDisp::get_backup_mode()
{
	PRL_RESULT ret;

	PRL_VM_BACKUP_MODE res = PBM_PUSH;

	if ((ret = PrlDispCfg_GetBackupMode(m_hDisp, &res)))
	{
		prl_err(ret, "PrlDispCfg_GetBackupMode %s", get_error_str(ret).c_str());
		return "unknown";
	}

	if (res == PBM_PUSH)
		return "push";
	else if (res == PBM_PUSH_REVERSED_DELTA)
		return "push-with-reversed-delta";
	else
		return "unknown";
}

int PrlDisp::get_backup_timeout(unsigned int *tmo)
{
	int ret;

	if ((ret = PrlDispCfg_GetBackupTimeout(m_hDisp, tmo)))
		return prl_err(ret, "PrlDispCfg_GetBackupTimeout %s [%d]",
				get_error_str(ret).c_str(), ret);

	set_updated();
	return 0;
}

int PrlDisp::set_backup_timeout(unsigned int tmo)
{
	int ret;

	if ((ret = PrlDispCfg_SetBackupTimeout(m_hDisp, tmo)))
		return prl_err(ret, "PrlDispCfg_SetBackupTimeout %s [%d]",
				get_error_str(ret).c_str(), ret);

	set_updated();
	return 0;
}

std::string PrlDisp::get_def_backup_storage()
{
	int ret;
	unsigned int len;
	char buf[4096];
	std::string out;

	len = sizeof(buf);
	ret = PrlDispCfg_GetDefaultBackupServer(m_hDisp, buf, &len);
	if (ret == 0)
		out = buf;

	if (!out.empty()) {
		len = sizeof(buf);
		ret = PrlDispCfg_GetBackupUserLogin(m_hDisp, buf, &len);
		if (ret == 0)
			out = std::string(buf) + std::string("@") + out;
	}

	return out;
}

int PrlDisp::set_def_backup_storage(const LoginInfo &server)
{
	int ret;

	if (!server.server.empty()) {
		if ((ret = PrlDispCfg_SetDefaultBackupServer(m_hDisp, server.server.c_str())))
			return prl_err(ret, "PrlDispCfg_SetDefaultBackupServer: %s",
					get_error_str(ret).c_str());
	} else {
		PrlDispCfg_SetDefaultBackupServer(m_hDisp, "");
		set_updated();
		return 0;
	}
	if (!server.user.empty()) {
		if ((ret = PrlDispCfg_SetBackupUserLogin(m_hDisp, server.user.c_str())))
			return prl_err(ret, "PrlDispCfg_SetBackupUserLogin: %s",
					get_error_str(ret).c_str());
	}
	bool passwd_from_stack = false;
	server.get_passwd_from_stack(passwd_from_stack);
	if (passwd_from_stack)
    	return prl_err(PRL_ERR_FAILURE, "Password authentication for the default backup server is deprecated.\n"
										"Use public key authentication instead.");
	set_updated();

	return 0;
}

std::string PrlDisp::get_backup_tmpdir()
{
	int ret;
	unsigned int len;
	char buf[4096];
	std::string out;

	len = sizeof(buf);
	ret = PrlDispCfg_GetBackupTmpDir(m_hDisp, buf, &len);
	if (ret == 0)
		out = buf;

	return out;
}

int PrlDisp::set_backup_tmpdir(const std::string &tmpdir, bool is_backup_mode_init)
{
	if (!is_backup_mode_init && tmpdir != get_backup_tmpdir())
	{
		if (!tmpdir.empty())
		{
			if (get_backup_mode() != "push-with-reversed-delta")
				fprintf(stderr, "WARNING: The tmpdir option was set without the backup-mode option. "
								"The backup mode was automatically set to push-with-reversed-delta.\n");
		}
		else if (get_backup_mode() == "push-with-reversed-delta") {
			fprintf(stderr, "WARNING: An empty tmpdir was set. The backup mode is still "
							"push-with-reversed-delta. The delta will be created in the home "
							"directory of the backed up VM.\n");
		}
		else {
			fprintf(stderr, "WARNING: Empty tmpdir and backup-mode were set. "
							"The backup mode will not be changed.\n");
		}
	}

	int ret;

	if ((ret = PrlDispCfg_SetBackupTmpDir(m_hDisp, tmpdir.c_str())))
		return prl_err(ret, "PrlDispCfg_SetBackupTmpDir %s [%d]",
		get_error_str(ret).c_str(), ret);

	set_updated();
	return 0;
}

int PrlDisp::update_offline_service(const OfflineSrvParam &offline_srv)
{
	PrlHandle hOffSrv;
	PRL_RESULT ret;
	std::string err;

	if ((ret = PrlOffmgmtService_Create(hOffSrv.get_ptr())))
		return prl_err(ret, "PrlOffmgmtService_Create: %s",
				get_error_str(ret).c_str());

	if ((ret = PrlOffmgmtService_SetName(hOffSrv.get_handle(), offline_srv.name.c_str())))
		return prl_err(ret, "PrlOffmgmtService_SetName: %s",
				get_error_str(ret).c_str());

	if ((ret = PrlOffmgmtService_SetPort(hOffSrv.get_handle(),  offline_srv.port)))
		return prl_err(ret, "PrlOffmgmtService_SetName: %s",
				get_error_str(ret).c_str());

	if ((ret = PrlOffmgmtService_SetUsedByDefault(hOffSrv.get_handle(),
							(PRL_BOOL)offline_srv.used_by_default)))
		return prl_err(ret, "PrlOffmgmtService_SetName: %s",
				get_error_str(ret).c_str());
	if (offline_srv.del) {
		prl_log(L_INFO, "delete offline service %s", offline_srv.name.c_str());
		PrlHandle hJob(PrlSrv_DeleteOfflineService(m_srv.get_handle(), hOffSrv.get_handle(), 0));
		if ((ret = get_job_retcode(hJob.get_handle(), err)))
			return prl_err(ret, "Failed to update offline service: %s", err.c_str());

	} else {
		prl_log(L_INFO, "update offline service %s port=%d default=%d",
					offline_srv.name.c_str(),
					offline_srv.port,
					offline_srv.used_by_default);
		PrlHandle hJob(PrlSrv_UpdateOfflineService(m_srv.get_handle(), hOffSrv.get_handle(), 0));
		if ((ret = get_job_retcode(hJob.get_handle(), err)))
			return prl_err(ret, "Failed to update offline service: %s", err.c_str());
	}

	return 0;
}

int PrlDisp::list_network_classes_config()
{
	PrlHandle h, hResult, hClassesList;
	PrlHandle hClass, hNetList;
	PRL_UINT32 resultCount, class_id;
	PRL_RESULT ret;
	std::string err;

	PrlHandle hListJob(PrlSrv_GetNetworkClassesList(m_srv.get_handle(), 0));
	if ((ret = get_job_retcode(hListJob.get_handle(), err)))
		return prl_err(ret, "PrlSrv_GetNetworkClassesList: %s", err.c_str());
	if ((ret = PrlJob_GetResult(hListJob.get_handle(), hResult.get_ptr())))
		return prl_err(ret, "PrlJob_GetResult: %s",
				get_error_str(ret).c_str());
	if ((ret = PrlResult_GetParam(hResult.get_handle(), hClassesList.get_ptr())))
		return prl_err(ret, "PrlResult_GetParams: %s",
				get_error_str(ret).c_str(), ret);

	if ((ret = PrlHndlList_GetItemsCount(hClassesList.get_handle(), &resultCount)))
		return prl_err(ret, "PrlHndlList_GetItemsCount: %s",
				get_error_str(ret).c_str());

	for (unsigned int i = 0; i < resultCount; i++) {
		PrlHandle hClass;
		if ((ret = PrlHndlList_GetItem(hClassesList.get_handle(), i, hClass.get_ptr())))
			return prl_err(ret, "PrlHndlList_GetItem: %s",
				get_error_str(ret).c_str());

		ret = PrlNetworkClass_GetClassId(hClass.get_handle(), &class_id);
		if (PRL_FAILED(ret))
			return prl_err(ret, "PrlNetworkClass_GetClassId: %s",
					get_error_str(ret).c_str());
		ret = PrlNetworkClass_GetNetworkList(hClass.get_handle(), hNetList.get_ptr());
		if (PRL_FAILED(ret))
			return prl_err(ret, "PrlNetworkClass_GetNetworkList: %s",
				get_error_str(ret).c_str());

		fprintf(stdout, "%d", class_id);
		PRL_UINT32 count;
		if ((ret = PrlStrList_GetItemsCount(hNetList.get_handle(), &count)) == 0) {
			for (unsigned int n = 0; n < count; n++) {
				char buf[128];
				unsigned int len = sizeof(buf);

				if ((ret = PrlStrList_GetItem(hNetList.get_handle(), n, buf, &len)))
					continue;
				fprintf(stdout, " %s", buf);
			}
		}
		fprintf(stdout, "\n");
	}
	return 0;
}

int PrlDisp::update_network_classes_config(const NetworkClassParam &param)
{
	PrlHandle h, hResult, hClassesList;
	PrlHandle hClass, hNetList;
	PRL_UINT32 resultCount, class_id;
	PRL_RESULT ret;
	std::string err;
	bool updated = false;

	PrlHandle hListJob(PrlSrv_GetNetworkClassesList(m_srv.get_handle(), 0));
	if ((ret = get_job_retcode(hListJob.get_handle(), err)))
		return prl_err(ret, "PrlSrv_GetNetworkClassesList: %s", err.c_str());
	if ((ret = PrlJob_GetResult(hListJob.get_handle(), hResult.get_ptr())))
		return prl_err(ret, "PrlJob_GetResult: %s",
				get_error_str(ret).c_str());
	if ((ret = PrlResult_GetParam(hResult.get_handle(), hClassesList.get_ptr())))
		return prl_err(ret, "PrlResult_GetParams: %s",
				get_error_str(ret).c_str(), ret);

	if ((ret = PrlHndlList_GetItemsCount(hClassesList.get_handle(), &resultCount)))
		return prl_err(ret, "PrlHndlList_GetItemsCount: %s",
				get_error_str(ret).c_str());

	for (unsigned int i = 0; i < resultCount; i++) {
		PrlHandle hClass;
		if ((ret = PrlHndlList_GetItem(hClassesList.get_handle(), i, hClass.get_ptr())))
			return prl_err(ret, "PrlHndlList_GetItem: %s",
				get_error_str(ret).c_str());

		ret = PrlNetworkClass_GetClassId(hClass.get_handle(), &class_id);
		if (PRL_FAILED(ret))
			return prl_err(ret, "PrlNetworkClass_GetClassId: %s",
					get_error_str(ret).c_str());
		// Update
		if (param.net_class == class_id) {
			if ((ret = PrlNetworkClass_GetNetworkList(hClass.get_handle(), hNetList.get_ptr())))
				return prl_err(ret, "PrlNetworkClass_GetNetworkList: %s",
						get_error_str(ret).c_str());

			PRL_UINT32 count;
			if ((ret = PrlStrList_GetItemsCount(hNetList.get_handle(), &count)))
				return prl_err(ret, "PrlStrList_GetItemsCount : %s",
						get_error_str(ret).c_str());

			for (unsigned int n = 0; n < count; n++) {
				char buf[128];
				unsigned int len = sizeof(buf);

				if ((ret = PrlStrList_GetItem(hNetList.get_handle(), n, buf, &len)))
					return prl_err(ret, "PrlStrList_GetItem: %s",
							get_error_str(ret).c_str());
				if (param.net == buf) {
					if ((ret = PrlStrList_RemoveItem(hNetList.get_handle(), n)))
						return prl_err(ret, "PrlStrList_GetItem: %s",
								get_error_str(ret).c_str());
				}
			}
			if (!param.del) {
				if ((ret = PrlStrList_AddItem(hNetList.get_handle(), param.net.c_str())))
					return prl_err(ret, "PrlStrList_AddItem: %s",
							get_error_str(ret).c_str());
			}
			if ((ret = PrlNetworkClass_SetNetworkList(hClass.get_handle(), hNetList.get_handle())))
				return prl_err(ret, "PrlNetworkClass_SetNetworkList: %s",
						get_error_str(ret).c_str());
			updated = true;
			break;
		}
	}

	if (!updated) {
		if ((ret = PrlApi_CreateStringsList(hNetList.get_ptr())))
			return prl_err(ret, "PrlApi_CreateStringsList: %s",
					get_error_str(ret).c_str());
		if ((ret = PrlStrList_AddItem(hNetList.get_handle(), param.net.c_str())))
			return prl_err(ret, "PrlStrList_AddItem: %s",
					get_error_str(ret).c_str());
		if ((ret = PrlNetworkClass_Create(param.net_class, hClass.get_ptr())))
			return prl_err(ret, "PrlNetworkClass_Create: %s",
					get_error_str(ret).c_str());
		if ((ret =  PrlNetworkClass_SetNetworkList(hClass.get_handle(), hNetList.get_handle())))
			return prl_err(ret, "PrlNetworkClass_SetNetworkList: %s",
					get_error_str(ret).c_str());
		if ((ret = PrlHndlList_AddItem(hClassesList.get_handle(), hClass.get_handle())))
			return prl_err(ret, "PrlHndlList_AddItem: %s",
					get_error_str(ret).c_str());
	}

	prl_log(L_INFO, "update network class entry: %u %s",
				param.net_class,
				param.net.c_str());

	PrlHandle hJob(PrlSrv_UpdateNetworkClassesList(m_srv.get_handle(), hClassesList.get_handle(), 0));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "PrlSrv_UpdateNetworkClassesList: %s", err.c_str());

	return 0;
}

int PrlDisp::get_network_shaping_config(PrlHandle &hShaping)
{
	PrlHandle hResult;
	PRL_RESULT ret;
	std::string err;

	PrlHandle hListJob(PrlSrv_GetNetworkShapingConfig(m_srv.get_handle(), 0));
	if ((ret = get_job_retcode(hListJob.get_handle(), err)))
		return prl_err(ret, "PrlSrv_GetNetworkShapingConfig: %s", err.c_str());
	if ((ret = PrlJob_GetResult(hListJob.get_handle(), hResult.get_ptr())))
		return prl_err(ret, "PrlJob_GetResult: %s",
				get_error_str(ret).c_str());
	if ((ret = PrlResult_GetParam(hResult.get_handle(), hShaping.get_ptr())))
		return prl_err(ret, "PrlResult_GetParams: %s",
				get_error_str(ret).c_str(), ret);
	return 0;
}

int PrlDisp::is_network_shaping_enabled()
{
	PrlHandle hShaping;
	PRL_BOOL bEnabled;
	PRL_RESULT ret;

	if ((ret = get_network_shaping_config(hShaping)))
		return 0;

	if ((ret = PrlNetworkShapingConfig_IsEnabled(hShaping.get_handle(),
					&bEnabled)) || !bEnabled)
		return 0;
	return 1;
}

static int print_network_shaping_bandwidth(PrlHandle &hShaping)
{
	PrlHandle hList;
	PRL_UINT32 resultCount;
	PRL_RESULT ret;
	std::string err;

	if ((ret = PrlNetworkShapingConfig_GetNetworkDeviceBandwidthList(hShaping.get_handle(),
					hList.get_ptr())))
		return prl_err(ret, "PrlNetworkShapingConfig_GetNetworkShapingList: %s",
				get_error_str(ret).c_str(), ret);
	if ((ret = PrlHndlList_GetItemsCount(hList.get_handle(), &resultCount)))
		return prl_err(ret, "PrlHndlList_GetItemsCount: %s",
				get_error_str(ret).c_str());

	fprintf(stdout, "BANDWIDTH\n");
	for (unsigned int i = 0; i < resultCount; i++) {
		PRL_UINT32 bandwidth;
		PrlHandle hEntry;
		if ((ret = PrlHndlList_GetItem(hList.get_handle(), i, hEntry.get_ptr())))
			return prl_err(ret, "PrlHndlList_GetItem: %s",
					get_error_str(ret).c_str());

		char buf[128];
		unsigned int len = sizeof(buf);

		if ((ret = PrlNetworkShapingBandwidthEntry_GetDevice(hEntry.get_handle(), buf, &len))) {
			prl_err(ret, "PrlNetworkShapingBandwidthEntry_GetDevice: %s",
                                get_error_str(ret).c_str());
			continue;
		}
		if ((ret = PrlNetworkShapingBandwidthEntry_GetBandwidth(hEntry.get_handle(), &bandwidth)))
			continue;
		fprintf(stdout, "%s %d \n", buf, bandwidth);
	}
	return 0;
}

static int print_network_shaping_totarrate(PrlHandle &hShaping)
{
	PrlHandle hList;
	PRL_UINT32 resultcount;
	PRL_RESULT ret;
	std::string err;

	if ((ret = PrlNetworkShapingConfig_GetNetworkShapingList(hShaping.get_handle(),
					hList.get_ptr())))
		return prl_err(ret, "prlnetworkshapingconfig_getnetworkshapinglist: %s",
				get_error_str(ret).c_str(), ret);
	if ((ret = PrlHndlList_GetItemsCount(hList.get_handle(), &resultcount)))
		return prl_err(ret, "prlhndllist_getitemscount: %s",
				get_error_str(ret).c_str());

	fprintf(stdout, "TOTALRATE\n");
	for (unsigned int i = 0; i < resultcount; i++) {
		PRL_UINT32 class_id, totalrate, rate;
		PrlHandle hEntry;
		if ((ret = PrlHndlList_GetItem(hList.get_handle(), i, hEntry.get_ptr())))
			return prl_err(ret, "prlhndllist_getitem: %s",
					get_error_str(ret).c_str());

		char buf[128];
		unsigned int len = sizeof(buf);

		if ((ret = PrlNetworkShapingEntry_GetDevice(hEntry.get_handle(), buf, &len)))
			continue;
		if ((ret = PrlNetworkShapingEntry_GetClassId(hEntry.get_handle(), &class_id)))
			continue;
		if ((ret = PrlNetworkShapingEntry_GetTotalRate(hEntry.get_handle(), &totalrate)))
			continue;
		if ((ret = PrlNetworkShapingEntry_GetRate(hEntry.get_handle(), &rate)))
			continue;
		fprintf(stdout, "%s %d %d %d\n", buf, class_id, totalrate, rate);
	}
	return 0;
}

void PrlDisp::get_net_shaping_rate_info(std::ostringstream &os)
{
	PrlHandle hList;
	PRL_UINT32 resultcount;
	PRL_RESULT ret;
	PrlHandle hShaping;

	if (get_network_shaping_config(hShaping))
		return;
	if (PrlNetworkShapingConfig_GetNetworkShapingList(hShaping.get_handle(),
					hList.get_ptr()))
		return;
	if (PrlHndlList_GetItemsCount(hList.get_handle(), &resultcount))
		return;
	for (unsigned int i = 0; i < resultcount; i++) {
		PRL_UINT32 class_id, rate;
		PrlHandle hEntry;
		if ((ret = PrlHndlList_GetItem(hList.get_handle(), i, hEntry.get_ptr())))
			return;
		if (PrlNetworkShapingEntry_GetClassId(hEntry.get_handle(), &class_id))
			return;
		if (PrlNetworkShapingEntry_GetRate(hEntry.get_handle(), &rate))
			return;
		if (i)
			os << " ";
		os << class_id <<":" << rate ;
	}
}

int PrlDisp::list_network_shaping_config()
{
	PrlHandle hShaping;
	PRL_BOOL bEnabled;
	PRL_RESULT ret;

	if ((ret = get_network_shaping_config(hShaping)))
		return ret;

	// print nothing if we are unable to get network shaping status or know
	// that it is disabled
	if ((ret = PrlNetworkShapingConfig_IsEnabled(hShaping.get_handle(),
					&bEnabled)) || !bEnabled)
		return 0;

	print_network_shaping_bandwidth(hShaping);
	print_network_shaping_totarrate(hShaping);

	return 0;
}

int PrlDisp::update_network_shaping_list(const NetworkShapingParam &param,
		PrlHandle &hShaping, bool *pupdated)
{
	PRL_RESULT ret;
	std::string err;
	PrlHandle hShapingList, hShapingEntry;
	PRL_UINT32 resultCount, class_id;
	char device[128];
	unsigned int len;
	bool updated = false;

	if (param.dev.empty())
		return 0;

	if ((ret = PrlNetworkShapingConfig_GetNetworkShapingList(hShaping.get_handle(),
					hShapingList.get_ptr())))
		return prl_err(ret, "PrlNetworkShapingConfig_GetNetworkShapingList: %s",
				get_error_str(ret).c_str(), ret);
	if ((ret = PrlHndlList_GetItemsCount(hShapingList.get_handle(), &resultCount)))
		return prl_err(ret, "PrlHndlList_GetItemsCount: %s",
				get_error_str(ret).c_str());

	for (unsigned int i = 0; i < resultCount; i++) {
		PrlHandle hShapingEntry;
		if ((ret = PrlHndlList_GetItem(hShapingList.get_handle(), i, hShapingEntry.get_ptr())))
			return prl_err(ret, "PrlHndlList_GetItem: %s",
				get_error_str(ret).c_str());

		device[0] = 0;
		len = sizeof(device);
		if ((ret = PrlNetworkShapingEntry_GetDevice(hShapingEntry.get_handle(), device, &len)))
			return prl_err(ret, "PrlNetworkShapingEntry_GetDevice: %s",
					get_error_str(ret).c_str());

		if ((ret = PrlNetworkShapingEntry_GetClassId(hShapingEntry.get_handle(), &class_id)))
			return prl_err(ret, "PrlNetworkShapingEntry_GetClassId: %s",
					get_error_str(ret).c_str());
		// Update
		if (param.dev == device && param.net_class == class_id) {
			if (param.del) {
				if ((ret = PrlHndlList_RemoveItem(hShapingList.get_handle(), i)))
					return prl_err(ret, "PrlHndlList_RemoveItem: %s",
						get_error_str(ret).c_str());
			} else {
				PrlHandle hNetList;

				if ((ret =  PrlNetworkShapingEntry_SetTotalRate(hShapingEntry.get_handle(), param.totalrate)))
					return prl_err(ret, "PrlNetworkShapingEntry_SetTotalRate: %s",
							get_error_str(ret).c_str());
				if ((ret =  PrlNetworkShapingEntry_SetRate(hShapingEntry.get_handle(), param.rate)))
					return prl_err(ret, "PrlNetworkShapingEntry_SetRate: %s",
							get_error_str(ret).c_str());

			}
			updated = true;
			break;
		}
	}

	if (!updated) {
		ret = PrlNetworkShapingEntry_Create(
				param.net_class,
				param.totalrate,
				hShapingEntry.get_ptr());
		if (PRL_FAILED(ret))
			return prl_err(ret, "PrlNetworkShapingEntry_Create: %s",
					get_error_str(ret).c_str());
		if ((ret = PrlNetworkShapingEntry_SetDevice(hShapingEntry.get_handle(), param.dev.c_str())))
			return prl_err(ret, "PrlNetworkShapingEntry_SetDevice: %s",
					get_error_str(ret).c_str());
		if ((ret = PrlNetworkShapingEntry_SetRate(hShapingEntry.get_handle(), param.rate)))
			return prl_err(ret, "PrlNetworkShapingEntry_SetRate: %s",
					get_error_str(ret).c_str());
		if ((ret = PrlHndlList_AddItem(hShapingList.get_handle(), hShapingEntry.get_handle())))
			return prl_err(ret, "PrlHndlList_AddItem: %s",
					get_error_str(ret).c_str());
		updated = true;
	}

	prl_log(L_INFO, "add shaping entry: %s class=%u totalrate=%u rate=%u",
			param.dev.c_str(),
			param.net_class,
			param.totalrate,
			param.rate);

	if (updated) {
		if ((ret = PrlNetworkShapingConfig_SetNetworkShapingList(hShaping.get_handle(),
					hShapingList.get_handle())))
			return prl_err(ret, "PrlNetworkShapingConfig_SetNetworkShapingList: %s",
					get_error_str(ret).c_str(), ret);
		*pupdated = true;
	}

	return 0;
}

int PrlDisp::update_network_bandwidth_list(const NetworkBandwidthParam &param,
		PrlHandle &hShaping, bool *pupdated)
{
	PRL_RESULT ret;
	std::string err;
	PrlHandle hList, hEntry;
	PRL_UINT32 resultCount;
	char device[128];
	unsigned int len;
	bool updated = false;

	if (param.dev.empty())
		return 0;

	if ((ret = PrlNetworkShapingConfig_GetNetworkDeviceBandwidthList(hShaping.get_handle(),
					hList.get_ptr())))
		return prl_err(ret, "PrlNetworkShapingConfig_GetNetworkDeviceBandwidthList: %s",
				get_error_str(ret).c_str(), ret);
	if ((ret = PrlHndlList_GetItemsCount(hList.get_handle(), &resultCount)))
		return prl_err(ret, "PrlHndlList_GetItemsCount: %s",
				get_error_str(ret).c_str());

	for (unsigned int i = 0; i < resultCount; i++) {
		PrlHandle hEntry;
		if ((ret = PrlHndlList_GetItem(hList.get_handle(), i, hEntry.get_ptr())))
			return prl_err(ret, "PrlHndlList_GetItem: %s",
				get_error_str(ret).c_str());

		device[0] = 0;
		len = sizeof(device);
		if ((ret = PrlNetworkShapingBandwidthEntry_GetDevice(hEntry.get_handle(), device, &len)))
			return prl_err(ret, "PrlNetworkShapingEntry_GetDevice: %s",
					get_error_str(ret).c_str());

		// Update
		if (param.dev == device) {
			if (param.del) {
				if ((ret = PrlHndlList_RemoveItem(hList.get_handle(), i)))
					return prl_err(ret, "PrlHndlList_RemoveItem: %s",
						get_error_str(ret).c_str());
			} else {
				PrlHandle hNetList;

				if ((ret = PrlNetworkShapingBandwidthEntry_SetBandwidth(hEntry.get_handle(), param.bandwidth)))
					return prl_err(ret, "PrlNetworkShapingEntry_SetTotalRate: %s",
							get_error_str(ret).c_str());

			}
			updated = true;
			break;
		}
	}

	if (!updated) {
		ret = PrlNetworkShapingBandwidthEntry_Create(
				param.dev.c_str(),
				param.bandwidth,
				hEntry.get_ptr());
		if (PRL_FAILED(ret))
			return prl_err(ret, "PrlNetworkShapingBandwidthEntry_Create: %s",
					get_error_str(ret).c_str());
		if ((ret = PrlHndlList_AddItem(hList.get_handle(), hEntry.get_handle())))
			return prl_err(ret, "PrlHndlList_AddItem: %s",
					get_error_str(ret).c_str());
		updated = true;
	}

	prl_log(L_INFO, "add bandwidth entry: %s %u",
			param.dev.c_str(),
			param.bandwidth);

	if (updated) {
		if ((ret = PrlNetworkShapingConfig_SetNetworkDeviceBandwidthList(hShaping.get_handle(),
					hList.get_handle())))
			return prl_err(ret, "PrlNetworkShapingConfig_SetNetworkShapingList: %s",
					get_error_str(ret).c_str(), ret);
		*pupdated = true;
	}

	return 0;
}

int PrlDisp::update_network_shaping_config(const DispParam &param)
{
	PrlHandle hResult, hShaping;
	PRL_RESULT ret;
	std::string err;
	bool updated = false;

	if ((ret = get_network_shaping_config(hShaping)))
		return ret;

	if (param.network_shaping.enable != -1) {
		if ((ret = PrlNetworkShapingConfig_SetEnabled(hShaping.get_handle(),
						!param.network_shaping.enable ? PRL_FALSE : PRL_TRUE)))
			return prl_err(ret, "PrlNetworkShapingConfig_SetEnabled: %s",
					get_error_str(ret).c_str(), ret);
		updated = true;
	}

	if ((ret = update_network_bandwidth_list(param.network_bandwidth, hShaping, &updated)))
		return ret;

	if ((ret = update_network_shaping_list(param.network_shaping, hShaping, &updated)))
		return ret;

	if (updated) {
		PrlHandle hJob(PrlSrv_UpdateNetworkShapingConfig(m_srv.get_handle(),
					hShaping.get_handle(), 0));
		if ((ret = get_job_retcode(hJob.get_handle(), err)))
			return prl_err(ret, "PrlSrv_UpdateNetworkShapingConfig: %s", err.c_str());
	}

	return 0;
}

int PrlDisp::set(const DispParam &param)
{
	int ret;
	std::string err;

	if (!param.offline_service.name.empty()) {
		if ((ret = update_offline_service(param.offline_service)))
			return ret;
	}

	if (!param.network_shaping.dev.empty() ||
		!param.network_bandwidth.dev.empty() ||
		param.network_shaping.enable != -1) {
		if ((ret = update_network_shaping_config(param)))
			return ret;
	}
	if (!param.network_class.net.empty()) {
		if ((ret = update_network_classes_config(param.network_class)))
			return ret;
	}

	if (param.set_vnc_encryption) {
		if ((ret = set_vnc_encryption(param.vnc_public_key,
						param.vnc_private_key)))
			return ret;
	}

	PrlHandle hJob(PrlSrv_CommonPrefsBeginEdit(m_srv.get_handle()));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "%s", err.c_str());

	if ((ret = get_config_handle()))
		return ret;

	if (param.mem_limit) {
		if ((ret = set_mem_limit(param.mem_limit)))
			return ret;
	}
	if (!param.vcmmd_policy.empty()) {
		if ((ret = set_vcmmd_policy(param.vcmmd_policy)))
			return ret;
	}
	if (param.allow_mng_settings != -1) {
		if ((ret = set_allow_mng_settings(param.allow_mng_settings)))
			return ret;
	}
	if (param.cep_mechanism != -1) {
		if ((ret = switch_cep_mechanism(param.cep_mechanism)))
			return ret;
	}
	if (param.verbose_log_level != -1) {
		if ((ret = switch_verbose_log_level(param.verbose_log_level)))
			return ret;
	}
	if (!param.min_security_level.empty()) {
		if ((ret = set_min_security_level(param.min_security_level)))
			return ret;
	}
	if (param.assign_mode != AM_NONE) {
		if ((ret = set_dev_assign_mode(param.device, param.assign_mode)))
			return ret;
	}
	if (!param.backup_path.empty()) {
		if ((ret = set_backup_path(param.backup_path)))
			return ret;
	}

	if (param.backup_mode.is_initialized())
	{
		if ((ret = set_backup_mode(param.backup_mode.get())))
				return ret;
	}

	if (param.set_vnc_clipboard != -1) {
		if ((ret = set_vnc_clipboard(param.set_vnc_clipboard)))
			return ret;
	}

	if (param.backup_timeout) {
		if ((ret = set_backup_timeout(param.backup_timeout)))
			return ret;
	}
	if (param.change_backup_settings) {
		if ((ret = set_def_backup_storage(param.def_backup_storage)))
			return ret;
	}

	if (param.backup_tmpdir) {
		if ((ret = set_backup_tmpdir(param.backup_tmpdir.get(),
									 param.backup_mode.is_initialized())))
			return ret;
	}
	if (param.log_rotation != -1) {
		if ((ret = switch_log_rotation(param.log_rotation)))
			return ret;
	}

	ret = set_confirmations_list(param.cmd_require_pwd_list);
	if (ret)
		return ret;

	if (!param.cpu_features_mask_changes.empty()) {
		if ((ret = set_cpu_features_mask(param.cpu_features_mask_changes)))
			return ret;
	}
	if (param.vm_cpulimit_type != -1) {
		if ((ret = set_vm_cpulimit_type(param.vm_cpulimit_type)))
			return ret;
	}

	if (is_updated()) {
		PrlHandle hJob(PrlSrv_CommonPrefsCommit(m_srv.get_handle(), m_hDisp));
		if ((ret = get_job_retcode(hJob.get_handle(), err)))
			return prl_err(ret, "Failed to apply new parameters: %s",
				get_error_str(ret).c_str());
		prl_log(0, "The Server has been successfully configured.");
	}

	return 0;
}

int PrlDisp::list(const DispParam &param)
{
	(void) param;
	list_network_classes_config();
	list_network_shaping_config();
	return 0;
}

int PrlDisp::usbassign(const UsbParam &param, bool use_json)
{
	std::string err;
	PRL_RESULT res;

	if( param.cmd != UsbParam::List )
	{
		PrlHandle hJob(PrlSrv_CommonPrefsBeginEdit(m_srv.get_handle()));
		if ((res = get_job_retcode(hJob.get_handle(), err)))
			return prl_err(res, "%s", err.c_str());
	}
	res = get_config_handle();
	if( PRL_FAILED(res) )
		return res;

	switch(param.cmd)
	{
	case UsbParam::List:
	{
		PrlOutFormatter &f = *(get_formatter(use_json));
		PRL_UINT32 nUsbIdents = 0;
		res = PrlDispCfg_GetUsbIdentityCount( m_hDisp, &nUsbIdents );
		if( !PRL_SUCCEEDED(res) )
			return prl_err(res, "PrlDispCfg_GetUsbIdentityCount return");

		f.open_list();
		for( PRL_UINT32 i = 0; i < nUsbIdents; i++ )
		{
			PrlHandle hUsbIdent;
			char buf[4096];
			unsigned int len;

			f.tbl_row_open();
			res = PrlDispCfg_GetUsbIdentity(m_hDisp, i, hUsbIdent.get_ptr());
			if( PRL_FAILED(res) )
				return prl_err(res, "PrlDispCfg_GetUsbIdentity %d", i);

			len = sizeof(buf);
			res = PrlUsbIdent_GetFriendlyName(hUsbIdent.get_handle(), buf, &len);
			if( PRL_FAILED(res) )
				return prl_err(res, "PrlUsbIdent_GetFriendlyName");
			f.tbl_add_item("Name", "%-40s ", buf);

			len = sizeof(buf);
			res = PrlUsbIdent_GetSystemName(hUsbIdent.get_handle(), buf, &len);
			if( PRL_FAILED(res) )
				return prl_err(res,"PrlUsbIdent_GetSystemName");
			f.tbl_add_item("System name", "'%s' ", buf);

			len = sizeof(buf);
			res = PrlUsbIdent_GetVmUuidAssociation(hUsbIdent.get_handle(), buf, &len);
			if (PRL_FAILED(res))
				return prl_err(res,"PrlUsbIdent_GetVmUuidAssociation");
			f.tbl_add_uuid("VM UUID", "%s", buf);
			f.tbl_row_close();
		}
		f.close_list();
		fprintf(stdout, "%s", f.get_buffer().c_str());
		break;
	}
	case UsbParam::Delete:
	case UsbParam::Set:
		res = PrlDispCfg_SetUsbIdentAssociation(m_hDisp, param.name.c_str(),param.id.c_str(),0);
		if( PRL_FAILED(res) )
			return prl_err(res,
						   "PrlDispCfg_SetUsbIdentAssociation name: '%s'' vmid: '%s'",
						   param.name.c_str(), param.id.c_str());
		set_updated();
		break;
	}

	if (is_updated()) {
		PrlHandle hJob(PrlSrv_CommonPrefsCommit(m_srv.get_handle(), m_hDisp));
		if ((res = get_job_retcode(hJob.get_handle(), err)))
			return prl_err(res, "Failed to apply new parameters: %s",
				get_error_str(res).c_str());
		prl_log(0, "The Server has been successfully configured.");
	}

	return 0;
}

void PrlDisp::clear()
{
	if (m_hDisp)
		PrlHandle_Free(m_hDisp);
	m_hDisp = 0;
}

PrlDisp::~PrlDisp()
{
	clear();
}

int PrlDisp::set_vnc_encryption(const std::string &public_key, const std::string &private_key)
{
	PRL_RESULT ret;
	std::string err;

	PrlHandle hJob(PrlSrv_SetVNCEncryption(m_srv.get_handle(), public_key.c_str(),
								private_key.c_str(), 0));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "Failed to set VNC encryption: %s", err.c_str());
	prl_log(0, "The VNC encryption has been successfully configured.");
	return 0;

}

int PrlDisp::set_vnc_clipboard(int on_off)
{
	PRL_RESULT ret;

	if ((ret = get_config_handle()))
		return ret;

	if ((ret = PrlDispCfg_SetVNCEnableClipboard(m_hDisp, on_off ? PRL_TRUE : PRL_FALSE)))
		return prl_err(ret, "PrlDispCfg_SetVNCEnableClipboard: %s",
			get_error_str(ret).c_str());
	set_updated();

	return 0;
}

int PrlDisp::get_vnc_clipboard()
{
	PRL_RESULT ret;
	PRL_BOOL res = 0;

	if ((ret = PrlDispCfg_IsVNCEnableClipboard(m_hDisp, &res)))
	{
		prl_err(ret, "PrlDispCfg_IsVNCEnableClipboard %s", get_error_str(ret).c_str());
		return 0;
	}

	return res;
}

int PrlDisp::set_vm_cpulimit_type(int vm_cpulimit_type)
{
	int ret;

	if ((ret = get_config_handle()))
		return ret;

	if ((ret = PrlDispCfg_SetVmCpuLimitType(m_hDisp, vm_cpulimit_type))) {
		return prl_err(ret, "PrlDispCfg_SetVmCpuLimitType: %s",
			get_error_str(ret).c_str());
	}
	set_updated();

	return 0;
}

