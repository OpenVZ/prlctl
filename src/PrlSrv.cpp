//////////////////////////////////////////////////////////////////////////////
///
/// @file PrlSrv.cpp
///
/// Server managenment
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

// #include "Interfaces/ParallelsDomModel.h"
#include <set>
#include <vector>
#include <sstream>
#include <algorithm>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include <PrlIOStructs.h>
#include <PrlPluginClasses.h>
#include <PrlApiDisp.h>
#include <PrlApiNet.h>
#include <PrlApiDeprecated.h>

#include "CmdParam.h"
#include "PrlSrv.h"
#include "PrlVm.h"
#include "PrlDev.h"
#include "Utils.h"
#include "PrlList.h"
#include "Logger.h"
#include "PrlDisp.h"
#include "PrlCleanup.h"

#ifdef _WIN_
#include <windows.h>
#include <conio.h>
#define snprintf _snprintf
#else
#include <unistd.h>
inline int _getch() { return getchar() ; }
#endif

#ifdef _LIN_
#include <netdb.h>
#include <malloc.h>
#include <string.h>
#endif

static int server_event_handler(PRL_HANDLE hEvent, void *data)
{
	PrlHandle h(hEvent);
	PRL_HANDLE_TYPE type;
	int ret;
	bool batch = (data != NULL) ? *(reinterpret_cast<bool *>(data)) : false;

	if ((ret = PrlHandle_GetType(h.get_handle(), &type))) {
		prl_log(L_DEBUG, "PrlHandle_GetType: %s",
				get_error_str(ret).c_str());
		return ret;
	}

	if (type == PHT_EVENT) {
		PRL_EVENT_TYPE evt_type;
		PRL_UINT32 progress;
		std::string stage;


		if ((ret = PrlEvent_GetType(h.get_handle(), &evt_type))) {
			prl_log(L_DEBUG, "PrlEvent_GetType: %s",
					get_error_str(ret).c_str());
			return ret;
		}
		prl_log(L_DEBUG, "EVENT type=%d", evt_type);
		if (evt_type == PET_DSP_EVT_APPLIANCE_DOWNLOAD_PROGRESS_CHANGED) {
			if (get_progress(h, progress, stage))
				return -1;
			if (batch)
				fprintf(stdout, "stage download %u%%\n", progress);
			else
				print_procent(progress);
		} else if (evt_type == PET_DSP_EVT_CONVERT_THIRD_PARTY_PROGRESS_CHANGED) {
			if (get_progress(h, progress, stage))
				return -1;
			if (batch)
				fprintf(stdout, "stage download %u%%\n", progress);
			else
				print_procent(progress);
		} else if (evt_type == PET_DSP_EVT_APPLIANCE_ARCHIVE_UNPACK_STARTED) {
			if (batch)
				fprintf(stdout, "stage extracting 0%%\n");
			else {
				fprintf(stdout, "extracting...");
				fflush(stdout);
			}
		} else if (evt_type == PET_DSP_EVT_APPLIANCE_ARCHIVE_UNPACK_FINISHED) {
			if (batch)
				fprintf(stdout, "stage extracting 100%%\n");
			else
				fprintf(stdout, "\n");
		} else if (evt_type == PET_DSP_EVT_VM_MESSAGE) {
			std::string err;
			get_result_error_string(hEvent, err);
			fprintf(stderr, "%s\n", err.c_str());
		}
	}
	return 0;
}

int PrlSrv::reg_event_callback(PRL_EVENT_HANDLER_PTR fn, void *data)
{
	int ret;

	ret = PrlSrv_RegEventHandler(get_handle(), fn, data);
	if (PRL_FAILED(ret))
		return prl_err(ret, "PrlSrv_RegEventHandler: %s",
				get_error_str(ret).c_str());
	return 0;
}

void PrlSrv::unreg_event_callback(PRL_EVENT_HANDLER_PTR fn, void *data)
{
	PrlSrv_UnregEventHandler(get_handle(), fn, data);
}

static bool is_local(const std::string &host)
{
	if (host.empty() ||
	    host == "localhost" ||
	    host == "localhost.localdomain" ||
	    host == "127.0.0.1")
	{
		return true;
	}
	return false;
}

int PrlSrv::login(const LoginInfo &login)
{
	PRL_RESULT ret;
	PRL_HANDLE hJob = PRL_INVALID_HANDLE;

	if (m_logged)
		logoff();
	ret = PrlSrv_Create(&m_hSrv);
	if (PRL_FAILED(ret))
		return prl_err(ret, "PrlSrv_Create returned the following error: %s",
				get_error_str(ret).c_str());
	if (is_local(login.server)) {
		prl_log(L_INFO, "Logging in");
		hJob = PrlSrv_LoginLocalEx(m_hSrv, NULL, 0, PSL_HIGH_SECURITY, PACF_NON_INTERACTIVE_MODE);
	} else {
#if 0
		struct hostent *he;

		if ((he = gethostbyname(login.server.c_str())) == NULL)
			return prl_err(-1, "unable to resolve '%s': %s",
					login.server.c_str(), hstrerror(h_errno));
#endif

		bool ok = false;
		std::string passwd(login.get_passwd_from_stack(ok));

		if (!ok) {
			ret = read_passwd(login.user, login.server, passwd);
			if (ret)
				return ret;
		}
		prl_log(L_INFO, "Logging in %s@%s", login.user.c_str(),
			login.server.c_str());


		hJob = PrlSrv_LoginEx(m_hSrv,
				login.server.c_str(),
				login.user.c_str(),
				passwd.c_str(),
				NULL,
				login.port,
				JOB_WAIT_TIMEOUT/*TODO: timeout support need to be added*/,
				PSL_HIGH_SECURITY,
				PACF_NON_INTERACTIVE_MODE);
	}

	std::string err;
	if ((ret = get_job_retcode(hJob, err))) {
		// Workaround for bug #266053
		if (login.server.empty() && ret == PRL_ERR_OUT_OF_DISK_SPACE)
			prl_err(ret, "Failed to login in local mode: %s"
				" Try to login with -l,--login option.",
				err.c_str());
		else
			prl_err(ret, "Login failed: %s", err.c_str());
	} else {

		do {
			PrlHandle job(PrlSrv_SetNonInteractiveSession(m_hSrv, PRL_TRUE, 0));
			if ((ret = get_job_retcode(job.get_handle(), err))) {
				prl_err(ret, "PrlSrv_SetNonInteractiveSession: %s", err.c_str());
				break;
			}
			PrlHandle hResult;
			if ((ret = PrlJob_GetResult(hJob, hResult.get_ptr()))) {
				prl_err(ret, "PrlJob_GetResult: %s",
						get_error_str(ret).c_str());
				break;
			}
			PrlHandle hResponse;
			if ((ret = PrlResult_GetParam(hResult.get_handle(), hResponse.get_ptr()))) {
				prl_err(ret, "PrlResult_GetParams: %s",
						get_error_str(ret).c_str(), ret);
				break;
			}

			char buf[64];
			PRL_UINT32 len = sizeof(buf);

			if ((ret = PrlLoginResponse_GetServerUuid(hResponse.get_handle(), buf, &len))) {
				prl_err(ret, "PrlLoginResponse_GetServerUuid: %s",
						get_error_str(ret).c_str());
				break;
			}
			m_uuid = buf;
			len = sizeof(buf);
			prl_log(L_DEBUG, "server uuid=%s", get_uuid());
			if ((ret = PrlLoginResponse_GetSessionUuid(hResponse.get_handle(), buf, &len))) {
				prl_err(ret, "PrlLoginResponse_GetSessionUuid: %s",
						get_error_str(ret).c_str());
				break;
			}
			m_sessionid = buf;
			prl_log(L_DEBUG, "sessionid=%s", get_sessionid());
			m_logged = true;

		} while (0);
	}
	PrlHandle_Free(hJob);
	return ret;
}

void PrlSrv::logoff()
{
	if (!m_logged)
		return;

	prl_log(L_INFO, "Logging off");
	PRL_RESULT ret;
	std::string err;

	PrlHandle hJob(PrlSrv_Logoff(m_hSrv));
	if ((ret = get_job_retcode(hJob.get_handle(), err, m_logoffTimeout)))
		prl_log(L_DEBUG, "Warning! PrlSrv_Logoff failed: %s", err.c_str());
	m_logged = false;
	// Clean internal state belong to given connection
	clear();
}

int PrlSrv::vm_from_result(PrlHandle &hResult, int i, PrlVm **vm)
{
	PrlHandle hVm;
	PRL_RESULT ret;

	ret = PrlResult_GetParamByIndex(hResult.get_handle(), i, hVm.get_ptr());
	if (PRL_FAILED(ret))
		return prl_err(ret, "PrlResult_GetParamByIndex: %s",
			get_error_str(ret).c_str());

	char uuid[128];
	PRL_UINT32 size = sizeof(uuid);

	ret = PrlVmCfg_GetUuid(hVm.get_handle(), uuid, &size);
	if (PRL_FAILED(ret))
		return prl_err(ret, "PrlVmCfg_GetUuid: %s",
			get_error_str(ret).c_str());

	char name[4096];
	size = sizeof(name);

	ret = PrlVmCfg_GetName(hVm.get_handle(), name, &size);
	if (PRL_FAILED(ret))
		return prl_err(ret, "PrlVmCfg_GetName: %s",
			get_error_str(ret).c_str());

	unsigned int ostype;
	ret = PrlVmCfg_GetOsType(hVm.get_handle(), &ostype);
	if (PRL_FAILED(ret))
		return prl_err(ret, "PrlVmCfg_GetOsType: %s",
			get_error_str(ret).c_str());

	*vm = new PrlVm(*this, hVm.release_handle(), std::string(uuid),
			std::string(name), ostype);
	return 0;
}

/* Get VM list from server
   In case id specified then return specified VM
*/
int PrlSrv::get_vm_list(PrlList<PrlVm *> &list, unsigned vmtype, bool full_info)
{
	PRL_RESULT ret;

	if (m_hSrv == PRL_INVALID_HANDLE)
		return prl_err(-1, "Failed to get the handle; you are not"
			" logged on to the server.");

	PrlHandle hResult;
	PRL_UINT32 resultCount = 0;
	PRL_UINT32 flags = vmtype | (full_info ? PGVLF_FILL_AUTOGENERATED : PGVLF_GET_STATE_INFO);

	PrlHandle hJob(PrlSrv_GetVmListEx(m_hSrv, flags));
	if ((ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &resultCount)))
		return prl_err(ret, "Failed to get the list of virtual machines: %s",
			get_error_str(ret).c_str());

	PrlHandle hVm;
	for (unsigned int i = 0; i < resultCount; i++) {
		PrlVm *vm;
		ret = vm_from_result(hResult, i, &vm);

		list.add(vm);
		prl_log(L_DEBUG, "Adding the virtual machine: %s", vm->get_name().c_str());
	}

	return ret;
}

int PrlSrv::update_vm_list(unsigned vmtype, bool full_info)
{
	m_VmList.del();
	return get_vm_list(m_VmList, vmtype, full_info);
}

int PrlSrv::problem_report(const CmdParamData &param)
{
	PRL_RESULT ret;
	PRL_UINT32 resultCount = 0;
	PrlHandle hResult, hProblemReport;

	//Create or request problem report from server
	if (!param.problem_report.stand_alone)
	{
		PrlHandle hJob(PrlSrv_GetPackedProblemReport(m_hSrv, 0));
		ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &resultCount);

		if (PRL_ERR_UNRECOGNIZED_REQUEST == ret)//Seems remote server has old scheme
		{
			PrlHandle hJob(PrlSrv_GetProblemReport(m_hSrv));
			if ((ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &resultCount)) != 0)
				return prl_err(-1, "Failed to request problem report: %s",
					get_error_str(ret).c_str());

		} else if (PRL_FAILED(ret)) {
			return prl_err(-1, "Failed to get packed problem report: %s",
				get_error_str(ret).c_str());
		}

		if ((ret = PrlResult_GetParam(hResult.get_handle(), hProblemReport.get_ptr())) != 0)
		{
			return prl_err(-1, "Failed to get problem report from result: %s",
				get_error_str(ret).c_str());
		}
	}
	else
	{
		if ((ret = PrlApi_CreateProblemReport(PRS_NEW_PACKED, hProblemReport.get_ptr())))
			return prl_err(-1, "Failed to create local instance of problem report: %s",
				get_error_str(ret).c_str());
		if ((ret = PrlReport_SetType(hProblemReport.get_handle(), PRT_USER_DEFINED_ON_DISCONNECTED_SERVER)) != 0)
				prl_log(L_ERR, "Failed to set problem report type: %s", get_error_str(ret).c_str());
	}

	if ((ret = assembly_problem_report(hProblemReport, param.problem_report, 0)) != 0)
		return prl_err(-1, "Failed to assembly problem report: %s",
			get_error_str(ret).c_str());

	if (param.problem_report.send)
		ret = send_problem_report(hProblemReport, param.problem_report);
	else
		ret = send_problem_report_on_stdout(hProblemReport);

	return ret;
}

int PrlSrv::pre_hibernate()
{
	PRL_RESULT ret;
	std::string err;

	PrlHandle hJob(PrlSrv_PrepareForHibernate(m_hSrv, 0));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "Prepare for hibernate failed: %s", err.c_str());

	return 0;
}

int PrlSrv::after_hibernate()
{
	PRL_RESULT ret;
	std::string err;

	PrlHandle hJob(PrlSrv_AfterHostResume(m_hSrv, 0));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "After host resume  failed: %s", err.c_str());

	return 0;
}

int PrlSrv::status_vm(const CmdParamData &param)
{
	int ret;
	std::string out;
	PrlVm *vm = NULL;

	ret = get_vm_config(param, &vm);
	if (ret)
		return ret;

	if (vm != NULL) {
		out = vm->get_vm_type_str();
		out += " ";
		out += param.id;
		vm->update_state();
		out += " exist ";
		out += vmstate2str(vm->get_state());
	} else {
		out = "VM ";
		out += param.id;
		out += " deleted";
	}
	printf("%s\n", out.c_str());
	return 0;
}

int PrlSrv::get_vm_config(const std::string &id, PrlVm **vm, bool ignore_not_found,
	int nFlags /*= PGVC_SEARCH_BY_UUID | PGVC_SEARCH_BY_NAME*/ )
{
	PRL_RESULT ret;

	if (m_hSrv == PRL_INVALID_HANDLE)
		return prl_err(-1, "Failed to get the handle; you are not"
			" logged on to the server.");

	PrlHandle hResult;

	PrlHandle hJob(PrlSrv_GetVmConfig(m_hSrv, id.c_str(), nFlags));
	std::string err;
	if ((ret = get_job_retcode(hJob.get_handle(), err))) {
		if (ret == PRL_ERR_VM_UUID_NOT_FOUND && ignore_not_found)
			return 0;
		return prl_err(ret, "Failed to get VM config: %s",
				err.c_str());
	}

	if ((ret = PrlJob_GetResult(hJob, hResult.get_ptr())))
		return prl_err(ret, "PrlJob_GetResult returned the following error:"
				" %s [%d]",
				get_error_str(ret).c_str(), ret);

	ret = vm_from_result(hResult, 0, vm);

	prl_log(L_DEBUG, "The virtual machine found: %s", (*vm)->get_name().c_str());

	return ret;
}

int PrlSrv::get_vm_config(const CmdParamData &param, PrlVm **vm,
	bool ignore_not_found /*= false*/)
{
	*vm = NULL;
	int ret = get_vm_config(param.id, vm, true);
	if ((ret == 0) && (*vm == NULL))
		ret = get_vm_config(param.original_id, vm, ignore_not_found,
			PGVC_SEARCH_BY_NAME);
	return ret;
}

int PrlSrv::run_action(const CmdParamData &param)
{
	int ret;

	if (!m_logged && !param.problem_report.stand_alone && !param.xmlrpc.action_provided) {
		if ((ret = login(param.login)))
			return ret;
	}
	if (param.action == SrvShutdownAction)
		return shutdown(param.disp.force,
				param.disp.suspend_vm_to_pram);
	else if (param.action == SrvHwInfoAction)
		return print_info(param.disp.info_license, param.disp.info_activation_id
							, param.disp.info_deactivation_id , param.use_json);
	else if (param.action == SrvInstallLicenseAction)
		return install_license(param.key, param.new_name, param.company, param.deferred);
	else if (param.action == VmRegisterAction)
		return register_vm(param);
	else if (param.action == VmConvertAction)
		return convert_vm(param);
	else if (param.action == VmCreateAction)
		if (param.vmtype == PVTF_CT)
			return create_ct(param);
		else
			return create_vm(param);
	else if (param.action == VmListAction)
		return list_vm(param);
	else if (param.action == SrvProblemReportAction)
		return problem_report(param);
	else if (param.action == VmRestoreAction) {
		return restore_vm(param);
	} else if (param.action == VmBackupDeleteAction) {
		return backup_delete(param);
	} else if (param.action == VmBackupListAction)
		return backup_list(param);
	else if (param.action == VmStatusAction)
		return status_vm(param);

	/* Per VM actions */
	PrlVm *vm = NULL;
	ret = get_vm_config(param, &vm);
	if (ret)
		return ret;

	if (vm == NULL)
		return prl_err(-1, "The %s virtual machine does not exist.",
			param.id.c_str());

	m_VmList.add(vm);

	if ((ret = vm->check_whether_encrypted(param.action)))
		return ret;

	if ((ret = vm->update_state()))
		return ret;

	switch (param.action) {
	case VmStartAction:
		return vm->start(param.start_opts);
	case VmMountAction:
		return vm->mount(param.mnt_opts);
	case VmUmountAction:
		return vm->umount();
	case VmChangeSidAction:
		return vm->change_sid();
	case VmResetUptimeAction:
		return vm->reset_uptime();
	case VmEncryptAction:
		return vm->encrypt(param);
	case VmDecryptAction:
		return vm->decrypt(param);
	case VmChangePasswdAction:
		return vm->change_passwd();
	case VmAuthAction:
		return vm->auth(param.user_name, param.user_password);
	case VmStopAction:
		return vm->stop(param);
	case VmResetAction:
		return vm->reset();
	case VmRestartAction:
		return vm->restart();
	case VmSuspendAction:
		return vm->suspend();
	case VmResumeAction:
		return vm->resume();
	case VmPauseAction:
		return vm->pause();
	case VmDestroyAction:
		return vm->destroy(param);
	case VmUnregisterAction:
		return vm->unreg();
	case VmCloneAction:
		return vm->clone(param.new_name, param.uuid, param.vm_location,
				param.clone_flags);
	case VmInstallToolsAction:
		return vm->install_tools();
	case VmSetAction: {
		if ((ret = vm->set(param)))
			return prl_err(ret, "Failed to configure the virtual machine.");
		return 0;
	}
	case VmSnapshot_Create:
		return vm->snapshot_create(param.snapshot);
	case VmSnapshot_SwitchTo:
		return vm->snapshot_switch_to(param.snapshot);
	case VmSnapshot_Delete:
		return vm->snapshot_delete(param.snapshot);
	case VmSnapshot_List:
		return vm->snapshot_list(param);
	case VmMigrateAction:
		return vm->migrate(param.migrate);
	case VmMoveAction:
		return vm->move(param.vm_location);
	case VmPerfStatsAction:
		return print_statistics(param, vm);
	case VmProblemReportAction:
		return vm->problem_report(param);
	case VmConsoleAction:
		return vm->console();
	case VmEnterAction:
	case VmExecAction:
		return vm->exec(param);
	case VmBackupAction:
		return backup_vm(param);
	case VmInternalCmd:
		return vm->internal_cmd(param.argv);
	default:
		return -1;
	}
}

int PrlSrv::run_disp_action(const CmdParamData &param)
{
	int ret;

	if (!m_logged && !param.problem_report.stand_alone && !param.xmlrpc.action_provided) {
		if ((ret = login(param.login)))
			return ret;
	}
	switch (param.action) {
	case SrvShutdownAction:
		return shutdown(param.disp.force,
				param.disp.suspend_vm_to_pram);
	case SrvHwInfoAction:
		return print_info(param.disp.info_license
			, param.disp.info_activation_id, param.disp.info_deactivation_id, param.use_json);
	case SrvUsrListAction:
		return list_user(param, param.use_json);
	case SrvUsrSetAction:
		return set_user(param);
	case SrvInstallLicenseAction:
		return install_license(param.key, param.new_name, param.company, param.deferred);
	case SrvDeferredLicenseAction:
		return deferred_license_op(param);
	case SrvUpdateLicenseAction:
		return update_license();
	case DispSetAction:
		 return m_disp->set(param.disp);
	case DispListAction:
		 return m_disp->list(param.disp);
	case DispUsbAction:
	{
		if( param.usb.cmd == UsbParam::Set )
		{
			if ((ret = update_vm_list(PVTF_CT|PVTF_VM)))
				return ret;
			PrlVm *vm = m_VmList.find(param.usb.id);
			if (!vm)
				return prl_err(-1, "The %s virtual machine does not exist.",
						param.usb.id.c_str());
			UsbParam newParam = param.usb;
			newParam.id = vm->get_uuid();
			return m_disp->usbassign( newParam, param.use_json );
		}
		return m_disp->usbassign( param.usb, param.use_json );
	}
	case SrvVNetAction:
		 return vnetwork(param.vnet, param.use_json);
	case SrvPrivNetAction:
		 return priv_network(param.privnet, param.use_json);
	case SrvPerfStatsAction:
		return print_statistics(param) ;
	case SrvProblemReportAction:
		return problem_report(param);
	case SrvPreHibernateAction:
		return pre_hibernate();
	case SrvAfterHibernateAction:
		return after_hibernate();
	case SrvInstApplianceAction:
		return appliance_install(param);
	case SrvCtTemplateAction:
		 return ct_templates(param.ct_tmpl, param.use_json);
	case SrvCopyCtTemplateAction:
		 return copy_ct_template(param.ct_tmpl, param.copy_ct_tmpl);
	case SrvPluginAction:
		return plugin(param.plugin, param.use_json);
	case SrvMonitorAction:
		return run_monitor();
	default:
		return -1;
	}
}


static void clear_device(PrlVm &vm, DevType type)
{
	for (PrlDev* d; (0 == vm.get_dev_info()) && (d = vm.find_dev(type, 0));)
		d->remove();
}

int PrlSrv::create_ct(const CmdParamData &param)
{
	PRL_RESULT ret;
	PRL_UINT32 resultCount;
	PrlVm *vm;

	if ((ret = update_vm_list(PVTF_CT|PVTF_VM)))
		return ret;

	if (m_VmList.find(param.id))
		return prl_err(-1, "The %s VM already exists.",
			param.id.c_str());
	prl_log(0, "Creating the Virtuozzo Container...");

	PrlHandle hVm;
	PrlHandle hCreateParam;
	PRL_GET_VM_CONFIG_PARAM_DATA create_param;

	create_param.nVmType = PVT_CT;
	create_param.sConfigSample = param.config_sample.c_str();
	create_param.nOsVersion = 0;

	PrlHandle hResult;
	PrlHandle hJob(PrlSrv_GetDefaultVmConfig(m_hSrv, &create_param, 0));
	if (!(ret = get_job_result(hJob, hResult.get_ptr(), &resultCount))) {
		if ((ret = PrlResult_GetParam(hResult.get_handle(), hVm.get_ptr())))
				prl_err(ret, "PrlSrv_GetDefaultVmConfig: %s",
						get_error_str(ret).c_str());
	} else {
		return prl_err(ret, "Failed to get Container configuration: %s",
					get_error_str(ret).c_str());
	}
	if ((ret = PrlVmCfg_SetVmType(hVm.get_handle(), PVT_CT)))
		return prl_err(ret, "PrlVmCfg_SetVmType: %s",
			get_error_str(ret).c_str());
	if (!param.ostemplate.empty())
		PrlVmCfg_SetOsTemplate(hVm.get_handle(), param.ostemplate.c_str());
	if (!param.config_sample.empty())
		PrlVmCfg_ApplyConfigSample(hVm, param.config_sample.c_str());

	vm = m_VmList.add(new PrlVm(*this, hVm.release_handle(), param.uuid, param.id, 0));
	vm->set_state(VMS_STOPPED);
	if ((ret = PrlVmCfg_SetName(vm->get_handle(), param.id.c_str())))
		return prl_err(ret, "PrlVmCfg_SetName returned the following error: %s",
				get_error_str(ret).c_str());
	if (param.nohdd)
		clear_device(*vm, DEV_HDD);

	if ((ret = vm->reg(param.vm_location)))
		return prl_err(ret, "Failed to create the virtual machine.");
	prl_log(0, "The Container has been successfully created.");
	return 0;
}

int PrlSrv::create_vm(const CmdParamData &param)
{
	PRL_RESULT ret;
	PrlVm *vm;

	if ((ret = update_vm_list(PVTF_CT|PVTF_VM)))
		return ret;

	if (m_VmList.find(param.id))
		return prl_err(-1, "The %s VM already exists.",
			param.id.c_str());
	/* in case ostemplate specified just clone VM */
	if (!param.ostemplate.empty()) {
		prl_log(0, "Creating the VM on the basis of the %s template...",
			param.ostemplate.c_str());
		if (!(vm = m_VmList.find(param.ostemplate)) ||
		    !vm->is_template())
			return prl_err(1, "Failed to find the %s template.",
				param.ostemplate.c_str());
		return vm->clone(param.id, param.uuid, param.vm_location, param.clone_flags);
	}
	prl_log(0, "Creating the virtual machine...");

	PrlHandle hVm;
	if ((ret = PrlSrv_CreateVm(m_hSrv, hVm.get_ptr())))
		return prl_err(ret, "Failed to create the VM: %s",
				get_error_str(ret).c_str());

	vm = m_VmList.add(new PrlVm(*this, hVm.release_handle(), param.uuid, param.id, 0));
	vm->set_state(VMS_STOPPED);
	if (param.dist) {
		if ((ret = vm->load_def_configuration(param.dist)))
			return ret;
	} else if (!param.config_sample.empty()) {
		if ((ret = vm->load_configuration(param.config_sample)))
			return ret;
	} else {
		if ((ret = vm->load_def_configuration(get_def_dist("win"))))
			return ret;

	}
	if ((ret = PrlVmCfg_SetName(vm->get_handle(), param.id.c_str())))
		return prl_err(ret, "PrlVmCfg_SetName returned the following error: %s",
				get_error_str(ret).c_str());

	if (param.nohdd)
		clear_device(*vm, DEV_HDD);

	if (param.hdd_block_size) {
		vm->get_dev_info();
		PrlDevList list = vm->get_devs();
		for (PrlDevList::const_iterator dev = list.begin(), end = list.end(); dev != end; ++dev) {
			if ((*dev)->get_type() != DEV_HDD)
				continue;
			if ((ret = PrlVmDevHd_SetBlockSize((*dev)->m_hDev, param.hdd_block_size)))
				return prl_err(ret, "PrlVmDevHd_SetBlockSize: %s",
						get_error_str(ret).c_str());
		}
	}

	PRL_UINT32 nFlags = 0;
	if (param.lion_recovery)
		nFlags |= PRNVM_CREATE_FROM_LION_RECOVERY_PARTITION;

	if (!param.uuid.empty())
		PrlVmCfg_SetUuid(vm->get_handle(), param.uuid.c_str());

	if ((ret = vm->reg(param.vm_location, nFlags)))
		return prl_err(ret, "Failed to create the virtual machine.");
	prl_log(0, "The VM has been successfully created.");
	return 0;
}

int PrlSrv::register_vm(const CmdParamData &param)
{
	PRL_RESULT ret;

	prl_log(0, "Register the virtual environment...");

	std::string err;
	PRL_UINT32 flags = PACF_NON_INTERACTIVE_MODE;
	if (!param.preserve_src_uuid)
		flags |= PRVF_REGENERATE_SRC_VM_UUID;
	if (param.force)
		flags |= PRCF_FORCE;
	if (param.ignore_ha_cluster)
		flags |= PRVF_IGNORE_HA_CLUSTER;

	if (param.uuid.empty()) {
		if (!param.preserve_uuid)
			flags |= PRVF_REGENERATE_VM_UUID;

		PrlHandle hJob(PrlSrv_RegisterVmEx(m_hSrv,
				param.vm_location.c_str(),
				flags));

		ret = get_job_retcode(hJob, err);
	} else {
		PrlHandle hJob(PrlSrv_RegisterVmWithUuid(m_hSrv,
				param.vm_location.c_str(),
				param.uuid.c_str(),
				flags));

		ret = get_job_retcode(hJob, err);
	}

	if (ret)
		return prl_err(ret, "Failed to register the virtual environment: %s",
				err.c_str());
	prl_log(0, "The virtual environment has been successfully registered.");

	return 0;
}

int PrlSrv::shutdown(bool force, bool suspend_vm_to_pram)
{
	PRL_RESULT ret;
	std::string err;
	PRL_UINT32 flags = (force ? PSHF_FORCE_SHUTDOWN : 0) |
			 (suspend_vm_to_pram ? PSHF_SUSPEND_VM_TO_PRAM : 0);

	prl_log(0, "Shut down the server...");

	reg_event_callback(server_event_handler, NULL);
	PrlHandle hJob(PrlSrv_ShutdownEx(m_hSrv, flags));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "Failed to shut down the server: %s", err.c_str());
	prl_log(0, "The server has been successfully shut down.");
	unreg_event_callback(server_event_handler);

	return 0;
}

int PrlSrv::get_hw_info(ResType type)
{
	PRL_RESULT ret = 0;
	PRL_UINT32 count = 0;

	switch (type) {
	case RES_HW_CPUS:
		if ((ret = PrlSrvCfg_GetCpuCount(get_srv_config_handle(), &count)))
			return prl_err(ret, "PrlSrvCfg_GetCpuCount: %s",
				get_error_str(ret).c_str());
		m_cpus = count;
		break;
	default:
		return prl_err(1, "PrlSrv::get_hw_info :"
			"unhandled device type: %d", type);
	}
	return ret;
}

PRL_HANDLE PrlSrv::get_srv_config_handle()
{
	PRL_RESULT ret;
	PRL_UINT32 resultCount = 0;
	PrlHandle hResult;

	if (!m_hSrvConf.valid()) {
		PrlHandle hJob(PrlSrv_GetSrvConfig(m_hSrv));
		if (!(ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &resultCount))) {
			if ((ret = PrlResult_GetParam(hResult.get_handle(), m_hSrvConf.get_ptr())))
				prl_err(ret, "GetSrvConfig PrlResult_GetParam: %s",
						get_error_str(ret).c_str());
		} else {
			prl_err(ret, "Failed to get the server configuration: %s",
					get_error_str(ret).c_str());
		}
	}

	return m_hSrvConf.get_handle();
}

int PrlSrv::get_hw_dev_info(DevType type)
{
	PRL_RESULT ret;
	PRL_UINT32 count;
	PrlSrvCfg_GetHardDisksCount_Ptr get_count_fn;
	PrlSrvCfg_GetHardDisk_Ptr get_handle_fn;

	switch (type) {
	case DEV_HDD:
		get_count_fn = PrlSrvCfg_GetHardDisksCount;
		get_handle_fn = PrlSrvCfg_GetHardDisk;
		break;
	case DEV_CDROM:
		get_count_fn = PrlSrvCfg_GetOpticalDisksCount;
		get_handle_fn = PrlSrvCfg_GetOpticalDisk;
		break;
	case DEV_FDD:
		get_count_fn = PrlSrvCfg_GetFloppyDisksCount;
		get_handle_fn = PrlSrvCfg_GetFloppyDisk;
		break;
	case DEV_NET:
		get_count_fn = PrlSrvCfg_GetNetAdaptersCount;
		get_handle_fn = PrlSrvCfg_GetNetAdapter;
		break;
	case DEV_USB:
		get_count_fn = PrlSrvCfg_GetUsbDevsCount;
		get_handle_fn = PrlSrvCfg_GetUsbDev;
		break;
	case DEV_SERIAL:
		get_count_fn = PrlSrvCfg_GetSerialPortsCount;
		get_handle_fn = PrlSrvCfg_GetSerialPort;
		break;
	case DEV_PARALLEL:
		get_count_fn = PrlSrvCfg_GetParallelPortsCount;
		get_handle_fn = PrlSrvCfg_GetParallelPort;
		break;
	case DEV_SOUND:
		get_count_fn = PrlSrvCfg_GetSoundOutputDevsCount;
		get_handle_fn = PrlSrvCfg_GetSoundOutputDev;
		break;
	case DEV_GENERIC_PCI:
		get_count_fn = PrlSrvCfg_GetGenericPciDevicesCount;
		get_handle_fn = PrlSrvCfg_GetGenericPciDevice;
		break;
	default:
		return prl_err(1, "PrlSrv::get_hw_dev_info :"
			"unhandled device type: %d", type);
	}

	if ((ret = get_count_fn(get_srv_config_handle(), &count)))
		return prl_err(ret, "Failed to get the number of devices on the server: %s",
				get_error_str(ret).c_str());
	for (unsigned int i = 0; i < count; ++i) {
		PrlHandle hDev;

		if ((ret = get_handle_fn(get_srv_config_handle(), i, hDev.get_ptr())))
			return prl_err(ret, "Failed to get the server device"
				" handle: %s", get_error_str(ret).c_str());
		PrlDevSrv *dev = m_DevList.add(new PrlDevSrv(*this, hDev.release_handle(), type));

		char buf[1024];
		unsigned int len = sizeof(buf);
		if (type == DEV_HDD) {
			unsigned int count = 0;

			if (!PrlSrvCfgHdd_GetDevName(dev->m_hDev, buf, &len))
				prl_log(L_INFO, "add hdd device: %s", buf);
			PrlSrvCfgHdd_GetPartsCount(dev->m_hDev, &count);
			for (unsigned int i = 0; i < count; ++i) {
				PrlHandle h;
				ret = PrlSrvCfgHdd_GetPart(dev->m_hDev, i, h.get_ptr());
				if (ret == 0 &&
				    !PrlSrvCfgHddPart_GetSysName(h.get_handle(), buf, &len))
				{
					PrlDevSrv *child;
					child = m_DevList.add(new PrlDevSrv(*this, h.release_handle(), DEV_HDD_PARTITION));
					child->set_parent(dev);
					prl_log(L_INFO, "add partition: %s", buf);
				}
			}
		} else if (type == DEV_NET) {
			if (!(ret = PrlSrvCfgDev_GetName(dev->m_hDev,
							buf, &len))) {
				prl_log(L_INFO, "Add the device: %s", buf);
				dev->m_name = buf;
			}
			len = sizeof(buf);
			if (!(ret = PrlSrvCfgNet_GetMacAddress(dev->m_hDev,
							buf, &len)))
				dev->m_mac = buf;
			dev->m_vlanTag = 0;
			ret = PrlSrvCfgNet_GetVlanTag(dev->m_hDev, &dev->m_vlanTag);
			if (PRL_FAILED(ret))
				dev->m_vlanTag = 0;
		} else {
			if (!(ret = PrlSrvCfgDev_GetName(dev->m_hDev, buf, &len)))
				prl_log(L_INFO, "add device: %s", buf);
		}
	}
	return 0;
}


int PrlSrv::get_hw_dev_info()
{
	get_hw_dev_info(DEV_HDD);
	get_hw_dev_info(DEV_CDROM);
	get_hw_dev_info(DEV_FDD);
	get_hw_dev_info(DEV_NET);
	get_hw_dev_info(DEV_USB);
	get_hw_dev_info(DEV_SERIAL);
	get_hw_dev_info(DEV_PARALLEL);
	get_hw_dev_info(DEV_GENERIC_PCI);
	return 0;
}

int PrlSrv::get_user_info(PrlList<PrlUser *> &users)
{
	PRL_RESULT ret;
	PRL_UINT32 resultCount = 0;
	PrlHandle hResult;

	PrlHandle hJob(PrlSrv_GetUserInfoList(m_hSrv));
	if ((ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &resultCount)))
		return prl_err(ret, "PrlSrv_GetUserInfoList: %s",
			get_error_str(ret).c_str());

	for (unsigned int i = 0; i < resultCount; i++) {
		PrlHandle hUserInfo;

		if ((ret = PrlResult_GetParamByIndex(hResult.get_handle(), i, hUserInfo.get_ptr())))
			return prl_err(ret, "PrlResult_GetParamByIndex: %s",
					get_error_str(ret).c_str());

		char name[256];
		unsigned int len = sizeof(name);
		if ((ret = PrlUsrInfo_GetName(hUserInfo.get_handle(), name, &len)))
			return prl_err(ret, "PrlUsrInfo_GetName: %s",
				get_error_str(ret).c_str());

		char uuid[64];
		len = sizeof(uuid);
		if ((ret = PrlUsrInfo_GetUuid(hUserInfo.get_handle(), uuid, &len)))
			return prl_err(ret, "PrlUsrInfo_GetUuid: %s",
				get_error_str(ret).c_str());

		char home[4096];
		len = sizeof(home);
		if ((ret = PrlUsrInfo_GetDefaultVmFolder(hUserInfo.get_handle(), home, &len)))
			return prl_err(ret, "PrlUsrInfo_GetDefaultVmFolder: %s",
				get_error_str(ret).c_str());

		unsigned int id;
		PrlUser *user = users.add(new PrlUser(name, uuid, home));
		if (!PrlUsrInfo_GetSessionCount(hUserInfo.get_handle(), &id))
			user->m_session_count = id;

		PRL_BOOL flag;
		if (!PrlUsrInfo_CanChangeSrvSets(hUserInfo.get_handle(), &flag))
			user->m_manage_srv_settings = prl_bool(flag);
	}
	return ret;
}

PrlDevSrv *PrlSrv::find_dev(DevType type, const std::string &name)
{
	if (m_DevList.empty())
		get_hw_dev_info(type);

	PrlDevSrvList::const_iterator it = m_DevList.begin(),
					eit = m_DevList.end();
	for (; it != eit; ++it) {
		if ((*it)->m_devType == type &&
		    ((*it)->get_name() == name ||
		     (*it)->get_id() == name))
		{
			return *it;
		}
	}
	return 0;
}

PrlDevSrv *PrlSrv::find_net_dev_by_mac(const std::string &mac)
{
	if (m_DevList.empty())
		get_hw_dev_info(DEV_NET);

	PrlDevSrvList::const_iterator it = m_DevList.begin();
	for (; it != m_DevList.end(); ++it) {
		if ((*it)->m_devType == DEV_NET &&
		    ((*it)->m_mac == mac))
			return *it;
	}
	return 0;
}

// every PRL adapter has this bit set (defined at PrlNetworkingConstants.h)
#define PRL_ADAPTER_START_INDEX 0x10000000

PrlDevSrv *PrlSrv::find_net_dev_by_idx(unsigned int idx, bool is_virtual)
{
	if (m_DevList.empty())
		get_hw_dev_info(DEV_NET);

	if (is_virtual)
		idx |= PRL_ADAPTER_START_INDEX;
	PrlDevSrvList::const_iterator it = m_DevList.begin();
	for (; it != m_DevList.end(); ++it) {
		if ((*it)->m_devType != DEV_NET)
			continue;
		if ((*it)->get_idx() == idx)
			return *it;
	}
	return 0;
}

int PrlSrv::get_new_dir(const char *dir, const char *pattern,
		std::string &new_dir_name) const
{
	PRL_RESULT ret;
	PRL_UINT32 count = 0;
	PrlHandle hResult;

	PrlHandle hJob(PrlSrv_FsGetDirEntries(m_hSrv, dir));
	if ((ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &count))) {
		if (ret == PRL_ERR_DIRECTORY_DOES_NOT_EXIST)
			return 0;
	}
	if (ret)
		return prl_err(ret, "PrlSrv_FsGetDirEntries: %s",
				get_error_str(ret).c_str());
	/* directory dir does not exist */
	if (count == 0)
		return 0;

	PrlHandle hFsInfo;

	if ((ret = PrlResult_GetParam(hResult.get_handle(), hFsInfo.get_ptr())))
		return prl_err(ret, "FsGetDirEntries PrlResult_GetParam: %s",
				get_error_str(ret).c_str());

	PrlFsInfo_GetChildEntriesCount(hFsInfo.get_handle(), &count);
	std::set<std::string> dir_list;
	unsigned int i;
	for (i = 0; i < count; i++) {
		PrlHandle hFsEntry;

		if ((ret = PrlFsInfo_GetChildEntry(hFsInfo.get_handle(), i, hFsEntry.get_ptr()))) {
			return prl_err(ret, "PrlFsInfo_GetChildEntry"
					" returned the following error: %s",
					get_error_str(ret).c_str());
		}
		char buf[4096];
		unsigned len = sizeof(buf);
		if (!PrlFsEntry_GetRelativeName(hFsEntry.get_handle(), buf, &len))
			dir_list.insert(buf);
	}
	unsigned int id = 1;
	for (i = 0; i < dir_list.size(); i++) {
		char str[256];
		sprintf(str, pattern, id++);
		if (dir_list.find(str) == dir_list.end()) {
			new_dir_name = dir;
			if (*new_dir_name.rbegin() != '/')
				new_dir_name += "/";
			new_dir_name += str;
			break;
		}
	}

	return 0;
}

unsigned int PrlSrv::get_cpu_count()
{
	if (!m_cpus)
		get_hw_info(RES_HW_CPUS);
	return m_cpus;
}

int PrlSrv::convert_handle_to_event(PRL_HANDLE h, PRL_HANDLE* phEvent)
{
	PRL_STR pBuf = 0;
	PRL_RESULT ret = PrlHandle_ToString( h, (PRL_VOID_PTR_PTR )&pBuf);
	if (PRL_FAILED(ret) || !pBuf) {
		prl_log(L_INFO, "PrlHandle_ToString: %s",
				get_error_str(ret).c_str());
		return ret;
	}

	PrlHandle hVm;
	if ((ret = PrlSrv_CreateVm(m_hSrv, hVm.get_ptr()))) {
		prl_log(L_INFO, "PrlSrv_CreateVm: %s",
				get_error_str(ret).c_str());
		PrlBuffer_Free(pBuf);
		return ret;
	}
	if ((ret = PrlVm_CreateAnswerEvent(hVm.get_handle(), phEvent, 0))) {
		prl_log(L_INFO, "PrlVm_CreateAnswerEvent: %s",
				get_error_str(ret).c_str());
		PrlBuffer_Free(pBuf);
		return ret;
	}

	ret = PrlEvent_FromString(*phEvent, pBuf);
	PrlBuffer_Free(pBuf);
	if (ret) {
		prl_log(L_INFO, "PrlEvent_FromString: %s",
				get_error_str(ret).c_str());
		return ret;
	}

	return ret;
}

enum LicenseFields
{
        LIC_GRACE_PERIOD        = (1<<0),
        LIC_CPU_TOTAL           = (1<<1),
        LIC_MAX_MEMORY          = (1<<2),
        LIC_VTD_AVAILABLE       = (1<<3),
        LIC_VMS_TOTAL           = (1<<4),
        LIC_RESTRICTIONS        = (1<<5),
        LIC_VZCC_USERS          = (1<<6),
        LIC_IS_VOLUME           = (1<<7),
        LIC_IS_CONFIRMED        = (1<<8),
        LIC_HWID                = (1<<9),
        LIC_USER                = (1<<10),
        LIC_COMPANY             = (1<<11),
        LIC_RKU_ALLOWED         = (1<<12),
        LIC_HA_ALLOWED          = (1<<13),
};

PrlLic & PrlSrv::get_verbose_lic_info(PrlLic &lic, PrlHandle &hLicInfo)
{
	/* we do not want to provide public API for license values decoding,
	 * thus use this hacky way to retrieve them */
#if 0
	PrlHandle hEvent;
	if (convert_handle_to_event(hLicInfo.get_handle(), hEvent.get_ptr()))
		return lic;

	PRL_UINT32 i, nCount;
	PRL_BOOL is_unlimited;
	PrlEvent_GetParamsCount(hEvent.get_handle(), &nCount);
	for (i = 0; i < nCount; i++) {
		PrlHandle hParam;
		PrlEvent_GetParam(hEvent.get_handle(), i, hParam.get_ptr());
		char buf[2048];
		PRL_UINT32 len = sizeof(buf);
		PrlEvtPrm_GetName(hParam.get_handle(), buf, &len);
		if (!strncmp(buf, EVT_PARAM_PRL_VZLICENSE_GRACE_PERIOD,
			sizeof(buf))) {
			PrlEvtPrm_ToUint32(hParam.get_handle(),
					&lic.m_grace_period);
			lic.set_fields |= LIC_GRACE_PERIOD;
		} else if (!strncmp(buf,
			EVT_PARAM_PRL_VZLICENSE_IS_EXPIRATION_UNLIM,
			sizeof(buf))) {
			PrlEvtPrm_ToBoolean(hParam.get_handle(),
					&is_unlimited);
			if (is_unlimited)
				lic.m_expiration_date = "unlimited";
		} else if (!strncmp(buf, EVT_PARAM_PRL_VZLICENSE_EXPIRATION_DATE,
			sizeof(buf))) {
			len = sizeof(buf);
			PrlEvtPrm_ToString(hParam.get_handle(), buf, &len);
			if (!is_unlimited)
				lic.m_expiration_date = convert_time(buf);
		} else if (!strncmp(buf, EVT_PARAM_PRL_VZLICENSE_START_DATE,
			sizeof(buf))) {
			len = sizeof(buf);
			PrlEvtPrm_ToString(hParam.get_handle(), buf, &len);
			lic.m_start_date = convert_time(buf);
		} else if (!strncmp(buf, EVT_PARAM_PRL_VZLICENSE_UPDATE_DATE,
			sizeof(buf))) {
			len = sizeof(buf);
			PrlEvtPrm_ToString(hParam.get_handle(), buf, &len);
			lic.m_update_date = convert_time(buf);
		} else if (!strncmp(buf, EVT_PARAM_PRL_VZLICENSE_CPU_TOTAL,
			sizeof(buf))) {
			PrlEvtPrm_ToUint32(hParam.get_handle(),
					&lic.m_cpu_total);
			lic.set_fields |= LIC_CPU_TOTAL;
		} else if (!strncmp(buf, EVT_PARAM_PRL_VZLICENSE_MAX_MEMORY,
			sizeof(buf))) {
			PrlEvtPrm_ToUint32(hParam.get_handle(),
					&lic.m_max_memory);
			lic.set_fields |= LIC_MAX_MEMORY;
		} else if (!strncmp(buf, EVT_PARAM_PRL_VZLICENSE_VTD_AVAILABLE, sizeof(buf))){
			PRL_BOOL vtd = PRL_FALSE;
			PrlEvtPrm_ToBoolean(hParam.get_handle(), &vtd);
			lic.m_vtd_available = (vtd == PRL_TRUE);
			lic.set_fields |= LIC_VTD_AVAILABLE;
		} else if (!strncmp(buf, EVT_PARAM_PRL_VZLICENSE_PRODUCT,
			sizeof(buf))) {
			len = sizeof(buf);
			PrlEvtPrm_ToString(hParam.get_handle(), buf, &len);
			lic.m_product = buf;
		} else if (!strncmp(buf, EVT_PARAM_PRL_VZLICENSE_VMS_TOTAL,
			sizeof(buf))) {
			PrlEvtPrm_ToUint32(hParam.get_handle(),
					&lic.m_vms_total);
			lic.set_fields |= LIC_VMS_TOTAL;
		} else if (!strncmp(buf, EVT_PARAM_PRL_VZLICENSE_KEY_NUMBER,
			sizeof(buf))) {
			len = sizeof(buf);
			PrlEvtPrm_ToString(hParam.get_handle(), buf, &len);
			lic.m_key_number = buf;
		} else if (!strncmp(buf, EVT_PARAM_PRL_VZLICENSE_HWID,
			sizeof(buf))) {
			len = sizeof(buf);
			PrlEvtPrm_ToString(hParam.get_handle(), buf, &len);
			lic.m_hwid = buf;
			lic.set_fields |= LIC_HWID;
		} else if (!strncmp(buf, EVT_PARAM_PRL_LICENSE_HAS_RESTRICTIONS,
			sizeof(buf))) {
			PRL_BOOL bTmp;
			PrlEvtPrm_ToBoolean(hParam.get_handle(),
					&bTmp);
			lic.m_has_restrictions = !!bTmp;
			lic.set_fields |= LIC_RESTRICTIONS;
		} else if (!strncmp(buf, EVT_PARAM_PRL_VZLICENSE_VZCC_USERS,
			sizeof(buf))) {
			PrlEvtPrm_ToUint64(hParam.get_handle(),
					&lic.m_vzcc_users);
			lic.set_fields |= LIC_VZCC_USERS;
		} else if (!strncmp(buf, EVT_PARAM_PRL_VZLICENSE_IS_VOLUME, sizeof(buf))){
			PRL_BOOL val = PRL_FALSE;
			PrlEvtPrm_ToBoolean(hParam.get_handle(), &val);
			lic.m_is_volume = (val == PRL_TRUE);
			lic.set_fields |= LIC_IS_VOLUME;
		} else if (!strncmp(buf, EVT_PARAM_PRL_LICENSE_OFFLINE_EXPIRATION_DATE,
			sizeof(buf))) {
			len = sizeof(buf);
			PrlEvtPrm_ToString(hParam.get_handle(), buf, &len);
			lic.m_offline_expiration_date = convert_time(buf);
		} else if (!strncmp(buf, EVT_PARAM_PRL_LICENSE_ACTIVATION_ID,
			sizeof(buf))) {
			len = sizeof(buf);
			PrlEvtPrm_ToString(hParam.get_handle(), buf, &len);
			lic.m_activation_id = buf;
		} else if (!strncmp(buf, EVT_PARAM_PRL_LICENSE_IS_CONFIRMED ,sizeof(buf))) {
			PRL_BOOL bTmp;
			PrlEvtPrm_ToBoolean(hParam.get_handle(), &bTmp);
			lic.m_is_confirmed = !!bTmp;
			lic.set_fields |= LIC_IS_CONFIRMED;
		} else if (!strncmp(buf, EVT_PARAM_PRL_LICENSE_USER,
			sizeof(buf))) {
			len = sizeof(buf);
			PrlEvtPrm_ToString(hParam.get_handle(), buf, &len);
			lic.m_name = buf;
			lic.set_fields |= LIC_USER;
		} else if (!strncmp(buf, EVT_PARAM_PRL_LICENSE_COMPANY,
			sizeof(buf))) {
			len = sizeof(buf);
			PrlEvtPrm_ToString(hParam.get_handle(), buf, &len);
			lic.m_company = buf;
			lic.set_fields |= LIC_COMPANY;
		} else if (!strncmp(buf, EVT_PARAM_PRL_VZLICENSE_RKU_ALLOWED,
			sizeof(buf))) {
			PRL_BOOL val = PRL_FALSE;
			PrlEvtPrm_ToBoolean(hParam.get_handle(), &val);
			lic.m_rku_allowed = (val == PRL_TRUE);
			lic.set_fields |= LIC_RKU_ALLOWED;
		} else if (!strncmp(buf, EVT_PARAM_PRL_VZLICENSE_HA_ALLOWED,
			sizeof(buf))) {
			PRL_BOOL val = PRL_FALSE;
			PrlEvtPrm_ToBoolean(hParam.get_handle(), &val);
			lic.m_ha_allowed = (val == PRL_TRUE);
			lic.set_fields |= LIC_HA_ALLOWED;
		} else if (!strncmp(buf, EVT_PARAM_PRL_LICENSE_DEFERRED_ACTIVATION, sizeof(buf)))
		{
			PRL_BOOL bTmp;
			PrlEvtPrm_ToBoolean(hParam.get_handle(), &bTmp);
			lic.m_deferred_allowed = !!bTmp;
		} else if (!strncmp(buf, EVT_PARAM_PRL_LICENSE_DEACTIVATION_ID,
			sizeof(buf))) {
			len = sizeof(buf);
			PrlEvtPrm_ToString(hParam.get_handle(), buf, &len);
			lic.m_deactivation_id = buf;
}
	}
#endif
	return lic;
}

PrlLic PrlSrv::get_lic_info()
{
	PRL_RESULT ret;
	PRL_UINT32 count = 0;
	PrlHandle hResult;
	PrlLic lic;

	PrlHandle hJob(PrlSrv_GetLicenseInfo(m_hSrv));
	if ((ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &count))) {
		prl_err(ret, "PrlSrv_GetLicenseInfo: %s",
				get_error_str(ret).c_str());
		return lic;
	}
	PrlHandle hLicInfo;

	if ((ret = PrlResult_GetParam(hResult.get_handle(), hLicInfo.get_ptr()))) {
		prl_err(ret, "PrlResult_GetParam: %s",
				get_error_str(ret).c_str());
		return lic;
	}

	if ((ret = PrlLic_GetStatus(hLicInfo.get_handle(), &lic.m_state))) {
		prl_err(ret, "PrlLic_GetStatus: %s",
			get_error_str(ret).c_str());
		return lic;
	}
	char buf[1024];
	PRL_UINT32 len;

	len = sizeof(buf);
	if ((ret = PrlLic_GetLicenseKey(hLicInfo.get_handle(), buf, &len)))
		prl_log(L_INFO, "PrlLic_GetLicenseKey: %s",
				get_error_str(ret).c_str());
	else
		lic.m_key = buf;

	return get_verbose_lic_info(lic, hLicInfo);
}

void PrlSrv::append_lic_info(PrlOutFormatter &f)
{
	get_lic_info().append_info(f);
}

void PrlSrv::append_lic_verbose_info(PrlOutFormatter &f)
{
	std::stringstream ss;

	if (f.type == OUT_FORMATTER_PLAIN)
		printf("Searching for installed licenses...\n");

	PrlLic lic = get_lic_info();
	if (lic.m_key.empty()) {
		if (f.type == OUT_FORMATTER_PLAIN){
			printf("No licenses installed.\n");
			if( lic.m_deferred_allowed )
				printf("Deferred activation allowed\n");
		}
		return;
	}

	std::string status;
	if (lic.m_state == PRL_ERR_LICENSE_VALID)
		status = "ACTIVE";
	else if ( PRL_LICENSE_IS_EXPIRED( lic.m_state ) )
		status = "EXPIRED";
	else if (lic.m_state == PRL_ERR_LICENSE_GRACED)
		status = "GRACED";
	else
		status = "INVALID";

	f.lic_add("status", status);
	f.lic_add("serial", lic.m_key);
	if (lic.set_fields & LIC_HWID)
		f.lic_add("hardware_id", lic.m_hwid);
	if (lic.set_fields & LIC_USER)
		f.lic_add("user", lic.m_name);
	if (lic.set_fields & LIC_COMPANY)
		f.lic_add("organization", lic.m_company);

	if (!lic.m_offline_expiration_date.empty())
		f.lic_add("unconfirmed_expiration_date", lic.m_offline_expiration_date);
	if (!lic.m_expiration_date.empty())
		f.lic_add("expiration", lic.m_expiration_date);
	if (!lic.m_start_date.empty())
		f.lic_add("start_date", lic.m_start_date);
	f.lic_add("graceperiod", lic.m_grace_period);
	if (!lic.m_update_date.empty())
		f.lic_add("license_update_date", lic.m_update_date);
	if (!lic.m_key_number.empty())
		f.lic_add("key_number", lic.m_key_number);
	if (lic.set_fields & LIC_CPU_TOTAL)
		f.lic_add("cpu_total", lic.m_cpu_total);
	if (lic.set_fields & LIC_MAX_MEMORY)
		f.lic_add("max_memory", lic.m_max_memory);
	if (lic.set_fields & LIC_VTD_AVAILABLE)
		f.lic_add("vtd_available",
			  lic.m_vtd_available ? "enabled" : "disabled");
	if (lic.set_fields & LIC_IS_CONFIRMED)
		f.lic_add("is_confirmed",
				lic.m_is_confirmed ? "yes" : "no" );

	/* numbers in JSON are doubles, so 52 bits available for
	 * mantissa, not enough to store long long */
	if (lic.set_fields & LIC_VZCC_USERS)
		f.lic_add("max_vzcc_users", (int)lic.m_vzcc_users);

	if (!lic.m_product.empty())
		f.lic_add("product", lic.m_product);
	if (lic.set_fields & LIC_VMS_TOTAL)
		f.lic_add("nr_vms", lic.m_vms_total);
	if (lic.set_fields & LIC_IS_VOLUME)
		f.lic_add( "is_volume", lic.m_is_volume ? "yes" : "no" );
	if (lic.set_fields & LIC_RESTRICTIONS)
		f.lic_add("advanced_restrictions",
			lic.m_has_restrictions ? "enabled" : "disabled");

	f.lic_add( "deferred_activation",
			lic.m_deferred_allowed?"enabled":"disabled");

	/* PSBM 6 introduced fields */
	if (lic.set_fields & LIC_RKU_ALLOWED)
		f.lic_add("rku_allowed", (int)lic.m_rku_allowed);
	if (lic.set_fields & LIC_HA_ALLOWED)
		f.lic_add("ha_allowed", (int)lic.m_ha_allowed);

}

void PrlSrv::append_activation_id(PrlOutFormatter &f)
{
	if (f.type == OUT_FORMATTER_PLAIN)
		printf("Asking for activation-id...\n");

	PrlLic lic = get_lic_info();
	if (lic.m_key.empty()) {
		if (f.type == OUT_FORMATTER_PLAIN)
			printf("No licenses installed.\n");
		return;
	}

	if (lic.m_activation_id.empty()) {
		if (f.type == OUT_FORMATTER_PLAIN)
			printf("Unable to get activation id.\n");
		return;
	}
	f.lic_add("activation_id", lic.m_activation_id);
}

void PrlSrv::append_deactivation_id(PrlOutFormatter &f)
{
	if (f.type == OUT_FORMATTER_PLAIN)
		printf("Asking for deactivation-id...\n");

	PrlLic lic = get_lic_info();

	if (lic.m_deactivation_id.empty()) {
		if (f.type == OUT_FORMATTER_PLAIN)
			printf("Unable to get deactivation id.\n");
		return;
	}
	f.lic_add("deactivation_id", lic.m_deactivation_id);
}

const PrlVm *PrlSrv::find_dev_in_use(PrlDevSrv *dev)
{
	if (m_VmList.empty()) {
		update_vm_list(PVTF_CT|PVTF_VM);
		/* fill devices info only for running VM */
		for (PrlVmList::iterator vm = m_VmList.begin(), end = m_VmList.end(); vm != end; ++vm) {
			(*vm)->update_state();
			VIRTUAL_MACHINE_STATE state = (*vm)->get_state();
			if (state == VMS_RUNNING || state == VMS_PAUSED || state == VMS_SUSPENDED || state == VMS_SUSPENDING_SYNC)
				(*vm)->get_dev_info();
		}
	}

	for (PrlVmList::iterator vm = m_VmList.begin(), end = m_VmList.end(); vm != end; ++vm) {
		if ((*vm)->find_dev(dev->get_id()))
			return *vm;
	}
	return NULL;
}

void PrlSrv::append_hw_info(PrlOutFormatter &f)
{
	int mode;
	const char *assignment = NULL;
	const char *used_by = NULL;

	get_hw_dev_info();
	f.open("Hardware info");
	PrlDevSrvList::const_iterator it = m_DevList.begin(),
		eit = m_DevList.end();
	for (; it != eit; ++it) {
		if ((*it)-> m_devType == DEV_GENERIC_PCI) {
			if ((mode = (*it)->get_assignment_mode()) != -1) {
				switch (mode) {
				case PGS_CONNECTED_TO_HOST:
					assignment = "host";
					break;
				case PGS_CONNECTED_TO_VM:
				case PGS_CONNECTING_TO_VM: {
					assignment = "vm";

					const PrlVm *vm = find_dev_in_use(*it);
					if (vm)
						used_by = vm->get_uuid().c_str();
					break;
				} case PGS_RESERVED:
					break;
				}
			}
		}

		f.add_host_dev((*it)->get_id().c_str(), (*it)->get_name().c_str(),
					   devtype2str((*it)->m_devType), assignment, used_by);
	}
	f.close();
}

int PrlSrv::get_srv_info()
{
	char buf[4096];
	unsigned int len;
	PRL_RESULT ret;
	PrlHandle hSrvInfo;

	if ((ret = PrlSrv_GetServerInfo(m_hSrv, hSrvInfo.get_ptr())))
		return prl_err(ret, "PrlSrv_GetServerInfo: %s",
			get_error_str(ret).c_str());
	len = sizeof(buf);
	if (!PrlSrvInfo_GetServerUuid(hSrvInfo.get_handle(), buf, &len))
		m_uuid = buf;

	len = sizeof(buf);
	if (!PrlSrvInfo_GetProductVersion(hSrvInfo.get_handle(), buf, &len))
		m_product_version = buf;

	len = sizeof(buf);
	if (!PrlSrvInfo_GetHostName(hSrvInfo.get_handle(), buf, &len))
		m_hostname = buf;

	return 0;
}

void PrlSrv::append_info(PrlOutFormatter &f)
{
	get_srv_info();

	f.add_uuid("ID", get_uuid());
	f.add("Hostname", m_hostname);

	std::string product;
	PRL_APPLICATION_MODE mode;
	if (!PrlApi_GetAppMode(&mode)) {
		switch(mode)
		{
		case PAM_SERVER:
			product = "Server";
			break;
		default:
			product = "Server";
		}//switch
	}//if

	f.add("Version", product + " " + m_product_version);
#if defined(_DEBUG)
	if (m_run_via_launchd != -1)
		f.add("Started as service", m_run_via_launchd ? "on" : "off");
#endif

	m_disp->append_info(f);

	return;
}

int PrlSrv::print_info(bool is_license_info
	, bool is_activation_id
	, bool is_deactivation_id
	, bool use_json)
{
	PrlOutFormatter &f = *(get_formatter(use_json));

	f.open_object();

	if (is_license_info)
		append_lic_verbose_info(f);
	else if (is_activation_id)
		append_activation_id(f);
	else if (is_deactivation_id)
		append_deactivation_id(f);
	else {
		append_info(f);
		append_lic_info(f);
		append_hw_info(f);
	}

	f.close_object();
	std::string out = f.get_buffer();
	printf("%s", out.c_str());
	if (use_json)
		printf("\n");
	delete (&f);
	return 0;
}

int PrlSrv::install_license(const std::string &key, const std::string &name,
		const std::string &company, bool deferred_mode)
{
	PRL_RESULT ret;

	if (key.empty())
		return prl_err(-1, "The license key is not specified.");

	std::string err;

	PRL_UINT32 nFlags=0;
	if( deferred_mode )
		nFlags |= PUPLF_STORE_PRECACHED_KEY;

	PrlHandle hJob(PrlSrv_UpdateLicenseEx(m_hSrv, key.c_str(), name.c_str(), company.c_str(), nFlags));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "Failed to install the license: %s",
				err.c_str());
	const char* success_msg = (!deferred_mode)
		? "The license has been successfully installed."
		: "The license has been successfully prepared for deferred installation.";
	prl_log(0, success_msg );
	return 0;
}

int PrlSrv::deferred_license_op(const CmdParamData &param )
{
	PRL_RESULT ret;

	if( param.deferred_install && param.deferred_remove)
		return prl_err(-1, "Wrong parameters.");

	std::string err;

	PRL_UINT32 nFlags=0;
	if( param.deferred_install )
		nFlags |= PUPLF_ACTIVATE_PRECACHED_KEY;
	else if( param.deferred_remove )
		nFlags |= PUPLF_REMOVE_PRECACHED_KEY;
	else
		return prl_err(-1, "Wrong parameters.");

	PrlHandle hJob(PrlSrv_UpdateLicenseEx(m_hSrv, "", "", "", nFlags));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "The license operation failed: %s",
				err.c_str());
	const char* success_msg = (param.deferred_install)
		? "The deferred license has been successfully installed."
		: "The deferred license has been successfully removed.";
	prl_log(0, success_msg );
	return 0;
}

static int server_event_handler(PRL_HANDLE hEvent, void *data);

int PrlSrv::update_license()
{
	PRL_RESULT ret;

	std::string err;

	PrlHandle hJob(PrlSrv_UpdateLicenseEx(m_hSrv, "", "", "", PUPLF_KICK_TO_UDPATE_CURR_FILE_LICENSE));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "Failed to update the license: %s",
		err.c_str());
	prl_log(0, "The license has been successfully updated.");
	return 0;
}

int PrlSrv::get_default_vm_location(std::string &location)
{
	int ret;

	if ((ret = m_disp->update_info()))
		return ret;
	location = m_disp->m_home;
	return 0;
}

void PrlSrv::clear()
{
	m_VmList.del();
	m_DevList.del();
	m_VNetList.del();
	if (m_disp)
		delete m_disp;
	m_disp = 0;
	if (m_hSrv)
		PrlHandle_Free(m_hSrv);
	m_hSrv = 0;;
}

PrlSrv::~PrlSrv()
{
	logoff();
}


int PrlSrv::fill_vnetworks_list(PrlVNetList &list) const
{
	list.del();

	std::string err;
	PRL_RESULT ret;
	PrlHandle hJob(PrlSrv_GetVirtualNetworkList(m_hSrv, 0));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "Failed to retrieve the list of Virtual"
			" Networks: %s", err.c_str());
	PrlHandle hResult;
	if ((ret = PrlJob_GetResult(hJob.get_handle(), hResult.get_ptr()))) {
		return prl_err(ret, "PrlJob_GetResult: %s",
				get_error_str(ret).c_str());
	}

	PRL_UINT32 nCount = 0;
	if ((ret = PrlResult_GetParamsCount(hResult.get_handle(), &nCount))) {
		return prl_err(ret, "PrlResult_GetParamsCount: %s",
				get_error_str(ret).c_str());
	}

	for(PRL_UINT32 i = 0; i < nCount; ++i)
	{
		PrlHandle *phTmp = new PrlHandle;
		if ((ret = PrlResult_GetParamByIndex(hResult.get_handle(), i,
						phTmp->get_ptr()))) {
			prl_log(L_ERR, "PrlResult_GetParamsByIndex [%u]: %s",
				i, get_error_str(ret).c_str());
			delete phTmp;
			continue;
		}
		list.add(phTmp);
	}

	return 0;
}

int PrlSrv::fill_vnetwork_handle(const VNetParam &vnet, PrlHandle &hVirtNet)
{
	PRL_RESULT ret;

	if (!hVirtNet.valid()) {
		ret = PrlVirtNet_Create(hVirtNet.get_ptr());
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_Create failed:"
				" %s", get_error_str(ret).c_str());
	}
	if (!vnet.vnet.empty()) {
		ret = PrlVirtNet_SetNetworkId(hVirtNet.get_handle(),
				vnet.vnet.c_str());
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetNetworkId"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (!vnet.description.empty()) {
		ret = PrlVirtNet_SetDescription(hVirtNet.get_handle(),
				vnet.description.c_str());
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetDescription"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (!vnet.host_ip.ip.empty()) {
		ret = PrlVirtNet_SetHostIPAddress(hVirtNet.get_handle(),
				vnet.host_ip.ip.c_str());
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetHostIPAddress"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (!vnet.host_ip.mask.empty()) {
		ret = PrlVirtNet_SetIPNetMask(hVirtNet.get_handle(),
				vnet.host_ip.mask.c_str());
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetIPNetMask"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (!vnet.host_ip6.ip.empty()) {
		ret = PrlVirtNet_SetHostIP6Address(hVirtNet.get_handle(),
				vnet.host_ip6.ip.c_str());
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetHostIP6Address"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (!vnet.host_ip6.ip6_mask.empty()) {
		ret = PrlVirtNet_SetIP6NetMask(hVirtNet.get_handle(),
				vnet.host_ip6.ip6_mask.c_str());
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetIP6NetMask"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (vnet.dhcp_enabled != -1) {
		ret = PrlVirtNet_SetDHCPServerEnabled(hVirtNet.get_handle(),
				vnet.dhcp_enabled ? PRL_TRUE : PRL_FALSE);
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetDHCPServerEnabled"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (vnet.dhcp6_enabled != -1) {
		ret = PrlVirtNet_SetDHCP6ServerEnabled(hVirtNet.get_handle(),
				vnet.dhcp6_enabled ? PRL_TRUE : PRL_FALSE);
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetDHCP6ServerEnabled"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (!vnet.dhcp_ip.empty()) {
		ret = PrlVirtNet_SetDhcpIPAddress(hVirtNet.get_handle(),
				vnet.dhcp_ip.c_str());
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetDhcpIPAddress"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (!vnet.dhcp_ip6.empty()) {
		ret = PrlVirtNet_SetDhcpIP6Address(hVirtNet.get_handle(),
				vnet.dhcp_ip6.c_str());
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetDhcpIP6Address"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (!vnet.ip_scope_start.empty()) {
		ret = PrlVirtNet_SetIPScopeStart(hVirtNet.get_handle(),
				vnet.ip_scope_start.c_str());
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetIPScopeStart"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (!vnet.ip_scope_end.empty()) {
		ret = PrlVirtNet_SetIPScopeEnd(hVirtNet.get_handle(),
				vnet.ip_scope_end.c_str());
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetIPScopeEnd"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (!vnet.ip6_scope_start.empty()) {
		ret = PrlVirtNet_SetIP6ScopeStart(hVirtNet.get_handle(),
				vnet.ip6_scope_start.c_str());
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetIP6ScopeStart"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (!vnet.ip6_scope_end.empty()) {
		ret = PrlVirtNet_SetIP6ScopeEnd(hVirtNet.get_handle(),
				vnet.ip6_scope_end.c_str());
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetIP6ScopeEnd"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (vnet.type != -1) {
		/* type is specified */
		ret = PrlVirtNet_SetNetworkType(hVirtNet.get_handle(),
				(PRL_NET_VIRTUAL_NETWORK_TYPE)vnet.type);
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetNetworkType"
				" failed: %s", get_error_str(ret).c_str());
		ret = PrlVirtNet_SetNATServerEnabled(hVirtNet.get_handle(),
				(PRL_BOOL)vnet.is_shared);
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetNATServerEnabled"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (!vnet.mac.empty()) {
		ret = PrlVirtNet_SetBoundCardMac(hVirtNet.get_handle(),
				vnet.mac.c_str());
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetBoundCardMac"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (!vnet.ifname.empty() && !vnet.vnet.empty()) {
		if (vnet.type == PVN_HOST_ONLY) {
			ret = PrlVirtNet_SetAdapterName(hVirtNet.get_handle(),
					vnet.ifname.c_str());
			if (PRL_FAILED(ret))
				return prl_err(ret, "Error: "
					"PrlVirtNet_SetAdapterName failed: %s",
					get_error_str(ret).c_str());
		} else {
			PrlDevSrv *net_dev = find_dev(DEV_NET, vnet.ifname);
			if (!net_dev)
				return prl_err(PRL_NET_ADAPTER_NOT_EXIST,
					"Failed to find network adapter %s on"
					" the server.",
					vnet.ifname.c_str());
			/* check that adapter is not bounded to any virtual network */
			PrlHandle existVirtNet;
			if (!find_vnetwork_handle_by_mac_and_vlan(
				net_dev->m_mac, net_dev->m_vlanTag, existVirtNet))
			{
				/* VN found */
				PRL_NET_VIRTUAL_NETWORK_TYPE nType;
				ret = PrlVirtNet_GetNetworkType(existVirtNet.get_handle(), &nType);
				if (PRL_FAILED(ret)) {
					return prl_err(ret, "Error: PrlVirtNet_GetNetworkType failed: %s",
							get_error_str(ret).c_str());
				}
				if (nType == PVN_BRIDGED_ETHERNET) {
					return prl_err(1, "Error: "
							"you cannot bind more than one "
							"bridged network to the same interface");
				}
			}
			ret = PrlVirtNet_SetBoundCardMac(hVirtNet.get_handle(),
					net_dev->m_mac.c_str());
			if (PRL_FAILED(ret))
				return prl_err(ret, "Error: "
					"PrlVirtNet_SetBoundCardMac failed: %s",
					get_error_str(ret).c_str());
			ret = PrlVirtNet_SetVlanTag(hVirtNet.get_handle(),
					net_dev->m_vlanTag);
			if (PRL_FAILED(ret))
				return prl_err(ret, "Error: "
					"PrlVirtNet_SetVlanTag failed: %s",
					get_error_str(ret).c_str());
		}
	}

	ret = fill_vnetwork_nat_rules(vnet, hVirtNet);
	if (PRL_FAILED(ret))
		return ret;

	return 0;
}

int PrlSrv::fill_vnetwork_nat_rules(const VNetParam &vnet, PrlHandle &hVirtNet)
{
	if (   vnet.nat_tcp_add_rules.empty()
		&& vnet.nat_udp_add_rules.empty()
		&& vnet.nat_tcp_del_rules.empty()
		&& vnet.nat_udp_del_rules.empty())
		return 0;

	PrlHandle hHandlesList;
	int ret = PrlApi_CreateHandlesList(hHandlesList.get_ptr());
	if (PRL_FAILED(ret))
		return prl_err(ret, "Error: PrlApi_CreateHandlesList failed:"
			" %s", get_error_str(ret).c_str());

	for(int t = PPF_TCP; t <= PPF_UDP; t++)
	{
		ret = PrlVirtNet_GetPortForwardList(hVirtNet.get_handle(),
											(PRL_PORT_FORWARDING_TYPE )t,
											hHandlesList.get_ptr());
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_GetPortForwardList failed:"
				" %s", get_error_str(ret).c_str());

		str_list_t all_nat_rules;
		const str_list_t& nat_del_rules
			= (t == PPF_TCP	? vnet.nat_tcp_del_rules : vnet.nat_udp_del_rules);
		const nat_rule_list_t& nat_add_rules
			= (t == PPF_TCP	? vnet.nat_tcp_add_rules : vnet.nat_udp_add_rules);

		PRL_UINT32 nCount = 0;
		ret = PrlHndlList_GetItemsCount(hHandlesList.get_handle(), &nCount);
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlHndlList_GetItemsCount failed:"
				" %s", get_error_str(ret).c_str());

		for(PRL_UINT32 i = 0; i < nCount; ++i)
		{
			PrlHandle hPortFwd;
			ret = PrlHndlList_GetItem(hHandlesList.get_handle(), i, hPortFwd.get_ptr());
			if (PRL_FAILED(ret))
				return prl_err(ret, "Error: PrlHndlList_GetItem failed:"
					" %s", get_error_str(ret).c_str());

			char buf[128];
			PRL_UINT32 len = sizeof(buf);
			ret = PrlPortFwd_GetRuleName(hPortFwd.get_handle(), buf, &len);
			if (PRL_FAILED(ret))
				return prl_err(ret, "Error: PrlPortFwd_GetRuleName failed:"
					" %s", get_error_str(ret).c_str());

			all_nat_rules.push_back(buf);
		}

		nat_rule_list_t::const_iterator it;
		for(it = nat_add_rules.begin(); it != nat_add_rules.end(); ++it)
		{
			if (std::find(all_nat_rules.begin(), all_nat_rules.end(), (*it).name)
				!= all_nat_rules.end())
				return prl_err(PRL_ERR_INVALID_ARG, "The rule with '%s' name already exists!", (*it).name.c_str() );
		}

		for(int j = (int )nCount - 1; j >= 0; --j)
		{
			PrlHandle hPortFwd;
			ret = PrlHndlList_GetItem(hHandlesList.get_handle(), (PRL_UINT32 )j, hPortFwd.get_ptr());
			if (PRL_FAILED(ret))
				return prl_err(ret, "Error: PrlHndlList_GetItem failed:"
					" %s", get_error_str(ret).c_str());

			char buf[128];
			PRL_UINT32 len = sizeof(buf);
			ret = PrlPortFwd_GetRuleName(hPortFwd.get_handle(), buf, &len);
			if (PRL_FAILED(ret))
				return prl_err(ret, "Error: PrlPortFwd_GetRuleName failed:"
					" %s", get_error_str(ret).c_str());

			str_list_t::const_iterator it = find(nat_del_rules.begin(), nat_del_rules.end(), buf);
			if (it == nat_del_rules.end())
				continue;

			ret = PrlHndlList_RemoveItem(hHandlesList.get_handle(), (PRL_UINT32 )j);
			if (PRL_FAILED(ret))
				return prl_err(ret, "Error: PrlHndlList_RemoveItem failed:"
					" %s", get_error_str(ret).c_str());
		}

		for(it = nat_add_rules.begin();	it != nat_add_rules.end(); ++it)
		{
			PrlHandle hPortFwd;
			ret = PrlPortFwd_Create(hPortFwd.get_ptr());
			if (PRL_FAILED(ret))
				return prl_err(ret, "Error: PrlPortFwd_Create failed:"
					" %s", get_error_str(ret).c_str());

			nat_rule_s nat_rule = *it;

			ret = PrlPortFwd_SetRuleName(hPortFwd.get_handle(), nat_rule.name.c_str());
			if (PRL_FAILED(ret))
				return prl_err(ret, "Error: PrlPortFwd_SetRuleName failed:"
					" %s", get_error_str(ret).c_str());
			ret = PrlPortFwd_SetRedirectIPAddress(hPortFwd.get_handle(), nat_rule.redir_entry.c_str());
			if (ret == PRL_ERR_INVALID_ARG)
			{
				ret = PrlPortFwd_SetRedirectVm(hPortFwd.get_handle(), nat_rule.redir_entry.c_str());
				if (PRL_FAILED(ret))
					return prl_err(ret, "Error: PrlPortFwd_SetRedirectVm failed:"
						" %s", get_error_str(ret).c_str());
			}
			else if (PRL_FAILED(ret))
				return prl_err(ret, "Error: PrlPortFwd_SetRedirectIPAddress failed:"
					" %s", get_error_str(ret).c_str());
			ret = PrlPortFwd_SetIncomingPort(hPortFwd.get_handle(), (PRL_UINT16 )nat_rule.in_port);
			if (PRL_FAILED(ret))
				return prl_err(ret, "Error: PrlPortFwd_SetIncomingPort failed:"
					" %s", get_error_str(ret).c_str());
			ret = PrlPortFwd_SetRedirectPort(hPortFwd.get_handle(), (PRL_UINT16 )nat_rule.redir_port);
			if (PRL_FAILED(ret))
				return prl_err(ret, "Error: PrlPortFwd_SetRedirectPort failed:"
					" %s", get_error_str(ret).c_str());

			ret = PrlHndlList_AddItem(hHandlesList.get_handle(), hPortFwd.get_handle());
			if (PRL_FAILED(ret))
				return prl_err(ret, "Error: PrlHndlList_AddItem failed:"
					" %s", get_error_str(ret).c_str());
		}

		ret = PrlVirtNet_SetPortForwardList(hVirtNet.get_handle(),
											(PRL_PORT_FORWARDING_TYPE )t,
											hHandlesList.get_handle());
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlVirtNet_SetPortForwardList failed:"
				" %s", get_error_str(ret).c_str());
	}

	return 0;
}

int PrlSrv::find_vnetwork_handle_by_name(
		const std::string &name, PrlHandle &hVirtNet)
{
	PRL_RESULT ret;

	ret = fill_vnetworks_list(m_VNetList);
	if (ret)
		return ret;

	char buf[1024];
	PRL_UINT32 len;
	PrlVNetList::const_iterator it = m_VNetList.begin();
	for (; it != m_VNetList.end(); ++it) {
		len = sizeof(buf);
		ret = PrlVirtNet_GetNetworkId((*it)->get_handle(),
				buf, &len);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlVirtNet_GetNetworkId"
				" failed: %s", get_error_str(ret).c_str());
			continue;
		}
		if (!strncmp(name.c_str(), buf, sizeof(buf))) {
			hVirtNet = *(*it);
			return 0;
		}
	}
	return 1;
}

int PrlSrv::find_vnetwork_handle_by_mac_and_vlan(const std::string &mac,
		unsigned short vlanTag, PrlHandle &hVirtNet)
{
	int ret;
	char buf[64];
	PRL_UINT32 len;
	PRL_UINT16 vnet_vlanTag;

	PrlVNetList::const_iterator it = m_VNetList.begin();
	for (; it != m_VNetList.end(); ++it) {
		len = sizeof(buf);
		ret = PrlVirtNet_GetBoundCardMac((*it)->get_handle(),
				buf, &len);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlVirtNet_GetBoundCardMac"
				" failed: %s", get_error_str(ret).c_str());
			continue;
		}

		ret = PrlVirtNet_GetVlanTag((*it)->get_handle(), &vnet_vlanTag);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlVirtNet_GetVlanTag"
				" failed: %s", get_error_str(ret).c_str());
			continue;
		}

		if (!strncmp(mac.c_str(), buf, sizeof(buf)) &&
			vlanTag == (unsigned short)vnet_vlanTag) {
			hVirtNet = *(*it);
			return 0;
		}
	}
	return 1;
}

int PrlSrv::find_vnetwork_handle_by_ifname(
		const std::string &ifname, PrlHandle &hVirtNet)
{
	PRL_RESULT ret;
	PrlDevSrv *net_dev;

	net_dev = find_dev(DEV_NET, ifname);
	if (!net_dev)
		return prl_err(PRL_NET_ADAPTER_NOT_EXIST,
			"Failed to find network adapter %s on the server.",
			ifname.c_str());

	ret = fill_vnetworks_list(m_VNetList);
	if (ret)
		return ret;

	return find_vnetwork_handle_by_mac_and_vlan(net_dev->m_mac,
			net_dev->m_vlanTag, hVirtNet);
}

int PrlSrv::find_vnetwork_handle(const VNetParam &vnet, PrlHandle &hVirtNet)
{
	PRL_RESULT ret;

	if (!vnet.vnet.empty()) {
		ret = find_vnetwork_handle_by_name(vnet.vnet,
				hVirtNet);
		if (ret)
			return prl_err(ret, "Virtual Network %s"
				" does not exist.", vnet.vnet.c_str());
	} else if (!vnet.ifname.empty()) {
		ret = find_vnetwork_handle_by_ifname(vnet.ifname,
				hVirtNet);
		if (ret)
			return prl_err(ret, "Failed to find a Virtual Network"
			" for network adapter %s.", vnet.ifname.c_str());
	} else
		return prl_err(1, "The Virtual Network name is not specified.");
	return 0;
}

void PrlSrv::print_boundto_bridged(const PrlHandle *phVirtNet,
	char *buf, int size, PrlOutFormatter &f, bool detailed)
{

	PRL_UINT32 len;
	PrlHandle hAdapter;
	PRL_RESULT ret = PrlVirtNet_GetBoundAdapterInfo(phVirtNet->get_handle(),
			get_srv_config_handle(), hAdapter.get_ptr());
	if (ret == PRL_ERR_NETWORK_ADAPTER_NOT_FOUND) {
		/* print MAC than */
		len = size;
		ret = PrlVirtNet_GetBoundCardMac(phVirtNet->get_handle(),
				buf, &len);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlVirtNet_GetBoundCardMac"
				" failed: %s", get_error_str(ret).c_str());
			fprintf(stdout, "\n");
			return;
		}
	} else if (PRL_FAILED(ret)) {
		prl_log(L_ERR, "Error: PrlVirtNet_GetBoundAdapterInfo"
				" failed: %s", get_error_str(ret).c_str());
		fprintf(stdout, "\n");
		return;
	} else {
		PRL_UINT32 len = size;
		ret = PrlSrvCfgDev_GetId(hAdapter.get_handle(), buf, &len);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlSrvCfgDev_GetId failed: %s",
					get_error_str(ret).c_str());
			fprintf(stdout, "\n");
			return;
		}
	}
	if (detailed)
		f.add("Bound To", buf);
	else
		f.tbl_add_item("Bound To", "%-15s", buf);

	len = size;
	ret = PrlVirtNet_GetAdapterName(phVirtNet->get_handle(), buf, &len);
	if (PRL_FAILED(ret)) {
		prl_log(L_ERR, "Error: PrlVirtNet_GetAdapterName"
				" failed: %s", get_error_str(ret).c_str());
		return;
	}

	if (detailed)
		f.add("Bridge", buf);
	else
		f.tbl_add_item("Bridge", "%-15s", buf);
}

void PrlSrv::print_boundto_host_only(const PrlHandle *phVirtNet,
		PrlOutFormatter &f, bool detailed)
{
	PRL_UINT32 nIndex;
	PRL_RESULT ret;
	char buf[1024];
	PRL_UINT32 len;
	PrlDevSrv *dev;
	PRL_BOOL bEnabled;

	ret = PrlVirtNet_GetAdapterIndex(phVirtNet->get_handle(),
			&nIndex);
	if (PRL_FAILED(ret)) {
		prl_log(L_ERR, "Error: PrlVirtNet_GetAdapterIndex"
				" failed: %s", get_error_str(ret).c_str());
		return;
	}

	/* since PSBM5 we have no virtual adapters for host-only
	 * networks  - so it is not an error if dev not found */
	dev = find_net_dev_by_idx(nIndex, true);
	if (detailed)
		f.add("Bound To", (dev) ? dev->m_name.c_str() : "");
	else
		f.tbl_add_item("Bound To", "%-15s", (dev) ? dev->m_name.c_str() : "");

	len = sizeof(buf);
	ret = PrlVirtNet_GetAdapterName(phVirtNet->get_handle(), buf, &len);
	if (PRL_FAILED(ret)) {
		prl_log(L_ERR, "Error: PrlVirtNet_GetAdapterName"
				" failed: %s", get_error_str(ret).c_str());
		return;
	}

	if (detailed)
		f.add("Bridge", buf);
	else
		f.tbl_add_item("Bridge", "%-15s", buf);

	if (!detailed)
		return;

	/* Information below printed only in detailed mode */
	/* Parallels adapter information */
	ret = PrlVirtNet_IsAdapterEnabled(phVirtNet->get_handle(), &bEnabled);
	if (PRL_FAILED(ret)) {
		prl_log(L_ERR, "Error: PrlVirtNet_IsAdapterEnabled"
				" failed: %s", get_error_str(ret).c_str());
		return;
	}

	if (bEnabled)
	{
		f.open("Parallels adapter");

		/* Host IPv4 address */
		len = sizeof(buf);
		ret = PrlVirtNet_GetHostIPAddress(phVirtNet->get_handle(), buf, &len);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlVirtNet_GetHostIPAddress"
					" failed: %s", get_error_str(ret).c_str());
			f.close();
			return;
		}
		f.add("IP address", buf);

		/* IPv4 Network Mask */
		len = sizeof(buf);
		ret = PrlVirtNet_GetIPNetMask(phVirtNet->get_handle(), buf, &len);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlVirtNet_GetIPNetMask"
					" failed: %s", get_error_str(ret).c_str());
			f.close();
			return;
		}
		f.add("Subnet mask", buf);

		/* Host IPv6 address */
		len = sizeof(buf);
		ret = PrlVirtNet_GetHostIP6Address(phVirtNet->get_handle(), buf, &len);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlVirtNet_GetHostIP6Address"
					" failed: %s", get_error_str(ret).c_str());
			f.close();
			return;
		}
		f.add("IPv6 address", buf);

		/* IPv6 Network Mask */
		len = sizeof(buf);
		ret = PrlVirtNet_GetIP6NetMask(phVirtNet->get_handle(), buf, &len);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlVirtNet_GetIP6NetMask"
					" failed: %s", get_error_str(ret).c_str());
			f.close();
			return;
		}
		f.add("IPv6 subnet mask", buf);

		f.close();
	}

	ret = PrlVirtNet_IsDHCPServerEnabled(phVirtNet->get_handle(), &bEnabled);
	if (PRL_FAILED(ret)) {
		prl_log(L_ERR, "Error: PrlVirtNet_IsDHCPServerEnabled"
				" failed: %s", get_error_str(ret).c_str());
		return;
	}

	if (bEnabled)
	{
		/* DHCPv4 server information */
		f.open("DHCPv4 server");

		/* DHCPv4 server IP address */
		len = sizeof(buf);
		ret = PrlVirtNet_GetDhcpIPAddress(phVirtNet->get_handle(), buf, &len);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlVirtNet_GetDhcpIPAddress"
					" failed: %s", get_error_str(ret).c_str());
			f.close();
			return;
		}
		f.add("Server address", buf);

		/* IP addresses scope start */
		len = sizeof(buf);
		ret = PrlVirtNet_GetIPScopeStart(phVirtNet->get_handle(), buf, &len);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlVirtNet_GetIPScopeStart"
					" failed: %s", get_error_str(ret).c_str());
			f.close();
			return;
		}
		f.add("IP scope start address", buf);

		/* IP addresses scope end */
		len = sizeof(buf);
		ret = PrlVirtNet_GetIPScopeEnd(phVirtNet->get_handle(), buf, &len);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlVirtNet_GetIPScopeEnd"
					" failed: %s", get_error_str(ret).c_str());
			f.close();
			return;
		}
		f.add("IP scope end address", buf);

		f.close();
	}

	ret = PrlVirtNet_IsDHCP6ServerEnabled(phVirtNet->get_handle(), &bEnabled);
	if (PRL_FAILED(ret)) {
		prl_log(L_ERR, "Error: PrlVirtNet_IsDHCP6ServerEnabled"
				" failed: %s", get_error_str(ret).c_str());
		return;
	}

	if (bEnabled)
	{
		/* DHCPv6 server information */
		f.open("DHCPv6 server");

		/* DHCPv6 server IP address */
		len = sizeof(buf);
		ret = PrlVirtNet_GetDhcpIP6Address(phVirtNet->get_handle(), buf, &len);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlVirtNet_GetDhcpIP6Address"
					" failed: %s", get_error_str(ret).c_str());
			f.close();
			return;
		}
		f.add("Server IPv6 address", buf);

		/* IP addresses scope start */
		len = sizeof(buf);
		ret = PrlVirtNet_GetIP6ScopeStart(phVirtNet->get_handle(), buf, &len);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlVirtNet_GetIP6ScopeStart"
					" failed: %s", get_error_str(ret).c_str());
			f.close();
			return;
		}
		f.add("IPv6 scope start address", buf);

		/* IP addresses scope end */
		len = sizeof(buf);
		ret = PrlVirtNet_GetIP6ScopeEnd(phVirtNet->get_handle(), buf, &len);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlVirtNet_GetIP6ScopeEnd"
					" failed: %s", get_error_str(ret).c_str());
			f.close();
			return;
		}
		f.add("IPv6 scope end address", buf);

		f.close();
	}
}

void PrlSrv::print_vnetwork_info(const PrlHandle *phVirtNet,
		PrlOutFormatter &f, bool detailed)
{
	PRL_RESULT ret;
	char buf[1024];
	PRL_UINT32 len;

	/* Network ID */
	len = sizeof(buf);
	ret = PrlVirtNet_GetNetworkId(phVirtNet->get_handle(), buf, &len);
	if (PRL_FAILED(ret)) {
		prl_log(L_ERR, "Error: PrlVirtNet_GetNetworkID failed: %s",
				get_error_str(ret).c_str());
		return;
	}

	if (detailed)
		f.open_object();
	else
		f.tbl_row_open();

	if (detailed)
		f.add("Network ID", buf);
	else if (strlen(buf) > 16)
		f.tbl_add_item("Network ID", "%-15.15s~  ", buf);
	else
		f.tbl_add_item("Network ID", "%-16.16s  ", buf);

	/* Type */
	PRL_NET_VIRTUAL_NETWORK_TYPE nType;
	ret = PrlVirtNet_GetNetworkType(phVirtNet->get_handle(), &nType);
	if (PRL_FAILED(ret)) {
		prl_log(L_ERR, "Error: PrlVirtNet_GetNetworkType failed: %s",
				get_error_str(ret).c_str());
		if (detailed)
			f.close_object();
		else
			f.tbl_row_close();
		return;
	}
	if (nType == PVN_BRIDGED_ETHERNET) {
		if (detailed)
			f.add("Type", "bridged");
		else
			f.tbl_add_item("Type", "%-10s", "bridged");

		/* Bound To */
		print_boundto_bridged(phVirtNet, buf, sizeof(buf), f, detailed);

	} else {
		PRL_BOOL bNatEnabled;
		ret = PrlVirtNet_IsNATServerEnabled(phVirtNet->get_handle(),
				&bNatEnabled);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlVirtNet_IsNatServerEnabled"
				" failed: %s", get_error_str(ret).c_str());
			if (detailed)
				f.close_object();
			else
				f.tbl_row_close();
			return;
		}
		if (bNatEnabled) {
			if (detailed)
				f.add("Type", "shared");
			else
				f.tbl_add_item("Type", "%-10s", "shared");

			/* BoundTo */
			print_boundto_host_only(phVirtNet, f, detailed);

			if (f.type == OUT_FORMATTER_PLAIN)
				f.add("", "\n", true, false, true);

			print_nat_info(*phVirtNet, f);

		} else {
			if (detailed)
				f.add("Type", "host-only");
			else
				f.tbl_add_item("Type", "%-10s", "host-only");

			/* BoundTo */
			print_boundto_host_only(phVirtNet, f, detailed);
		}
	}

    if (detailed)
        f.close_object();
    else
        f.tbl_row_close();
}

void PrlSrv::print_nat_info(const PrlHandle& hVirtNet,	PrlOutFormatter &f)
{
    f.open("NAT server");

	for(int t = PPF_TCP; t <= PPF_UDP; t++)
	{
		PrlHandle hHandlesList;
		PRL_RESULT ret = PrlVirtNet_GetPortForwardList( hVirtNet.get_handle(),
														(PRL_PORT_FORWARDING_TYPE )t,
														hHandlesList.get_ptr());
		if (PRL_FAILED(ret))
		{
			 prl_log(L_ERR, "Error: PrlVirtNet_GetPortForwardList failed:"
				" %s", get_error_str(ret).c_str());
			 continue;
		}

		PRL_UINT32 nCount = 0;
		ret = PrlHndlList_GetItemsCount(hHandlesList.get_handle(), &nCount);
		if (PRL_FAILED(ret))
		{
			prl_log(L_ERR, "Error: PrlHndlList_GetItemsCount failed:"
				" %s", get_error_str(ret).c_str());
			continue;
		}

		if (nCount)
			f.open(t == PPF_TCP ? "TCP rules" : "UDP rules");

		for(PRL_UINT32 i = 0; i < nCount; ++i)
		{
			PrlHandle hPortFwd;
			ret = PrlHndlList_GetItem(hHandlesList.get_handle(), i, hPortFwd.get_ptr());
			if (PRL_FAILED(ret))
			{
				prl_log(L_ERR, "Error: PrlHndlList_GetItem failed:"
					" %s", get_error_str(ret).c_str());
				continue;
			}

			nat_rule_s nat_rule;
			char buf[128];
			PRL_UINT16 port = 0;

			PRL_UINT32 len = sizeof(buf);
			ret = PrlPortFwd_GetRuleName(hPortFwd.get_handle(), buf, &len);
			if (PRL_FAILED(ret))
				prl_log(L_ERR, "Error: PrlPortFwd_GetRuleName failed:"
					" %s", get_error_str(ret).c_str());
			else
				nat_rule.name = buf;

			len = sizeof(buf);
			ret = PrlPortFwd_GetRedirectVm(hPortFwd.get_handle(), buf, &len);
			if (PRL_FAILED(ret))
				prl_log(L_ERR, "Error: PrlPortFwd_GetRedirectVm failed:"
					" %s", get_error_str(ret).c_str());
			else
				nat_rule.redir_entry = buf;

			if (nat_rule.redir_entry.empty())
			{
				len = sizeof(buf);
				ret = PrlPortFwd_GetRedirectIPAddress(hPortFwd.get_handle(), buf, &len);
				if (PRL_FAILED(ret))
					prl_log(L_ERR, "Error: PrlPortFwd_GetRedirectIPAddress failed:"
						" %s", get_error_str(ret).c_str());
				else
					nat_rule.redir_entry = buf;
			}

			ret = PrlPortFwd_GetIncomingPort(hPortFwd.get_handle(), &port);
			if (PRL_FAILED(ret))
				prl_log(L_ERR, "Error: PrlPortFwd_GetIncomingPort failed:"
					" %s", get_error_str(ret).c_str());
			else
				nat_rule.in_port = port;

			ret = PrlPortFwd_GetRedirectPort(hPortFwd.get_handle(), &port);
			if (PRL_FAILED(ret))
				prl_log(L_ERR, "Error: PrlPortFwd_GetRedirectPort failed:"
					" %s", get_error_str(ret).c_str());
			else
				nat_rule.redir_port = port;

			bool is_inline = (f.type != OUT_FORMATTER_JSON);

			f.open(nat_rule.name.c_str(), is_inline);
			f.add("redirect IP/VM id", nat_rule.redir_entry, is_inline);
			f.add("in port", nat_rule.in_port, "", is_inline);
			f.add("redirect port", nat_rule.redir_port, "", is_inline);
			f.close(is_inline);
		}

		if (nCount)
			f.close();
	}

	f.close();
}

int PrlSrv::convert_vm(const CmdParamData &param)
{
	PRL_RESULT ret;

	prl_log(0, "Converting VM %s", param.id.c_str());

	PrlHandle hJob(PrlSrv_Register3rdPartyVm(m_hSrv,
											 param.id.c_str(),
											 param.vm_location.c_str(),
											 param.force?
											   PR3F_ALLOW_UNKNOWN_OS:0));
	const PrlHook *h = get_cleanup_ctx().register_hook(cancel_job, hJob.get_handle());
	reg_event_callback( server_event_handler, 0/*hJob.get_handle() */);
	std::string err;
	ret = get_job_retcode(hJob.get_handle(), err);
	unreg_event_callback( server_event_handler, 0/* hJob.get_handle()*/ );
	get_cleanup_ctx().unregister_hook( h );
	if (ret)
		return prl_err(ret, "\nFailed to convert the VM: %s", err.c_str());

	prl_log(0, "The VM has been successfully converted.");
	return 0;
}



int PrlSrv::vnetwork(const VNetParam &vnet, bool use_json)
{
	if (vnet.cmd == VNetParam::List)
		return vnetwork_list(use_json);

	PrlHandle hVirtNet;
	PRL_RESULT ret;
	std::string err;

	reg_event_callback(server_event_handler, NULL);

	if (vnet.cmd == VNetParam::Add) {
		if (fill_vnetwork_handle(vnet, hVirtNet))
			return 1;

		PrlHandle hJob(PrlSrv_AddVirtualNetwork(m_hSrv,
					hVirtNet.get_handle(), 0));
		if ((ret = get_job_retcode(hJob.get_handle(), err)))
			return prl_err(ret, "Failed to add Virtual Network %s:"
				" %s", vnet.vnet.c_str(), err.c_str());
		if (vnet.type == PVN_BRIDGED_ETHERNET)
			return 0;
	}

	if ((ret = find_vnetwork_handle(vnet, hVirtNet)))
		return ret;

	/* FIXME: here we apply host-only network settings once again, since all
	 * the settings, that we have passed over command line, are being reset
	 * to defaults inside PrlSrv_AddVirtualNetwork() - need to properly split
	 * fill_vnetwork_handle() into parts and call each part once */
	if (fill_vnetwork_handle(vnet, hVirtNet))
		return 1;

	if (vnet.cmd == VNetParam::Add || vnet.cmd == VNetParam::Set) {
		PrlHandle hJob(PrlSrv_UpdateVirtualNetwork(m_hSrv,
					hVirtNet.get_handle(), 0));
		if ((ret = get_job_retcode(hJob.get_handle(), err)))
			return prl_err(ret, "Failed to update Virtual Network"
				" %s: %s", vnet.vnet.c_str(), err.c_str());
	} else if (vnet.cmd == VNetParam::Del) {
		PrlHandle hJob(PrlSrv_DeleteVirtualNetwork(m_hSrv,
					hVirtNet.get_handle(), 0));
		if ((ret = get_job_retcode(hJob.get_handle(), err)))
			return prl_err(ret, "Failed to delete Virtual Network"
				" %s: %s", vnet.vnet.c_str(), err.c_str());
	} else if (vnet.cmd == VNetParam::Info) {
		PrlOutFormatter &f = *(get_formatter(use_json, "\t"));

		print_vnetwork_info(&hVirtNet, f, 1);

		fprintf(stdout, "%s", f.get_buffer().c_str());
	}

	unreg_event_callback(server_event_handler);

	return 0;
}

int PrlSrv::vnetwork_list(bool use_json)
{
	PRL_RESULT ret;
	PrlOutFormatter &f = *(get_formatter(use_json));

	if ((ret = fill_vnetworks_list(m_VNetList)))
		return ret;

	if (f.type == OUT_FORMATTER_PLAIN)
		fprintf(stdout, "%-17s %-9s %-14s %-15s\n",
			"Network ID", "Type", "Bound To", "Bridge");
	PrlVNetList::const_iterator it = m_VNetList.begin();
	f.open_list();
	for (; it != m_VNetList.end(); ++it)
		print_vnetwork_info(*it, f, 0);
	f.close_list();

	fprintf(stdout, "%s", f.get_buffer().c_str());

	return 0;
}

int PrlSrv::fill_priv_networks_list(PrlPrivNetList &list) const
{
	list.del();

	std::string err;
	PRL_RESULT ret;
	PrlHandle hJob(PrlSrv_GetIPPrivateNetworksList(m_hSrv, 0));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "Failed to retrieve the list of IP"
			" private networks: %s", err.c_str());
	PrlHandle hResult;
	if ((ret = PrlJob_GetResult(hJob.get_handle(), hResult.get_ptr()))) {
		return prl_err(ret, "PrlJob_GetResult: %s",
				get_error_str(ret).c_str());
	}

	PRL_UINT32 nCount = 0;
	if ((ret = PrlResult_GetParamsCount(hResult.get_handle(), &nCount))) {
		return prl_err(ret, "PrlResult_GetParamsCount: %s",
				get_error_str(ret).c_str());
	}

	for(PRL_UINT32 i = 0; i < nCount; ++i)
	{
		PrlHandle *phTmp = new PrlHandle;
		if ((ret = PrlResult_GetParamByIndex(hResult.get_handle(), i,
						phTmp->get_ptr()))) {
			prl_log(L_ERR, "PrlResult_GetParamsByIndex [%u]: %s",
				i, get_error_str(ret).c_str());
			delete phTmp;
			continue;
		}
		list.add(phTmp);
	}

	return 0;
}

int PrlSrv::find_priv_network_handle(const PrivNetParam &privnet, PrlHandle &hPrivNet)
{
	PRL_RESULT ret;

	if (privnet.name.empty())
		return prl_err(1, "The IP private network name is not specified.");

	ret = fill_priv_networks_list(m_PrivNetList);
	if (ret)
		return ret;

	char buf[1024];
	PRL_UINT32 len;
	PrlPrivNetList::const_iterator it = m_PrivNetList.begin();
	for (; it != m_PrivNetList.end(); ++it) {
		len = sizeof(buf);
		ret = PrlIPPrivNet_GetName((*it)->get_handle(),
				buf, &len);
		if (PRL_FAILED(ret)) {
			prl_log(L_ERR, "Error: PrlIPPrivNet_GetName"
				" failed: %s", get_error_str(ret).c_str());
			continue;
		}
		if (!strncmp(privnet.name.c_str(), buf, sizeof(buf))) {
			hPrivNet = *(*it);
			return 0;
		}
	}

	return prl_err(ret, "IP private network %s does not exist.",
		privnet.name.c_str());
}

int PrlSrv::get_ip(const PrlHandle *phPrivNet, ip_list_t &ips)
{
	PRL_RESULT ret;
	PrlHandle h;

	if ((ret = PrlIPPrivNet_GetNetAddresses(phPrivNet->get_handle(),
					h.get_ptr())))
		return prl_err(ret, "PrlIPPrivNet_GetNetAddresses: %s",
				get_error_str(ret).c_str());

	PRL_UINT32 count;
	PrlStrList_GetItemsCount(h.get_handle(), &count);
	for (unsigned int i = 0; i < count; i++) {
		char buf[128];
		unsigned int len = sizeof(buf);

		PrlStrList_GetItem(h.get_handle(), i, buf, &len);
		ips.add(buf);
	}

	return 0;
}

int PrlSrv::fill_priv_network_handle(const PrivNetParam &privnet, PrlHandle &hPrivNet)
{
	PRL_RESULT ret;
	const char *name = NULL;

	if (!hPrivNet.valid()) {
		ret = PrlIPPrivNet_Create(hPrivNet.get_ptr());
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlIPPrivNet_Create failed:"
				" %s", get_error_str(ret).c_str());
	}
	if (!privnet.name.empty()) {
		name = privnet.name.c_str();
		ret = PrlIPPrivNet_SetName(hPrivNet.get_handle(), name);
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlIPPrivNet_SetNetworkId"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (privnet.is_global != -1) {
		ret = PrlIPPrivNet_SetGlobal(hPrivNet.get_handle(), (PRL_BOOL)privnet.is_global);
		if (PRL_FAILED(ret))
			return prl_err(ret, "Error: PrlIPPrivNet_SetGlobal"
				" failed: %s", get_error_str(ret).c_str());
	}
	if (!privnet.ip.empty() || !privnet.ip_del.empty()) {
		ip_list_t ips, cur_ips;
		int err;
		PrlHandle h;

		/* retrieve current list of configured IPs */
		err = get_ip(&hPrivNet, cur_ips);
		if (err)
			return err;

		/* merge it with ip and ip_del lists */
		for (ip_list_t::const_iterator it = cur_ips.begin(); it != cur_ips.end(); it++)
			if (!privnet.ip_del.find(*it) && !privnet.ip.find(*it))
				ips.push_back(*it);
		for (ip_list_t::const_iterator it = privnet.ip.begin(); it != privnet.ip.end(); it++)
			ips.push_back(*it);

		/* put result to new strings list handler */
		PrlApi_CreateStringsList(h.get_ptr());
		for (ip_list_t::const_iterator it = ips.begin(), eit = ips.end(); it != eit; ++it)
			PrlStrList_AddItem(h.get_handle(), it->to_str().c_str());

		/* adn finally set list to PrivNet handle */
		if ((ret = PrlIPPrivNet_SetNetAddresses(hPrivNet.get_handle(), h.get_handle()))) {
			if (ret == PRL_ERR_INVALID_ARG)
				return prl_err(ret, "The following IP addresses are invalid: '%s'",
					ips.to_str().c_str());
			else
				return prl_err(ret, "PrlIPPrivNet_SetNetAddresses: %s",
					get_error_str(ret).c_str());
		}
	}
	return 0;
}

void PrlSrv::print_priv_network_info(const PrlHandle *phPrivNet, PrlOutFormatter &f)
{
	PRL_RESULT ret;
	char buf[1024];
	PRL_UINT32 len = sizeof(buf);
	PRL_BOOL bIsGlobal;
	ip_list_t ips;

	/* Network ID */
	ret = PrlIPPrivNet_GetName(phPrivNet->get_handle(), buf, &len);
	if (PRL_FAILED(ret)) {
		prl_log(L_ERR, "Error: PrlIPPrivNet_GetName failed: %s",
				get_error_str(ret).c_str());
		return;
	}

	if (strlen(buf) > 16)
		f.tbl_add_item("Name", "%-15.15s~  ", buf);
	else
		f.tbl_add_item("Name", "%-16.16s  ", buf);

	/* Is global */
	ret = PrlIPPrivNet_IsGlobal(phPrivNet->get_handle(), &bIsGlobal);
	if (PRL_FAILED(ret)) {
		prl_log(L_ERR, "Error: PrlIPPrivNet_IsGlobal failed: %s",
				get_error_str(ret).c_str());
		return;
	}

	if (f.type == OUT_FORMATTER_PLAIN)
		f.tbl_add_item("Is global", "%s ", bIsGlobal ? "x" : " ");
	else
		f.add("Is global", (bIsGlobal != PRL_FALSE));

	/* Netmasks */
	if (!get_ip(phPrivNet, ips))
		f.tbl_add_item("Netmasks", "%s", ips.to_str().c_str());
}

int PrlSrv::priv_network(const PrivNetParam &privnet, bool use_json)
{
	PrlHandle hPrivNet;
	PRL_RESULT ret;
	std::string err;

	if (privnet.cmd == PrivNetParam::Set || privnet.cmd == PrivNetParam::Del) {
		if ((ret = find_priv_network_handle(privnet, hPrivNet)))
			return ret;
	}

	if (fill_priv_network_handle(privnet, hPrivNet))
		return 1;

	if (privnet.cmd != PrivNetParam::List)
		reg_event_callback(server_event_handler, NULL);

	if (privnet.cmd == PrivNetParam::Add) {
		PrlHandle hJob(PrlSrv_AddIPPrivateNetwork(m_hSrv,
					hPrivNet.get_handle(), 0));
		if ((ret = get_job_retcode(hJob.get_handle(), err)))
			return prl_err(ret, "Failed to add the IP private network %s:"
				" %s", privnet.name.c_str(), err.c_str());
	} else if (privnet.cmd == PrivNetParam::Set) {
		PrlHandle hJob(PrlSrv_UpdateIPPrivateNetwork(m_hSrv,
					hPrivNet.get_handle(), 0));
		if ((ret = get_job_retcode(hJob.get_handle(), err)))
			return prl_err(ret, "Failed to update the IP private network"
				" %s: %s", privnet.name.c_str(), err.c_str());
	} else if (privnet.cmd == PrivNetParam::Del) {
		PrlHandle hJob(PrlSrv_RemoveIPPrivateNetwork(m_hSrv,
					hPrivNet.get_handle(), 0));
		if ((ret = get_job_retcode(hJob.get_handle(), err)))
			return prl_err(ret, "Failed to remove the IP private network"
				" %s: %s", privnet.name.c_str(), err.c_str());
	} else if (privnet.cmd == PrivNetParam::List) {
		PrlOutFormatter &f = *(get_formatter(use_json));

		if ((ret = fill_priv_networks_list(m_PrivNetList)))
			return ret;

		if (f.type == OUT_FORMATTER_PLAIN)
			fprintf(stdout, "Name              G Netmasks\n");
		PrlPrivNetList::const_iterator it = m_PrivNetList.begin();
		f.open_list();
		for (; it != m_PrivNetList.end(); ++it) {
			f.tbl_row_open();
			print_priv_network_info(*it, f);
			f.tbl_row_close();
		}
		f.close_list();

		fprintf(stdout, "%s", f.get_buffer().c_str());
	}

	unreg_event_callback(server_event_handler);

	return 0;
}

int PrlSrv::appliance_install(const CmdParamData &param)
{
	int ret;
	std::string file;

	if (param.file.empty())
		return prl_err(-1, "The appliance descriptor file is not specified.");
	if (file2str(param.file.c_str(), file))
		return -1;
	PrlHandle hAppCfg;
	if ((ret = PrlAppliance_Create(hAppCfg.get_ptr())))
		return prl_err(-1, "PrlAppliance_Create %s",
				get_error_str(ret).c_str());

	if ((ret = PrlHandle_FromString(hAppCfg.get_handle(), file.c_str())))
		return prl_err(-1, "PrlHandle_FromString %s",
				get_error_str(ret).c_str());

	std::string err;
	reg_event_callback(server_event_handler, (void*) &param.batch);
	PrlHandle hJob(PrlSrv_InstallAppliance(m_hSrv, hAppCfg.get_handle(),
				param.vm_location.c_str(), (PACF_NON_INTERACTIVE_MODE | PIAF_FORCE)));

	const PrlHook *h = get_cleanup_ctx().register_hook(cancel_job, hJob.get_handle());

	PrlHandle hResult;
	PRL_UINT32 resultCount = 0;

	/* Fixme: timeout is unlimited */
	ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &resultCount);
	if (ret == 0) {

		for (unsigned int i = 0; i < resultCount; i++) {
			PRL_RESULT res;
			PrlHandle hVm;
			if ((res = PrlResult_GetParamByIndex(hResult.get_handle(), i, hVm.get_ptr()))) {
				prl_err(res, "PrlResult_GetParamByIndex: %s",  get_error_str(res).c_str());
				break;
			}

			char uuid[256];
			unsigned int len = sizeof(uuid);

			ret = PrlVmCfg_GetUuid(hVm.get_handle(), uuid, &len);
			if (PRL_FAILED(ret)) {
				prl_err(ret, "PrlVmCfg_GetUuid: %s",
						get_error_str(ret).c_str());
				continue;
			}

			if (param.batch)
				fprintf(stdout, "UUID %s\n", uuid);
			else
				prl_log(0, "Vm registered with UUID: %s", uuid);

			break;
		}
	}

	unreg_event_callback(server_event_handler);
	get_cleanup_ctx().unregister_hook(h);
	if (ret)
		return prl_err(ret, "Appliance installation failed: %s",
				err.c_str());
	if (!param.batch)
		prl_err(ret, "Appliance successfully installed.");

	return 0;
}

int PrlSrv::fill_ct_templates_list(PrlCtTemplateList &list) const
{
	list.del();

	std::string err;
	PRL_RESULT ret;
	PrlHandle hJob(PrlSrv_GetCtTemplateList(m_hSrv, 0));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "Failed to retrieve the list of Container"
			" templates: %s", err.c_str());
	PrlHandle hResult;
	if ((ret = PrlJob_GetResult(hJob.get_handle(), hResult.get_ptr()))) {
		return prl_err(ret, "PrlJob_GetResult: %s",
				get_error_str(ret).c_str());
	}

	PRL_UINT32 nCount = 0;
	if ((ret = PrlResult_GetParamsCount(hResult.get_handle(), &nCount))) {
		return prl_err(ret, "PrlResult_GetParamsCount: %s",
				get_error_str(ret).c_str());
	}

	for(PRL_UINT32 i = 0; i < nCount; ++i)
	{
		PrlHandle *phTmp = new PrlHandle;
		if ((ret = PrlResult_GetParamByIndex(hResult.get_handle(), i,
						phTmp->get_ptr()))) {
			prl_log(L_ERR, "PrlResult_GetParamsByIndex [%u]: %s",
				i, get_error_str(ret).c_str());
			delete phTmp;
			continue;
		}
		list.add(phTmp);
	}

	return 0;
}

void PrlSrv::print_ct_template_info(const PrlHandle *phTmpl, size_t width, PrlOutFormatter &f)
{
	PRL_RESULT ret;
	char buf[1024];
	std::stringstream fmt;
	PRL_UINT32 len = sizeof(buf);

	/* Name */
	ret = PrlCtTemplate_GetName(phTmpl->get_handle(), buf, &len);
	if (PRL_FAILED(ret)) {
		prl_log(L_ERR, "Error: PrlCtTemplate_GetName failed: %s",
				get_error_str(ret).c_str());
		return;
	}
	if (strlen(buf) > width)
		fmt << "%-" << width << '.' << width << "s~ ";
	else
		fmt << "%-" << width + 1 << '.' << width + 1 << "s ";
	f.tbl_add_item("Name", fmt.str().c_str(), buf);

	/* Type */
	PRL_CT_TEMPLATE_TYPE nType;
	ret = PrlCtTemplate_GetType(phTmpl->get_handle(), &nType);
	if (PRL_FAILED(ret)) {
		prl_log(L_ERR, "Error: PrlCtTemplate_GetType failed: %s",
				get_error_str(ret).c_str());
		return;
	}

	f.tbl_add_item("Type", "%-5s", (nType == PCT_TYPE_EZ_OS) ? "os" : "app");

	/* Arch */
	PRL_CPU_MODE nArch;
	ret = PrlCtTemplate_GetCpuMode(phTmpl->get_handle(), &nArch);
	if (PRL_FAILED(ret)) {
		prl_log(L_ERR, "Error: PrlCtTemplate_GetCpuMode failed: %s",
				get_error_str(ret).c_str());
		return;
	}
	f.tbl_add_item("Arch", "%-7s", (nArch == PCM_CPU_MODE_64) ? "x86_64" : "i386");

	/* Is cached */
	PRL_BOOL bCached;
	ret = PrlCtTemplate_IsCached(phTmpl->get_handle(),
			&bCached);
	if (PRL_FAILED(ret)) {
		prl_log(L_ERR, "Error: PrlCtTemplate_IsCached"
				" failed: %s", get_error_str(ret).c_str());
		return;
	}

	if (bCached)
		f.tbl_add_item("Cached", "%-7s", "yes");
	else if (nType == PCT_TYPE_EZ_OS)
		f.tbl_add_item("Cached", "%-7s", "no");
	else
		f.tbl_add_item("Cached", "%-7s", "-");

	/* Description */
	len = sizeof(buf);
	ret = PrlCtTemplate_GetDescription(phTmpl->get_handle(), buf, &len);
	if (PRL_FAILED(ret)) {
		prl_log(L_ERR, "Error: PrlCtTemplate_GetDescription failed: %s",
				get_error_str(ret).c_str());
		return;
	}
	f.tbl_add_item("Description", "%s", buf);
}

int PrlSrv::ct_templates(const CtTemplateParam &tmpl, bool use_json)
{
	PrlOutFormatter &f = *(get_formatter(use_json));
	PRL_RESULT ret;

	if (tmpl.cmd == CtTemplateParam::List) {
		PrlCtTemplateList list;
		if ((ret = fill_ct_templates_list(list)))
			return ret;

		PrlCtTemplateList::const_iterator it;
		size_t width = 20;
		for (it = list.begin(); it != list.end(); ++it) {
			PRL_RESULT ret;
			char buf[1024];
			PRL_UINT32 len = sizeof(buf);
			ret = PrlCtTemplate_GetName((*it)->get_handle(), buf, &len);
			if (PRL_SUCCEEDED(ret) && strlen(buf) > width)
				width = strlen(buf);
		}

		if (f.type == OUT_FORMATTER_PLAIN)
			fprintf(stdout, "%-*s Type Arch   Cached Description\n", (int)(width+1), "Name");
		f.open_list();
		for (it = list.begin(); it != list.end(); ++it) {
			f.tbl_row_open();
			print_ct_template_info(*it, width, f);
			f.tbl_row_close();
		}
		f.close_list();
		fprintf(stdout, "%s", f.get_buffer().c_str());
	} else if (tmpl.cmd == CtTemplateParam::Remove) {
		std::string err;
		PrlHandle hJob(PrlSrv_RemoveCtTemplate(m_hSrv, tmpl.name.c_str(),
			(tmpl.os_name.empty()) ? NULL : tmpl.os_name.c_str(), 0));
		if ((ret = get_job_retcode(hJob.get_handle(), err)))
			return prl_err(ret, "Failed to remove Container"
				" template '%s': %s", tmpl.name.c_str(),
				err.c_str());
	}

	return 0;
}

int PrlSrv::copy_ct_template(const CtTemplateParam &tmpl, const CopyCtTemplateParam &copy_tmpl)
{
	PRL_RESULT ret;
	PRL_HANDLE hJob;
	PrlSrv dst;
	unsigned int security_level;
	unsigned int flags;

	prl_log(0, "Migrate the CT template %s on %s", tmpl.name.c_str(), copy_tmpl.dst.server.c_str());

	if ((ret = dst.login(copy_tmpl.dst)))
		return ret;

	security_level = get_min_security_level();
	if (security_level < dst.get_min_security_level())
		security_level = dst.get_min_security_level();

	if (security_level < copy_tmpl.security_level)
		security_level = copy_tmpl.security_level;
	prl_log(L_DEBUG, "security_level=%d", security_level);

	flags = security_level;
	if (copy_tmpl.force)
		flags |= PCTMPL_FORCE;
	hJob = PrlSrv_CopyCtTemplate(
				m_hSrv,
				tmpl.name.c_str(),
				(tmpl.os_name.empty()) ? NULL : tmpl.os_name.c_str(),
				copy_tmpl.dst.server.c_str(),
				copy_tmpl.dst.port,
				dst.get_sessionid(),
				flags,
				0);
//TODO : progress	reg_event_callback(migrate_event_handler, 0);
	const PrlHook *h = get_cleanup_ctx().register_hook(cancel_job, hJob);
	std::string err;
	/* Fixme: timeout is unlimited */
	if ((ret = get_job_retcode(hJob, err)))
	{
		PrlHandle hResult;
		PRL_UINT32 resultCount = 0;
		int rc;

		prl_err(ret, "Failed to copy the CT template: %s", err.c_str());

		if ((rc = PrlJob_GetResult(hJob, hResult.get_ptr())))
			return prl_err(rc, "PrlJob_GetResult: %s [%d]",
				get_error_str(rc).c_str(), rc);

		if ((rc = PrlResult_GetParamsCount(hResult.get_handle(), &resultCount)))
			return prl_err(rc, "PrlResult_GetParamsCount  %s [%d]",
				get_error_str(rc).c_str(), rc);

		prl_log(L_DEBUG, "resultCount: %d", resultCount);
		for (unsigned int i = 0; i < resultCount; i++) {
			PrlHandle hEvent;
			char buf[4096];
			unsigned int len = sizeof(buf)-1;

			rc = PrlResult_GetParamByIndex(hResult.get_handle(), i, hEvent.get_ptr());
			if (rc) {
				prl_err(rc, "PrlResult_GetParamByIndex %s [%d]",
	                                get_error_str(rc).c_str(), rc);
				break;
			}
			if (!PrlEvent_GetErrCode(hEvent.get_handle(), &rc) &&
			    !PrlEvent_GetErrString(hEvent.get_handle(), PRL_FALSE, PRL_FALSE, buf, &len))
			{
				prl_err(0, "%s", buf);
			}
		}
	}
	else
		prl_log(0, "The CT template have been successfully migrated.");
	get_cleanup_ctx().unregister_hook(h);
	PrlHandle_Free(hJob);

	return ret;
}

int PrlSrv::plugin(const PluginParam& plugin_param, bool use_json)
{
	PRL_RESULT ret = PRL_ERR_UNEXPECTED;

	if (plugin_param.cmd == PluginParam::List)
	{
		PrlOutFormatter &f = *(get_formatter(use_json));
		std::string hdr_plugin_id = "Plugin ID                              ";
		std::string hdr_version = "Version        ";
		std::string hdr_vendor = "Vendor                                  ";
		std::string hdr_descr = "Description";
		if (f.type == OUT_FORMATTER_PLAIN)
			printf("%s%s%s%s\n", hdr_plugin_id.c_str(), hdr_version.c_str(),
					hdr_vendor.c_str(), hdr_descr.c_str());

		std::string err;

		PrlHandle hJob(PrlSrv_GetPluginsList( m_hSrv, GUID_CLS_BASE_STR, 0 ));
		if ((ret = get_job_retcode(hJob.get_handle(), err)))
			return prl_err(ret, "Failed to get plugins list: %s", err.c_str());

		PrlHandle hResult;
		PRL_UINT32 resultCount = 0;

		if ((ret = PrlJob_GetResult(hJob, hResult.get_ptr())))
			return prl_err(ret, "PrlJob_GetResult: %s [%d]",
				get_error_str(ret).c_str(), ret);

		if ((ret = PrlResult_GetParamsCount(hResult.get_handle(), &resultCount)))
			return prl_err(ret, "PrlResult_GetParamsCount: %s [%d]",
				get_error_str(ret).c_str(), ret);

		f.open_list();
		for(PRL_UINT32 j = 0; j < resultCount; ++j)
		{
			PrlHandle hPluginInfo;
			std::stringstream fmt;

			f.tbl_row_open();
			if ((ret = PrlResult_GetParamByIndex(hResult.get_handle(), j, hPluginInfo.get_ptr())))
				return prl_err(ret, "PrlResult_GetParamByIndex: %s [%d]",
							get_error_str(ret).c_str(), ret);

			char buf[4096];

			PRL_UINT32 len = sizeof(buf)-1; buf[0] = '\0';
			if ( (ret = PrlPluginInfo_GetId(hPluginInfo.get_handle(), buf, &len))
				&& ret != PRL_ERR_NO_DATA )
				return prl_err(ret, "PrlPluginInfo_GetId: %s [%d]",
							get_error_str(ret).c_str(), ret);
			f.tbl_add_uuid("Plugin ID", "%s ", buf);

			len = sizeof(buf)-1; buf[0] = '\0';
			if ( (ret = PrlPluginInfo_GetVersion(hPluginInfo.get_handle(), buf, &len))
				&& ret != PRL_ERR_NO_DATA )
				return prl_err(ret, "PrlPluginInfo_GetVersion: %s [%d]",
							get_error_str(ret).c_str(), ret);
			fmt << "%-" << hdr_version.size() - 1 << "s ";
			f.tbl_add_item("Version", fmt.str().c_str(), buf);

			len = sizeof(buf)-1; buf[0] = '\0';
			if ( (ret = PrlPluginInfo_GetVendor(hPluginInfo.get_handle(), buf, &len))
				&& ret != PRL_ERR_NO_DATA )
				return prl_err(ret, "PrlPluginInfo_GetVendor: %s [%d]",
							get_error_str(ret).c_str(), ret);

			fmt.str(std::string());
			fmt << "\"%-" << hdr_vendor.size() - 3 << "s\" ";
			f.tbl_add_item("Vendor", fmt.str().c_str(), buf);

			len = sizeof(buf)-1; buf[0] = '\0';
			if ( (ret = PrlPluginInfo_GetShortDescription(hPluginInfo.get_handle(), buf, &len))
				&& ret != PRL_ERR_NO_DATA )
				return prl_err(ret, "PrlPluginInfo_GetShortDescription: %s [%d]",
							get_error_str(ret).c_str(), ret);
			f.tbl_add_item("Description", "\"%s\"", buf);

			f.tbl_row_close();
		}

		f.close_list();
		fprintf(stdout, "%s", f.get_buffer().c_str());
	}
	else if (plugin_param.cmd == PluginParam::Refresh)
	{
		std::string err;

		PrlHandle hJob(PrlSrv_RefreshPlugins( m_hSrv, 0 ));
		if ((ret = get_job_retcode(hJob.get_handle(), err)))
			return prl_err(ret, "Failed to refresh installed plugins: %s", err.c_str());

		prl_log(0, "Refresh installed plugins was done successfully.");
	}

	return ret;
}

static int server_event_handler_monitor(PRL_HANDLE hEvent, void *data)
{
	PrlHandle h(hEvent);
	PRL_HANDLE_TYPE type;
	int ret;
	PRL_CHAR buf[256];
	PRL_UINT32 buflen = sizeof(buf);
	PrlSrv *srv = (reinterpret_cast<PrlSrv *>(data));

	if ((ret = PrlHandle_GetType(h.get_handle(), &type))) {
		prl_log(L_ERR, "PrlHandle_GetType: %s",
				get_error_str(ret).c_str());
		return ret;
	}

	if (type != PHT_EVENT)
		return 0;

	PRL_EVENT_TYPE evt_type;

	if ((ret = PrlEvent_GetType(h.get_handle(), &evt_type))) {
		prl_log(L_DEBUG, "PrlEvent_GetType: %s",
				get_error_str(ret).c_str());
		return ret;
	}

	if (evt_type == PET_DSP_EVT_VM_STATE_CHANGED ||
			evt_type == PET_DSP_EVT_VM_CONFIG_CHANGED ||
			evt_type == PET_DSP_EVT_VM_CREATED ||
			evt_type == PET_DSP_EVT_VM_ADDED) {

		PrlVm *vm;
		PrlOutFormatter &f = *(get_formatter(true));

		PrlEvent_GetIssuerId(hEvent, buf, &buflen);
		srv->get_vm_config(std::string(buf), &vm, false);

		f.open_object();

		switch (evt_type) {
		case PET_DSP_EVT_VM_STATE_CHANGED:
			f.add("event_type", "VM_STATE_CHANGED");
			break;
		case PET_DSP_EVT_VM_CONFIG_CHANGED:
			f.add("event_type", "VM_CONFIG_CHANGED");
			break;
		case PET_DSP_EVT_VM_CREATED:
			f.add("event_type", "VM_CREATED");
			break;
		case PET_DSP_EVT_VM_ADDED:
			f.add("event_type", "VM_ADDED");
			break;
		default:
			f.add("event_type", "UNKNOWN");
		}

		f.open("vm_info");
		vm->append_configuration(f);
		f.close();
		f.close_object();

		fputs(f.get_buffer().c_str(), stdout);
		fflush(stdout);
	} else if (evt_type == PET_DSP_EVT_VM_DELETED) {
		PrlOutFormatter &f = *(get_formatter(true));

		PrlEvent_GetIssuerId(hEvent, buf, &buflen);

		f.open_object();
		f.add("event_type", "VM_DELETED");
		f.open("vm_info");
		f.add_uuid("ID", buf);
		f.close();
		f.close_object();

		fputs(f.get_buffer().c_str(), stdout);
		fflush(stdout);

	}

	return 0;
}

int PrlSrv::run_monitor(void)
{
	char c;

	reg_event_callback(server_event_handler_monitor, this);

	/* wait until stdin is closed */
	while (fread(&c, 1, 1, stdin));

	return 0;
}

int PrlSrv::set_user(const CmdParamData &param) {
	if (param.user.def_vm_home.empty())
		return 0;

	PrlHandle j1 = PrlSrv_UserProfileBeginEdit(m_hSrv);
	PRL_RESULT result;
	std::string e;
	result = get_job_retcode(j1.get_handle(), e);
	if (0 != result)
		return prl_err(result, "PrlSrv_UserProfileBeginEdit: %s", e.c_str());

	PrlHandle j2 = PrlSrv_GetUserProfile(m_hSrv), r;
	PRL_UINT32 n = 0;
	result = get_job_result(j2.get_handle(), r.get_ptr(), &n);
	if (0 != result)
		return prl_err(result, "PrlSrv_GetUserProfile: %s",
			get_error_str(result).c_str());

	PrlHandle p;
	result = PrlResult_GetParam(r.get_handle(), p.get_ptr());
	if (0 != result)
		return prl_err(result, "PrlResult_GetParam: %s",
			get_error_str(result).c_str());

	result = PrlUsrCfg_SetDefaultVmFolder(p.get_handle(),
				param.user.def_vm_home.c_str());
	if (0 != result)
		return prl_err(result, "PrlUsrCfg_SetDefaultVmFolder: %s",
			get_error_str(result).c_str());

	PrlHandle j3 = PrlSrv_UserProfileCommit(m_hSrv, p.get_handle());
	result = get_job_retcode(j3.get_handle(), e);
	if (0 != result)
		return prl_err(result, "PrlSrv_UserProfileCommit: %s", e.c_str());

	prl_log(0, "A default virtual machine folder for the user was updated successfully.");
	return 0;
}

unsigned int PrlSrv::get_min_security_level()
{
	if (m_disp == NULL || m_disp->update_info() != 0)
		return 0;

	return m_disp->m_min_security_level;
}
