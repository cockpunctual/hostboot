/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/diag/prdf/plat/mem/prdfMemRrd.C $                     */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2018,2023                        */
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

/** @file prdfMemRrd.C */

// Platform includes
#include <prdfMemRrd.H>

#include <exp_rank.H>
#include <kind.H>
#include <hwp_wrappers.H>

using namespace TARGETING;

namespace PRDF
{

using namespace PlatServices;

template<TARGETING::TYPE T>
uint32_t RrdEvent<T>::checkEcc( const uint32_t & i_eccAttns,
                                STEP_CODE_DATA_STRUCT & io_sc,
                                bool & o_done )
{
    #define PRDF_FUNC "[RrdEvent<T>::checkEcc] "

    uint32_t o_rc = SUCCESS;

    do
    {
        if ( i_eccAttns & MAINT_UE )
        {
            PRDF_TRAC( "[RrdEvent] UE Detected: 0x%08x,0x%02x",
                       iv_chip->getHuid(), getKey() );

            io_sc.service_data->setSignature( iv_chip->getHuid(),
                                              PRDFSIG_MaintUE );

            // At this point we don't actually have an address for the UE. The
            // best we can do is get the address in which the command stopped.
            MemAddr addr;
            o_rc = getMemMaintAddr<T>( iv_chip, addr );
            if ( SUCCESS != o_rc )
            {
                PRDF_ERR( PRDF_FUNC "getMemMaintAddr(0x%08x) failed",
                          iv_chip->getHuid() );
                break;
            }

            o_rc = MemEcc::handleMemUe<T>( iv_chip, addr,
                                           UE_TABLE::SCRUB_UE, io_sc );
            if ( SUCCESS != o_rc )
            {
                PRDF_ERR( PRDF_FUNC "handleMemUe(0x%08x,0x%02x) failed",
                          iv_chip->getHuid(), getKey() );
                break;
            }

            #ifdef __HOSTBOOT_RUNTIME
            // Because of the UE, any further TPS requests will likely have no
            // effect. So ban all subsequent requests.
            MemDbUtils::banTps<T>( iv_chip, addr.getRank() );
            #endif

            // Leave the mark in place and abort this procedure.
            o_done = true; break;
        }

        if ( i_eccAttns & MAINT_RCE_ETE )
        {
            io_sc.service_data->setSignature( iv_chip->getHuid(),
                                              PRDFSIG_MaintRETRY_CTE );

            // Add the rank to the callout list.
            MemoryMru mm { iv_chip->getTrgt(), iv_rank, iv_port,
                           MemoryMruData::CALLOUT_RANK };
            io_sc.service_data->SetCallout( mm );

            // Make the error log predictive.
            io_sc.service_data->setServiceCall();

            // Don't abort continue the procedure.
        }

    } while (0);

    return o_rc;

    #undef PRDF_FUNC
}

//------------------------------------------------------------------------------

template<TARGETING::TYPE T>
uint32_t RrdEvent<T>::verifyRowRepair( STEP_CODE_DATA_STRUCT & io_sc,
                                       bool & o_done )
{
    #define PRDF_FUNC "[RrdEvent<T>::verifyRowRepair] "

    uint32_t o_rc = SUCCESS;

    do
    {
        if ( TD_PHASE_1 != iv_phase ) break; // nothing to do

        o_done = true;

        // If it is safe to remove the chip mark, do so. Then the row repair
        // has been successfully deployed.
        if ( MarkStore::isSafeToRemoveChipMark<T>( iv_chip, iv_rank ) )
        {
            PRDF_TRAC( PRDF_FUNC "Row repair deployed successfully: "
                       "0x%08x,0x%02x", iv_chip->getHuid(), getKey() );

            io_sc.service_data->setSignature( iv_chip->getHuid(),
                                              PRDFSIG_RrdRowDeployed );

            o_rc = MarkStore::clearChipMark<T>( iv_chip, iv_rank, iv_port );
            if ( SUCCESS != o_rc )
            {
                PRDF_ERR( PRDF_FUNC "clearChipMark(0x%08x,0x%02x,%x) failed",
                          iv_chip->getHuid(), getKey(), iv_port );
                break;
            }
        }
        // Else if it is not safe to remove the chip mark, the spare row is bad
        else
        {
            PRDF_TRAC( PRDF_FUNC "Spare row is bad: 0x%08x,0x%02x",
                       iv_chip->getHuid(), getKey() );

            io_sc.service_data->setSignature( iv_chip->getHuid(),
                                              PRDFSIG_RrdBadRow );

            // Take actions to cleanup the chip mark
            bool junk = false;
            o_rc = MarkStore::chipMarkCleanup<T>(iv_chip, iv_rank, iv_port,
                                                 io_sc, junk);
            if ( SUCCESS != o_rc )
            {
                PRDF_TRAC( PRDF_FUNC "chipMarkCleanup(0x%08x, 0x%02x) failed",
                           iv_chip->getHuid(), getKey() );
                break;
            }
        }

    } while (0);

    return o_rc;

    #undef PRDF_FUNC
}

//------------------------------------------------------------------------------

template<>
uint32_t RrdEvent<TYPE_OCMB_CHIP>::startCmd()
{
    #define PRDF_FUNC "[RrdEvent<TYPE_OCMB_CHIP>::startCmd] "

    uint32_t o_rc = SUCCESS;

    // Check for Odyssey OCMBs
    if (isOdysseyOcmb(iv_chip->getTrgt()))
    {
        mss::mcbist::stop_conditions<mss::mc_type::ODYSSEY> stopCond;

        // Set the per-symbol counters to count all 3 CE types: hard, soft, int
        stopCond.set_nce_soft_symbol_count_enable( mss::ON);
        stopCond.set_nce_inter_symbol_count_enable(mss::ON);
        stopCond.set_nce_hard_symbol_count_enable( mss::ON);

        // Set the per-symbol MCE counters to count only hard MCEs
        stopCond.set_mce_hard_symbol_count_enable(mss::ON);

        // Start the time based scrub procedure on this master rank.
        o_rc = startTdScrub<TYPE_OCMB_CHIP>( iv_chip, iv_rank, iv_port,
                                             MASTER_RANK, stopCond );
    }
    // Default to Explorer OCMBs
    else
    {
        mss::mcbist::stop_conditions<mss::mc_type::EXPLORER> stopCond;

        // Set the per-symbol counters to count all 3 CE types: hard, soft, int
        stopCond.set_nce_soft_symbol_count_enable( mss::ON);
        stopCond.set_nce_inter_symbol_count_enable(mss::ON);
        stopCond.set_nce_hard_symbol_count_enable( mss::ON);

        // Set the per-symbol MCE counters to count only hard MCEs
        stopCond.set_mce_hard_symbol_count_enable(mss::ON);

        // Start the time based scrub procedure on this master rank.
        o_rc = startTdScrub<TYPE_OCMB_CHIP>( iv_chip, iv_rank, iv_port,
                                             MASTER_RANK, stopCond );
    }

    if ( SUCCESS != o_rc )
    {
        PRDF_ERR( PRDF_FUNC "startTdScrub(0x%08x,0x%2x,%x) failed",
                  iv_chip->getHuid(), getKey(), iv_port );
    }

    return o_rc;

    #undef PRDF_FUNC
}

//------------------------------------------------------------------------------

template<TARGETING::TYPE T>
uint32_t RrdEvent<T>::startNextPhase( STEP_CODE_DATA_STRUCT & io_sc )
{
    uint32_t signature = 0;

    switch ( iv_phase )
    {
        case TD_PHASE_0:
            iv_phase  = TD_PHASE_1;
            signature = PRDFSIG_StartRrdPhase1;
            break;

        default: PRDF_ASSERT( false ); // invalid phase
    }

    PRDF_TRAC( "[RrdEvent] Starting RRD Phase %d: 0x%08x,0x%02x",
               iv_phase, iv_chip->getHuid(), getKey() );

    io_sc.service_data->AddSignatureList( iv_chip->getTrgt(), signature );

    return startCmd();
}

//------------------------------------------------------------------------------

// Avoid linker errors with the template.
template class RrdEvent<TYPE_OCMB_CHIP>;

} // end namespace PRDF

