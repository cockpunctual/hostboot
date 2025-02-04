/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/diag/prdf/plat/prdfPlatServices_rt.C $                */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2016,2023                        */
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
 * @file  prdfPlatServices_rt.C
 * @brief Wrapper code for external interfaces used by PRD.
 *
 * This file contains code that is strictly specific to Hostboot. All code that
 * is common between FSP and Hostboot should be in the respective common file.
 */

// Framework includes
#include <prdfErrlUtil.H>
#include <prdfTrace.H>

// Platform includes
#include <prdfOcmbDataBundle.H>
#include <prdfMemScrubUtils.H>
#include <prdfPlatServices.H>

// Other includes
#include <runtime/interface.h>
#include <p10_l3err_extract.H>
#include <p10_l2err_extract.H>
#include <p10_l3err_linedelete.H>
#include <p10_l2err_linedelete.H>
#include <p10_proc_gettracearray.H>
#include <pm_common_ext.H>
#include <p10_stop_api.H>

#include <rt_todintf.H>

#include <exp_rank.H>
#include <kind.H>
#include <hwp_wrappers.H>
#include <plat_hwp_invoker.H>

//------------------------------------------------------------------------------

using namespace TARGETING;

namespace PRDF
{

namespace PlatServices
{

//##############################################################################
//##                        Memory specific functions
//##############################################################################

void __dyndealloc( uint64_t i_saddr, uint64_t i_eaddr, MemoryError_t i_type )
{
    #define PRDF_FUNC "[PlatServices::__dyndealloc] "

    do
    {
        if ( !g_hostInterfaces || !g_hostInterfaces->memory_error )
        {
            PRDF_ERR( PRDF_FUNC "memory_error() interface is not defined" );
            break;
        }

        int32_t rc = g_hostInterfaces->memory_error( i_saddr, i_eaddr, i_type );
        if ( SUCCESS != rc )
        {
            PRDF_ERR( PRDF_FUNC "memory_error() failed" );
            break;
        }

    } while (0);

    #undef PRDF_FUNC
}

void sendPageGardRequest( uint64_t i_saddr )
{
    // Note that both addresses will be the same for a page gard.
    __dyndealloc( i_saddr, i_saddr, MEMORY_ERROR_CE );
}

void sendDynMemDeallocRequest( uint64_t i_saddr, uint64_t i_eaddr )
{
    __dyndealloc( i_saddr, i_eaddr, MEMORY_ERROR_UE );
}

void sendPredDeallocRequest( uint64_t i_saddr, uint64_t i_eaddr )
{
    __dyndealloc( i_saddr, i_eaddr, MEMORY_ERROR_PREDICTIVE );
}

//##############################################################################
//##                Explorer Maintenance Command wrappers
//##############################################################################

template<>
uint32_t stopBgScrub<TYPE_OCMB_CHIP>( ExtensibleChip * i_chip )
{
    #define PRDF_FUNC "[PlatServices::stopBgScrub<TYPE_OCMB_CHIP>] "

    PRDF_ASSERT( nullptr != i_chip );
    PRDF_ASSERT( TYPE_OCMB_CHIP == i_chip->getType() );

    uint32_t rc = SUCCESS;

    fapi2::Target<fapi2::TARGET_TYPE_OCMB_CHIP> fapiTrgt ( i_chip->getTrgt() );

    errlHndl_t errl;

    if (isOdysseyOcmb(i_chip->getTrgt()))
    {
        FAPI_INVOKE_HWP( errl, ody_stop, fapiTrgt );
    }
    else
    {
        FAPI_INVOKE_HWP( errl, exp_stop, fapiTrgt );
    }

    if ( nullptr != errl )
    {
        PRDF_ERR( PRDF_FUNC "stop(0x%08x) failed", i_chip->getHuid());
        PRDF_COMMIT_ERRL( errl, ERRL_ACTION_REPORT );
        rc = FAIL;
    }

    return rc;

    #undef PRDF_FUNC
}

//------------------------------------------------------------------------------

template<>
uint32_t resumeBgScrub<TYPE_OCMB_CHIP>( ExtensibleChip * i_chip,
        STEP_CODE_DATA_STRUCT & io_sc )
{
    #define PRDF_FUNC "[PlatServices::resumeBgScrub<TYPE_OCMB_CHIP>] "

    PRDF_ASSERT( nullptr != i_chip );
    PRDF_ASSERT( TYPE_OCMB_CHIP == i_chip->getType() );

    uint32_t o_rc = SUCCESS;

    // Get the OCMB fapi target
    fapi2::Target<fapi2::TARGET_TYPE_OCMB_CHIP> fapiTrgt ( i_chip->getTrgt() );

    do
    {
        // Clear all of the counters and maintenance ECC attentions.
        o_rc = prepareNextCmd<TYPE_OCMB_CHIP>( i_chip );
        if ( SUCCESS != o_rc )
        {
            PRDF_ERR( PRDF_FUNC "prepareNextCmd(0x%08x) failed",
                      i_chip->getHuid() );
            break;
        }

        // Resume the command on the next address.
        errlHndl_t errl;

        if (isOdysseyOcmb(i_chip->getTrgt()))
        {
            // Check UE and CE stop counters to determine stop conditions
            mss::mcbist::stop_conditions<mss::mc_type::ODYSSEY> stopCond;
            if ( getOcmbDataBundle(i_chip)->iv_ueStopCounter.thReached(io_sc) )
            {
                // If we've reached the limit of UEs we're allowed to stop on
                // per rank, only set the stop on mpe stop condition.
                stopCond.set_pause_on_mpe(mss::ON);
            }
            else if (
                getOcmbDataBundle(i_chip)->iv_ceStopCounter.thReached(io_sc))
            {
                // If we've reached the limit of CEs we're allowed to stop on
                // per rank, set all the normal stop conditions except stop on
                // CE
                stopCond.set_pause_on_aue(mss::ON);

                #ifdef CONFIG_HBRT_PRD

                stopCond.set_pause_on_mpe(mss::ON)
                        .set_pause_on_ue(mss::ON);

                // In MNFG mode, stop on RCE_ETE to get an accurate callout for
                // IUEs
                if ( mfgMode() ) stopCond.set_thresh_rce(1);

                #endif
            }
            else
            {
                // If we haven't reached threshold on the number of UEs or CEs
                // we have stopped on, do not change the stop conditions.
                stopCond = mss::mcbist::stop_conditions<mss::mc_type::ODYSSEY>(
                mss::mcbist::stop_conditions<mss::mc_type::ODYSSEY>::DONT_CHANGE);
            }

            FAPI_INVOKE_HWP( errl, ody_continue_cmd, fapiTrgt,
                mss::mcbist::end_boundary::DONT_CHANGE, stopCond );
        }
        else
        {
            // Check UE and CE stop counters to determine stop conditions
            mss::mcbist::stop_conditions<mss::mc_type::EXPLORER> stopCond;
            if ( getOcmbDataBundle(i_chip)->iv_ueStopCounter.thReached(io_sc) )
            {
                // If we've reached the limit of UEs we're allowed to stop on
                // per rank, only set the stop on mpe stop condition.
                stopCond.set_pause_on_mpe(mss::ON);
            }
            else if (
                getOcmbDataBundle(i_chip)->iv_ceStopCounter.thReached(io_sc))
            {
                // If we've reached the limit of CEs we're allowed to stop on
                // per rank, set all the normal stop conditions except stop on
                // CE
                stopCond.set_pause_on_aue(mss::ON);

                #ifdef CONFIG_HBRT_PRD

                stopCond.set_pause_on_mpe(mss::ON)
                        .set_pause_on_ue(mss::ON);

                // In MNFG mode, stop on RCE_ETE to get an accurate callout for
                // IUEs
                if ( mfgMode() ) stopCond.set_thresh_rce(1);

                #endif
            }
            else
            {
                // If we haven't reached threshold on the number of UEs or CEs
                // we have stopped on, do not change the stop conditions.
                stopCond = mss::mcbist::stop_conditions<mss::mc_type::EXPLORER>(
                mss::mcbist::stop_conditions<mss::mc_type::EXPLORER>::DONT_CHANGE);
            }

            FAPI_INVOKE_HWP( errl, exp_continue_cmd, fapiTrgt,
                mss::mcbist::end_boundary::DONT_CHANGE, stopCond );
        }

        if ( nullptr != errl )
        {
            PRDF_ERR( PRDF_FUNC "continue_cmd(0x%08x) failed",
                      i_chip->getHuid() );
            PRDF_COMMIT_ERRL( errl, ERRL_ACTION_REPORT );
            o_rc = FAIL; break;
        }

    } while (0);

    return o_rc;

    #undef PRDF_FUNC
}

//##############################################################################
//##                       Core/cache functions
//##############################################################################

bool queryEcoMode(TARGETING::TargetHandle_t i_trgt)
{
    PRDF_ASSERT(nullptr != i_trgt);
    PRDF_ASSERT(TYPE_CORE == getTargetType(i_trgt));

    fapi2::Target<fapi2::TARGET_TYPE_CORE> fapiTrgt{i_trgt};

    fapi2::ATTR_ECO_MODE_Type attr;

    fapi2::ReturnCode rc = FAPI_ATTR_GET(fapi2::ATTR_ECO_MODE, fapiTrgt, attr);

    errlHndl_t errl = fapi2::rcToErrl(rc);
    if (nullptr != errl)
    {
        PRDF_ERR("[PlatServices::queryEcoMode] Failed to get ATTR_ECO_MODE: "
                 "i_trgt=0x%08X", getHuid(i_trgt));
        PRDF_COMMIT_ERRL(errl, ERRL_ACTION_REPORT);

        return false; // assume ECO mode is not enabled.
    }

    return fapi2::ENUM_ATTR_ECO_MODE_ENABLED == attr;
}

//##############################################################################
//##                       Line Delete Functions
//##############################################################################

int32_t extractL3Err( TargetHandle_t i_coreTgt, bool i_ce,
                      p10_l3err_extract_err_data &o_errorAddr)
{
    int32_t o_rc = SUCCESS;
    errlHndl_t err = nullptr;
    bool errFound = false;

    fapi2::variable_buffer ta_data( P10_TRACEARRAY_NUM_ROWS *
                                    P10_TRACEARRAY_BITS_PER_ROW);
    proc_gettracearray_args args;

    args.trace_bus = PROC_TB_L3_1;
    args.stop_pre_dump = true;
    args.ignore_mux_setting = false;
    args.collect_dump = true;
    args.reset_post_dump = false;
    args.restart_post_dump = false;

    fapi2::Target<fapi2::TARGET_TYPE_CORE> fapiTrgt (i_coreTgt);

    FAPI_INVOKE_HWP( err,
                     p10_proc_gettracearray,
                     fapiTrgt,
                     args,
                     ta_data);
    if (nullptr != err)
    {
        PRDF_ERR( "[PlatServices::extractL2Err] huid: 0x%08x gettracearray "
                  "failed", getHuid(i_coreTgt));
        PRDF_COMMIT_ERRL( err, ERRL_ACTION_REPORT );
        return FAIL;
    }

    FAPI_INVOKE_HWP( err,
                     p10_l3err_extract,
                     fapiTrgt,
                     ta_data,
                     i_ce ? L3ERR_CE : L3ERR_CE_UE,
                     o_errorAddr,
                     errFound );

    if (nullptr != err)
    {
        PRDF_ERR( "[PlatServices::extractL3Err] huid: 0x%08x failed",
                  getHuid(i_coreTgt));
        PRDF_COMMIT_ERRL( err, ERRL_ACTION_REPORT );
        o_rc = FAIL;
    }

    if ( !errFound )
    {
        PRDF_ERR( "[PlatServices::extractL3Err] huid: 0x%08x No Error Found",
                  getHuid(i_coreTgt));
        o_rc = FAIL;
    }

    return o_rc;
}


int32_t l3LineDelete(TargetHandle_t i_coreTgt,
                     const p10_l3err_extract_err_data& i_l3_err_data)
{
    using namespace stopImageSection;
    errlHndl_t err = nullptr;
    const uint64_t retryCount = 100;

    // Apply Line Delete
    fapi2::Target<fapi2::TARGET_TYPE_CORE> fapiTrgt (i_coreTgt);
    FAPI_INVOKE_HWP( err,
                     p10_l3err_linedelete,
                     fapiTrgt,
                     i_l3_err_data,
                     retryCount);
    if(nullptr != err)
    {
        PRDF_ERR( "[PlatServices::l3LineDelete] HUID: 0x%08x failed",
                  getHuid(i_coreTgt));
        PRDF_COMMIT_ERRL( err, ERRL_ACTION_REPORT );
        return FAIL;
    }

    // Cannot write HCODE in ECO mode.
    if (queryEcoMode(i_coreTgt))
        return SUCCESS; // nothing more to do

    // Do HCODE update to preserve line delete
    BitStringBuffer ldData(64);
    ldData.setBit(0); //Trigger
    ldData.setFieldJustify(1, 4, 0x2); // Purge Type (LD=0x2)
    ldData.setFieldJustify(12, 5, i_l3_err_data.member);
    ldData.setFieldJustify(17, 12, i_l3_err_data.real_address_46_57);

    uint64_t scomVal = (((uint64_t)ldData.getFieldJustify(0, 32)) << 32) |
                        ((uint64_t)ldData.getFieldJustify(32, 32));

    err = RTPM::hcode_update(PROC_STOP_SECTION_L3, PROC_STOP_SCOM_APPEND,
                             i_coreTgt, 0x2001860E, scomVal);
    if (nullptr != err)
    {
        PRDF_ERR( "[PlatServices::l3LineDelete] HUID: 0x%08x hcode_update "
                  "failed", getHuid(i_coreTgt));
        PRDF_COMMIT_ERRL( err, ERRL_ACTION_REPORT );
        return FAIL;
    }

    return SUCCESS;
}

int32_t extractL2Err( TargetHandle_t i_coreTgt, bool i_ce,
                      p10_l2err_extract_err_data &o_errorAddr)
{
    errlHndl_t err = nullptr;
    bool errFound = false;
    fapi2::variable_buffer ta_data( P10_TRACEARRAY_NUM_ROWS *
                                    P10_TRACEARRAY_BITS_PER_ROW);
    proc_gettracearray_args args;

    args.trace_bus = PROC_TB_L20;
    args.stop_pre_dump = true;
    args.ignore_mux_setting = false;
    args.collect_dump = true;
    args.reset_post_dump = false;
    args.restart_post_dump = false;

    fapi2::Target<fapi2::TARGET_TYPE_CORE> fapiTrgt (i_coreTgt);

    FAPI_INVOKE_HWP( err,
                     p10_proc_gettracearray,
                     fapiTrgt,
                     args,
                     ta_data);
    if (nullptr != err)
    {
        PRDF_ERR( "[PlatServices::extractL2Err] huid: 0x%08x gettracearray "
                  "failed", getHuid(i_coreTgt));
        PRDF_COMMIT_ERRL( err, ERRL_ACTION_REPORT );
        return FAIL;
    }

    FAPI_INVOKE_HWP( err,
                     p10_l2err_extract,
                     fapiTrgt,
                     ta_data,
                     i_ce ? L2ERR_CE : L2ERR_CE_UE,
                     o_errorAddr,
                     errFound );

    if (nullptr != err)
    {
        PRDF_ERR( "[PlatServices::extractL2Err] huid: 0x%08x failed",
                  getHuid(i_coreTgt));
        PRDF_COMMIT_ERRL( err, ERRL_ACTION_REPORT );
        return FAIL;
    }

    if ( !errFound )
    {
        PRDF_ERR( "[PlatServices::extractL2Err] huid: 0x%08x No Error Found",
                  getHuid(i_coreTgt));
        return FAIL;
    }

    return SUCCESS;
}

int32_t l2LineDelete(TargetHandle_t i_coreTgt,
                     const p10_l2err_extract_err_data& i_l2_err_data)
{
    using namespace stopImageSection;
    errlHndl_t err = nullptr;
    const uint64_t retryCount = 100;

    // Apply Line Delete
    fapi2::Target<fapi2::TARGET_TYPE_CORE> fapiTrgt (i_coreTgt);
    FAPI_INVOKE_HWP( err,
                     p10_l2err_linedelete,
                     fapiTrgt,
                     i_l2_err_data,
                     retryCount);
    if(nullptr != err)
    {
        PRDF_ERR( "[PlatServices::l2LineDelete] HUID: 0x%08x failed",
                  getHuid(i_coreTgt));
        PRDF_COMMIT_ERRL( err, ERRL_ACTION_REPORT );
        return FAIL;
    }

    // Cannot write HCODE in ECO mode.
    if (queryEcoMode(i_coreTgt))
        return SUCCESS; // nothing more to do

    // Do HCODE update to preserve line delete
    BitStringBuffer ldData(64);
    ldData.setBit(0); //Trigger
    ldData.setFieldJustify(1, 4, 0x2); // Purge Type (LD=0x2)
    ldData.setFieldJustify(17, 3, i_l2_err_data.member);
    ldData.setFieldJustify(20, 8, i_l2_err_data.real_address_47_56);
    if (i_l2_err_data.bank)
        ldData.setBit(28);

    uint64_t scomVal = (((uint64_t)ldData.getFieldJustify(0, 32)) << 32) |
                        ((uint64_t)ldData.getFieldJustify(32, 32));

    err = RTPM::hcode_update(PROC_STOP_SECTION_L2, PROC_STOP_SCOM_APPEND,
                             i_coreTgt, 0x2002800E, scomVal);
    if (nullptr != err)
    {
        PRDF_ERR( "[PlatServices::l2LineDelete] HUID: 0x%08x hcode_update "
                  "failed", getHuid(i_coreTgt));
        PRDF_COMMIT_ERRL( err, ERRL_ACTION_REPORT );
        return FAIL;
    }

    return SUCCESS;
}

int32_t pmCallout( TargetHandle_t i_tgt,
                   RasAction& o_ra,
                   uint32_t &o_deadCores,
                   std::vector < StopErrLogSectn >& o_ffdcList )
{
    errlHndl_t err = nullptr;
    fapi2::buffer <uint32_t> deadCores;

    //Get homer image buffer
    uint64_t l_homerPhysAddr = 0x0;
    l_homerPhysAddr = i_tgt->getAttr<ATTR_HOMER_PHYS_ADDR>();
    void* l_homerVAddr = HBPM::convertHomerPhysToVirt(i_tgt,l_homerPhysAddr);

    fapi2::Target<fapi2::TARGET_TYPE_PROC_CHIP> fapiTrgt (i_tgt);

    FAPI_INVOKE_HWP( err,
                     p10_pm_callout,
                     l_homerVAddr,
                     fapiTrgt,
                     deadCores,
                     o_ffdcList,
                     o_ra );

    if(nullptr != err)
    {
        PRDF_ERR( "[PlatServices::pmCallout] HUID: 0x%08x failed",
                  getHuid(i_tgt));
        PRDF_COMMIT_ERRL( err, ERRL_ACTION_REPORT );
        return FAIL;
    }

    o_deadCores = (uint32_t) deadCores;
    return SUCCESS;
}

//##############################################################################
//##                         TOD functions
//##############################################################################

void requestNewTODTopology( uint32_t i_oscPos,
                            const TargetHandle_t& i_procOscTgt,
                            const TargetHandleList& i_badChipList,
                            bool i_informPhyp)
{
    #define PRDF_FUNC "[PlatServices::requestNewTODTopology] "
    if ( i_badChipList.size() > 0 || i_procOscTgt != nullptr )
    {
        errlHndl_t err = TOD::resetBackupTopology( i_oscPos, i_procOscTgt,
                                               i_badChipList, i_informPhyp );

        if (nullptr != err)
        {
            PRDF_ERR( PRDF_FUNC " failed. oscPos: %d "
                      "oscTgt: 0x%08x, chip blacklist size: %d",
                      i_oscPos, getHuid(i_procOscTgt), i_badChipList.size() );
            PRDF_COMMIT_ERRL( err, ERRL_ACTION_REPORT );
        }
    }
    else
    {
        PRDF_ERR( PRDF_FUNC "No chips in black list");
    }
    #undef PRDF_FUNC
}

//------------------------------------------------------------------------------

} // end namespace PlatServices

} // end namespace PRDF

