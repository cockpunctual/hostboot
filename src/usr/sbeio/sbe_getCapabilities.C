/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/sbeio/sbe_getCapabilities.C $                         */
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
/**
* @file sbe_getCapabilities.C
* @brief Get the capabilities from the SBE
*/

#include <errl/errlmanager.H> // errlHndl_t
#include <sbeio/sbe_psudd.H>  // SbeFifo::psuCommand
#include "sbe_fifodd.H"       // SbeFifo::fifoGetCapabilitiesRequest
#include <sbeio/sbe_utils.H>  // sbeCapabilities2_t
#include <sbeio/sbeioreasoncodes.H> // SBEIO_PSU, SBEIO_FIFO,
#include <targeting/common/commontargeting.H>  // get_huid
#include <targeting/targplatutil.H>  //getCurrentNodeTarget
#include <util/align.H>            // ALIGN_X
#include <sys/mm.h>                // mm_virt_to_phys
#include <sbeio/sbeioif.H>

extern trace_desc_t* g_trac_sbeio;

namespace SBEIO
{
using namespace TARGETING;
using namespace ERRORLOG;

 /**
 * @brief Apply the SBE capabilities to the given target.
 * @note ATTR_SBE_FIFO_CAPABILITIES is not set in this function.
 *
 * @param[in]  i_target Target to apply the SBE capabilities on
 *
 * @param[in]  i_capabilities The SBE capabilities themselves
 *
 */
void applySbeCapabilities(TargetHandle_t i_target,
                          sbeCapabilities_t &i_capabilities)
{
    // Get the SBE Version from the SBE Capabilities and set the
    // attribute associated with SBE Version
    ATTR_SBE_VERSION_INFO_type l_sbeVersionInfo =
                  TWO_UINT16_TO_UINT32(i_capabilities.majorVersion,
                                       i_capabilities.minorVersion);
    i_target->setAttr<ATTR_SBE_VERSION_INFO>(l_sbeVersionInfo);
    TRACFCOMP(g_trac_sbeio,"applySbeCapabilities: Retrieved SBE Version: 0x%X", l_sbeVersionInfo);

    // Get the SBE Commit ID from the SBE Capabilities and set the
    // attribute associated with SBE Commit ID
    ATTR_SBE_COMMIT_ID_type l_sbeCommitId = i_capabilities.commitId;
    i_target->setAttr<ATTR_SBE_COMMIT_ID>(l_sbeCommitId);
    TRACFCOMP(g_trac_sbeio,"applySbeCapabilities: Retrieved SBE Commit ID: 0x%X", l_sbeCommitId);

    // Get the SBE Release Tag from the SBE Capabilities and set the
    // attribute associated with SBE Release Tag
    ATTR_SBE_RELEASE_TAG_type l_sbeReleaseTagString = {0};

    // Make sure the sizes are compatible.
    static_assert(SBE_RELEASE_TAG_MAX_CHARS <= ATTR_SBE_RELEASE_TAG_max_chars,
        "Copy error - size of source is greater than size of destination.");

    // Copy the release tags over into a more compatible type and set
    // the SBE Release Tags attribute
    strncpy(l_sbeReleaseTagString, i_capabilities.releaseTag, SBE_RELEASE_TAG_MAX_CHARS);

    i_target->setAttr<ATTR_SBE_RELEASE_TAG>(l_sbeReleaseTagString);

    TRACFCOMP(g_trac_sbeio,"applySbeCapabilities: Retrieved SBE Release Tag: %s", l_sbeReleaseTagString);

    // Odyssey does not support SBE halting and TPMs do not apply to it.
    if (i_target->getAttr<ATTR_TYPE>() == TYPE_PROC)
    {
        // SBE only supported accurate Hostboot requested halt reporting to FSP
        // as of version 1.4 or later.
        const uint8_t sbeSupportsHaltReporting = (((i_capabilities.majorVersion == 1)
                                                    && (i_capabilities.minorVersion >= 4))
                                                 || ((i_capabilities.majorVersion >  1)));

        i_target->setAttr<ATTR_SBE_SUPPORTS_HALT_STATUS>(sbeSupportsHaltReporting);
        TRACFCOMP(g_trac_sbeio,"applySbeCapabilities: SBE supports Hostboot requested halt status reporting: %d",
                sbeSupportsHaltReporting);

        // Only interrogate the boot processor's SBE to determine if the current
        // node is capable of having the boot processor's SBE perform the SMP
        // stitching and TPM measurement extending.
        if(i_target->getAttr<ATTR_PROC_MASTER_TYPE>() == PROC_MASTER_TYPE_ACTING_MASTER)
        {
            // SBE support for TPM Extend Mode as of version 2.1 or later.
            const uint8_t sbeSupportsTpmExtendMode =
                (   (   (i_capabilities.majorVersion == 2)
                        && (i_capabilities.minorVersion >= 1))
                    || (   (i_capabilities.majorVersion >  2)));

#ifndef __HOSTBOOT_RUNTIME
            TargetHandle_t l_nodeTarget = UTIL::getCurrentNodeTarget();
#else
            TargetHandle_t l_nodeTarget = UTIL::assertGetMasterNodeTarget();
#endif
            l_nodeTarget->
                setAttr<ATTR_SBE_HANDLES_SMP_TPM_EXTEND>(sbeSupportsTpmExtendMode);
            TRACFCOMP(g_trac_sbeio,"applySbeCapabilities: SBE supports TPM Extend Mode: %d",
                    sbeSupportsTpmExtendMode);
        }
    }
}

/**
 * getPsuSbeCapabilities
 */
errlHndl_t getPsuSbeCapabilities(TargetHandle_t i_target)
{
    errlHndl_t l_errl(nullptr);

    TRACDCOMP(g_trac_sbeio, ENTER_MRK "getPsuSbeCapabilities");

    // Cache the SBE Capabilities' size for future uses
    size_t l_sbeCapabilitiesSize = sizeof(sbeCapabilities2_t);

    auto l_alignedMemHandle = sbeMalloc(l_sbeCapabilitiesSize);

    // Clear buffer up to the size of the capabilities
    memset(l_alignedMemHandle.dataPtr, 0, l_sbeCapabilitiesSize);

    // Create a PSU request message and initialize it
    SbePsu::psuCommand l_psuCommand(
                SbePsu::SBE_REQUIRE_RESPONSE |
                SbePsu::SBE_REQUIRE_ACK,                 //control flags
                SbePsu::SBE_PSU_GENERIC_MESSAGE,         //command class
                SbePsu::SBE_PSU_MSG_GET_CAPABILITIES);   //command
    l_psuCommand.cd7_getSbeCapabilities_CapabilitiesSize =
                ALIGN_X(l_sbeCapabilitiesSize,
                        SbePsu::SBE_ALIGNMENT_SIZE_IN_BYTES);
    l_psuCommand.cd7_getSbeCapabilities_CapabilitiesAddr =
                l_alignedMemHandle.physAddr;

    // Create a PSU response message
    SbePsu::psuResponse l_psuResponse;

    do
    {
        bool command_unsupported = false;

        // Make the call to perform the PSU Chip Operation
        l_errl = SbePsu::getTheInstance().performPsuChipOp(
                        i_target,
                        &l_psuCommand,
                        &l_psuResponse,
                        SbePsu::MAX_PSU_SHORT_TIMEOUT_NS,
                        SbePsu::SBE_GET_CAPABILITIES_REQ_USED_REGS,
                        SbePsu::SBE_GET_CAPABILITIES_RSP_USED_REGS,
                        SbePsu::COMMAND_SUPPORT_OPTIONAL,
                        &command_unsupported);

        // Before continuing, make sure this request is honored

        if (command_unsupported)
        { // Traces have already been logged
            break;
        }

        if (l_errl)
        {
            TRACFCOMP(g_trac_sbeio,
                      "getPsuSbeCapabilities: "
                      "Call to performPsuChipOp failed, error returned");

            TRACDBIN(g_trac_sbeio,
              "getPsuSbeCapabilities: capabilities data",
              l_alignedMemHandle.dataPtr,
              l_sbeCapabilitiesSize);

            break;
        }

        // Sanity check - are HW and HB communications in sync?
        if ((SbePsu::SBE_PSU_GENERIC_MESSAGE !=
                               l_psuResponse.sbe_commandClass) ||
            (SbePsu::SBE_PSU_MSG_GET_CAPABILITIES !=
                               l_psuResponse.sbe_command))
        {
            TRACFCOMP(g_trac_sbeio,
                      "Call to performPsuChipOp returned an unexpected "
                      "message type; "
                      "command class returned:0x%X, "
                      "expected command class:0x%X, "
                      "command returned:0x%X, "
                      "expected command:0x%X",
                      l_psuResponse.sbe_commandClass,
                      SbePsu::SBE_PSU_GENERIC_MESSAGE,
                      l_psuResponse.sbe_command,
                      SbePsu::SBE_PSU_MSG_GET_CAPABILITIES);

            /*@
             * @errortype
             * @moduleid          SBEIO_PSU
             * @reasoncode        SBEIO_RECEIVED_UNEXPECTED_MSG
             * @userdata1         Target HUID
             * @userdata2[0:15]   Requested command class
             * @userdata2[16:31]  Requested command
             * @userdata2[32:47]  Returned command class
             * @userdata2[48:63]  Returned command
             * @devdesc           Call to PSU Chip Op returned an
             *                    unexpected message type.
             */
            l_errl = new ErrlEntry(
                ERRL_SEV_INFORMATIONAL,
                SBEIO_PSU,
                SBEIO_RECEIVED_UNEXPECTED_MSG,
                get_huid(i_target),
                TWO_UINT32_TO_UINT64(
                  TWO_UINT16_TO_UINT32(SbePsu::SBE_PSU_GENERIC_MESSAGE,
                                       SbePsu::SBE_PSU_MSG_GET_CAPABILITIES),
                  TWO_UINT16_TO_UINT32(l_psuResponse.sbe_commandClass,
                                       l_psuResponse.sbe_command) ));

            l_errl->collectTrace(SBEIO_COMP_NAME, 256);

            break;
        }

        // Check for any difference in sizes (expected vs returned)
        // This may happen but it is not a show stopper, just note it
        if (l_psuResponse.sbe_capabilities_size != l_sbeCapabilitiesSize)
        {
            TRACFCOMP(g_trac_sbeio, "getPsuSbeCapabilities:"
                     "Call to performPsuChipOp returned an unexpected size: "
                     "capabilities size returned:%d, "
                     "expected capabilities size:%d, ",
                     l_psuResponse.sbe_capabilities_size,
                     l_sbeCapabilitiesSize);
        }

        // Create an SBE Capabilities structure to make it easy to pull data;
        // clear memory before use
        sbeCapabilities2_t l_sbeCapabilities;
        memset(&l_sbeCapabilities, 0, sizeof(sbeCapabilities2_t));

        // If the returned size is greater than or equal to the needed size,
        // then copy all of the SBE capabilities
        if (l_psuResponse.sbe_capabilities_size >= l_sbeCapabilitiesSize)
        {
            memcpy (&l_sbeCapabilities,
                   l_alignedMemHandle.dataPtr,
                   l_sbeCapabilitiesSize);
        }
        // If the returned size is less than the needed size and is not zero
        // then copy what was given
        else if (l_psuResponse.sbe_capabilities_size)
        {
            memcpy (&l_sbeCapabilities,
                   l_alignedMemHandle.dataPtr,
                   l_psuResponse.sbe_capabilities_size);
        }

        // If data returned, retrieve it
        if (l_psuResponse.sbe_capabilities_size)
        {
            applySbeCapabilities(i_target, *reinterpret_cast<sbeCapabilities_t *>(&l_sbeCapabilities));
        }
    } while (0);

    // Free the buffer
    sbeFree(l_alignedMemHandle);

    TRACFCOMP(g_trac_sbeio, EXIT_MRK "getPsuSbeCapabilities");

    return l_errl;
};


#ifndef __HOSTBOOT_RUNTIME  //no FIFO at runtime
/**
 *  getFifoSbeCapabilities
 */
errlHndl_t getFifoSbeCapabilities(TargetHandle_t i_target)
{
    errlHndl_t l_errl(nullptr);

    TRACDCOMP(g_trac_sbeio, ENTER_MRK "getFifoSbeCapabilities on 0x%08X target", get_huid(i_target));

    do
    {
        // update this based on version
        uint8_t capabilities_array_size = 0;

        // Create a FIFO request message. Default ctor initializes for Odyssey but a couple fields need tweaking
        // depending on context.
        SbeFifo::fifoGetCapabilitiesRequest l_fifoRequest;

        if (i_target->getAttr<ATTR_TYPE>() == TYPE_OCMB_CHIP)
        {
            // Odyssey uses getCapabilities. Presently, the chipId field is unused so leave it 0.
            capabilities_array_size = SBEIO::SBE_MAX_CAPABILITIES;
        }
        else
        {
            // P10 uses getCapabilities2
            capabilities_array_size = SBEIO::SBE_MAX_CAPABILITIES_2;
            l_fifoRequest.command = SbeFifo::SBE_FIFO_CMD_GET_CAPABILITIES_2;
        }

        l_errl = sbeioInterfaceChecks(i_target,
                                      SbeFifo::SBE_FIFO_CLASS_GENERIC_MESSAGE,
                                      l_fifoRequest.command);
        if (l_errl)
        {
            break;
        }

        // Create a FIFO response message.  No need to initialize
        SbeFifo::fifoStandardResponse * l_fifoResponseEnd;

        // create a large buffer for MAX response
        uint8_t l_fifoResponseBuffer[sizeof(sbeCapabilities_t)
                                    + (sizeof(uint32_t) * capabilities_array_size)
                                    + sizeof(*l_fifoResponseEnd)] = {0};

        // Make the call to perform the FIFO Chip Operation
        l_errl = SbeFifo::getTheInstance().performFifoChipOp(
                        i_target,
                        reinterpret_cast<uint32_t *>(&l_fifoRequest),
                        reinterpret_cast<uint32_t *>(l_fifoResponseBuffer),
                        sizeof(l_fifoResponseBuffer));

        if (l_errl)
        {
            TRACFCOMP(g_trac_sbeio,
                      "Call to performFifoChipOp failed, error returned");
            break;
        }

        // Different versions change capabilities array size.
        SBEIO::sbeCapabilities_t * pSbeCapabilities =
            reinterpret_cast<SBEIO::sbeCapabilities_t *>(l_fifoResponseBuffer);

        // offset into l_fifoResponseBuffer for start of l_fifoResponseEnd
        uint32_t rspEndOffset = sizeof(SBEIO::sbeCapabilities_t)
                              + (sizeof(pSbeCapabilities->capabilities[0]) * capabilities_array_size);

        l_fifoResponseEnd = reinterpret_cast<SbeFifo::fifoStandardResponse *>
                                (l_fifoResponseBuffer + rspEndOffset);

        // Sanity check - are HW and HB communications in sync?
        if ((SbeFifo::FIFO_STATUS_MAGIC != l_fifoResponseEnd->status.magic)  ||
            (SbeFifo::SBE_FIFO_CLASS_GENERIC_MESSAGE != l_fifoResponseEnd->status.commandClass) ||
            (l_fifoRequest.command != l_fifoResponseEnd->status.command))
        {
            TRACFCOMP(g_trac_sbeio,
                      "Call to performFifoChipOp returned an unexpected "
                      "message type; "
                      "magic code returned:0x%X, "
                      "expected magic code:0x%X, "
                      "command class returned:0x%X, "
                      "expected command class:0x%X, "
                      "command returned:0x%X, "
                      "expected command:0x%X",
                      l_fifoResponseEnd->status.magic,
                      SbeFifo::FIFO_STATUS_MAGIC,
                      l_fifoResponseEnd->status.commandClass,
                      SbeFifo::SBE_FIFO_CLASS_GENERIC_MESSAGE,
                      l_fifoResponseEnd->status.command,
                      l_fifoRequest.command);

            /*@
             * @errortype
             * @moduleid          SBEIO_FIFO
             * @reasoncode        SBEIO_RECEIVED_UNEXPECTED_MSG
             * @userdata1         Target HUID
             * @userdata2[0:15]   Requested command class
             * @userdata2[16:31]  Requested command
             * @userdata2[32:47]  Returned command class
             * @userdata2[48:63]  Returned command
             * @devdesc           Call to FIFO Chip Op returned an
             *                    unexpected message type.
             */
            l_errl = new ErrlEntry(
                ERRL_SEV_INFORMATIONAL,
                SBEIO_FIFO,
                SBEIO_RECEIVED_UNEXPECTED_MSG,
                get_huid(i_target),
                TWO_UINT32_TO_UINT64(
                  TWO_UINT16_TO_UINT32(SbeFifo::SBE_FIFO_CLASS_GENERIC_MESSAGE,
                                       l_fifoRequest.command),
                  TWO_UINT16_TO_UINT32(l_fifoResponseEnd->status.commandClass,
                                       l_fifoResponseEnd->status.command) ));

            l_errl->collectTrace(SBEIO_COMP_NAME, 256);
            break;
        }

        applySbeCapabilities(i_target, *pSbeCapabilities);

        TRACDCOMP(g_trac_sbeio, "getFifoSbeCapabilities version %d.%d found for 0x%08X target (capabilities array size: %d)",
                pSbeCapabilities->majorVersion,pSbeCapabilities->minorVersion,
                get_huid(i_target), capabilities_array_size);
        TRACDBIN(g_trac_sbeio,"SBE capabilities array",
                 pSbeCapabilities->capabilities,
                 (sizeof(pSbeCapabilities->capabilities[0]) * capabilities_array_size));

        // Fill in ATTR_SBE_FIFO_CAPABILITIES for this processor's SBE
        // verify attribute size large enough for all capabilities array
        static_assert(sizeof(ATTR_SBE_FIFO_CAPABILITIES_type) >= sizeof(sbeCapabilities2_t::capabilities),
                      "ATTR_SBE_FIFO_CAPABILITIES size out of sync with sbeCapabilities2_t capabilities field size" );

        TARGETING::ATTR_SBE_FIFO_CAPABILITIES_type l_fifo_capabilities = {};

        memcpy(l_fifo_capabilities,
               pSbeCapabilities->capabilities,
               (sizeof(pSbeCapabilities->capabilities[0]) * capabilities_array_size));

        i_target->setAttr<TARGETING::ATTR_SBE_FIFO_CAPABILITIES>(l_fifo_capabilities);

    }
    while(0);

    TRACDCOMP(g_trac_sbeio, EXIT_MRK "getFifoSbeCapabilities");

    return l_errl;
};
#endif //#ifdef __HOSTBOOT_RUNTIME

} //end namespace SBEIO
