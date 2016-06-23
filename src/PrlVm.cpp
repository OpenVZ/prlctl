///////////////////////////////////////////////////////////////////////////////
///
/// @file PrlVm.cpp
///
/// VM management
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifndef _WIN_
#include <termios.h>
#include <sys/ioctl.h>
#endif
#include <fstream>
#include <sstream>
#include <memory>
#include <vector>
#include <iostream>
#include <utility>
#include <map>

#include <PrlApiDeprecated.h>
#include <PrlApiDisp.h>
#include <PrlApiNet.h>
#include <PrlOses.h>
#include <boost/foreach.hpp>

#include "EventSyncObject.h"
//#include "Interfaces/ParallelsDomModel.h"
#define EVT_PARAM_VM_EXEC_APP_RET_CODE		"vm_exec_app_ret_code"

#include "CmdParam.h"
#include "PrlSrv.h"
#include "PrlVm.h"
#include "PrlDev.h"
#include "Utils.h"
#include "Logger.h"
#include "PrlCleanup.h"
#include "PrlSnapshot.h"
#include "PrlSharedFolder.h"
#include "PrlOutFormatter.h"

#ifndef _WIN_
#define STDIN_FILE_DESC fileno(stdin)
#define STDOUT_FILE_DESC fileno(stdout)
#define STDERR_FILE_DESC fileno(stderr)
#else
#define STDIN_FILE_DESC  GetStdHandle(STD_INPUT_HANDLE)
#define STDOUT_FILE_DESC GetStdHandle(STD_OUTPUT_HANDLE)
#define STDERR_FILE_DESC GetStdHandle(STD_ERROR_HANDLE)
#endif

#ifndef _WIN_
#include <unistd.h>
#endif

#ifdef _LIN_
#include <sys/types.h>
#include <sys/wait.h>
#endif

PrlVm::PrlVm(PrlSrv &srv, PRL_HANDLE hVm, const std::string &uuid,
		const std::string &name, unsigned int ostype):
	m_srv(srv), m_hVm(hVm), m_uuid(uuid), m_name(name),
	m_ostype(ostype),
	m_template(false),
	m_VmState(VMS_UNKNOWN),
	m_updated(PRL_FALSE),
	m_is_vnc_server_started(PRL_FALSE),
	m_efi_boot(false),
	m_select_boot_dev(false)
{
	char buf[1024];
	unsigned int len;

	if (!uuid.empty()) {
	        PrlVmCfg_SetUuid(m_hVm, uuid.c_str());
	} else {
		len = sizeof(buf);
		PrlVmCfg_GetUuid(m_hVm, buf, &len);
		m_uuid = buf;
	}

	PRL_BOOL bTemplate = PRL_FALSE;
	if (PrlVmCfg_IsTemplate(m_hVm, &bTemplate) == 0)
		m_template = !!bTemplate;

	len = sizeof(buf);
	if (PrlVmCfg_GetCtId(m_hVm, buf, &len) == 0)
		m_ctid = buf;

	PRL_BOOL bEfiEnabled = PRL_FALSE;
	if (PrlVmCfg_IsEfiEnabled(m_hVm, &bEfiEnabled) == 0)
		m_efi_boot = !!bEfiEnabled;

	PRL_BOOL bSelAllow = PRL_FALSE;
	if (PrlVmCfg_IsAllowSelectBootDevice(m_hVm, &bSelAllow) == 0)
		m_select_boot_dev = !!bSelAllow;

	len = sizeof(buf);
	if (PrlVmCfg_GetExternalBootDevice(m_hVm, buf, &len) == 0)
		m_ext_boot_dev = buf;
}

const char *PrlVm::get_vm_type_str() const
{
	return (get_vm_type() == PVT_VM) ? "VM" : "CT";
}

PRL_VM_TYPE PrlVm::get_vm_type() const
{
	PRL_VM_TYPE type = PVT_VM;
	int ret;

	if ((ret = PrlVmCfg_GetVmType(m_hVm, &type)))
		prl_log(L_ERR, "PrlVmCfg_GetVmType: %s",
				get_error_str(ret).c_str());


	return type;
}

PrlDev *PrlVm::new_dev(PRL_HANDLE hDev, DevType type, unsigned int idx)
{
	PrlDev *dev = 0;
	PrlDevNet *netDev = 0;

	switch (type) {
	case DEV_HDD:
	case DEV_HDD_PARTITION:
		dev = new PrlDevHdd(*this, hDev, type, idx);
		break;
	case DEV_CDROM:
		dev = new PrlDevCdrom(*this, hDev, type, idx);
		break;
	case DEV_NET:
		netDev = new PrlDevNet(*this, hDev, type, idx);
		dev = netDev;
		m_DevNetList.push_back(netDev);
		break;
	case DEV_FDD:
		dev = new PrlDevFdd(*this, hDev, type, idx);
		break;
	case DEV_USB:
		dev = new PrlDevUsb(*this, hDev, type, idx);
		break;
	case DEV_SERIAL:
		dev = new PrlDevSerial(*this, hDev, type, idx);
		break;
	case DEV_PARALLEL:
		dev = new PrlDevParallel(*this, hDev, type, idx);
		break;
	case DEV_SOUND:
		dev = new PrlDevSound(*this, hDev, type, idx);
		break;
	case DEV_GENERIC_PCI:
		dev = new PrlDevGenericPci(*this, hDev, type, idx);
		break;
	case DEV_NONE:
		prl_log(L_DEBUG, "Unknown device type");
		return 0;
	}

	prl_log(L_DEBUG, "PrlVm::new_dev: %s", dev->get_id().c_str());
	return dev;
}

int PrlVm::start(int flags)
{
	PRL_RESULT ret;

	prl_log(0, "Starting the %s...", get_vm_type_str());

	std::string err;

	PrlHandle hJob(PrlVm_StartEx(m_hVm, PSM_VM_START, flags));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to start the %s: %s",
				get_vm_type_str(), err.c_str());
	else
		prl_log(0, "The %s has been successfully started.",
				 get_vm_type_str());

	return ret;
}

int PrlVm::mount_info()
{
	PRL_RESULT ret;
	PRL_UINT32 resultCount;

	PrlHandle hJob(PrlVm_Mount(m_hVm, NULL, PMVD_INFO));
	PrlHandle hResult;
	if ((ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &resultCount)))
		return ret;

	std::string res;
	ret = get_result_as_string(hResult, res);
	if (ret == 0)
		printf("%s", res.c_str());

	return 0;
}

int PrlVm::mount(int flags)
{
	PRL_RESULT ret;

	if (flags & PMVD_INFO)
		return mount_info();

	prl_log(0, "Mounting the %s...", get_vm_type_str());

	std::string err;

	PrlHandle hJob(PrlVm_Mount(m_hVm, NULL, flags));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to mount the %s: %s",
				get_vm_type_str(), err.c_str());
	else
		prl_log(0, "The %s has been successfully mounted.",
				get_vm_type_str());

	return ret;
}

int PrlVm::umount()
{
	PRL_RESULT ret;

	prl_log(0, "Unmounting the %s...", get_vm_type_str());

	std::string err;

	PrlHandle hJob(PrlVm_Umount(m_hVm, 0));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to unmount the %s: %s",
				get_vm_type_str(), err.c_str());
	else
		prl_log(0, "The %s has been successfully unmounted.",
				get_vm_type_str());

	return ret;
}

int PrlVm::change_sid()
{
	PRL_RESULT ret;

	prl_log(0, "Performing change SID operation to the %s...",
			get_vm_type_str());

	std::string err;

	PrlHandle hJob(PrlVm_ChangeSid(m_hVm, 0));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to change SID of the %s: %s",
				get_vm_type_str(), err.c_str());
	else
		prl_log(0, "The %s SID has been successfully changed.",
				get_vm_type_str());

	return ret;
}

int PrlVm::reset_uptime()
{
	PRL_RESULT ret;

	prl_log(0, "Performing reset uptime operation to the %s...",
			get_vm_type_str());

	std::string err;

	PrlHandle hJob(PrlVm_ResetUptime(m_hVm, 0));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to reset uptime of the %s: %s",
				get_vm_type_str(), err.c_str());
	else
		prl_log(0, "The %s uptime has been successfully reset.",
				get_vm_type_str());

	return ret;
}

int PrlVm::auth(const std::string &user_name, const std::string &user_password)
{
	PRL_RESULT ret;

	prl_log(0, "Authenticating user with %s security database...",
			get_vm_type_str());

	std::string err;

	PrlHandle hJob(PrlVm_AuthWithGuestSecurityDb(m_hVm, user_name.c_str(), user_password.c_str(), 0));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to authenticate user with %s security database: %s",
				get_vm_type_str(), err.c_str());
	else
		prl_log(0, "The user has been successfully authenticated.");

	return ret;
}

#define VM_SHUTDOWN_TIMEOUT 120 * 1000
int PrlVm::stop(const CmdParamData &param)
{
	PRL_RESULT ret;

	prl_log(0, "Stopping the %s...", get_vm_type_str());

	std::string err;

	PRL_UINT32 nStopMode = PSM_SHUTDOWN;
	PRL_UINT32 nFlags = 0;
	if (param.fast)
		nStopMode = PSM_KILL;
	else if (param.use_acpi)
		nStopMode = PSM_ACPI;

	if (param.noforce)
		nFlags = PSF_NOFORCE;
	else if (param.force)
		nFlags = PSF_FORCE;

	PrlHandle hJob(PrlVm_StopEx(m_hVm, nStopMode, nFlags));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "Failed to stop the %s: %s",
				get_vm_type_str(), err.c_str());
	if (param.fast) {
		prl_log(0, "The %s has been forcibly stopped",
				get_vm_type_str());
	} else {
		prl_log(0, "The %s has been successfully stopped.",
				get_vm_type_str());
	}

	return ret;
}

int PrlVm::reset()
{
	PRL_RESULT ret;

	prl_log(0, "Resetting the %s...", get_vm_type_str());

	std::string err;

	PrlHandle hJob(PrlVm_Reset(m_hVm));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to reset the %s: %s",
				get_vm_type_str(), err.c_str());
	else
		prl_log(0, "The %s has been successfully reset.",
				get_vm_type_str());

	return ret;
}

int PrlVm::restart()
{
	PRL_RESULT ret;

	prl_log(0, "Restarting the %s...", get_vm_type_str());

	std::string err;

	PrlHandle hJob(PrlVm_Restart(m_hVm));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to restart the %s: %s",
				get_vm_type_str(), err.c_str());
	else
		prl_log(0, "The %s has been successfully restarted.",
				get_vm_type_str());

	return ret;
}

int PrlVm::suspend()
{
	PRL_RESULT ret;

	prl_log(0, "Suspending the %s...", get_vm_type_str());

	std::string err;

	PrlHandle hJob(PrlVm_Suspend(m_hVm));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to suspend the %s: %s",
				get_vm_type_str(), err.c_str());
	else
		prl_log(0, "The %s has been successfully suspended.",
				get_vm_type_str());

	return ret;
}

int PrlVm::resume()
{
	PRL_RESULT ret;

	prl_log(0, "Resuming the %s...", get_vm_type_str());

	std::string err;

	PrlHandle hJob(PrlVm_Resume(m_hVm));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to resume the %s: %s",
				get_vm_type_str(), err.c_str());
	else
		prl_log(0, "The %s has been successfully resumed.",
				get_vm_type_str());

	return ret;
}

int PrlVm::pause(bool acpi)
{
	PRL_RESULT ret;

	prl_log(0, "Pause the %s...", get_vm_type_str());

	std::string err;

	PrlHandle hJob(PrlVm_Pause(m_hVm, acpi));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to pause the %s: %s",
				get_vm_type_str(), err.c_str());
	else
		prl_log(0, "The %s has been successfully paused.",
				get_vm_type_str());

	return ret;
}

static int migrate_event_handler(PRL_HANDLE hEvent, void *data)
{
	(void)data;
	PrlHandle h(hEvent);
	PRL_HANDLE_TYPE type;
	int ret;

	data = 0;
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
		prl_log(L_DEBUG, "EVENT type=%d", evt_type);
		if (evt_type == PET_DSP_EVT_VM_MIGRATE_PROGRESS_CHANGED) {
			print_progress(h.get_handle());
		} else if (evt_type == PET_DSP_EVT_VM_MIGRATE_STARTED) {
			prl_log(L_INFO, "Migration started.");
		} else if (evt_type == PET_DSP_EVT_VM_MIGRATE_FINISHED) {
			prl_log(L_INFO, "Migration finished.");
		} else if (evt_type == PET_DSP_EVT_VM_MIGRATE_CANCELLED) {
			prl_log(L_INFO, "Migration cancelled!");
		} else if (evt_type == PET_DSP_EVT_VM_MESSAGE) {
			std::string m;
			get_result_error_string(h.get_handle(), m);
			prl_log(L_ERR, m.c_str());
		} else if (evt_type == PET_JOB_STAGE_PROGRESS_CHANGED) {
			print_vz_progress(h);
		}
	}
	return 0;
}

int PrlVm::reg_event_callback(PRL_EVENT_HANDLER_PTR fn, void *data)
{
	int ret;

	ret = PrlVm_RegEventHandler(get_handle(), fn, data);
	if (PRL_FAILED(ret))
		return prl_err(ret, "PrlSrv_RegEventHandler: %s",
				get_error_str(ret).c_str());
	return 0;
}

void PrlVm::unreg_event_callback(PRL_EVENT_HANDLER_PTR fn, void *data)
{
	PrlVm_UnregEventHandler(get_handle(), fn, data);
}

namespace Snapshot {
	template<class T>
	struct Proxy {
		Proxy(PRL_HANDLE _vm, T& _handler): m_vm(_vm), m_priv_data(&_handler) {
			int ret = PrlVm_RegEventHandler(m_vm, &callback, &_handler);
			if (PRL_FAILED(ret))
				prl_err(ret, "PrlSrv_RegEventHandler: %s",
						get_error_str(ret).c_str());
		}
		~Proxy() {
			PrlVm_UnregEventHandler(m_vm, &callback, m_priv_data);
		}
	private:
		static int callback(PRL_HANDLE hEvent, void *data) {
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
				static_cast<T*>(data)->handle(evt_type, hEvent);
			}
			return 0;
		}

		PRL_HANDLE m_vm;
		void *m_priv_data;
	};
	struct Handler {
		Handler(VIRTUAL_MACHINE_STATE _state, CEventSyncObject& _event):
				m_wait(VMS_RUNNING == _state), m_event(&_event) {
		}
		void handle(PRL_EVENT_TYPE _type, PRL_HANDLE _event) {
			switch (_type) {
			case PET_DSP_EVT_VM_STARTED:
				m_wait = true;
				break;
			case PET_DSP_EVT_VM_MEMORY_SWAPPING_STARTED:
				prl_log(L_INFO, "\tmemory swapping started");
				break;
			case PET_DSP_EVT_VM_MEMORY_SWAPPING_FINISHED:
				prl_log(L_INFO, "\tmemory swapping finished");
				m_event->Signal();
				break;
			case PET_DSP_EVT_VM_QUESTION: {
					PRL_BOOL answer = PRL_FALSE;
					PrlEvent_IsAnswerRequired(_event, &answer);
					if (answer) {
						prl_log(0, "Question event has arrived, skip waiting.");
						m_event->Signal();
					}
				}
				break;
			default:
				break;
			}
		}
		bool isToWait() const {
			return m_wait;
		}
	private:
		bool m_wait;
		CEventSyncObject* m_event;
	};
	struct Event {
		Event(PRL_HANDLE _vm, VIRTUAL_MACHINE_STATE _state):
			m_handler(_state, m_event), m_proxy(_vm, m_handler) {
		}
		int wait() {
			if (m_handler.isToWait() && !m_event.Wait(g_nJobTimeout))
				return prl_err(-1, "timeout expired");

			return 0;
		}
	private:
		CEventSyncObject m_event;
		Handler m_handler;
		Proxy<Handler> m_proxy;
	};
};

int PrlVm::snapshot_create(const SnapshotParam &param)
{
	PRL_RESULT ret;
	std::string err;

	prl_log(0, "Creating the snapshot...");
	std::auto_ptr<Snapshot::Event> e;
	if (param.wait && (get_vm_type() == PVT_VM))
		e.reset(new Snapshot::Event(m_hVm, m_VmState));

	std::string sid;
	PRL_UINT32 resultCount = 0;
	PrlHandle hResult;

	PrlHandle hJob(PrlVm_CreateSnapshot(m_hVm, param.name.c_str(),
							param.desc.c_str()));
	if ((ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &resultCount)))
		return prl_err(-1, "Failed to create the snapshot: %s",
			get_error_str(ret).c_str());
	get_result_as_string(hResult.get_handle(), sid);

	if (NULL != e.get()) {
		int rc = e->wait();
		if (0 != rc)
			return rc;
	}
	prl_log(0, "The snapshot with id %s has been successfully created.",
			sid.c_str());

	return 0;
}

int PrlVm::snapshot_switch_to(const SnapshotParam &param)
{
	PRL_RESULT ret;
	const std::string &id = param.id;
	std::string err;
	PRL_UINT32 nFlags = param.skip_resume ? PSSF_SKIP_RESUME : 0;

	if (id.empty())
		return prl_err(-1, "Snapshot id is not specified.");

	prl_log(0, "Switch to the snapshot...");
	std::auto_ptr<Snapshot::Event> e;
	if (param.wait && (get_vm_type() == PVT_VM))
		e.reset(new Snapshot::Event(m_hVm, m_VmState));

	PrlHandle hJob(PrlVm_SwitchToSnapshotEx(m_hVm, id.c_str(), nFlags));
	if ((ret = get_job_retcode(hJob.get_handle(), err))) {
		return prl_err(ret, "Failed to switch to snapshot: %s", err.c_str());
	}

	if (NULL != e.get()) {
		int rc = e->wait();
		if (0 != rc)
			return rc;
	}

	prl_log(0, "The %s has been successfully switched.",
			get_vm_type_str());

	return ret;
}

int PrlVm::snapshot_delete(const SnapshotParam &param)
{
	PRL_RESULT ret;
	const std::string &id = param.id;

	if (id.empty())
		return prl_err(-1, "Snapshot id is not specified.");
	std::string err;

	prl_log(0, "Delete the snapshot...");
	PrlHandle hJob(PrlVm_DeleteSnapshot(m_hVm, id.c_str(),
		param.del_with_children ? PRL_TRUE : PRL_FALSE));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to delete snapshot: %s", err.c_str());
	else
		prl_log(0, "The snapshot has been successfully deleted.");

	return ret;
}

int PrlVm::snapshot_get_tree(std::string &out)
{
	PRL_RESULT ret;
	PRL_UINT32 resultCount = 0;

	std::string err;
	PrlHandle hResult;

	PrlHandle hJob(PrlVm_GetSnapshotsTreeEx(m_hVm, PGST_WITHOUT_SCREENSHOTS));
	if ((ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &resultCount)))
		return prl_err(-1, "Failed to get the snapshot tree: %s",
			get_error_str(ret).c_str());

	if ((ret = get_result_as_string(hResult.get_handle(), out)))
		return ret;

	prl_log(L_DEBUG, "\n\n%s\n\n", out.c_str());

	return 0;
}

int PrlVm::snapshot_list(const CmdParamData &param)
{
	int ret;
	std::string buf;

	if ((ret = snapshot_get_tree(buf)))
		return ret;

	PrlSnapshotTree tree;

	tree.parse(buf.c_str());

	if (param.snapshot.tree)
		tree.print_tree();
	else if (!param.snapshot.id.empty())
		tree.print_info(param.snapshot.id);
	else
		tree.print_list(param.list_no_hdr);

	return 0;
}

int PrlVm::problem_report(const CmdParamData &param)
{
	PRL_RESULT ret;
	PRL_UINT32 resultCount = 0;
	PrlHandle hResult, hProblemReport;

	PrlHandle hJob(PrlVm_GetPackedProblemReport(m_hVm, 0));
	ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &resultCount);

	if (PRL_ERR_UNRECOGNIZED_REQUEST == ret)//Seems remote server has old scheme
	{
		PrlHandle hJob(PrlVm_GetProblemReport(m_hVm));
		if ((ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &resultCount)) == 0)
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
//https://bugzilla.sw.ru/show_bug.cgi?id=481561
#ifdef EXTERNALLY_AVAILABLE_BUILD
	if ((ret = assembly_problem_report(hProblemReport, param.problem_report, 0)) != 0)
#else
	if ((ret = assembly_problem_report(hProblemReport, param.problem_report, 0)) != 0)
#endif
		return prl_err(-1, "Failed to assembly problem report: %s",
			get_error_str(ret).c_str());

	if (param.problem_report.send)
		ret = send_problem_report(hProblemReport, param.problem_report);
	else
		ret = send_problem_report_on_stdout(hProblemReport);

	return ret;
}

static int run_vzctl(const std::string &ctid, const char *command)
{
#if _LIN_
	char buf[64];
	char *args[4];

	args[0] = (char *) "/usr/sbin/vzctl";
	args[1] = (char *) command;
	sprintf(buf, "%s", ctid.c_str());
	args[2] = buf;
	args[3] = NULL;

	execv(args[0], args);
	return prl_err(-1, "failed to call %s %s %s",
			args[0], args[1], args[2]);
#else
	(void) id;
	(void) command;

	return prl_err(-1, "Unimplemented");
#endif
}

int PrlVm::console()
{
	if (get_vm_type() == PVT_CT)
		return run_vzctl(get_ctid(), "console");
	return prl_err(-1, "Unimplemented");
}

#ifdef _LIN_
static struct termios saved_ts;

static void term_handler(int sig)
{
	tcsetattr(0, TCSAFLUSH, &saved_ts);
}
#endif

int PrlVm::exec(const CmdParamData &param)
{
	char **argv = param.argv;
	Action action = param.action;
	PRL_RESULT ret;
	std::string err;
	const char enter_cmd[] = "enter";

	if (action == VmExecAction && (!argv || !argv[0]))
		return prl_err(-1, "Incorrect exec command");

	if (get_vm_type() == PVT_CT && action == VmEnterAction)
		return run_vzctl(get_ctid(), "enter");

	PrlHandle hLoginJob(PrlVm_LoginInGuest(m_hVm, PRL_PRIVILEGED_GUEST_OS_SESSION, 0, 0));
	const PrlHook *hLoginCleanupHook =
		get_cleanup_ctx().register_hook(login_cancel_job, hLoginJob.get_handle());
	if ((ret = get_job_retcode(hLoginJob.get_handle(), err)))
		return prl_err(ret, "%s", err.c_str());

	get_cleanup_ctx().unregister_hook(hLoginCleanupHook);

	PrlHandle hResult;
	if ((ret = PrlJob_GetResult(hLoginJob.get_handle(), hResult.get_ptr())))
		return prl_err(ret, "PrlJob_GetResult: %s",  get_error_str(ret).c_str());

	PrlHandle hVmGuest;
	if ((ret = PrlResult_GetParam(hResult.get_handle(), hVmGuest.get_ptr())))
		return prl_err(ret, "PrlResult_GetParam: %s",  get_error_str(ret).c_str());
	const PrlHook *hSessionCleanupHook =
		get_cleanup_ctx().register_hook(cancel_session, hVmGuest.get_handle());
	PrlHandle hIoJob(PrlVm_Connect(m_hVm,
				PDCT_LOW_QUALITY_WITHOUT_COMPRESSION));
	if ((ret = get_job_retcode(hIoJob.get_handle(), err, JOB_WAIT_TIMEOUT)))
		return prl_err(ret, "PrlVm_Connect: %s", err.c_str());
	PrlHandle hArgs;
	PrlApi_CreateStringsList(hArgs.get_ptr());
	if (action == VmEnterAction)
		PrlStrList_AddItem(hArgs.get_handle(), enter_cmd);
	else
		for (char **p = argv + 1; *p; p++)
			PrlStrList_AddItem(hArgs.get_handle(), *p);

	PrlHandle hEnvs;
	PrlApi_CreateStringsList(hEnvs.get_ptr());
#ifndef _WIN_
	if (action == VmEnterAction &&
	    m_ostype == PVS_GUEST_TYPE_LINUX) {
		const char *envs_enter[] = {"HOME=/",
			"HISTFILE=/dev/null",
			"PATH=/bin:/sbin:/usr/bin:/usr/sbin:.",
			"SHELL=/bin/bash",
		};
		const char *envs[] = { "TERM" };
		char buf[64];
		char *val;
		size_t i;
		for (i = 0; i < sizeof(envs_enter)/sizeof(*envs_enter);  i++)
			PrlStrList_AddItem(hEnvs.get_handle(), envs_enter[i]);
		buf[sizeof(buf)-1] = 0;
		for (i = 0; i < sizeof(envs)/sizeof(*envs); i++) {
			if ((val = getenv(envs[i])) != NULL) {
				snprintf(buf, sizeof(buf)-1, "%s=%s", envs[i], val);
				PrlStrList_AddItem(hEnvs.get_handle(), buf);
			}
		}
		struct winsize winsz;
		if (ioctl(0, TIOCGWINSZ, &winsz) == 0) {
			snprintf(buf, sizeof(buf)-1, "LINES=%d", winsz.ws_row);
			PrlStrList_AddItem(hEnvs.get_handle(), buf);
			snprintf(buf, sizeof(buf)-1, "COLUMNS=%d", winsz.ws_col);
			PrlStrList_AddItem(hEnvs.get_handle(), buf);
		}
	}
#endif
#ifdef _LIN_
	bool termiosFlag = false;
#endif
	do {
		PRL_UINT32 nFlags;
		const char *cmd;

		if (action == VmEnterAction) {
#ifdef _LIN_		
			if (m_ostype == PVS_GUEST_TYPE_LINUX) {
				struct termios ts;
				struct sigaction sa = { 0 };

				sa.sa_handler = term_handler;

				tcgetattr(0, &ts);

				sigaction(SIGINT, &sa, NULL);
				sigaction(SIGHUP, &sa, NULL);
				sigaction(SIGTERM, &sa, NULL);
				sigaction(SIGQUIT, &sa, NULL);

				memcpy(&saved_ts, &ts, sizeof(ts));
				cfmakeraw(&ts);
				ts.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL);
				tcsetattr(0, TCSAFLUSH, &ts);

				termiosFlag = true;
			}
#endif
			nFlags = PFD_ALL | PRPM_RUN_PROGRAM_ENTER;
			cmd = enter_cmd;
		} else {
			nFlags = PFD_ALL;
			if (param.exec_in_shell)
				nFlags |= PRPM_RUN_PROGRAM_IN_SHELL;
			cmd = argv[0];
		}
		PrlHandle hExecJob(PrlVmGuest_RunProgram(hVmGuest.get_handle(),
					cmd, hArgs.get_handle(),
					hEnvs.get_handle(), nFlags,
								STDIN_FILE_DESC,
								STDOUT_FILE_DESC,
								STDERR_FILE_DESC));

		get_cleanup_ctx().register_hook(cancel_job, hExecJob.get_handle());
		if ((ret = get_job_retcode(hExecJob.get_handle(), err))) {
			handle_job_err(hExecJob, ret);
			prl_err(ret, "PrlVmGuest_RunProgram: %s", err.c_str());
			break;
		}

		if ((ret = PrlJob_GetResult(hExecJob.get_handle(), hResult.get_ptr()))) {
			prl_err(ret, "PrlJob_GetResult: %s",  get_error_str(ret).c_str());
			break;
		}

		PRL_UINT32 nCount = 0;
		if ((ret = PrlResult_GetParamsCount(hResult.get_handle(), &nCount))) {
			prl_err(ret, "PrlResult_GetParamsCount: %s", get_error_str(ret).c_str());
			break;
		}

		if (nCount > 0) {
			PrlHandle hEvent;
			if ((ret = PrlResult_GetParamByIndex(hResult.get_handle(), 0, hEvent.get_ptr()))) {
				prl_err(ret, "PrlResult_GetParamByIndex: %s",  get_error_str(ret).c_str());
				break;
			}

			PrlHandle hRetCode;
			if ((ret = PrlEvent_GetParamByName(hEvent.get_handle(), EVT_PARAM_VM_EXEC_APP_RET_CODE, hRetCode.get_ptr()))) {
				prl_err(ret, "PrlEvent_GetParamByName (EVT_PARAM_VM_EXEC_APP_RET_CODE): %s",  get_error_str(ret).c_str());
				break;
			}

			PRL_UINT32 retcode;
			if ((ret = PrlEvtPrm_ToUint32(hRetCode.get_handle(), &retcode))) {
				prl_err(ret, "PrlEvtPrm_ToUint32: %s",  get_error_str(ret).c_str());
				break;
			}

			ret = retcode;
		}
	} while (0);

#ifdef _LIN_
	if (termiosFlag) {
		struct sigaction sa = { 0 };

		sa.sa_handler = SIG_DFL;

		tcsetattr(0, TCSAFLUSH, &saved_ts);

		sigaction(SIGINT, &sa, NULL);
		sigaction(SIGHUP, &sa, NULL);
		sigaction(SIGTERM, &sa, NULL);
		sigaction(SIGQUIT, &sa, NULL);
	}
#endif

	get_cleanup_ctx().unregister_hook(hSessionCleanupHook);
	PrlVm_Disconnect(m_hVm);

	PrlHandle hJob(PrlVmGuest_Logout(hVmGuest.get_handle(), 0));
	get_job_retcode(hJob.get_handle(), err);

	return ret;
}

int PrlVm::set_userpasswd(const std::string &userpasswd, bool crypted)
{
	PRL_RESULT ret;
	std::string err;

	std::string::size_type pos = userpasswd.find_first_of(":");
	std::string user;
	std::string passwd;
	if (pos != std::string::npos) {
		user = userpasswd.substr(0, pos);
		passwd = userpasswd.substr(pos + 1);
	}
	PrlHandle h(PrlVm_SetUserPasswd(m_hVm,
				user.c_str(), passwd.c_str(),
				crypted ? PSPF_PASSWD_CRYPTED : 0));
	if ((ret = get_job_retcode(h.get_handle(), err)))
		return prl_err(ret, "%s", err.c_str());

	if (ret == 0)
		prl_log(0, "Authentication tokens updated successfully.");

	return ret;
}

int PrlVm::update_state()
{
	PRL_RESULT ret, err;

	m_VmState = VMS_UNKNOWN;
	ret = PrlVmCfg_GetConfigValidity(m_hVm, &err);
	if (PRL_FAILED(ret))
		return prl_err(ret, "PrlResult_GetParamByIndex: %s",
				get_error_str(ret).c_str());

	PRL_BOOL bVal;
	PrlVmCfg_IsConfigInvalid(err, &bVal);
	if (bVal == PRL_TRUE)
		return 0;

	PrlHandle hVmInfo;
	if ((ret = PrlVmCfg_GetVmInfo(m_hVm, hVmInfo.get_ptr())))
		return prl_err(ret, "PrlVmCfg_GetVmInfo: %s",
			get_error_str(ret).c_str());
	if ((ret = PrlVmInfo_GetState(hVmInfo.get_handle(), &m_VmState)))
		prl_log(L_ERR, "PrlVmInfo_GetState: %s",
			get_error_str(ret).c_str());
	if ((ret = PrlVmInfo_IsVncServerStarted(hVmInfo.get_handle(), &m_is_vnc_server_started)))
		prl_log(L_ERR, "PrlVmInfo_IsVncServerStarted: %s",
				get_error_str(ret).c_str());
	update_owner(hVmInfo.get_handle());

	return ret;
}

int PrlVm::fixup_configuration()
{
	PRL_RESULT ret;
	PRL_UINT32 count;

	if ((ret = PrlVmCfg_GetDevsCount(m_hVm, &count)))
		return prl_err(ret, "PrlVmCfg_GetDevsCount: %s",
			get_error_str(ret).c_str());
	if (count == 0)
		return 0;

	PRL_HANDLE_PTR handles = (PRL_HANDLE_PTR) malloc(sizeof(PRL_HANDLE) *
								count);
	if (!(ret = PrlVmCfg_GetDevsList(m_hVm, handles, &count))) {
		for (unsigned int i = 0; i < count; ++i) {
			PRL_HANDLE_TYPE type = PHT_ERROR;

			PrlHandle_GetType(handles[i], &type);
			if (type == PHT_VIRTUAL_DEV_NET_ADAPTER) {
				/* generate uniq MAC */
				prl_log(L_DEBUG, "PrlVmDevNet_GenerateMacAddr");
				PrlVmDevNet_GenerateMacAddr(handles[i]);
			}
			PrlHandle_Free(handles[i]);
		}
	} else {
		prl_err(ret, "PrlVm_GetDevsList: %s",
				get_error_str(ret).c_str());
	}
	free(handles);

	return ret;
}

int PrlVm::load_def_configuration(const OsDistribution *dist)
{
	PRL_RESULT ret;

	if (!dist)
		return prl_err(-1, "The distribution is not specified.");
	prl_log(0, "Generate the %s configuration for %s.",
			get_vm_type_str(), get_dist_by_id(dist->ver));

	PrlHandle hSrvConf(m_srv.get_srv_config_handle());

	ret = PrlVmCfg_SetDefaultConfig(m_hVm, hSrvConf.get_handle(), dist->ver, true);
	if (ret)
		return prl_err(ret, "PrlVmCfg_SetDefaultConfig: %s",
			get_error_str(ret).c_str());
	return 0;
}

static int load_sample(const char *name, std::string &sample)
{
	std::string fname(name);
	std::ifstream ifs(name);


	/* Fixme: do not know how to check file existence in portable way */
	if (!ifs) {
#if defined (_WIN_)
		//Fixme: [TARGETDIR]/VM Samples/*.xml
		fname = "/VM Samples/";
#elif defined(_LIN_)
		fname =  "/usr/share/parallels-server-sdk/vmsamples/";
#elif defined(_MAC_)
		fname = PRL_DIRS_PARALLELS_SERVER_FRAMEWORK_DIR;
		fname += "/Versions/";
		fname += ui2string(VER_FULL_BUILD_NUMBER_RELEASE_MAJOR);
		fname += ".";
		fname += ui2string(VER_FULL_BUILD_NUMBER_RELEASE_MINOR);
		fname += "/Resources/VM Samples/";
#endif
		fname += name;
		ifs.open(fname.c_str(), std::ifstream::in);
		if (!ifs.good())
			return prl_err(-1, "Failed to open the %s sample.",
					fname.c_str());
	}
	return file2str(fname.c_str(), sample);
}

int PrlVm::load_configuration(const std::string &name)
{
	PRL_RESULT ret;
	std::string sample;

	prl_log(0, "Load the %s configuration.", get_vm_type_str());

	ret = load_sample(name.c_str(), sample);
	if (ret)
		return ret;

	if ((ret = PrlVm_FromString(m_hVm, sample.c_str())))
		return prl_err(ret, "Faile to load the %s configuration: %s",
				get_vm_type_str(), get_error_str(ret).c_str());
	/* Preserv original uuid & name */
	if ((ret = PrlVmCfg_SetUuid(m_hVm, get_uuid().c_str())))
		return prl_err(ret, "PrlVmCfg_SetUuid returned the following error: %s",
				get_error_str(ret).c_str());

	if ((ret = PrlVmCfg_SetName(m_hVm, get_name().c_str())))
		return prl_err(ret, "PrlVmCfg_SetName returned the following error: %s",
				get_error_str(ret).c_str());

	if ((ret = fixup_configuration()))
		return ret;
	set_updated();
	return 0;
}

int PrlVm::apply_configuration(const std::string &sample)
{
	int ret;

	if ((ret = PrlVmCfg_ApplyConfigSample(m_hVm, sample.c_str())))
		return prl_err(ret, "PrlVmCfg_ApplyConfigSample: %s",
				get_error_str(ret).c_str());
	set_updated();
	return 0;
}

static int progress_event_handler(PRL_HANDLE hEvent, void *)
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
		static std::string stage;
		PRL_EVENT_TYPE evt_type;

		if ((ret = PrlEvent_GetType(h.get_handle(), &evt_type))) {
			prl_log(L_DEBUG, "PrlEvent_GetType: %s",
					get_error_str(ret).c_str());
			return ret;
		}
		prl_log(L_DEBUG, "EVENT type=%d", evt_type);
		switch(evt_type)
		{
		case PET_JOB_FILE_COPY_PROGRESS_CHANGED:
			print_progress(h, stage);
			break;
		case PET_VM_INF_END_BUNCH_COPYING:
			print_procent(100, stage);
			stage.clear();
			break;
		case PET_VM_INF_START_BUNCH_COPYING:
		{
			std::string op = get_dev_from_handle( h );

			if( !op.empty() )
				stage = "Copying " + op;
			else
				stage.clear();

			break;
		}
		case PET_JOB_STAGE_PROGRESS_CHANGED:
			print_vz_progress(h);
			break;
		default:
			break;
		}
	}
	return 0;
}

int PrlVm::reg(const std::string &location, PRL_UINT32 nFlags)
{
	PRL_RESULT ret;

	prl_log(L_INFO, "Register the %s.",
			get_vm_type_str(), get_vm_type_str());

	std::string err;

	m_srv.reg_event_callback(progress_event_handler);

	nFlags |= PACF_NON_INTERACTIVE_MODE;/* Forcing non interactive mode on */
	PrlHandle hJob(PrlVm_RegEx(m_hVm, location.c_str(), nFlags));
	get_cleanup_ctx().register_hook(cancel_job, hJob.get_handle());
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to register the %s: %s",
				get_vm_type_str(), err.c_str());

	m_srv.unreg_event_callback(progress_event_handler);

	return ret;
}


int PrlVm::clone(const std::string &name, const std::string &uuid,
		const std::string &location, unsigned int flags)
{
	PRL_RESULT ret;

	if (name.empty())
		return prl_err(1, "Failed to clone the %s; the --name option is not"
			" specified.", get_vm_type_str());

	prl_log(0, "Clone the %s %s to %s %s...",
			get_name().c_str(),
			get_vm_type_str(),
			(flags & PCVF_CLONE_TO_TEMPLATE)? "template" : get_vm_type_str(),
			name.c_str());

	std::string err;

	m_srv.reg_event_callback(progress_event_handler);
	PrlHandle hJob(PrlVm_CloneWithUuid(m_hVm, name.c_str(), uuid.c_str(), location.c_str(), flags));
	const PrlHook *h = get_cleanup_ctx().register_hook(cancel_job, hJob.get_handle());
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "\nFailed to clone the %s: %s",
				get_vm_type_str(), err.c_str());
	else
		prl_log(0, "\nThe %s has been successfully cloned.", get_vm_type_str());
	m_srv.unreg_event_callback(progress_event_handler);
	get_cleanup_ctx().unregister_hook(h);

	return ret;
}

int PrlVm::move(const std::string &location)
{
	PRL_RESULT ret;

	if (location.empty())
		return prl_err(1, "Failed to move the %s; the --dst option is not"
			" specified.", get_vm_type_str());

	prl_log(0, "Move the %s %s to %s...",
			get_name().c_str(),
			get_vm_type_str(),
			location.c_str());

	std::string err;
	PRL_UINT32 flags = 0;

	m_srv.reg_event_callback(progress_event_handler);
	PrlHandle hJob(PrlVm_Move(m_hVm, location.c_str(), flags));
	const PrlHook *h = get_cleanup_ctx().register_hook(cancel_job, hJob.get_handle());
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to move the %s: %s",
				get_vm_type_str(), err.c_str());
	else
		prl_log(0, "\nThe %s has been successfully moved.", get_vm_type_str());
	m_srv.unreg_event_callback(progress_event_handler);
	get_cleanup_ctx().unregister_hook(h);

	return ret;
}

int PrlVm::unreg()
{
	PRL_RESULT ret;

	prl_log(L_INFO, "Unregister the %s.", get_vm_type_str());

	std::string err;

	PrlHandle hJob(PrlVm_Unreg(m_hVm));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to unregister the %s: %s",
				get_vm_type_str(), err.c_str());
	else
		prl_log(0, "The %s has been successfully unregistered.",
				get_vm_type_str());

	return ret;
}

int PrlVm::destroy(const CmdParamData &param)
{
	PRL_RESULT ret;

	prl_log(0, "Removing the %s...", get_vm_type_str());
	std::string err;
	VIRTUAL_MACHINE_STATE state = get_state();

	if (param.force && (state == VMS_RUNNING || state == VMS_PAUSED || state == VMS_UNKNOWN)) {
		CmdParamData p;
		p.force = p.fast = true;
		if ((ret = stop(p)) && state != VMS_UNKNOWN)
			return ret;
	}

	PrlHandle hJob(PrlVm_Delete(m_hVm, PRL_INVALID_HANDLE));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to remove the %s: %s",
				get_vm_type_str(), err.c_str());
	else
		prl_log(0, "The %s has been successfully removed.",
				get_vm_type_str());

	return ret;
}

int PrlVm::get_boot_list(PrlList<PrlBootEntry *> &bootlist,
		 unsigned int &max_index) const
{
	PRL_RESULT ret;
	PRL_UINT32 count;

	if ((ret = PrlVmCfg_GetBootDevCount(m_hVm, &count)))
		return prl_err(ret, "PrlVmCfg_GetBootDevCount: %s",
			get_error_str(ret).c_str());
	max_index = 0;
	for (unsigned int i = 0; i < count; ++i) {
		PrlHandle hBootDev;
		PRL_DEVICE_TYPE type;
		unsigned int idx, index;
		PRL_BOOL inuse;
		const PrlDev *dev;

		if ((ret = PrlVmCfg_GetBootDev(m_hVm, i, hBootDev.get_ptr())))
			return prl_err(ret, "PrlVmCfg_GetBootDev: %s",
				get_error_str(ret).c_str());
		if ((ret = PrlBootDev_GetType(hBootDev.get_handle(), &type)))
			return prl_err(ret, "PrlBootDev_GetType",
				get_error_str(ret).c_str());
		if ((ret = PrlBootDev_GetIndex(hBootDev.get_handle(), &idx)))
			return prl_err(ret, "PrlBootDev_GetIndex",
				get_error_str(ret).c_str());
		if ((ret = PrlBootDev_GetSequenceIndex(hBootDev.get_handle(), &index)))
			return prl_err(ret, "PrlBootDev_GetSequenceIndex",
				get_error_str(ret).c_str());
		if ((ret = PrlBootDev_IsInUse(hBootDev.get_handle(), &inuse)))
			return prl_err(ret, "PrlBootDev_IsInUse: %s",
				get_error_str(ret).c_str());
		if (max_index < index)
			max_index = index;
		if ((dev = find_dev(prl_dev_type2type(type), idx))) {
			prl_log(L_DEBUG, "boot: %s %d",
				dev->m_id.c_str(), index);
			bootlist.add(new PrlBootEntry(index,
						inuse ? true : false, dev));
		} else {
			prl_log(L_DEBUG, "Missed boot: type=%d idx=%d index=%d",
				type, idx, index);
		}
	}
	return 0;
}

int PrlVm::add_boot_entry(const PrlBootEntry *entry, unsigned int index) const
{
	PRL_RESULT ret;
	PrlHandle hBootDev;
	const PrlDev *dev = entry->m_dev;

	prl_log(L_INFO, "Create a new boot record: %s %d",
			dev->m_id.c_str(), index);
	if ((ret = PrlVmCfg_CreateBootDev(m_hVm, hBootDev.get_ptr())))
		return prl_err(ret, "PrlVmCfg_CreateBootDev",
			get_error_str(ret).c_str());
	if ((ret = PrlBootDev_SetType(hBootDev.get_handle(),	type2prl_dev_type(
							dev->get_type()))))
		return prl_err(ret, "PrlBootDev_SetType",
			get_error_str(ret).c_str());
	if ((ret = PrlBootDev_SetIndex(hBootDev.get_handle(), dev->get_idx())))
		return prl_err(ret, "PrlBootDev_SetIndex",
			get_error_str(ret).c_str());
	if ((ret = PrlBootDev_SetSequenceIndex(hBootDev.get_handle(), index)))
		return prl_err(ret, "PrlBootDev_SetSequenceIndex: %s",
			 get_error_str(ret).c_str());
	if ((ret = PrlBootDev_SetInUse(hBootDev.get_handle(), entry->m_inuse)))
		return prl_err(ret, "PrlBootDev_SetInUse: %s",
			get_error_str(ret).c_str());

	return 0;
}

const PrlBootEntry *PrlVm::find_boot_entry(
		const PrlList<PrlBootEntry *> &bootlist,
		const PrlDev *dev) const
{
	PrlList<PrlBootEntry *>::const_iterator it, eit = bootlist.end();

	for (it = bootlist.begin(); it != eit; ++it)
		if ((*it)->m_dev == dev)
			return *it;
	return 0;

}

void PrlVm::del_boot_entry(PrlList<PrlBootEntry *> &bootlist,
		const PrlDev *dev)
{
	PrlList<PrlBootEntry *>::iterator it, eit = bootlist.end();

	for (it = bootlist.begin(); it != eit; ++it)
		if ((*it)->m_dev == dev)
			break;
	if (it != eit)
	{
		bootlist.erase(it);
		delete (*it);
	}
}

int PrlVm::del_boot_list()
{
	PRL_RESULT ret;
	PRL_UINT32 count;

	if ((ret = PrlVmCfg_GetBootDevCount(m_hVm, &count))) {
		return prl_err(ret, "PrlVmCfg_GetBootDevCount: %s",
			get_error_str(ret).c_str());
	}
	for (unsigned int i = 0; i < count;  ++i) {
		PrlHandle hBootDev;
		if ((ret = PrlVmCfg_GetBootDev(m_hVm, 0, hBootDev.get_ptr()))) {
			prl_log(L_WARN, "PrlVmCfg_GetBootDev: %s",
				get_error_str(ret).c_str());
			continue;
		}
		if ((ret = PrlBootDev_Remove(hBootDev.get_handle())))
			return prl_err(ret, "PrlBootDev_Remove: %s",
				get_error_str(ret).c_str());
	}
	return 0;
}

int PrlVm::set_boot_dev(const PrlDev *dev, int bootorder, bool inuse) const
{
	PRL_RESULT ret;
	PrlList<PrlBootEntry *> bootlist;
	unsigned int max_index;

	if ((ret = get_boot_list(bootlist, max_index)))
		return ret;

	const PrlBootEntry *old_entry = find_boot_entry(bootlist, dev);
	if (!old_entry) {
		const PrlBootEntry entry(bootorder, inuse, dev);
		ret = add_boot_entry(&entry, ++max_index);
	}
	bootlist.del();

	return ret;
}

int PrlVm::set_boot_list(const CmdParamData &param)
{
	int ret;
	str_list_t names;
	str_list_t::const_iterator i;
	PrlList<PrlBootEntry *> bootlist;
	unsigned int max_index = 0;
	PrlDev *dev;

	 /* PMC has advanced logic that every device in hardware list with
	 * suitable for booting type is present in boot order list _always_.
	 * This logic automates addition of new devices - they automatically
	 * added to boot order. The same for new VM - all default devices
	 * automatically appears in boot list. In order to make PMC happy lets
	 * do not reset booting list but mark devices as disabled. */
	if ((ret = get_boot_list(bootlist, max_index)))
		return ret;

	/* reset original list */
	if ((ret = del_boot_list()))
		return ret;

	/* add specified devices */
	max_index = 0;
	names = split(param.dev.name);
	for (i = names.begin(); i != names.end(); ++i) {
		if (!(dev = m_DevList.find(*i)))
			return prl_err(1, "The %s device does not exist.",
				(*i).c_str());
		del_boot_entry(bootlist, dev);

		const PrlBootEntry entry((unsigned int)-1, true, dev);
		ret = add_boot_entry(&entry, ++max_index);
		if (ret) {
			bootlist.del();
			return ret;
		}
	}

	/* add the rest as "disabled" */
	PrlList<PrlBootEntry *>::iterator it, eit = bootlist.end();
	for (it = bootlist.begin(); it != eit; ++it)
	{
		(*it)->m_inuse = false;
		ret = add_boot_entry(*it, ++max_index);
		if (ret)
			break;
	}
	bootlist.del();

	set_updated();
	return ret;
}

std::string PrlVm::get_bootdev_info()
{
	PrlList<PrlBootEntry *> bootlist;
	unsigned int max_index;

	if (get_boot_list(bootlist, max_index))
		return "";

	std::string out;
	PrlList<PrlBootEntry *>::iterator it, eit = bootlist.end();

	for (it = bootlist.begin(); it != eit; ++it) {
		PrlBootEntry *entry = *it;
		if (entry->m_inuse) {
			out += entry->m_dev->m_id;
			out += " ";
		}
	}
	bootlist.del();
	return out;
}

int PrlVm::set_vnc(const VncParam &param)
{
	int ret;
	bool updated = false;

	if (param.mode != -1) {
		VncParam vnc = PrlVm::get_vnc_param();
		if (param.mode != PRD_DISABLED) {
			if (vnc.mode != param.mode && param.passwd.empty() && !param.nopasswd)
				return prl_err(-1, "A password is required to enable VNC support,"
						" or the --vnc-nopasswd option must be used.");
		}
		if (param.mode == PRD_MANUAL) {
			if (vnc.mode != param.mode && param.port == 0)
				return prl_err(-1, "A port is required to enable VNC support in manual mode");
		}
		if ((ret = PrlVmCfg_SetVNCMode(m_hVm, (PRL_VM_REMOTE_DISPLAY_MODE) param.mode)))
			return prl_err(ret, "PrlVmCfg_SetVNCMode",
					get_error_str(ret).c_str());
		updated = true;
	}
	if (param.port > 0) {
		if ((ret = PrlVmCfg_SetVNCPort(m_hVm, param.port)))
			return prl_err(ret, "PrlVmCfg_SetVNCPort",
					get_error_str(ret).c_str());
		updated = true;
	}
	if (!param.address.empty()) {
		if ((ret = PrlVmCfg_SetVNCHostName(m_hVm, param.address.c_str())))
			return prl_err(ret, "PrlVmCfg_GetVNCHostName",
					get_error_str(ret).c_str());
		updated = true;
	}
	if (!param.passwd.empty() || param.nopasswd) {
		std::string passwd;
		/* if we got '-' - read actual password from terminal or stdin */
		if (param.passwd == "-") {
			if ((ret = read_passwd(passwd, "Please, enter VNC console password: ")))
				return prl_err(-1, "Failed to read password for VNC console");
		} else {
			passwd = param.passwd;
		}

		ret = PrlVmCfg_SetVNCPassword(m_hVm, passwd.c_str());
		/* erase password value in case destructor doesn't do that for us */
		passwd.clear();
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetVNCPassword",
					get_error_str(ret).c_str());
		updated = true;

	}
	if (updated) {
		prl_log(0, "Configure VNC: %s", param.get_info().c_str());
		set_updated();
	}

	return 0;
}

VncParam PrlVm::get_vnc_param()
{
	VncParam vnc;
	char buf[1024];
	PRL_UINT32 len;

	PrlVmCfg_GetVNCMode(m_hVm, (PRL_VM_REMOTE_DISPLAY_MODE_PTR) &vnc.mode);
	PrlVmCfg_GetVNCPort(m_hVm, &vnc.port);
	len = sizeof(buf);
	if (PrlVmCfg_GetVNCHostName(m_hVm, buf, &len) == 0)
		vnc.address = buf;

	return vnc;
}

void PrlVm::get_vnc_info(PrlOutFormatter &f)
{
	VncParam vnc = get_vnc_param();

	vnc.append_info(f);
}

unsigned int PrlVm::new_dev_idx(DevType type) const
{
	PrlDevList::const_iterator it = m_DevList.begin(),
					eit = m_DevList.end();
	unsigned int idx = 0;

	for (; it != eit; ++it)
		if ((*it)->get_type() == type)
			idx++;
	return idx;
}

int PrlVm::create_dev(DevType type, const DevInfo &param)
{
	PRL_RESULT ret;
	PRL_DEVICE_TYPE dtype = PDE_GENERIC_DEVICE;
	PrlHandle hDev;

	if (type == DEV_GENERIC_PCI) {
		const PrlDevSrv *dev = find_srv_dev(DEV_GENERIC_PCI, param.device);
		if (dev == NULL)
			return prl_err(-1, "No such device: %s ", param.device.c_str());

		PRL_GENERIC_PCI_DEVICE_CLASS pci_class;
		if ((ret = PrlSrvCfgPci_GetDeviceClass(dev->get_handle(), &pci_class)))
			return prl_err(-1, "PrlSrvCfgPci_GetDeviceClass: %s",
					get_error_str(ret).c_str());

		/* Move generic PCI to predefined class */
		switch (pci_class) {
		case PGD_PCI_NETWORK:
			dtype = PDE_GENERIC_NETWORK_ADAPTER;
			type = DEV_NET;
			break;
		case PGD_PCI_DISPLAY:
			dtype = PDE_PCI_VIDEO_ADAPTER;
			break;
		case PGD_PCI_SOUND:
		case PGD_PCI_OTHER:
			dtype = PDE_GENERIC_PCI_DEVICE;
			break;
		}
	} else if (type == DEV_HDD && !param.storage_url.empty()) {
		dtype = PDE_ATTACHED_BACKUP_DISK;
	} else {
		dtype = type2prl_dev_type(type);
	}

	if ((ret = PrlVmCfg_AddDefaultDeviceEx(m_hVm,
					m_srv.get_srv_config_handle(),
					dtype, hDev.get_ptr())))
		return prl_err(ret, "Failed to create the device: %s",
			get_error_str(ret).c_str());
	unsigned int idx = 0;

	/* Fixme: some devices has no indexes (usb, sound).
	   Ignore error there
	*/
	PrlVmDev_GetIndex(hDev.get_handle(), &idx);
	PrlDev *dev = m_DevList.add(new_dev(hDev.release_handle(), type, idx));
	if ((ret = dev->create(param)))
		return ret;
	return 0;
}

PrlDevSrv *PrlVm::find_srv_dev(DevType type, const std::string &name) const
{
	return m_srv.find_dev(type, name);
}

PrlDev *PrlVm::find_dev(DevType type, unsigned int idx) const
{
	return m_DevList.find(type, idx);
}

PrlDev *PrlVm::find_dev(const std::string &sname) const
{
	for (PrlDevList::const_iterator dev = m_DevList.begin(), end = m_DevList.end(); dev != end; ++dev) {
		if ((*dev)->get_sname() == sname)
			return *dev;
	}
	return 0;
}

int PrlVm::get_dev_info()
{
	PRL_RESULT ret;
	PRL_UINT32 count;

	if ((ret = PrlVmCfg_GetDevsCount(m_hVm, &count)))
		return prl_err(ret, "PrlVmCfg_GetDevsCount: %s",
			get_error_str(ret).c_str());
	if (count == 0)
		return 0;
	PRL_HANDLE_PTR handles = (PRL_HANDLE_PTR) malloc(sizeof(PRL_HANDLE) *
								count);

	/* m_DevNetList is filtered vector of m_DevList pointers */
	m_DevNetList.clear();

	m_DevList.del();
	if (!(ret = PrlVmCfg_GetDevsList(m_hVm, handles, &count))) {
		for (unsigned int i = 0; i < count; ++i) {
			PRL_HANDLE_TYPE hType = PHT_ERROR;

			PrlHandle_GetType(handles[i], &hType);
			DevType type = handle2type(hType);
			if (type == DEV_NONE)
				prl_log(L_DEBUG, "Unknown device type=%x", hType);

			unsigned int idx = 0;
			PrlVmDev_GetIndex(handles[i], &idx);
			PrlDev *dev = new_dev(handles[i], type, idx);
			if (dev)
				m_DevList.add(dev);
			else
				/* Unknown device */
				PrlHandle_Free(handles[i]);
		}
	} else {
		prl_err(ret, "PrlVm_GetDevsList: %s",
				get_error_str(ret).c_str());
	}
	free(handles);
	return ret;
}

int PrlVm::get_new_dir(const char *pattern, std::string &new_dir,
		const char *dir) const
{
	PRL_RESULT ret;
	std::string location;

	if (!dir) {
		if ((ret = get_home_dir(location)))
			return ret;
	} else {
		location = dir;
	}
	if ((ret = m_srv.get_new_dir(location.c_str(), pattern, new_dir)))
		return ret;

	prl_log(L_DEBUG, "PrlVm::get_new_dir: %s", new_dir.c_str());
	return 0;
}

std::string PrlVm::get_dist() const
{
	PRL_RESULT ret;
	unsigned int id;

	if ((ret  = PrlVmCfg_GetOsVersion(m_hVm, &id))) {
		prl_log(L_INFO, "PrlVmCfg_GetOsVersion: %s",
			get_error_str(ret).c_str());
		return std::string("");
	}
	return std::string(get_dist_by_id(id));
}

std::string PrlVm::get_ostemplate() const
{
	PRL_RESULT ret;
	char buf[4096];
	unsigned int len = sizeof(buf);

	if ((ret  = PrlVmCfg_GetOsTemplate(m_hVm, buf, &len))) {
		prl_log(L_INFO, "PrlVmCfg_GetOsTemplate: %s",
			get_error_str(ret).c_str());
		return std::string("");
	}
	return std::string(buf);
}

int PrlVm::get_home_dir(std::string &dir) const
{
	PRL_RESULT ret;
	char buf[4096];
	unsigned int len = sizeof(buf);

	if ((ret = PrlVmCfg_GetHomePath(m_hVm, buf, &len)))
		return prl_err(ret, "PrlVmCfg_GetHomePath: %s",
			get_error_str(ret).c_str());
	/* remove config.pvs from the path /home_dir/config.pvs */
	unsigned int sfx_len = sizeof("config.pvs") - 1;
	if (len > sfx_len) {
		char *se = buf + len - sfx_len - 1;
		if (strcmp(se, "config.pvs") == 0)
			*se = 0;
	}
	dir = buf;

	return 0;
}

int PrlVm::update_owner(PRL_HANDLE hVmInfo)
{
	PRL_RESULT ret;
	PrlHandle hVmAcl;

	if ((ret = PrlVmInfo_GetAccessRights(hVmInfo, hVmAcl.get_ptr())))
		return prl_err(ret, "PrlVmInfo_GetAccessRights: %s",
			get_error_str(ret).c_str());

	char buf[1024];
	unsigned int len = sizeof(buf);
	if ((ret = PrlAcl_GetOwnerName(hVmAcl.get_handle(), buf, &len)))
		return prl_err(ret, "PrlAcl_GetOwnerName: %s",
			get_error_str(ret).c_str());
	m_owner = buf;

	return 0;
}

int PrlVm::get_vm_info()
{
	PRL_RESULT ret;

	get_confirmation_list();

	if ((ret = get_dev_info()))
		return ret;
	update_state();

	return 0;
}

int PrlVm::get_confirmation_list()
{
	PRL_HANDLE hList;
	PRL_RESULT ret = PrlVmCfg_GetConfirmationsList(m_hVm, &hList);
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

		if (cmd == PAR_VM_GUI_VIEW_MODE_CHANGE_ACCESS)
			m_confirmation_list.push_back("exit-fullscreen");

		// change-vm-state
		if (cmd == PAR_VM_START_ACCESS)
			m_confirmation_list.push_back("start-vm");
		if (cmd == PAR_VM_STOP_ACCESS)
			m_confirmation_list.push_back("stop-vm");
		if (cmd == PAR_VM_PAUSE_ACCESS)
			m_confirmation_list.push_back("pause-vm");
		if (cmd == PAR_VM_RESET_ACCESS)
			m_confirmation_list.push_back("reset-vm");
		if (cmd == PAR_VM_SUSPEND_ACCESS)
			m_confirmation_list.push_back("suspend-vm");
		if (cmd == PAR_VM_RESUME_ACCESS)
			m_confirmation_list.push_back("resume-vm");
		if (cmd == PAR_VM_DROPSUSPENDEDSTATE_ACCESS)
			m_confirmation_list.push_back("drop-vm-suspended-state");
		if (cmd == PAR_VM_RESTART_GUEST_ACCESS)
			m_confirmation_list.push_back("restart-guest");

		// manage-snapshots
		if (cmd == PAR_VM_CREATE_SNAPSHOT_ACCESS)
			m_confirmation_list.push_back("create-snapshot");
		if (cmd == PAR_VM_SWITCH_TO_SNAPSHOT_ACCESS)
			m_confirmation_list.push_back("switch-to-snapshot");
		if (cmd == PAR_VM_DELETE_SNAPSHOT_ACCESS)
			m_confirmation_list.push_back("delete-snapshot");

		if (cmd == PAR_VM_CHANGE_GUEST_OS_PASSWORD_ACCESS)
			m_confirmation_list.push_back("change-guest-pwd");
	}

	return 0;
}

unsigned int PrlVm::get_cpu_count() const
{
	PRL_RESULT ret;
	unsigned int count = 0;

	if ((ret = PrlVmCfg_GetCpuCount(m_hVm, &count)))
		prl_err(-1, "PrlVmCfg_GetCpuCount: %s",
			get_error_str(ret).c_str());
	return count;
}

PRL_VM_ACCELERATION_LEVEL PrlVm::get_cpu_acc_level() const
{
	PRL_RESULT ret;
	PRL_VM_ACCELERATION_LEVEL level = PVA_ACCELERATION_DISABLED;

	if ((ret = PrlVmCfg_GetCpuAccelLevel(m_hVm, &level)))
		prl_err(-1, "PrlVmCfg_GetCpuAccelLevel: %s",
			get_error_str(ret).c_str());

	return level;
}

PRL_CPU_MODE PrlVm::get_cpu_mode() const
{
	PRL_RESULT ret;
	PRL_CPU_MODE mode = PCM_CPU_MODE_32;

	if ((ret = PrlVmCfg_GetCpuMode(m_hVm, &mode)))
		prl_err(-1, "PrlVmCfg_GetCpuMode: %s",
			get_error_str(ret).c_str());

	return mode;
}

bool PrlVm::is_cpu_hotplug_enabled() const
{
	PRL_RESULT ret;
	PRL_BOOL enabled = PRL_FALSE;

	if ((ret = PrlVmCfg_IsCpuHotplugEnabled(m_hVm, &enabled)))
		prl_err(ret, "PrlVmCfg_IsCpuHotplugEnabled: %s",
			get_error_str(ret).c_str());
	return prl_bool(enabled);
}

int PrlVm::set_cpu_hotplug(const std::string &value)
{
	PRL_RESULT ret;
	PRL_BOOL enable = PRL_FALSE;

	if (value == "on")
		enable = PRL_TRUE;
	prl_log(0, "set cpu hotplug: %d", enable);
	if ((ret = PrlVmCfg_SetCpuHotplugEnabled(m_hVm, enable)))
                return prl_err(ret, ":PrlVmCfg_SetCpuHotplugEnabled %s",
                        get_error_str(ret).c_str());
	set_updated();
	return 0;
}

int PrlVm::set_cpu_count(unsigned int num)
{
	PRL_RESULT ret;
	unsigned int total;

	if ((total = m_srv.get_cpu_count()) < num)
		return prl_err(-1, "An incorrect value for the CPU parameter (%d)"
			" is specified. The maximal CPU number may equal %d.",
			num, total);
	prl_log(0, "set cpus(%d): %d", total, num);
	if ((ret = PrlVmCfg_SetCpuCount(m_hVm, num)))
                return prl_err(ret, ":PrlVmCfg_SetCpuCount %s",
                        get_error_str(ret).c_str());
	set_updated();
	return 0;
}

int PrlVm::set_distribution(PRL_UINT32 nOsVersion)
{
    PRL_RESULT ret;

    prl_log(0, "set distribution: %s", get_dist_by_id(nOsVersion));
    if ((ret = PrlVmCfg_SetOsVersion(m_hVm, nOsVersion)))
	return prl_err(ret, ":PrlVmCfg_SetOsVersion %s",
		       get_error_str(ret).c_str());
    set_updated();
    return 0;
}

int PrlVm::set_cpuunits(unsigned int cpuunits)
{
	PRL_RESULT ret;

	prl_log(0, "set cpuunits %d", cpuunits);
	if ((ret = PrlVmCfg_SetCpuUnits(m_hVm, cpuunits)))
		return prl_err(-1, "PrlVmCfg_SetCpuUnits: %s",
			get_error_str(ret).c_str());

	set_updated();
	return 0;
}

int PrlVm::set_cpumask(const std::string &mask)
{
	PRL_RESULT ret;

	prl_log(0, "set cpu mask %s",  mask.c_str());
	if ((ret = PrlVmCfg_SetCpuMask(m_hVm,
			(mask == "all" ? "" : mask.c_str()))))
	{
		return prl_err(-1, "Failed to set cpumask: %s",
			ret == PRL_ERR_INVALID_ARG ?
			"invalid cpu mask is specified" : get_error_str(ret).c_str());
	}

	set_updated();
	return 0;
}

std::string PrlVm::get_cpumask()
{
	char buf[512];
	unsigned int len = sizeof(buf);

	if (PrlVmCfg_GetCpuMask(m_hVm, buf, &len))
		return std::string();
	return std::string(buf);
}

int PrlVm::set_nodemask(const std::string &mask)
{
	PRL_RESULT ret;

	prl_log(0, "set node mask %s",  mask.c_str());
	if ((ret = PrlVmCfg_SetNodeMask(m_hVm,
			(mask == "all" ? "" : mask.c_str()))))
	{
		return prl_err(-1, "Failed to set nodemask: %s",
			ret == PRL_ERR_INVALID_ARG ?
			"invalid node mask is specified" : get_error_str(ret).c_str());
	}

	set_updated();
	return 0;
}

int PrlVm::get_nodemask(std::string& mask)
{
	PRL_RESULT ret;
	char buf[512];
	unsigned int len = sizeof(buf);

	if ((ret = PrlVmCfg_GetNodeMask(m_hVm, buf, &len)))
		return prl_err(-1, "PrlVmCfg_GetNodeMask: %s",
			get_error_str(ret).c_str());

	mask = buf;
	return 0;
}

int PrlVm::get_cpuunits(unsigned int *cpuunits)
{
	/* cpuunits makes sense on Linux/VZWIN for now */
	PRL_RESULT ret;

	if ((ret = PrlVmCfg_GetCpuUnits(m_hVm, cpuunits)))
		return prl_err(-1, "PrlVmCfg_GetCpuUnits: %s",
			get_error_str(ret).c_str());

	return 0;
}


int PrlVm::set_cpulimit(PRL_CONST_CPULIMIT_DATA_PTR cpulimit)
{
	PRL_RESULT ret;

	if (cpulimit->type == 0)
		return 0;

	prl_log(0, "set cpulimit %d%s",
			cpulimit->value,
			cpulimit->type == PRL_CPULIMIT_MHZ ? "Mhz" : "%");
	if ((ret = PrlVmCfg_SetCpuLimitEx(m_hVm, cpulimit)))
		return prl_err(-1, "PrlVmCfg_SetCpuLimit: %s",
			get_error_str(ret).c_str());

	set_updated();

	return 0;
}

int PrlVm::get_cpulimit(PRL_CPULIMIT_DATA_PTR cpulimit)
{
	/* cpulimit makes sense on Linux only for now */
	PRL_RESULT ret;

	if ((ret = PrlVmCfg_GetCpuLimitEx(m_hVm, cpulimit)))
		return prl_err(-1, "PrlVmCfg_GetCpuLimit: %s",
			get_error_str(ret).c_str());

	return 0;
}

int PrlVm::get_cpulimitmode(unsigned int *limitmode)
{
	PRL_RESULT ret;

	if ((ret = PrlVmCfg_GetGuestCpuLimitType(m_hVm, limitmode)))
		return prl_err(-1, "PrlVmCfg_GetGuestCpuLimitType: %s",
			get_error_str(ret).c_str());

	return 0;
}

int PrlVm::set_ioprio(unsigned int ioprio)
{
	PRL_RESULT ret;

	prl_log(0, "set ioprio %d", ioprio);
	if ((ret = PrlVmCfg_SetIoPriority(m_hVm, ioprio)))
		return prl_err(-1, "PrlVmCfg_SetIoPriority: %s",
			get_error_str(ret).c_str());

	set_updated();
	return 0;
}

int PrlVm::get_ioprio(unsigned int *ioprio)
{
	/* ioprio makes sense on Linux only for now */
#ifdef _LIN_
	PRL_RESULT ret;

	if ((ret = PrlVmCfg_GetIoPriority(m_hVm, ioprio)))
		return prl_err(-1, "PrlVmCfg_GetIoPriority: %s",
			get_error_str(ret).c_str());
#else
	*ioprio = -1;
#endif

	return 0;
}

int PrlVm::set_iolimit(unsigned int iolimit)
{
	PRL_RESULT ret;
	PRL_IOLIMIT_DATA data;

	prl_log(0, "Set up iolimit: %d", iolimit);
	data.type = PRL_IOLIMIT_BS;
	data.value = iolimit;
	if ((ret = PrlVmCfg_SetIoLimit(m_hVm, &data)))
		return prl_err(-1, "PrlVmCfg_SetIoLimit: %s",
			get_error_str(ret).c_str());

	set_updated();
	return 0;
}

int PrlVm::get_iolimit(unsigned int *iolimit)
{
	/* iolimit makes sense on Linux only for now */
#ifdef _LIN_
	PRL_RESULT ret;
	PRL_IOLIMIT_DATA data;

	data.type = PRL_IOLIMIT_BS;
	if ((ret = PrlVmCfg_GetIoLimit(m_hVm, &data)))
		return prl_err(-1, "PrlVmCfg_GetIoLimit: %s",
			get_error_str(ret).c_str());
	*iolimit = data.value;
#else
	*iolimit = -1;
#endif

	return 0;
}

int PrlVm::set_iopslimit(unsigned int limit)
{
	PRL_RESULT ret;

	prl_log(0, "set IOPS limit %d", limit);
	if ((ret = PrlVmCfg_SetIopsLimit(m_hVm, limit)))
		return prl_err(-1, "PrlVmCfg_SetIopsLimit: %s",
			get_error_str(ret).c_str());

	set_updated();
	return 0;
}

int PrlVm::get_iopslimit(unsigned int *limit)
{
	PRL_RESULT ret;

	*limit = (unsigned int)-1;
	ret = PrlVmCfg_GetIopsLimit(m_hVm, limit);
	if (ret && ret != PRL_ERR_UNIMPLEMENTED)
		return prl_err(-1, "PrlVmCfg_GetIoPriority: %s",
			get_error_str(ret).c_str());
	return 0;
}

unsigned int PrlVm::get_memsize() const
{
	PRL_RESULT ret;
	unsigned int size = 0;

	if ((ret = PrlVmCfg_GetRamSize(m_hVm, &size)))
		prl_err(ret, "PrlVmCfg_GetRamSize %s",
			get_error_str(ret).c_str());
	return size;
}

int PrlVm::set_memsize(unsigned int num)
{
	PRL_RESULT ret;

	prl_log(0, "Set the memsize parameter to %dMb.", num);
	if ((ret = PrlVmCfg_SetRamSize(m_hVm, num)))
		return prl_err(ret, "PrlVmCfg_SetRamSize %s",
			get_error_str(ret).c_str());

	set_updated();
	return 0;
}

unsigned int PrlVm::get_videosize() const
{
	PRL_RESULT ret;
	unsigned int size = 0;

	if ((ret = PrlVmCfg_GetVideoRamSize(m_hVm, &size)))
		prl_err(ret, "PrlVmCfg_GetVideoRamSize: %s",
				get_error_str(ret).c_str());
	return size;
}

int PrlVm::set_videosize(unsigned int num)
{
	PRL_RESULT ret;

	prl_log(0, "Set the videosize parameter to %dMb.", num);
	if ((ret = PrlVmCfg_SetVideoRamSize(m_hVm, num)))
		return prl_err(ret, "PrlVmCfg_SetVideoRamSize: %s",
			get_error_str(ret).c_str());

	set_updated();
	return 0;
}

int PrlVm::get_3d_acceleration() const
{
	PRL_RESULT ret;
	PRL_VIDEO_3D_ACCELERATION n3DAccelerationMode = P3D_DISABLED;

	if ((ret = PrlVmCfg_Get3DAccelerationMode(m_hVm, &n3DAccelerationMode)))
		prl_err(ret, "PrlVmCfg_Get3DAccelerationMode: %s",
				get_error_str(ret).c_str());
	return n3DAccelerationMode;
}

int PrlVm::set_3d_acceleration(int mode)
{
	PRL_RESULT ret;
	PRL_VIDEO_3D_ACCELERATION n3DAccelerationMode = (PRL_VIDEO_3D_ACCELERATION )mode;

	prl_log(0, "Set 3d acceleration: %d.", mode);
	if ((ret = PrlVmCfg_Set3DAccelerationMode(m_hVm, n3DAccelerationMode)))
		return prl_err(ret, "PrlVmCfg_Set3DAccelerationMode: %s",
			get_error_str(ret).c_str());

	set_updated();
	return 0;
}

bool PrlVm::is_vertical_sync_enabled() const
{
	PRL_RESULT ret;
	PRL_BOOL enabled = PRL_FALSE;

	if ((ret = PrlVmCfg_IsVerticalSynchronizationEnabled(m_hVm, &enabled)))
		prl_err(ret, "PrlVmCfg_IsVerticalSynchronizationEnabled: %s",
			get_error_str(ret).c_str());
	return prl_bool(enabled);
}

bool PrlVm::is_high_resolution_enabled() const
{
	PRL_RESULT ret;
	PRL_BOOL enabled = PRL_FALSE;

	if ((ret = PrlVmCfg_IsHighResolutionEnabled(m_hVm, &enabled)))
		prl_err(ret, "PrlVmCfg_IsHighResolutionEnabled: %s",
			get_error_str(ret).c_str());
	return prl_bool(enabled);
}

int PrlVm::set_vertical_sync(int enabled)
{
	PRL_RESULT ret;
	PRL_BOOL bEnabled = enabled ? PRL_TRUE : PRL_FALSE;

	prl_log(0, "Set vertical synchronization: %d.", enabled);
	if ((ret = PrlVmCfg_SetVerticalSynchronizationEnabled(m_hVm, bEnabled)))
		return prl_err(ret, "PrlVmCfg_SetVerticalSynchronizationEnabled: %s",
			get_error_str(ret).c_str());

	set_updated();
	return 0;
}

bool PrlVm::is_mem_hotplug_enabled() const
{
	PRL_RESULT ret;
	PRL_BOOL enabled = PRL_FALSE;

	if ((ret = PrlVmCfg_IsRamHotplugEnabled(m_hVm, &enabled)))
		prl_err(ret, "PrlVmCfg_IsRamHotplugEnabled: %s",
			get_error_str(ret).c_str());
	return prl_bool(enabled);
}

int PrlVm::set_mem_hotplug(int value)
{
	PRL_RESULT ret;
	PRL_BOOL enable = PRL_FALSE;

	if (value)
		enable = PRL_TRUE;
	prl_log(0, "set mem hotplug: %d", enable);
	if ((ret = PrlVmCfg_SetRamHotplugEnabled(m_hVm, enable)))
                return prl_err(ret, ":PrlVmCfg_SetRamHotplugEnabled %s",
                        get_error_str(ret).c_str());
	set_updated();
	return 0;
}

void PrlVm::get_memguarantee(PrlOutFormatter &f)
{
	PRL_RESULT ret;
	PRL_MEMGUARANTEE_DATA memguarantee;

	if ((ret = PrlVmCfg_GetMemGuaranteeSize(m_hVm, &memguarantee)))
		prl_err(ret, "PrlVmCfg_GetMemGuaranteeSize %s",
			get_error_str(ret).c_str());

	f.open("memory_guarantee", true);

	switch(memguarantee.type)
	{
	case PRL_MEMGUARANTEE_AUTO:
		f.add("auto", true);
		break;
	case PRL_MEMGUARANTEE_PERCENTS:
		f.add("value", memguarantee.value, "%", true, true);
		break;
	}

	f.close(true);
}

int PrlVm::set_memguarantee(const CmdParamData &param)
{
	PRL_RESULT ret;

	switch (param.memguarantee.type)
	{
	case PRL_MEMGUARANTEE_AUTO:
		prl_log(0, "set memguarantee: auto");
		break;
	case PRL_MEMGUARANTEE_PERCENTS:
		prl_log(0, "set memguarantee: %d%%", param.memguarantee.value);
		break;
	}
	if ((ret = PrlVmCfg_SetMemGuaranteeSize(m_hVm, &param.memguarantee)))
                return prl_err(ret, "PrlVmCfg_SetMemGuaranteeSize %s",
                        get_error_str(ret).c_str());

	set_updated();
	return 0;
}

int PrlVm::set_desc(const std::string &desc)
{
	PRL_RESULT ret;

	if ((ret = PrlVmCfg_SetDescription(m_hVm, desc.c_str())))
		return prl_err(ret, "PrlVmCfg_SetDescription: %s",
			get_error_str(ret).c_str());

	set_updated();
	return 0;
}

std::string PrlVm::get_desc() const
{
	PRL_RESULT ret;
	char buf[4096];
	unsigned int len = sizeof(buf);

	if ((ret = PrlVmCfg_GetDescription(m_hVm, buf, &len)))
		return "";

	char *p = buf;
	while (*p++ != '\0') {
		if (*p == '\n')
			*p =  ' ';
	}
	return std::string(buf);
}

int PrlVm::set_autostart(const std::string &mode)
{
	PRL_RESULT ret;
	PRL_VM_AUTOSTART_OPTION prl_mode;

	if (mode == "off")
		prl_mode = PAO_VM_START_MANUAL;
	else if (mode == "on")
		prl_mode = PAO_VM_START_ON_LOAD;
	else if (mode == "auto")
		prl_mode = PAO_VM_START_ON_RELOAD;
	else if (mode == "start-app")
		prl_mode = PAO_VM_START_ON_GUI_APP_STARTUP;
	else if (mode == "open-window")
		prl_mode = PAO_VM_START_ON_GUI_VM_WINDOW_OPEN;
	else
		return prl_err(-1, "The specified auto-start mode is"
			" not supported: %s.", mode.c_str());

	if ((ret = PrlVmCfg_SetAutoStart(m_hVm, prl_mode)))
		return prl_err(ret, "PrlVmCfg_SetAutoStart: %s",
			get_error_str(ret).c_str());
	set_updated();

	return 0;
}

std::string PrlVm::get_autostart_info() const
{
	PRL_VM_AUTOSTART_OPTION mode;
	PRL_RESULT ret;

	if ((ret = PrlVmCfg_GetAutoStart(m_hVm, &mode))) {
		prl_err(ret, "PrlVmCfg_GetAutoStart: %s",
			get_error_str(ret).c_str());
		return "";
	}

	PRL_APPLICATION_MODE appMode = PAM_UNKNOWN;
	PrlApi_GetAppMode(&appMode);

	switch (mode) {
	case PAO_VM_START_MANUAL:
		return "off";
	case PAO_VM_START_ON_LOAD:
		return "on";
	case PAO_VM_START_ON_RELOAD:
		return "auto";
	case PAO_VM_START_ON_GUI_APP_STARTUP:
		return "start-app";
	case PAO_VM_START_ON_GUI_VM_WINDOW_OPEN:
		return "open-window";
	default:
		return "unknown";
	}

	return "";
}

int PrlVm::set_autostart_delay(unsigned int delay)
{
	PRL_RESULT ret;

	prl_log(L_INFO, "set_autostart_delay: %u", delay);
	if ((ret = PrlVmCfg_SetAutoStartDelay(m_hVm, delay)))
		return prl_err(ret, "PrlVmCfg_SetAutoStartDelay: %s",
			get_error_str(ret).c_str());
	set_updated();

	return 0;
}

int PrlVm::set_autostop(const std::string &mode)
{
	PRL_VM_AUTOSTOP_OPTION prl_mode;
	PRL_RESULT ret;

	if (mode == "stop")
		prl_mode = PAO_VM_STOP;
	else if (mode == "suspend")
		prl_mode = PAO_VM_SUSPEND;
	else if (mode == "shutdown")
		prl_mode = PAO_VM_SHUTDOWN;
	else
		return prl_err(-1, "The specified auto-stop mode is not"
			" supported: %s.", mode.c_str());
	if ((ret = PrlVmCfg_SetAutoStop(m_hVm, prl_mode)))
		return prl_err(ret, "PrlVmCfg_SetAutoStop: %s",
			get_error_str(ret).c_str());
	set_updated();

	return 0;
}

int PrlVm::set_autocompact(int enabled)
{
	int ret;

	if ((ret = PrlVmCfg_SetAutoCompressEnabled(m_hVm, (PRL_BOOL)enabled)))
		return prl_err(ret, "PrlVmCfg_SetAutoCompressEnabled: %s",
				get_error_str(ret).c_str());
	set_updated();

	return 0;
}

bool PrlVm::get_autocompact()
{
	int ret;
	PRL_BOOL enabled;

	if ((ret = PrlVmCfg_IsAutoCompressEnabled(m_hVm, &enabled)))
		return prl_err(ret, "PrlVmCfg_IsAutoCompressEnabled: %s",
				get_error_str(ret).c_str());

	return !!enabled;
}

std::string PrlVm::get_autostop_info() const
{
	PRL_VM_AUTOSTOP_OPTION mode;
	PRL_RESULT ret;

	if ((ret = PrlVmCfg_GetAutoStop(m_hVm, &mode))) {
		prl_err(ret, "PrlVmCfg_GetAutoStop: %s",
			get_error_str(ret).c_str());
		return "";
	}
	switch (mode) {
	case PAO_VM_STOP:
		return "stop";
	case PAO_VM_SUSPEND:
		return "suspend";
	case PAO_VM_SHUTDOWN:
		return "shutdown";
	}
	return "";
}

int PrlVm::set_startup_view(const std::string &mode)
{
	PRL_VM_WINDOW_MODE prl_mode;
	PRL_RESULT ret;

	if (mode == "same")
		prl_mode = PWM_DEFAULT_WINDOW_MODE;
	else if (mode == "window")
		prl_mode = PWM_WINDOWED_WINDOW_MODE;
	else if (mode == "fullscreen")
		prl_mode = PWM_FULL_SCREEN_WINDOW_MODE;
	else
		return prl_err(-1, "The specified startup view mode is not"
			" supported: %s.", mode.c_str());

	if ((ret = PrlVmCfg_SetWindowMode(m_hVm, prl_mode)))
		return prl_err(ret, "PrlVmCfg_SetWindowMode: %s",
			get_error_str(ret).c_str());
	set_updated();

	return 0;
}

std::string PrlVm::get_startup_view_info() const
{
	PRL_VM_WINDOW_MODE mode;
	PRL_RESULT ret;

	if ((ret = PrlVmCfg_GetWindowMode(m_hVm, &mode))) {
		prl_err(ret, "PrlVmCfg_GetWindowMode: %s",
			get_error_str(ret).c_str());
		return "";
	}

	switch (mode) {
	case PWM_DEFAULT_WINDOW_MODE:
		return "same";
	case PWM_WINDOWED_WINDOW_MODE:
		return "window";
	case PWM_FULL_SCREEN_WINDOW_MODE:
		return "fullscreen";
	}
	return "";
}

int PrlVm::set_on_shutdown(const std::string &mode)
{
	PRL_VM_ACTION_ON_STOP prl_mode;
	PRL_RESULT ret;

	if (mode == "window")
		prl_mode = PAS_KEEP_VM_WINDOW_OPEN;
	else if (mode == "close")
		prl_mode = PAS_CLOSE_VM_WINDOW;
	else if (mode == "quit")
		prl_mode = PAS_QUIT_APPLICATION;
	else
		return prl_err(-1, "The specified on shutdown mode is not"
			" supported: %s.", mode.c_str());

	if ((ret = PrlVmCfg_SetActionOnStopMode(m_hVm, prl_mode)))
		return prl_err(ret, "PrlVmCfg_SetActionOnStopMode: %s",
			get_error_str(ret).c_str());
	set_updated();

	return 0;
}

std::string PrlVm::get_on_shutdown_info() const
{
	PRL_VM_ACTION_ON_STOP mode;
	PRL_RESULT ret;

	if ((ret = PrlVmCfg_GetActionOnStopMode(m_hVm, &mode))) {
		prl_err(ret, "PrlVmCfg_GetActionOnStopMode: %s",
			get_error_str(ret).c_str());
		return "";
	}

	switch (mode) {
	case PAS_KEEP_VM_WINDOW_OPEN:
		return "window";
	case PAS_CLOSE_VM_WINDOW:
		return "close";
	case PAS_QUIT_APPLICATION:
		return "quit";
	}
	return "";
}

int PrlVm::set_on_window_close(const std::string &mode)
{
	PRL_VM_ACTION_ON_WINDOW_CLOSE prl_mode;
	PRL_RESULT ret;

	if (mode == "suspend")
		prl_mode = PWC_VM_SUSPEND;
	else if (mode == "shutdown")
		prl_mode = PWC_VM_SHUTDOWN;
	else if (mode == "stop")
		prl_mode = PWC_VM_STOP;
	else if (mode == "ask")
		prl_mode = PWC_VM_ASK_USER;
	else if (mode == "keep-running")
		prl_mode = PWC_VM_DO_NOTHING;
	else
		return prl_err(-1, "The specified on close window mode is not"
			" supported: %s.", mode.c_str());

	if ((ret = PrlVmCfg_SetActionOnWindowClose(m_hVm, prl_mode)))
		return prl_err(ret, "PrlVmCfg_SetActionOnWindowClose: %s",
			get_error_str(ret).c_str());
	set_updated();

	return 0;
}

std::string PrlVm::get_on_window_close_info() const
{
	PRL_VM_ACTION_ON_WINDOW_CLOSE mode;
	PRL_RESULT ret;

	if ((ret = PrlVmCfg_GetActionOnWindowClose(m_hVm, &mode))) {
		prl_err(ret, "PrlVmCfg_GetActionOnWindowClose: %s",
			get_error_str(ret).c_str());
		return "";
	}

	switch (mode) {
	case PWC_VM_SUSPEND:
		return "suspend";
	case PWC_VM_SHUTDOWN:
		return "shutdown";
	case PWC_VM_STOP:
		return "stop";
	case PWC_VM_ASK_USER:
		return "ask";
	case PWC_VM_DO_NOTHING:
		return "keep-running";
	default:;
	}
	return "";
}

int PrlVm::set_undo_disks(const std::string &mode)
{
	PRL_UNDO_DISKS_MODE prl_mode;
	PRL_RESULT ret;

	if (mode == "off")
		prl_mode = PUD_DISABLE_UNDO_DISKS;
	else if (mode == "discard")
		prl_mode = PUD_REVERSE_CHANGES;
	else if (mode == "ask")
		prl_mode = PUD_PROMPT_BEHAVIOUR;
	else
		return prl_err(-1, "The specified startup view mode is not"
			" supported: %s.", mode.c_str());

	if ((ret = PrlVmCfg_SetUndoDisksMode(m_hVm, prl_mode)))
		return prl_err(ret, "PrlVmCfg_SetUndoDisksMode: %s",
			get_error_str(ret).c_str());
	set_updated();

	return 0;
}

std::string PrlVm::get_undo_disks_info() const
{
	PRL_UNDO_DISKS_MODE mode;
	PRL_RESULT ret;

	if ((ret = PrlVmCfg_GetUndoDisksMode(m_hVm, &mode))) {
		prl_err(ret, "PrlVmCfg_GetUndoDisksMode: %s",
			get_error_str(ret).c_str());
		return "";
	}

	switch (mode) {
	case PUD_DISABLE_UNDO_DISKS:
		return "off";
	case PUD_REVERSE_CHANGES:
		return "discard";
	case PUD_PROMPT_BEHAVIOUR:
		return "ask";
	default:;
	}
	return "";
}

std::string PrlVm::get_system_flags()
{
	int ret;
	char buf[4096];
	unsigned int len = sizeof(buf);

	if ((ret = PrlVmCfg_GetSystemFlags(m_hVm, buf, &len)))
		return "";

	return std::string(buf);
}

int PrlVm::set_system_flags(const std::string &flags)
{
	int ret;

	if ((ret = PrlVmCfg_SetSystemFlags(m_hVm, flags.c_str())))
		return prl_err(ret, "PrlVmCfg_SetSystemFlags: %s",
			get_error_str(ret).c_str());
	set_updated();
	return 0;
}

int PrlVm::set_name(const std::string &name)
{
	PRL_RESULT ret;

	prl_log(0, "Set name of %s to '%s'",
			get_vm_type_str(), name.c_str());
	ret = PrlVmCfg_SetName(m_hVm, name.c_str());
	if (ret)
		return prl_err(ret, "PrlVmCfg_IsTemplate: %s",
			get_error_str(ret).c_str());
	set_updated();

	return 0;
}

int PrlVm::set_template_sign(int template_sign)
{
	PRL_RESULT ret;

	prl_log(0, "Set template flag of %s to %i",
			get_vm_type_str(), template_sign);
	ret = PrlVmCfg_SetTemplateSign(m_hVm, template_sign);
	if (ret)
		return prl_err(ret, "PrlVmCfg_SetTemplateSign: %s",
			get_error_str(ret).c_str());
	set_updated();

	return 0;
}

PRL_VM_LOCATION PrlVm::get_location()
{
	PRL_VM_LOCATION nVmLocation = PVL_UNKNOWN;
	PrlVmCfg_GetLocation(m_hVm, &nVmLocation);
	return (nVmLocation);
}

FeaturesParam PrlVm::get_features()
{
	FeaturesParam feature;
	PRL_BOOL enabled;

	feature.type = get_vm_type();
	if (get_vm_type() == PVT_CT) {
		PRL_UINT32 on, off;

		PrlVmCfg_GetFeaturesMask(m_hVm, &on, &off);
		feature.known = on | off;
		feature.mask = on;
	} else {

#define SET_FEATURE(id, enabled)		\
	do {					\
		feature.known |= id;		\
		if (enabled)			\
			feature.mask |= id;	\
		else				\
			feature.mask &= ~id;	\
	} while (0);				\

		if (!PrlVmCfg_IsShareClipboard(m_hVm, &enabled))
			SET_FEATURE(FT_ShareClipboard, enabled)
		if (!PrlVmCfg_IsTimeSynchronizationEnabled(m_hVm, &enabled))
			SET_FEATURE(FT_TimeSynchronization, enabled)
		if (!PrlVmCfg_IsTimeSyncSmartModeEnabled(m_hVm, &enabled))
			SET_FEATURE(FT_TimeSyncSmartMode, enabled)
		if (!PrlVmCfg_IsSharedProfileEnabled(m_hVm, &enabled))
			SET_FEATURE(FT_SharedProfile, enabled)
		if (!PrlVmCfg_IsUseDesktop(m_hVm, &enabled))
			SET_FEATURE(FT_UseDesktop, enabled)
		if (!PrlVmCfg_IsUseDocuments(m_hVm, &enabled))
			SET_FEATURE(FT_UseDocuments, enabled)
		if (!PrlVmCfg_IsUsePictures(m_hVm, &enabled))
			SET_FEATURE(FT_UsePictures, enabled)
		if (!PrlVmCfg_IsUseMusic(m_hVm, &enabled))
			SET_FEATURE(FT_UseMusic, enabled)
		PRL_UINT32 val;
		if (!PrlVmCfg_GetTimeSyncInterval(m_hVm, &val)) {
			SET_FEATURE(FT_TimeSyncInterval, 1)
			feature.time_sync_interval = val;
		}
		if (!PrlVmCfg_IsSmartGuardEnabled(m_hVm, &enabled))
			SET_FEATURE(FT_SmartGuard, enabled)
		if (!PrlVmCfg_IsSmartGuardNotifyBeforeCreation(m_hVm, &enabled))
			SET_FEATURE(FT_SmartGuardNotify, enabled)
		if (!PrlVmCfg_GetSmartGuardInterval(m_hVm, &val)) {
			SET_FEATURE(FT_SmartGuardInterval, 1)
			feature.smart_guard_interval = val;
		}
		if (!PrlVmCfg_GetSmartGuardMaxSnapshotsCount(m_hVm, &val)) {
			SET_FEATURE(FT_SmartGuardMaxSnapshots, 1)
			feature.smart_guard_max_snapshots = val;
		}
		if (!PrlVmCfg_IsSmartMountEnabled(m_hVm, &enabled))
			SET_FEATURE(FT_SmartMount, enabled)
		if (!PrlVmCfg_IsSmartMountRemovableDrivesEnabled(m_hVm, &enabled))
			SET_FEATURE(FT_SmartMountRemovableDrives, enabled)
		if (!PrlVmCfg_IsSmartMountDVDsEnabled(m_hVm, &enabled))
			SET_FEATURE(FT_SmartMountDVDs, enabled)
		if (!PrlVmCfg_IsSmartMountNetworkSharesEnabled(m_hVm, &enabled))
			SET_FEATURE(FT_SmartMountNetworkShares, enabled)
	}

	return feature;
}

int PrlVm::set_features(const FeaturesParam &feature)
{
	PRL_RESULT ret;

	if (get_vm_type() != feature.type)
		return prl_err(-1, "Unable to use %s features for %s.",
				(feature.type == PVT_VM) ? "VM" : "CT",
				get_vm_type_str());

	if (get_vm_type() == PVT_CT) {
		ret = PrlVmCfg_SetFeaturesMask(m_hVm, feature.mask,
				feature.known & ~feature.mask);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetFeaturesMask: %s",
					get_error_str(ret).c_str());
	} else {
		if (feature.known & FT_ShareClipboard) {
			PrlVmCfg_SetShareClipboard(m_hVm,
					(PRL_BOOL) (feature.mask & FT_ShareClipboard));
		}
		if (feature.known & FT_TimeSynchronization) {
			PrlVmCfg_SetTimeSynchronizationEnabled(m_hVm,
					(PRL_BOOL) (feature.mask & FT_TimeSynchronization));
		}
		if (feature.known & FT_TimeSyncSmartMode) {
			PrlVmCfg_SetTimeSyncSmartModeEnabled(m_hVm,
					(PRL_BOOL) (feature.mask & FT_TimeSyncSmartMode));
		}
		if (feature.known & FT_SharedProfile) {
			PrlVmCfg_SetSharedProfileEnabled(m_hVm,
					(PRL_BOOL) (feature.mask & FT_SharedProfile));
		}
		if (feature.known & FT_UseDesktop) {
			PrlVmCfg_SetUseDesktop(m_hVm,
					(PRL_BOOL) (feature.mask & FT_UseDesktop));
		}
		if (feature.known & FT_UseDocuments) {
			PrlVmCfg_SetUseDocuments(m_hVm,
					(PRL_BOOL) (feature.mask & FT_UseDocuments));
		}
		if (feature.known & FT_UsePictures) {
			PrlVmCfg_SetUsePictures(m_hVm,
					(PRL_BOOL) (feature.mask & FT_UsePictures));
		}
		if (feature.known & FT_UseMusic) {
			PrlVmCfg_SetUseMusic(m_hVm,
					(PRL_BOOL) (feature.mask & FT_UseMusic));
		}
		if (feature.known & FT_TimeSyncInterval) {
			PrlVmCfg_SetTimeSyncInterval(m_hVm,
					feature.time_sync_interval);
		}
		if (feature.known & FT_SmartGuard) {
			PrlVmCfg_SetSmartGuardEnabled(m_hVm,
					(PRL_BOOL) (feature.mask & FT_SmartGuard));
		}
		if (feature.known & FT_SmartGuardNotify) {
			PrlVmCfg_SetSmartGuardNotifyBeforeCreation(m_hVm,
					(PRL_BOOL) (feature.mask & FT_SmartGuardNotify));
		}
		if (feature.known & FT_SmartGuardInterval) {
			PrlVmCfg_SetSmartGuardInterval(m_hVm,
					feature.smart_guard_interval);
		}
		if (feature.known & FT_SmartGuardMaxSnapshots) {
			PrlVmCfg_SetSmartGuardMaxSnapshotsCount(m_hVm,
					feature.smart_guard_max_snapshots);
		}
		if (feature.known & FT_SmartMount) {
			PrlVmCfg_SetSmartMountEnabled(m_hVm,
					(PRL_BOOL) (feature.mask & FT_SmartMount));
		}
		if (feature.known & FT_SmartMountRemovableDrives) {
			PrlVmCfg_SetSmartMountRemovableDrivesEnabled(m_hVm,
					(PRL_BOOL) (feature.mask & FT_SmartMountRemovableDrives));
		}
		if (feature.known & FT_SmartMountDVDs) {
			PrlVmCfg_SetSmartMountDVDsEnabled(m_hVm,
					(PRL_BOOL) (feature.mask & FT_SmartMountDVDs));
		}
		if (feature.known & FT_SmartMountNetworkShares) {
			PrlVmCfg_SetSmartMountNetworkSharesEnabled(m_hVm,
					(PRL_BOOL) (feature.mask & FT_SmartMountNetworkShares));
		}
	}

	prl_log(0, "set features: %s", feature2str(feature).c_str());
	set_updated();

	return 0;
}

int PrlVm::set_cap(const CapParam &capability)
{
	int ret;

	if (get_vm_type() != PVT_CT)
		return prl_err(-1, "Capabilities can be set for Containers only.");

	if (get_state() != VMS_STOPPED)
		return prl_err(-1, "Unable to set the capability for the running Container.");

	if (m_ostype != PVS_GUEST_TYPE_LINUX)
		return prl_err(-1, "Unable to set the capability."
			" The Container is running a non-Linux operating system.");

	cap_t cap_mask;

	ret = PrlVmCfg_GetCapabilitiesMask(m_hVm, &cap_mask);
	if (ret)
		return prl_err(ret, "PrlVmCfg_GetCapabilitiesMask: %s",
				get_error_str(ret).c_str());

	cap_mask = (cap_mask | capability.mask_on) & ~capability.mask_off;

	ret = PrlVmCfg_SetCapabilitiesMask(m_hVm, cap_mask);
	if (ret)
		return prl_err(ret, "PrlVmCfg_SetCapabilitiesMask: %s",
				get_error_str(ret).c_str());

	prl_log(0, "Set capabilities: %s", capability2str(capability).c_str());
	set_updated();

	return 0;
}

int PrlVm::set_netfilter(const Netfilter::Mode& netfilter)
{
	if (get_vm_type() != PVT_CT)
		return prl_err(-1, "Netfilter can be set for Containers only.");

	if (get_state() == VMS_RUNNING)
		return prl_err(-1, "Unable to set netfilter for the running Container.");

	int ret = PrlVmCfg_SetNetfilterMode(m_hVm, netfilter.id);
	if (ret)
		return prl_err(ret, "PrlVmCfg_SetNetfilterMode: %s", get_error_str(ret).c_str());

	prl_log(0, "Set netfilter: %s", netfilter.name.c_str());
	set_updated();

	return 0;
}

Netfilter::Mode PrlVm::get_netfilter() const
{
	if (get_vm_type() == PVT_VM)
		return Netfilter::Mode();

	PRL_NETFILTER_MODE m;
	int ret = PrlVmCfg_GetNetfilterMode(m_hVm, &m);
	if (ret)
	{
		prl_log(L_ERR, "PrlVmCfg_GetNetfilterMode: %s", get_error_str(ret).c_str());
		return Netfilter::Mode();
	}

	return Netfilter::fromId(m);
}

int PrlVm::set_ct_resources(const CmdParamData &param)
{
	int ret;

	for (ct_resource_list_t::const_iterator it = param.ct_resource.begin(),
						eit = param.ct_resource.end(); it != eit; it++)
	{
		prl_log(0, "Set %s %llu", prl_ct_resource2str(it->id), it->limit);
		ret = PrlVmCfg_SetResource(m_hVm, it->id, it->barrier, it->limit);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetResource: %s",
					get_error_str(ret).c_str());
		set_updated();

	}
	return 0;
}

std::string PrlVm::get_hostname()
{
	char buf[512];
	unsigned int len = sizeof(buf);

	if (PrlVmCfg_GetHostname(m_hVm, buf, &len))
		return std::string();
	return std::string(buf);
}

std::string PrlVm::get_nameservers()
{
	PrlHandle h;

	if (PrlVmCfg_GetDnsServers(m_hVm, h.get_ptr()))
		return std::string();
	return handle2str(h);
}

std::string PrlVm::get_searchdomains()
{
	PrlHandle h;

	if (PrlVmCfg_GetSearchDomains(m_hVm, h.get_ptr()))
		return std::string();
	return handle2str(h);
}

int PrlVm::set_net(const CmdParamData &param)
{
	PRL_RESULT ret;

	if (!param.hostname.empty()) {
		if ((ret = PrlVmCfg_SetHostname(m_hVm, param.hostname.c_str())))
			return prl_err(ret, "PrlVmCfg_SetHostname: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (!param.nameserver.empty()) {
		PrlHandle hArgs;

		str_list2handle(param.nameserver, hArgs);

		if ((ret = PrlVmCfg_SetDnsServers(m_hVm, hArgs.get_handle())))
			return prl_err(ret, "PrlVmCfg_SetDnsServers: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (!param.searchdomain.empty()) {
		PrlHandle hArgs;

		str_list2handle(param.searchdomain, hArgs);

		if ((ret = PrlVmCfg_SetSearchDomains(m_hVm, hArgs.get_handle())))
			return prl_err(ret, "PrlVmCfg_SetSearchDomains: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (!param.off_srv.empty()) {
		PrlHandle hArgs;

		str_list2handle(param.off_srv, hArgs);

		if ((ret = PrlVmCfg_SetOfflineServices(m_hVm, hArgs.get_handle())))
			return prl_err(ret, "PrlVmCfg_SetOfflineServices: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.off_man != -1) {
		if ((ret = PrlVmCfg_SetOfflineManagementEnabled(m_hVm, (PRL_BOOL) param.off_man)))
			return prl_err(ret, "PrlVmCfg_SetOfflineManagementEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.apply_iponly != -1) {
		if ((ret = PrlVmCfg_SetAutoApplyIpOnly(m_hVm, (PRL_BOOL) param.apply_iponly)))
			return prl_err(ret, "PrlVmCfg_SetAutoApply: %s",
					get_error_str(ret).c_str());
		set_updated();
	}

	return 0;
}

bool PrlVm::is_vtx_enabled() const
{
	PRL_RESULT ret;
	PRL_BOOL enabled = PRL_FALSE;

	if ((ret = PrlVmCfg_IsCpuVtxEnabled(m_hVm, &enabled)))
		prl_err(ret, "PrlVmCfg_IsCpuVtxEnabled: %s",
			get_error_str(ret).c_str());
	return prl_bool(enabled);
}

bool PrlVm::is_template() const
{
	PRL_RESULT ret;
	PRL_BOOL enabled = PRL_FALSE;

	if ((ret = PrlVmCfg_IsTemplate(m_hVm, &enabled)))
		prl_err(ret, "PrlVmCfg_IsTemplate: %s",
			get_error_str(ret).c_str());

	return prl_bool(enabled);
}

int PrlVm::validate_config(PRL_VM_CONFIG_SECTIONS section) const
{
	PRL_RESULT ret, retcode;
	PrlHandle hResult;
	PRL_UINT32 resultCount = 0;
	std::string err;

	PrlHandle hJob(PrlVm_ValidateConfig(m_hVm, section));
	if ((ret = PrlJob_Wait(hJob.get_handle(), g_nJobTimeout)))
		return prl_err(ret, "Unable to commit %s configuration: %s",
			get_vm_type_str(), get_error_str(ret).c_str());

	if ((ret = PrlJob_GetRetCode(hJob.get_handle(), &retcode)))
		return prl_err(ret, "PrlJob_GetRetCode: %s",
			get_error_str(ret).c_str());

	if ((ret = PrlJob_GetResult(hJob.get_handle(), hResult.get_ptr())))
		return prl_err(ret, "PrlJob_GetResult: %s [%d]",
				get_error_str(ret).c_str(), ret);

	if ((ret = PrlResult_GetParamsCount(hResult.get_handle(), &resultCount)))
		return prl_err(ret, "PrlResult_GetParamsCount  %s [%d]",
				get_error_str(ret).c_str(), ret);
	if (resultCount == 0)
		return 0;

	prl_log(0, "%s configuration validation result(s) :",
			get_vm_type_str(), resultCount);
	for (unsigned int i = 0; i < resultCount; i++) {
		PrlHandle hEvent;
		char buf[4096];
		unsigned int len;

		ret = PrlResult_GetParamByIndex(hResult.get_handle(), i, hEvent.get_ptr());
		if (ret)
			break;
		if (!PrlEvent_GetErrCode(hEvent.get_handle(), &ret))
		{
			std::string out;

			out += "* ";
			len = sizeof(buf);
			if (PrlEvent_GetErrString(hEvent.get_handle(), PRL_TRUE, PRL_FALSE, buf, &len) == 0)
				out += buf;
			len = sizeof(buf);
			if (PrlEvent_GetErrString(hEvent.get_handle(), PRL_FALSE, PRL_FALSE, buf, &len) == 0)
				out += buf;
			prl_log(0, "%s", out.c_str());
		}
	}
	return retcode;
}


static int commit_event_handler(PRL_HANDLE hEvent, void *data)
{
	PrlHandle h(hEvent);
	PRL_HANDLE_TYPE type;
	int ret;
	char *vm_uuid = (char *) data;

	if ((ret = PrlHandle_GetType(h.get_handle(), &type))) {
		prl_log(L_DEBUG, "PrlHandle_GetType: %s",
				get_error_str(ret).c_str());
		return ret;
	}

	if (type == PHT_EVENT) {
		char uuid[256] = "";
		unsigned int len;
		PRL_EVENT_TYPE evt_type;

		if ((ret = PrlEvent_GetType(h.get_handle(), &evt_type))) {
			prl_log(L_DEBUG, "PrlEvent_GetType: %s",
					get_error_str(ret).c_str());
			return ret;
		}

		//get VM uuid
		len = sizeof(uuid);
		ret = PrlEvent_GetIssuerId(h.get_handle(), uuid, &len);
		if (PRL_FAILED(ret)) {
			prl_log(L_DEBUG, "PrlEvent_GetIssuerId %s",
				    get_error_str(ret).c_str());
			return ret;
		}

		prl_log(L_DEBUG, "vmuuid=%s IssuerId=%s\n", vm_uuid, uuid);
		if (vm_uuid != NULL) {
			if (strcmp(uuid, vm_uuid))
			    return 0;
		}

		if (evt_type == PET_DSP_EVT_VM_MESSAGE || evt_type == PET_DSP_EVT_VM_CONFIG_APPLIED) {
			std::string err;
			get_result_error_string(hEvent, err);
			fprintf(stdout, "%s\n", err.c_str());
		}
	}

	return 0;
}

int PrlVm::commit_configuration(const CmdParamData &param)
{
	PRL_RESULT ret, retcode;
	PrlHandle hResult;
	PRL_UINT32 resultCount = 0;
	std::string err;
	char vm_uuid[256];

	strcpy(vm_uuid, get_id().c_str());
	m_srv.reg_event_callback(commit_event_handler, vm_uuid);
	m_srv.reg_event_callback(progress_event_handler);

	PrlHandle hJob(PrlVm_CommitEx(m_hVm,param.commit_flags));
	if ((ret = PrlJob_Wait(hJob.get_handle(), g_nJobTimeout)))
		return prl_err(ret, "Unable to commit %s configuration: %s",
			get_vm_type_str(), get_error_str(ret).c_str());
	if ((ret = PrlJob_GetRetCode(hJob.get_handle(), &retcode)))
		return prl_err(ret, "PrlJob_GetRetCode: %s",
			get_error_str(ret).c_str());
	if (retcode)
	{
		if ((ret = PrlJob_GetResult(hJob.get_handle(), hResult.get_ptr())))
			return prl_err(ret, "PrlJob_GetResult: %s [%d]",
				get_error_str(ret).c_str(), ret);

		if ((ret = PrlResult_GetParamsCount(hResult.get_handle(), &resultCount)))
			return prl_err(ret, "PrlResult_GetParamsCount  %s [%d]",
				get_error_str(ret).c_str(), ret);

		char buf[4096];
		unsigned int len = sizeof(buf);

		for (unsigned int i = 0; i < resultCount; i++) {

			PrlHandle hEvent;

			ret = PrlResult_GetParamByIndex(hResult.get_handle(), i, hEvent.get_ptr());
			if (ret)
				break;
			len = sizeof(buf);
			if (!PrlEvent_GetErrCode(hEvent.get_handle(), &ret) &&
			    !PrlEvent_GetErrString(hEvent.get_handle(), PRL_FALSE, PRL_FALSE, buf, &len))
			{
				if( i > 0 )
					err += "\n"; // to separate from next error
				err += buf;
			}
		}

		PrlHandle hErr;
		if ( !resultCount && !PrlJob_GetError(hJob.get_handle(), hErr.get_ptr()) )
		{
			get_result_error_string(hErr.get_handle(), err);
			std::string d(get_details(hJob));
			if (!d.empty())
				err += " (Details: " + d + ")";
			++resultCount;
		}

		prl_log(L_DEBUG, "resultCount: %d", resultCount);

		prl_err(retcode, "Unable to commit %s configuration: %s",
			get_vm_type_str(),
			resultCount == 0 ?
				get_error_str(retcode).c_str() : err.c_str());
	}

	m_srv.unreg_event_callback(commit_event_handler);
	m_srv.unreg_event_callback(progress_event_handler);

	return retcode;
}

int PrlVm::set_rate(const rate_list_t &rate)
{
	int ret;
	PrlHandle hRateList;

	PrlApi_CreateHandlesList(hRateList.get_ptr());

	for (rate_list_t::const_iterator it = rate.begin(), eit = rate.end(); it != eit; it++) {
		PrlHandle hRate;

		ret = PrlNetworkRate_Create(it->classid, it->rate, hRate.get_ptr());
		if (ret)
			return prl_err(-1, "PrlNetworkRate_Create: %s",
						 get_error_str(ret).c_str());
		ret = PrlHndlList_AddItem(hRateList.get_handle(), hRate.get_handle());
		if (ret)
			return prl_err(-1, "PrlHndlList_AddItem: %s",
						 get_error_str(ret).c_str());
	}
	if ((ret = PrlVmCfg_SetNetworkRateList(m_hVm, hRateList.get_handle())))
		 return prl_err(-1, "PrlVmCfg_SetNetworkRates: %s",
					 get_error_str(ret).c_str());
	set_updated();

	return 0;
}

int PrlVm::set(const CmdParamData &param)
{
	PRL_RESULT ret;
	PrlDev *dev = 0;
	std::string err;

	// Process Connect/Disconnect before PrlVm_BeginEdit bug #480156
	if (param.dev.cmd == Connect || param.dev.cmd == Disconnect) {
		if ((ret = get_dev_info()))
			return ret;
		if (!(dev = m_DevList.find(param.dev.name))) {
			PrlDevSrv *sdev;
			/* Try to find real USB device*/
			if (!(sdev = find_srv_dev(DEV_USB, param.dev.name)))
				return prl_err(1, "The %s device does not"
					" exist.", param.dev.name.c_str());
			if (!(dev = m_DevList.find("usb")))
				return prl_err(1, "The usb device does not"
					" configured.");
		}
		if (param.dev.cmd == Connect) {
			if ((ret = dev->connect(param.dev.name)))
				return ret;
		} else if (param.dev.cmd == Disconnect) {
			if ((ret = dev->disconnect(param.dev.name)))
				return ret;
		}
	}

	PrlHandle hJob(PrlVm_BeginEdit(m_hVm));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "failed to apply settings %s", err.c_str());
	if (!param.config_sample.empty()) {
		if ((ret = apply_configuration(param.config_sample)))
			return ret;
	}

	/* VM configuration */
	if (param.dist) {
	    if ((ret = set_distribution(param.dist->ver)))
		return ret;
	}
	if (param.cpus_present) {
		if ((ret = set_cpu_count(param.cpus)))
			return ret;
	}
	if (!param.cpu_hotplug.empty()) {
		if ((ret = set_cpu_hotplug(param.cpu_hotplug)))
			return ret;
	}
	if (param.cpuunits) {
		if ((ret = set_cpuunits(param.cpuunits)))
			return ret;
	}
	if (!param.cpumask.empty()) {
		if ((ret = set_cpumask(param.cpumask)))
			return ret;
	}
	if (!param.nodemask.empty()) {
		if ((ret = set_nodemask(param.nodemask)))
			return ret;
	}
	if ((ret = set_cpulimit(&param.cpulimit)))
		return ret;
	if (param.ioprio != (unsigned int) -1) {
		if ((ret = set_ioprio(param.ioprio)))
			return ret;
	}
	if (param.iolimit != (unsigned int) -1) {
		if ((ret = set_iolimit(param.iolimit)))
			return ret;
	}
	if (param.iopslimit != (unsigned int) -1) {
		if ((ret = set_iopslimit(param.iopslimit)))
			return ret;
	}

	if (param.memsize) {
		if ((ret = set_memsize(param.memsize)))
			return ret;
	}
	if (param.videosize) {
		if ((ret = set_videosize(param.videosize)))
			return ret;
	}
	if (param.v3d_accelerate != -1) {
		if ((ret = set_3d_acceleration(param.v3d_accelerate)))
			return ret;
	}
	if (param.vertical_sync != -1) {
		if ((ret = set_vertical_sync(param.vertical_sync)))
			return ret;
	}
	if (param.high_resolution != -1) {
		PRL_BOOL enable = param.high_resolution;
		ret = PrlVmCfg_SetHighResolutionEnabled(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetHighResolutionEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.mem_hotplug != -1) {
		if ((ret = set_mem_hotplug(param.mem_hotplug)))
			return ret;
	}
	if (param.memguarantee_set) {
		if ((ret = set_memguarantee(param)))
			return ret;
	}
	if (!param.desc.empty()) {
		if ((ret = set_desc(param.desc)))
			return ret;
	}
	if (!param.autostart.empty()) {
		if ((ret = set_autostart(param.autostart)))
			return ret;
	}
	if (param.autostart_delay != (unsigned int)-1) {
		if ((ret = set_autostart_delay(param.autostart_delay)))
			return ret;
	}
	if (!param.autostop.empty()) {
		if ((ret = set_autostop(param.autostop)))
			return ret;
	}
	if (param.autocompact != -1) {
		if ((ret = set_autocompact(param.autocompact)))
			return ret;
	}
	if (!param.startup_view.empty()) {
		if ((ret = set_startup_view(param.startup_view)))
			return ret;
	}
	if (!param.on_shutdown.empty()) {
		if ((ret = set_on_shutdown(param.on_shutdown)))
			return ret;
	}
	if (!param.on_window_close.empty()) {
		if ((ret = set_on_window_close(param.on_window_close)))
			return ret;
	}
	if (!param.undo_disks.empty()) {
		if ((ret = set_undo_disks(param.undo_disks)))
			return ret;
	}
	if (!param.system_flags.empty()) {
		if ((ret = set_system_flags(param.system_flags)))
			return ret;
	}
	if (!param.name.empty()) {
		if ((ret = set_name(param.name)))
			return ret;
	}
	if (param.features.known) {
		if ((ret = set_features(param.features)))
			return ret;
	}
	if (!param.cap.empty()) {
		if ((ret = set_cap(param.cap)))
			return ret;
	}
	if (param.netfilter.isValid()) {
		if ((ret = set_netfilter(param.netfilter)))
			return ret;
	}
	if (!param.ct_resource.empty()) {
		if ((ret = set_ct_resources(param)))
			return ret;
	}
	if (param.template_sign != -1) {
		if ((ret = set_template_sign(param.template_sign)))
			return ret;
	}
	if ((ret = set_net(param)))
		return ret;

	/* Device configuration */
	if (param.dev.cmd == Add) {
		if ((ret = get_dev_info()))
			return ret;
		if ((ret = create_dev(param.dev.type, param.dev)))
			return ret;
		dev = m_DevList.back();
		if (dev->is_updated())
			set_updated();
	}
	if (param.dev.cmd == Del) {
		if ((ret = get_dev_info()))
			return ret;
		str_list_t names = split(param.dev.name);
		str_list_t::const_iterator it;
		for (it = names.begin(); it != names.end(); ++it) {
			if (!(dev = m_DevList.find(*it)) &&
			    !(dev = find_dev(*it)))
			{
				return prl_err(1, "The %s device does not exist.",
					(*it).c_str());
			}
			if ((ret = dev->remove()))
				return ret;
			set_updated();
		}
	}
	if (param.dev.cmd == Set) {
		if ((ret = get_dev_info()))
			return ret;
		if (param.dev.name.empty()) {
			/* set params to the first device in the list */
			dev = m_DevList.find(param.dev.type);
		} else {
			dev = m_DevList.find(param.dev.name);
			if (!dev) // use SystemMame
				dev = find_dev(param.dev.name);
		}
		if (!dev)
			return prl_err(1, "The %s device does not exist.",
				param.dev.name.c_str());
		if ((ret = dev->configure(param.dev)))
			return ret;
		if (dev->is_updated())
			set_updated();
	}

	if (param.backup_cmd != None) {
		if ((ret = get_dev_info()))
			return ret;
		if (param.backup_cmd == Add)
			ret = attach_backup_disks(param);
		else if (param.backup_cmd == Del)
			ret = detach_backup_disks(param.backup_id);
		if (ret)
			return ret;
	}

	if (param.dev.cmd == BootOrder) {
		if ((ret = get_dev_info()))
			return ret;
		if ((ret = set_boot_list(param)))
			return ret;
	}
	if ((ret = set_vnc(param.vnc)))
		return ret;
	if (!param.userpasswd.empty()) {
		if ((ret = set_userpasswd(param.userpasswd, param.crypted)))
			return ret;
	}
	/* Use default answers enabling sign */
	if (-1 != param.use_default_answers) {
		int ret;
		PRL_BOOL enable = param.use_default_answers;

		ret = PrlVmCfg_SetUseDefaultAnswers(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetUseDefaultAnswers: %s",
				get_error_str(ret).c_str());
		set_updated();
	}
	if (param.tools_autoupdate != -1) {
		int ret;
		PRL_BOOL enable = param.tools_autoupdate;

		ret = PrlVmCfg_SetToolsAutoUpdateEnabled(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetUseDefaultAnswers: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.smart_mount != -1) {
		int ret;
		PRL_BOOL enable = param.smart_mount;

		ret = PrlVmCfg_SetSmartMountEnabled(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetSmartMountEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (!param.rate.empty()) {
		ret = set_rate(param.rate);
		if (ret)
			return ret;
	}
	if (param.ratebound != -1) {
		PRL_BOOL enable = param.ratebound;
		ret = PrlVmCfg_SetRateBound(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetRateBound: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.keyboard_optimize != -1) {
		PRL_OPTIMIZE_MODIFIERS_MODE mode = (PRL_OPTIMIZE_MODIFIERS_MODE )param.keyboard_optimize;
		ret = PrlVmCfg_SetOptimizeModifiersMode(m_hVm, mode);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetOptimizeModifiersMode: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.efi_boot != -1) {
		PRL_BOOL enable = param.efi_boot;
		ret = PrlVmCfg_SetEfiEnabled(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetEfiEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.select_boot_dev != -1) {
		PRL_BOOL allow = param.select_boot_dev;
		ret = PrlVmCfg_SetAllowSelectBootDevice(m_hVm, allow);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetAllowSelectBootDevice: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if ( ! param.ext_boot_dev.empty()) {
		ret = PrlVmCfg_SetExternalBootDevice(m_hVm, param.ext_boot_dev.c_str());
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetExternalBootDevice: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (!param.app_templates.empty()) {
		ret = set_templates(param.app_templates);
		if (ret)
			return ret;
	}
	if (param.ha_enable != -1) {
		PRL_BOOL enable = param.ha_enable;
		ret = PrlVmCfg_SetHighAvailabilityEnabled(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetHighAvailabilityEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.ha_prio != -1) {
		ret = PrlVmCfg_SetHighAvailabilityPriority(m_hVm, (PRL_UINT32)param.ha_prio);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetHighAvailabilityPriority: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	ret = set_confirmations_list(param.disp.cmd_require_pwd_list);
	if (ret)
		return ret;

	if (param.lock_on_suspend != -1) {
		PRL_BOOL enable = param.lock_on_suspend;
		ret = PrlVmCfg_SetLockGuestOnSuspendEnabled(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetLockGuestOnSuspendEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.isolate_vm != -1) {
		PRL_BOOL enable = param.isolate_vm;
		ret = PrlVmCfg_SetIsolatedVmEnabled(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetIsolatedVmEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.smart_guard != -1) {
		PRL_BOOL enable = param.smart_guard;
		ret = PrlVmCfg_SetSmartGuardEnabled(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetSmartGuardEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.sg_notify_before_create != -1) {
		PRL_BOOL enable = param.sg_notify_before_create;
		ret = PrlVmCfg_SetSmartGuardNotifyBeforeCreation(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetSmartGuardNotifyBeforeCreation: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.sg_interval != 0) {
		PRL_UINT32 val = param.sg_interval;
		ret = PrlVmCfg_SetSmartGuardInterval(m_hVm, val);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetSmartGuardInterval: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.sg_max_snapshots != 0) {
		PRL_UINT32 val = param.sg_max_snapshots;
		ret = PrlVmCfg_SetSmartGuardMaxSnapshotsCount(m_hVm, val);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetSmartGuardMaxSnapshotsCount: %s",
					get_error_str(ret).c_str());
		set_updated();
	}

	if (param.faster_vm != -1) {
		PRL_BOOL enable = param.faster_vm;
		ret = PrlVmCfg_SetDiskCacheWriteBack(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetDiskCacheWriteBack: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.adaptive_hypervisor != -1) {
		PRL_BOOL enable = param.adaptive_hypervisor;
		ret = PrlVmCfg_SetAdaptiveHypervisorEnabled(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetAdaptiveHypervisorEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.disable_winlogo != -1) {
		PRL_BOOL enable = param.disable_winlogo;
		ret = PrlVmCfg_SetSwitchOffWindowsLogoEnabled(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetSwitchOffWindowsLogoEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.auto_compress != -1) {
		PRL_BOOL enable = param.auto_compress;
		ret = PrlVmCfg_SetAutoCompressEnabled(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetAutoCompressEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.nested_virt != -1) {
		PRL_BOOL enable = param.nested_virt;
		ret = PrlVmCfg_SetNestedVirtualizationEnabled(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetNestedVirtualizationEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.pmu_virt != -1) {
		PRL_BOOL enable = param.pmu_virt;
		ret = PrlVmCfg_SetPMUVirtualizationEnabled(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetPMUVirtualizationEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.longer_battery_life != -1) {
		PRL_BOOL enable = param.longer_battery_life;
		ret = PrlVmCfg_SetLongerBatteryLifeEnabled(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetLongerBatteryLifeEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.battery_status != -1) {
		PRL_BOOL enable = param.battery_status;
		ret = PrlVmCfg_SetBatteryStatusEnabled(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetBatteryStatusEnabled: %s",
					get_error_str(ret).c_str());
		set_updated();
	}
	if (param.is_template != -1) {
		PRL_BOOL enable = param.is_template;
		ret = PrlVmCfg_SetTemplateSign(m_hVm, enable);
		if (ret)
			return prl_err(ret, "PrlVmCfg_SetTemplateSign: %s",
					get_error_str(ret).c_str());
		set_updated();
	}

	if (is_updated()) {
		if ((ret = commit_configuration(param)))
			return ret;

		load_config();
		if (param.backup_cmd != None) {
			if (param.backup_cmd == Add)
				prl_log(0, "Please note that any data written to the raw disk "
					"image will be lost when the image is detached.");
		} else if (param.dev.cmd == Add || param.dev.cmd == Set) {
			const std::string name = dev->get_name();
			if (get_dev_info())
				return 0;
			if (!(dev = m_DevList.find(name)))
				return 0;
			const char *action_str = param.dev.cmd == Add ? "Created" : "Configured";
			prl_log(0, "%s %s", action_str, dev->get_info().c_str());

		}

		prl_log(0, "\nThe %s has been successfully configured.",
				get_vm_type_str());
	}

	return 0;
}

PRL_VM_TOOLS_STATE PrlVm::get_tools_state(std::string &version) const
{
	PRL_RESULT ret;
	PrlHandle hResult;
	PRL_UINT32 resultCount = 0;
	PRL_VM_TOOLS_STATE state = PTS_UNKNOWN;

	PrlHandle hJob(PrlVm_GetToolsState(m_hVm));
	if ((ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &resultCount))) {
		prl_log(L_INFO, "PrlVm_GetToolsState: %s",
			get_error_str(ret).c_str());
		return state;
	}
	PrlHandle hTools;
	if ((ret = PrlResult_GetParam(hResult.get_handle(), hTools.get_ptr()))) {
		prl_log(L_INFO, "PrlResult_GetParam: %s",
			get_error_str(ret).c_str());
		return state;
	}

	if ((ret = PrlVmToolsInfo_GetState(hTools.get_handle(), &state)))
	{
		prl_log(L_INFO, "PrlVmToolsInfo_GetState: %s",
			get_error_str(ret).c_str());
		return state;
	}

	char buf[32] = "";
	unsigned int len = sizeof(buf);
	if (!(ret = PrlVmToolsInfo_GetVersion(hTools.get_handle(), buf, &len)))
		prl_log(L_INFO, "PrlVmToolsInfo_GetVersion: %s",
			get_error_str(ret).c_str());

	version = buf;

	return state;

}

int PrlVm::install_tools()
{
	PRL_RESULT ret;

	prl_log(0, "Installing...");

	std::string err;

	PrlHandle hJob(PrlVm_InstallTools(m_hVm));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to mount the guest tools ISO: %s", err.c_str());
	else
	{
		prl_log(0, "The guest tools ISO is mounted.\n"
			"Inside Linux VM, run 'mount /dev/cdrom /mnt/cdrom', then 'bash /mnt/cdrom/install'.\n"
			"Inside Windows VM, use autorun or open the drive in Explorer and run the installer."
		);
	}

	return ret;
}

void PrlVm::get_tools_info(PrlOutFormatter &f)
{
	std::string version;
	PRL_VM_TOOLS_STATE state;

	f.open("GuestTools", true);
	state = get_tools_state(version);
	switch (state) {
	case PTS_POSSIBLY_INSTALLED:
		f.add("state", "possibly_installed", true);
		break;
	case PTS_INSTALLED:
		f.add("state", "installed", true);
		break;
	case PTS_NOT_INSTALLED:
		f.add("state", "not_installed", true);
		break;
	case PTS_OUTDATED:
		f.add("state", "outdated", true);
		break;
	}

	if (!version.empty())
		f.add("version", version, true);

	f.close(true);
}

int PrlVm::migrate(const MigrateParam &param)
{
#if _LIN_
#define LLOGIN "/usr/share/pmigrate/pmigrate_local_login.py"

	int ret;
	int in[2], out[2];
	pid_t pid;
	char host[BUFSIZ];
	char buffer[BUFSIZ];
	ssize_t sz;
	std::vector <char *> ssh_argv;
	char *sessionid = NULL;
	unsigned int security_level = PSL_LOW_SECURITY;
	char *p;

	/* if session id set as argument, will use it */
	if (param.sessionid.size())
		return migrate_internal(param);

	ssh_argv.push_back( const_cast <char *> ("ssh") );
	ssh_argv.push_back( const_cast <char *> ("-T") );
	ssh_argv.push_back( const_cast <char *> ("-o") );
	ssh_argv.push_back( const_cast <char *> ("BatchMode=yes") );

	str_list_t::const_iterator it;
	for (it = param.ssh_opts.begin(); it != param.ssh_opts.end(); ++it)
		ssh_argv.push_back( const_cast <char *> (it->c_str()) );

	ssh_argv.push_back(host);
	ssh_argv.push_back( const_cast <char *> (LLOGIN) );
	ssh_argv.push_back(NULL);

	/* try to get session id via ssh */
	strncpy(host, param.dst.server.c_str(), sizeof(host));
	if ((pipe(in) != 0) || (pipe(out) != 0))
		return prl_err(-1, "pipe() error : '%m', ignored");

	pid = fork();
	if (pid == -1) {
		close(in[1]); close(out[0]);
		close(in[0]); close(out[1]);
		prl_log(0, "fork() error : '%m', ignored");
	} else if (pid == 0) {
		/* redirect stdout to out and stdin to in */
		close(in[1]); close(out[0]);
		dup2(in[0], STDIN_FILENO);
		dup2(out[1], STDOUT_FILENO);
		close(in[0]); close(out[1]);
		execvp(ssh_argv[0], &ssh_argv[0]);
		exit(1);
	}
	close(in[0]); close(out[1]);
	sz = read(out[0], buffer, sizeof(buffer)-1);
	if (sz > 0) {
		const char *token = "sessionid=";
		const char *token2 = "securitylevel=";

		buffer[sz] = '\0';
		if (strncmp(buffer, token, strlen(token)) == 0) {
			sessionid = buffer + strlen(token);
			if ((p = strchr(sessionid, '\n'))) {
				*p = '\0';
				if (strncmp(++p, token2, strlen(token2)) == 0) {
					p += strlen(token2);
					security_level = atoi(p);
				}
			}
		} else {
			prl_log(0, "SSH public key authentication failed: %s", buffer);
		}
	}

	/* start migration */
	if (sessionid) {
		MigrateParam newparam = param;
		newparam.sessionid = sessionid;
		if (security_level > newparam.security_level)
			newparam.security_level = security_level;
		ret = migrate_internal(newparam);
		// write 'enter' to ssh input to stop remote session
		write(in[1], "\n", 1);
		waitpid(pid, NULL, -1);
		close(in[1]); close(out[0]);
	} else {
		close(in[1]); close(out[0]);
		ret = migrate_internal(param);
	}

	return ret;
#else
	return migrate_internal(param);
#endif
}

int PrlVm::migrate_internal(const MigrateParam &param)
{
	int ret;
	PRL_HANDLE hJob;
	PrlSrv dst;
	unsigned int security_level;

	prl_log(0, "Migrate the %s %s on %s %s (%s)",
		get_vm_type_str(),
		get_name().c_str(),
		param.dst.server.c_str(),
		param.dst_id.c_str(),
		param.vm_location.c_str());

	security_level = m_srv.get_min_security_level();
	if (param.sessionid.empty()) {
		if ((ret = dst.login(param.dst)))
			return ret;
		if (security_level < dst.get_min_security_level())
			security_level = dst.get_min_security_level();

		if (security_level < param.security_level)
			security_level = param.security_level;

		prl_log(L_DEBUG, "security_level=%d", security_level);
		hJob = PrlVm_MigrateWithRename(m_hVm,
					dst.get_handle(),
					param.dst_id.c_str(), param.vm_location.c_str(),
					security_level | param.flags, 0, PRL_TRUE);
	} else {
		if (security_level < param.security_level)
			security_level = param.security_level;

		prl_log(L_DEBUG, "security_level=%d", security_level);
		hJob = PrlVm_MigrateWithRenameEx(m_hVm,
					param.dst.server.c_str(),
					param.dst.port,
					param.sessionid.c_str(),
					param.dst_id.c_str(), param.vm_location.c_str(),
					security_level | param.flags, 0, PRL_TRUE);
	}

	reg_event_callback(migrate_event_handler, 0);

	const PrlHook *h = get_cleanup_ctx().register_hook(migrate_cancel_job, m_hVm);
	std::string err;
	/* Fixme: timeout is unlimited */
	ret = get_job_retcode(hJob, err);
	unreg_event_callback(migrate_event_handler, 0);
	if (ret)
	{
		PrlHandle hResult;
		PRL_UINT32 resultCount = 0;
		int rc;

		prl_err(ret, "\nFailed to migrate the %s: %s",
				get_vm_type_str(), err.c_str());

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
		prl_log(0, "\nThe %s has been successfully migrated.",
				get_vm_type_str());
	get_cleanup_ctx().unregister_hook(h);
	PrlHandle_Free(hJob);

	return ret;

}

int PrlVm::get_uptime(PRL_UINT64 *uptime, std::string &start_date)
{
	PRL_RESULT ret;

	ret = PrlVmCfg_GetUptime(m_hVm, uptime);
	if (ret == 0)
	{
		char buf[BUFSIZ];
		PRL_UINT32 nBufSize = sizeof(buf);
		ret = PrlVmCfg_GetUptimeStartDate(m_hVm, buf, &nBufSize);
		if (ret == 0)
			start_date = buf;
	}
	return ret;
}

int PrlVm::get_real_ip(ip_list_t &ips, unsigned int timeout)
{
	PRL_RESULT ret;
	std::string err;

	PrlHandle hLoginJob(PrlVm_LoginInGuest(m_hVm, PRL_PRIVILEGED_GUEST_OS_SESSION, 0, 0));
	if ((ret = get_job_retcode(hLoginJob.get_handle(), err, timeout))) {
		prl_log(L_DEBUG, "PrlVm_LoginInGuest: %s", err.c_str());
		return ret;
	}

	PrlHandle hResult;
	if ((ret = PrlJob_GetResult(hLoginJob.get_handle(), hResult.get_ptr()))) {
		prl_log(L_DEBUG, "PrlJob_GetResult: %s",  get_error_str(ret).c_str());
		return ret;
	}

	PrlHandle hVmGuest;
	if ((ret = PrlResult_GetParam(hResult.get_handle(), hVmGuest.get_ptr()))) {
		prl_log(L_DEBUG, "PrlResult_GetParam: %s", get_error_str(ret).c_str());
		return ret;
	}

	do {

		PRL_UINT32 count;
		PrlHandle hJob(PrlVmGuest_GetNetworkSettings(hVmGuest.get_handle(), 0));
		if ((ret = get_job_result(hJob.get_handle(), hResult.get_ptr(), &count, timeout))) {
			prl_log(L_DEBUG, "PrlVmGuest_GetNetworkSettings; %s", get_error_str(ret).c_str());
			break;
		}

		PrlHandle hSrvConfig;
		if ((ret = PrlResult_GetParam(hResult.get_handle(), hSrvConfig.get_ptr()))) {
			prl_log(L_DEBUG, "PrlResult_GetParam: %s", get_error_str(ret).c_str());
			break;
		}
		PRL_UINT32 dev_count;
		if ((ret = PrlSrvCfg_GetNetAdaptersCount( hSrvConfig.get_handle(), &dev_count))) {
			prl_log(L_DEBUG, "PrlSrvCfg_GetNetAdaptersCount: %s", get_error_str(ret).c_str());
			break;
		}
		for (unsigned idx = 0; idx < dev_count; idx++) {
			PrlHandle hDev;
			if ((ret = PrlSrvCfg_GetNetAdapter(hSrvConfig.get_handle(), idx, hDev.get_ptr()))) {
				prl_log(L_DEBUG, "PrlSrvCfg_GetNetAdapter: %s", get_error_str(ret).c_str());
				break;
			}
			PrlHandle h;

			if ((ret = PrlSrvCfgNet_GetNetAddresses(hDev.get_handle(), h.get_ptr()))) {
				prl_log(L_DEBUG, "PrlVmDevNet_GetNetAddresses: %s", get_error_str(ret).c_str());
				break;
			}
			PRL_UINT32 count;
			PrlStrList_GetItemsCount(h.get_handle(), &count);
			for (unsigned int i = 0; i < count; i++) {
				char buf[128];
				unsigned int len = sizeof(buf);
				if ((ret = PrlStrList_GetItem(h.get_handle(), i, buf, &len))) {
					prl_log(L_DEBUG, "PrlStrList_GetItem: %s", get_error_str(ret).c_str());
					continue;
				}
				ips.add(buf);
			}
		}
	} while (0);

	PrlHandle hJob(PrlVmGuest_Logout(hVmGuest.get_handle(), 0));
	get_job_retcode(hJob.get_handle(), err);

	return ret;
}

void PrlVm::get_optimization_info(PrlOutFormatter &f)
{
	PRL_RESULT ret;
	PRL_BOOL bVal = PRL_FALSE;

	if ((ret = PrlVmCfg_IsDiskCacheWriteBack(m_hVm, &bVal)))
	{
		prl_log(L_INFO, "PrlVmCfg_IsDiskCacheWriteBack: %s",
			get_error_str(ret).c_str());
	}
	f.add("Faster virtual machine", (bVal ? "on" : "off"));

	if ((ret = PrlVmCfg_IsAdaptiveHypervisorEnabled(m_hVm, &bVal)))
	{
		prl_log(L_INFO, "PrlVmCfg_IsAdaptiveHypervisorEnabled: %s",
			get_error_str(ret).c_str());
	}
	f.add("Adaptive hypervisor", (bVal ? "on" : "off"));

	if ((ret = PrlVmCfg_IsSwitchOffWindowsLogoEnabled(m_hVm, &bVal)))
	{
		prl_log(L_INFO, "PrlVmCfg_IsSwitchOffWindowsLogoEnabled: %s",
			get_error_str(ret).c_str());
	}
	f.add("Disabled Windows logo", (bVal ? "on" : "off"));

	if ((ret = PrlVmCfg_IsAutoCompressEnabled(m_hVm, &bVal)))
	{
		prl_log(L_INFO, "PrlVmCfg_IsAutoCompressEnabled: %s",
			get_error_str(ret).c_str());
	}
	f.add("Auto compress virtual disks", (bVal ? "on" : "off"));

	if ((ret = PrlVmCfg_IsNestedVirtualizationEnabled(m_hVm, &bVal)))
	{
		prl_log(L_INFO, "PrlVmCfg_IsNestedVirtualizationEnabled: %s",
			get_error_str(ret).c_str());
	}
	f.add("Nested virtualization", (bVal ? "on" : "off"));

	if ((ret = PrlVmCfg_IsPMUVirtualizationEnabled(m_hVm, &bVal)))
	{
		prl_log(L_INFO, "PrlVmCfg_IsPMUVirtualizationEnabled: %s",
			get_error_str(ret).c_str());
	}
	f.add("PMU virtualization", (bVal ? "on" : "off"));
}

int PrlVm::set_confirmations_list(const std::map<std::string , bool >& cmd_list)
{
	std::vector< std::pair<PRL_ALLOWED_VM_COMMAND, bool > > vCmds;

	std::map<std::string , bool >::const_iterator it;
	for(it = cmd_list.begin(); it != cmd_list.end(); ++it)
	{
		if ((*it).first == "exit-fullscreen")
			vCmds.push_back(std::make_pair(PAR_VM_GUI_VIEW_MODE_CHANGE_ACCESS, (*it).second));
		if ((*it).first == "change-vm-state")
		{
			vCmds.push_back(std::make_pair(PAR_VM_START_ACCESS , (*it).second));
			vCmds.push_back(std::make_pair(PAR_VM_STOP_ACCESS , (*it).second));
			vCmds.push_back(std::make_pair(PAR_VM_PAUSE_ACCESS , (*it).second));
			vCmds.push_back(std::make_pair(PAR_VM_RESET_ACCESS , (*it).second));
			vCmds.push_back(std::make_pair(PAR_VM_SUSPEND_ACCESS , (*it).second));
			vCmds.push_back(std::make_pair(PAR_VM_RESUME_ACCESS , (*it).second));
			vCmds.push_back(std::make_pair(PAR_VM_DROPSUSPENDEDSTATE_ACCESS , (*it).second));
			vCmds.push_back(std::make_pair(PAR_VM_RESTART_GUEST_ACCESS , (*it).second));
		}
		if ((*it).first == "manage-snapshots")
		{
			vCmds.push_back(std::make_pair(PAR_VM_CREATE_SNAPSHOT_ACCESS , (*it).second));
			vCmds.push_back(std::make_pair(PAR_VM_SWITCH_TO_SNAPSHOT_ACCESS , (*it).second));
			vCmds.push_back(std::make_pair(PAR_VM_DELETE_SNAPSHOT_ACCESS , (*it).second));
		}
		if ((*it).first == "change-guest-pwd")
			vCmds.push_back(std::make_pair(PAR_VM_CHANGE_GUEST_OS_PASSWORD_ACCESS , (*it).second));
	}

	PRL_HANDLE hList = PRL_INVALID_HANDLE;
	PRL_RESULT ret = PrlVmCfg_GetConfirmationsList(m_hVm, &hList);
	if (ret)
		return prl_err(ret, "PrlVmCfg_GetConfirmationsList: %s",
				get_error_str(ret).c_str());

	ret = edit_allow_command_list(hList, vCmds);
	if (ret)
		return ret;

	ret = PrlVmCfg_SetConfirmationsList(m_hVm, hList);
	if (ret)
		return prl_err(ret, "PrlVmCfg_SetConfirmationsList: %s",
				get_error_str(ret).c_str());

	set_updated();

	return 0;
}

std::string PrlVm::get_netdev_name()
{
	std::string out;

	get_dev_info();

	BOOST_FOREACH(PrlDevNet *net, m_DevNetList) {
		if (!out.empty())
			out += ",";
		if (net != NULL)
			out += net->get_veth_name();
	}
	return out;
}

bool PrlVm::get_ha_enable() const
{
	PRL_RESULT ret;
	PRL_BOOL enabled = PRL_FALSE;
	ret = PrlVmCfg_IsHighAvailabilityEnabled(m_hVm, &enabled);
	if (PRL_FAILED(ret))
		prl_log(L_INFO, "PrlVmCfg_IsHighAvailabilityEnabled: %s",
			get_error_str(ret).c_str());
	return prl_bool(enabled);
}

unsigned int PrlVm::get_ha_prio() const
{
	PRL_RESULT ret;
	PRL_UINT32 prio = 0;
	ret = PrlVmCfg_GetHighAvailabilityPriority(m_hVm, &prio);
	if (PRL_FAILED(ret))
		prl_log(L_INFO, "PrlVmCfg_GetHighAvailabilityPriority: %s",
			get_error_str(ret).c_str());
	return prio;
}

void PrlVm::get_high_availability_info(PrlOutFormatter &f)
{
	f.open("High Availability", true);
	f.add("enabled", get_ha_enable() ? "yes" : "no", true);
	f.add("prio", get_ha_prio(), "", true);
	f.close(true);
}

void PrlVm::append_configuration(PrlOutFormatter &f)
{
	PRL_APPLICATION_MODE appMode = PAM_UNKNOWN;
	PrlApi_GetAppMode(&appMode);

	get_vm_info();

	f.add_uuid("ID", get_id().c_str());

#ifdef _WIN_
	if (get_vm_type() == PVT_CT)
#endif
	{
		f.add("EnvID", get_ctid());
	}

	f.add("Name", get_name());
	f.add("Description", get_desc());
	f.add("Type", get_vm_type_str());
	f.add("State", vmstate2str(get_state()));
	f.add("OS", get_dist());
	f.add("Template", m_template ? "yes" : "no");

	PRL_UINT64 uptime;
	std::string start_date;
	if (get_uptime(&uptime, start_date) == 0)
		f.add_uptime("Uptime", uptime, start_date);

	std::string x;
	get_home_dir(x);
	f.add("Home", x);

	f.add("Owner", m_owner);
	get_tools_info(f);
	f.add("Autostart", get_autostart_info());
	f.add("Autostop", get_autostop_info());
	f.add("Autocompact", (get_autocompact() ? "on" : "off"));
	f.add("Undo disks", get_undo_disks_info());
	f.add("Boot order", get_bootdev_info());
	f.add("EFI boot", (m_efi_boot ? "on" : "off"));
	f.add("Allow select boot device", (m_select_boot_dev ? "on" : "off"));
	f.add("External boot device", m_ext_boot_dev);
	get_vnc_info(f);
	f.add("Remote display state", (m_is_vnc_server_started ? "running" : "stopped"));

	f.open("Hardware");
	f.open("cpu", true);

	unsigned int cpu_count = get_cpu_count();
	if (cpu_count == PRL_CPU_UNLIMITED) // unlimited
		f.add("cpus", "unlimited", true);
	else
		f.add("cpus", cpu_count, "", true);

	f.add("VT-x", is_vtx_enabled());
	f.add("hotplug", is_cpu_hotplug_enabled());

	const char *tmp;
	std::string stmp;
	switch (get_cpu_acc_level()) {
	case PVA_ACCELERATION_DISABLED:
		 f.add("accl", "disable", true);
		break;
	case PVA_ACCELERATION_NORMAL:
		 f.add("accl", "normal", true);
		break;
	case PVA_ACCELERATION_HIGH:
		 f.add("accl", "high", true);
		break;
	}

	switch (get_cpu_mode()) {
	case PCM_CPU_MODE_32:
		f.add("mode", "32", true);
		break;
	case PCM_CPU_MODE_64:
		f.add("mode", "64", true);
		break;
	}

	unsigned int cpuunits;
	if (!get_cpuunits(&cpuunits) && cpuunits)
		f.add("cpuunits", cpuunits, "", true);

	PRL_CPULIMIT_DATA cpulimit = {0, PRL_CPULIMIT_MHZ};
	if (get_cpulimit(&cpulimit) == 0 &&
	    cpulimit.value != 0) {
		if (cpulimit.type == PRL_CPULIMIT_MHZ)
			tmp = "Mhz";
		else
			tmp = "%";
		f.add("cpulimit", cpulimit.value, tmp, true);

		unsigned int limitmode;
		if (get_cpulimitmode(&limitmode) == 0)
		{
			f.add("cpulimitmode", limitmode == PRL_VM_CPULIMIT_GUEST
				? "guest" : "full", true);
		}
	}

	unsigned int val;
	if (!get_ioprio(&val) && val != (unsigned int) -1)
		f.add("ioprio", val, "", true);

	if (!get_iolimit(&val) && val != (unsigned int) -1) {
		f.add("iolimit", ui2string(val), "", true);
	}

	if (!get_iopslimit(&val) &&
			val != (unsigned int) -1 && val != 0) {
		f.add("iopslimit", val, "", true);
	}

	std::string cpumask = get_cpumask();
	if (!cpumask.empty())
		f.add("mask", cpumask, true);

	std::string nodemask;
	if (!get_nodemask(nodemask) && !nodemask.empty())
		f.add("nodemask", nodemask, true);

	f.close(true);

	f.open("memory", true);
	f.add("size", get_memsize(), "Mb", true, true);
	f.add("hotplug", is_mem_hotplug_enabled());
	f.close(true);

	std::string v3d = "off";
	switch(get_3d_acceleration())
	{
	case P3D_ENABLED_HIGHEST:
		v3d = "highest"; break;
	case P3D_ENABLED_DX9:
		v3d = "DirectX 9.x"; break;
	default:;
	}

	f.open("video", true);
	f.add("size", get_videosize(), "Mb", true, true);
	f.add("3d acceleration", v3d, true);
	f.add("vertical sync", is_vertical_sync_enabled() ? "yes" : "no", true);
	f.close(true);

	get_memguarantee(f);

	{
		PrlDevList::const_iterator it = m_DevList.begin(),
			eit = m_DevList.end();

		for (; it != eit; ++it)
			(*it)->append_info(f);
	}
	f.close();

	FeaturesParam feature = get_features();
	if (get_vm_type() == PVT_CT) {
		f.add("Features", feature2str(feature));
	} else {
		if (feature.mask & FT_SmartMount) {
			f.open_shf("SmartMount", true);

			f.open("removable drives", true);
			f.add_isenabled(prl_bool(feature.mask & FT_SmartMountRemovableDrives));
			f.close(true);

			f.open("CD/DVD drives", true);
			f.add_isenabled(prl_bool(feature.mask & FT_SmartMountDVDs));
			f.close(true);

			f.open("network shares", true);
			f.add_isenabled(prl_bool(feature.mask & FT_SmartMountNetworkShares));
			f.close(true);

			f.close();
		} else {
			f.open_shf("SmartMount", false);
			f.close();
		}
	}

	get_optimization_info(f);

	PRL_RESULT ret;
	PRL_BOOL bEnabled;
	if (PrlVmCfg_IsOfflineManagementEnabled(m_hVm, &bEnabled) == 0) {
		f.open_shf("Offline management", prl_bool(bEnabled));
		PrlHandle hList;
		if (bEnabled && PrlVmCfg_GetOfflineServices(m_hVm, hList.get_ptr()) == 0) {
			PRL_UINT32 count;
			PrlStrList_GetItemsCount(hList.get_handle(), &count);
			stmp = "";
			for (unsigned int i = 0; i < count; i++) {
				char buf[256];
				unsigned int len = sizeof(buf);
				if ((ret = PrlStrList_GetItem(hList.get_handle(), i, buf, &len))) {
					prl_log(L_DEBUG, "PrlStrList_GetItem: %s", get_error_str(ret).c_str());
					continue;
				}
				stmp += "'";stmp += buf; stmp += "' ";
			}
			f.add("services", stmp);
		}
		f.close();
	}
	std::string hostname = get_hostname();
	if (!hostname.empty())
		f.add("Hostname", hostname);

	std::string dnsservers = get_nameservers();
	if (!dnsservers.empty())
		f.add("DNS Servers", dnsservers);

	std::string searchdomains = get_searchdomains();
	if (!searchdomains.empty())
		f.add("Search Domains", searchdomains);

	if (is_full_info_mode())
	{
		Netfilter::Mode m = get_netfilter();
		if (m.isValid())
			f.add("Netfilter", m.name);

		get_high_availability_info(f);
	}

}

void PrlVm::clear()
{
	if (m_hVm) {
		PrlHandle_Free(m_hVm);
		m_hVm = 0;
	}
	m_DevList.del();
}

int PrlVm::internal_cmd(char **argv)
{
	PRL_RESULT ret;
	std::string err;

	if (!argv || !argv[0])
		return prl_err(-1, "Command name is not specified.");

	prl_log(0, "Executing command '%s' ...", argv[0]);

	PrlHandle hArgs;
	PrlApi_CreateStringsList(hArgs.get_ptr());

	for (char **p = argv + 1; *p; p++)
		PrlStrList_AddItem(hArgs.get_handle(), *p);

	PrlHandle hJob(PrlVm_InternalCommand(m_hVm, argv[0], hArgs.get_handle()));

	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		return prl_err(ret, "Executing command '%s' failed. Error %s", argv[0], err.c_str());

	prl_log(0, "Executing command '%s' finished successfully.", argv[0]);
	return ret;
}

PrlVm::~PrlVm()
{
	clear();
}


int VncParam::set_mode(const std::string &str)
{
	if (str == "off")
		mode = PRD_DISABLED;
	else if (str == "auto")
		mode = PRD_AUTO;
	else if (str == "manual")
		mode = PRD_MANUAL;
	else
		return -1;
	return 0;
}

std::string VncParam::mode2str() const
{
	std::string out;

	switch (mode) {
	case PRD_DISABLED:
		out = "off";
		break;
	case PRD_AUTO:
		out = "auto";
		break;
	case PRD_MANUAL:
		out = "manual";
		break;
	}
	return out;
}

std::string VncParam::get_info() const
{
	PrlOutFormatterPlain f;
	append_info(f);
	return f.get_buffer();
}

void VncParam::append_info(PrlOutFormatter &f) const
{
	f.open("Remote display", true);
	if (mode != -1)
		f.add("mode", mode2str(), true);

	if (port > 0)
		f.add("port", port, "", true);

	if (!address.empty())
		f.add("address", address, true);

	f.close(true);
}

int PrlVm::set_templates(str_list_t lstTemplates)
{
	int ret;
	PrlHandle hAppTmpls;

	str_list2handle(lstTemplates, hAppTmpls);
	if ((ret = PrlVmCfg_SetAppTemplateList(m_hVm, hAppTmpls)))
		return prl_err(ret, "PrlVmCfg_SetAppTemplates",
				get_error_str(ret).c_str());
	prl_log(0, "Configure Templates: %s", lstTemplates.to_str().c_str());
	set_updated();

	return 0;
}

int PrlVm::load_config()
{
	PRL_RESULT ret;
	std::string err;

	PrlHandle hJob(PrlVm_RefreshConfig(m_hVm));
	if ((ret = get_job_retcode(hJob.get_handle(), err)))
		prl_err(ret, "Failed to load config: %s", err.c_str());
	return ret;
}

int PrlVm::attach_backup_disk(const std::string& id, const std::string& disk,
	const DevInfo *param)
{
	DevInfo dev_info;
	if (param)
		dev_info = *param;
	dev_info.type = DEV_HDD;
	dev_info.storage_url = StorageUrl(id, disk).getUrl();
	int ret;
	if ((ret = create_dev(dev_info.type, dev_info)))
		return ret;
	set_updated();
	return 0;
}

void PrlVm::search_attached_backups(std::list<PrlDevHdd *>& disks)
{
	for (PrlDevList::const_iterator dev = m_DevList.begin(), end = m_DevList.end();
		dev != end; ++dev) {
		if ((*dev)->get_type() != DEV_HDD)
			continue;
		PrlDevHdd *disk = (PrlDevHdd *)*dev;
		StorageUrl url;
		disk->get_storage_url(url);
		if (url.isEmpty())
			continue;
		disks.push_back(disk);
	}
}

struct MatchBackup {
	MatchBackup(const std::string& id, const std::string& disk)
		: m_id(id), m_disk(disk)
	{
	}

	bool operator()(const PrlDevHdd *disk)
	{
		StorageUrl url;
		disk->get_storage_url(url);
		return (url.getBackupId() == m_id && url.getDiskName() == m_disk);
	}
private:
	const std::string& m_id;
	const std::string& m_disk;
};

int PrlVm::attach_backup_disks(const CmdParamData& param)
{
	if (!param.backup_disk.empty())
		return attach_backup_disk(param.backup_id, param.backup_disk, &param.dev);

	int ret;
	std::list<std::string> disks;
	if ((ret = m_srv.get_backup_disks(param.backup_id, disks)))
		return ret;
	std::list<PrlDevHdd *> existing;
	search_attached_backups(existing);
	for (std::list<std::string>::const_iterator it = disks.begin();
		it != disks.end(); ++it) {
		std::list<PrlDevHdd *>::const_iterator p = std::find_if(existing.begin(),
			existing.end(), MatchBackup(param.backup_id, *it));
		if (p != existing.end()) {
			prl_log(0, "Disk '%s' from backup '%s' is already attached",
				it->c_str(), param.backup_id.c_str());
			continue;
		}
		if ((ret = attach_backup_disk(param.backup_id, *it, NULL)))
			return ret;
	}
	return 0;
}

int PrlVm::detach_backup_disks(const std::string& id)
{
	int ret;
	std::list<PrlDevHdd *> disks;
	search_attached_backups(disks);
	if (disks.empty()) {
		prl_log(0, "Backups are not attached");
		return 0;
	}
	bool removed = false;
	for (std::list<PrlDevHdd *>::const_iterator disk = disks.begin(), end = disks.end();
		disk != end; ++disk) {
		StorageUrl url;
		(*disk)->get_storage_url(url);
		if (id != "all" && url.getBackupId() != id)
			continue;
		if ((ret = (*disk)->remove()))
			return ret;
		set_updated();
		removed = true;
	}
	if (!removed)
		prl_log(0, "Disks from backup '%s' are not attached", id.c_str());
	return 0;
}

