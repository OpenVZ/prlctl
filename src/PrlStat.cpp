/*
 * Copyright (c) 2015 Parallels IP Holdings GmbH
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
 * Our contact details: Parallels IP Holdings GmbH, Vordergasse 59, 8200
 * Schaffhausen, Switzerland.
 */

#include <PrlApiDisp.h>
#include <PrlApiStat.h>
#include <PrlPerfCounters.h>

#include "PrlSrv.h"
#include "Logger.h"
#include "Utils.h"
#include "PrlCleanup.h"

#ifdef _WIN_
#include <windows.h>
#include <conio.h>
#define snprintf _snprintf
#else
#include <unistd.h>
#include <stdio.h>
inline int _getch() { return getchar() ; }
#endif

#include <string.h>
#include <time.h>

static PRL_RESULT print_perfstats(PRL_HANDLE handle, const CmdParamData &param)
{
	(void)param;
	unsigned int param_count;
	PRL_RESULT ret = PrlEvent_GetParamsCount(handle, &param_count);
	if (PRL_FAILED(ret))
		return prl_err(ret, "PrlEvent_GetParamsCount returned the following error: %s",
				get_error_str(ret).c_str());

	if (!param_count)
		return PRL_ERR_SUCCESS;

	// Print VM uuid if necessary
	if (param.list_all) {
		char uuid[NORMALIZED_UUID_LEN + 1] = {0};
		PRL_UINT32 size = sizeof(uuid);
		PRL_HANDLE hVm;
		if (PRL_SUCCEEDED(PrlEvent_GetVm(handle, &hVm))) {
			PrlVmCfg_GetUuid(hVm, uuid, &size);
			PrlHandle_Free(hVm);
		}
		printf("%s\n", uuid);
	}
	for (unsigned int ndx = 0; ndx < param_count; ++ndx) {
		PrlHandle hPrm;
		ret = PrlEvent_GetParam(handle, ndx, hPrm.get_ptr());
		if (PRL_FAILED(ret))
			return prl_err(ret, "PrlEvent_GetParam returned the following error: %s",
					get_error_str(ret).c_str());

		char name_buff[1024];
		unsigned int len = sizeof(name_buff) - 1;
		ret = PrlEvtPrm_GetName(hPrm.get_handle(), name_buff, &len);
		if (PRL_FAILED(ret))
			return prl_err(ret, "PrlEvtPrm_GetName returned the following error: %s",
					get_error_str(ret).c_str());
		char val_buff[1024];
		len = sizeof(val_buff) - 1;
		val_buff[len] = 0;

		PRL_PARAM_FIELD_DATA_TYPE nFieldType = PFD_UNKNOWN;
		PrlEvtPrm_GetType(hPrm.get_handle(), &nFieldType);
		if (nFieldType == PFD_BINARY) {
			if (strncmp(name_buff, PRL_NET_CLASSFUL_TRAFFIC_PTRN, sizeof(PRL_NET_CLASSFUL_TRAFFIC_PTRN) - 1) == 0) {
				PRL_STAT_NET_TRAFFIC net_stat_buf;
				len = sizeof(PRL_STAT_NET_TRAFFIC);

				if (PrlEvtPrm_GetBuffer(hPrm.get_handle(), &net_stat_buf, &len) == 0) {

					for (unsigned int i = 0; i < PRL_TC_CLASS_MAX; i++)
						fprintf(stdout, "\t%20s %2d %20llu %10u %20llu %10u\n", name_buff, i,
								net_stat_buf.incoming[i], net_stat_buf.incoming_pkt[i],
								net_stat_buf.outgoing[i], net_stat_buf.outgoing_pkt[i]);
				}
			}
		} else {
			ret = PrlEvtPrm_ToString(hPrm.get_handle(), val_buff, &len);
			if (PRL_FAILED(ret))
				return prl_err(ret, "PrlEvtPrm_ToString returned the following error: %s",
						get_error_str(ret).c_str());

			fprintf(stdout, "\t%s:\t%s\n", name_buff, val_buff);
		}

	}

	return PRL_ERR_SUCCESS;
}

static PRL_RESULT perfstats_callback(PRL_HANDLE handle, void *user_data, PRL_EVENT_TYPE process_type)
{
	PrlHandle clean(handle);
	PRL_RESULT ret;
	PRL_EVENT_TYPE e_type;

	const CmdParamData &param = *(const CmdParamData*)user_data;

	ret = PrlEvent_GetType(handle, &e_type);
	if (PRL_FAILED(ret)) {
		prl_log(L_DEBUG, "Warning! PrlSrv_Logoff failed: %s", get_error_str(ret).c_str());
		return PRL_ERR_SUCCESS;
	}

	if (e_type != process_type)
		return PRL_ERR_SUCCESS;

	print_perfstats(handle, param);
	return PRL_ERR_SUCCESS;
}

static PRL_RESULT perfstats_srv_callback(PRL_HANDLE handle, void *user_data)
{
	return perfstats_callback(handle, user_data, PET_DSP_EVT_PERFSTATS) ;
}

static PRL_RESULT perfstats_vm_callback(PRL_HANDLE handle, void *user_data)
{
	return perfstats_callback(handle, user_data, PET_DSP_EVT_VM_PERFSTATS) ;
}

int PrlSrv::print_statistics(const CmdParamData &param, PrlVm *vm)
{
	PRL_RESULT ret;
	std::string err;

	if (param.list_all && !vm) {
		ret = update_vm_list(param.vmtype);
		if (PRL_FAILED(ret))
			return ret;
	}

	const PrlHook *hHook = get_cleanup_ctx().register_hook(call_exit, NULL);

	if (!param.statistics.loop) {
		if (param.action == SrvPerfStatsAction) {
			PrlHandle hEvent;
			PrlHandle hJob(PrlSrv_GetPerfStats(get_handle(), param.statistics.filter.c_str()));
			if (PRL_FAILED(ret = get_job_result_object(hJob.get_handle(), hEvent.get_ptr())))
				return ret;
			print_perfstats(hEvent.get_handle(), param);
		}

		if (param.action == VmPerfStatsAction || param.list_all) {
			for (PrlVmList::iterator it=m_VmList.begin(), end=m_VmList.end(); it!=end; ++it) {
				if (!param.list_all && (*it)!=vm)
					continue;
				PrlHandle hEvent;
				PrlHandle hJob(PrlVm_GetPerfStats((*it)->get_handle(), param.statistics.filter.c_str()));
				if (PRL_FAILED(ret = get_job_result_object(hJob.get_handle(), hEvent.get_ptr())))
					return ret;
				print_perfstats(hEvent.get_handle(), param);
			}
		}
	} else {
		if (param.action == SrvPerfStatsAction) {
			ret = PrlSrv_RegEventHandler(get_handle(), &perfstats_srv_callback, (void*)&param);
			if (PRL_FAILED(ret))
				return prl_err(ret, "PrlSrv_RegEventHandler returned the following error: %s",
						get_error_str(ret).c_str());
			PrlHandle hJob(PrlSrv_SubscribeToPerfStats(get_handle(), param.statistics.filter.c_str()));
			if (PRL_FAILED(get_job_retcode_predefined(hJob.get_handle(), err)))
				return prl_err(ret, "PrlSrv_SubscribeToPerfStats returned the following error: %s", err.c_str());
		}

		if (param.action == VmPerfStatsAction || param.list_all) {
			for (PrlVmList::iterator it = m_VmList.begin(), end = m_VmList.end(); it != end; ++it) {
				if (!param.list_all && (*it) != vm)
					continue;

				ret = PrlVm_RegEventHandler((*it)->get_handle(), &perfstats_vm_callback, (void*)&param);
				if (PRL_FAILED(ret))
					return prl_err(ret, "PrlVm_RegEventHandler returned the following error: %s",
							get_error_str(ret).c_str());
				PrlHandle hJob(PrlVm_SubscribeToPerfStats((*it)->get_handle(), param.statistics.filter.c_str()));
				if (PRL_FAILED(get_job_retcode_predefined(hJob.get_handle(), err)))
					return prl_err(ret, "PrlVm_SubscribeToPerfStats returned the following error: %s", err.c_str());
			}
		}

		int ch = 0 ;
		while (ch!=0x0A && ch!=0x0D && ch!=0x03) {
			ch = _getch();
		}
		fprintf(stdout, "\n");
		get_cleanup_ctx().unregister_hook(hHook);
	}
	return 0;
}

