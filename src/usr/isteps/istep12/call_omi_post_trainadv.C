/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/isteps/istep12/call_omi_post_trainadv.C $             */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2015,2023                        */
/* [+] International Business Machines Corp.                              */
/*                                                                        */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/*                                                                        */
/* IBM_PROLOG_END_TAG                                                     */
/**
 * @file    call_omi_post_trainadv.C
 *
 *  Support file for Istep 12.9 OMI post train routines
 *      p10_io_omi_post_trainadv
 *
 */
#include    <stdint.h>

#include    <trace/interface.H>
#include    <initservice/taskargs.H>
#include    <errl/errlentry.H>

#include    <isteps/hwpisteperror.H>
#include    <errl/errludtarget.H>

#include    <initservice/isteps_trace.H>
#include    <istepHelperFuncs.H>          // captureError

#include    <fapi2/plat_hwp_invoker.H>

//HWP
#include    <p10_io_omi_post_trainadv.H>

#include    <chipids.H>                     // POWER_CHIPID::ODYSSEY_16

#include    <sbeio/sbeioif.H>

using   namespace   ISTEP;
using   namespace   ISTEP_ERROR;
using   namespace   ISTEPS_TRACE;
using   namespace   TARGETING;
using   namespace   SBEIO;

namespace ISTEP_12
{

void* call_omi_post_trainadv (void *io_pArgs)
{
    IStepError l_StepError;
    errlHndl_t l_err = nullptr;

    TargetHandleList l_procTargetList;
    getAllChips(l_procTargetList, TYPE_PROC);
    TRACFCOMP( g_trac_isteps_trace, "call_omi_post_trainadv entry. "
        "%d PROCs found", l_procTargetList.size());

    // 12.9.a p10_io_omi_post_trainadv.C
    //        - Debug routine for IO characterization
    for (const auto & l_proc_target : l_procTargetList)
    {
        TRACFCOMP( g_trac_isteps_trace,
            "p10_io_omi_post_trainadv HWP target HUID 0x%.8x",
            get_huid(l_proc_target));

        //  call the HWP with each target
        fapi2::Target <fapi2::TARGET_TYPE_PROC_CHIP> l_fapi_proc_target
                (l_proc_target);

        FAPI_INVOKE_HWP(l_err, p10_io_omi_post_trainadv, l_fapi_proc_target);

        //  process return code.
        if ( l_err )
        {
            TRACFCOMP( g_trac_isteps_trace,
                "ERROR : call p10_io_omi_post_trainadv HWP(): failed on "
                "target 0x%08X. " TRACE_ERR_FMT,
                get_huid(l_proc_target),
                TRACE_ERR_ARGS(l_err));

            // Capture error
            captureErrorOcmbUpdateCheck(l_err, l_StepError, HWPF_COMP_ID, l_proc_target);
        }
        else
        {
            TRACFCOMP( g_trac_isteps_trace,
                     "SUCCESS : p10_io_omi_post_trainadv HWP");
        }
    }

    // get RUN_ODY_HWP_FROM_HOST
    const auto l_runOdyHwpFromHost =
       TARGETING::UTIL::assertGetToplevelTarget()->getAttr<ATTR_RUN_ODY_HWP_FROM_HOST>();

    // 12.9.b ody_io_omi_post_trainadv.C
    //        - Debug routine for IO characterization
    for (const auto & l_proc_target : l_procTargetList)
    {
        if (l_proc_target->getAttr< ATTR_CHIP_ID>() != POWER_CHIPID::ODYSSEY_16)
        {
            continue;
        }

        TRACFCOMP( g_trac_isteps_trace,
                "ody_io_omi_post_trainadv HWP target HUID 0x%.8x",
                get_huid(l_proc_target));

        if (l_runOdyHwpFromHost)
        {
            fapi2::Target <fapi2::TARGET_TYPE_PROC_CHIP> l_fapi_proc_target(l_proc_target);
            /* This is not invoked, nor is it in plan.
            FAPI_INVOKE_HWP(l_err, ody_io_omi_post_trainadv, l_fapi_proc_target);
             */
        }
        else
        {
            l_err = sendExecHWPRequest(l_proc_target, IO_ODY_OMI_POSTTRAIN_ADV);
        }

        if ( l_err )
        {
            TRACFCOMP( g_trac_isteps_trace,
                "ERROR : call ody_io_omi_post_trainadv HWP(): failed on "
                "target 0x%08X. " TRACE_ERR_FMT,
                get_huid(l_proc_target),
                TRACE_ERR_ARGS(l_err));

            // Capture error
            captureErrorOcmbUpdateCheck(l_err, l_StepError, HWPF_COMP_ID, l_proc_target);
        }
        else
        {
            TRACFCOMP( g_trac_isteps_trace,
                     "SUCCESS : ody_io_omi_post_trainadv HWP");
        }
    }

    TRACFCOMP( g_trac_isteps_trace, "call_omi_post_trainadv exit" );

    // end task, returning any errorlogs to IStepDisp
    return l_StepError.getErrorHandle();
}

};
