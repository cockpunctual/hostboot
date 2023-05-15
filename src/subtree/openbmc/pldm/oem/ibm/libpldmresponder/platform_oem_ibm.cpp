#include "platform_oem_ibm.hpp"

#include "common/utils.hpp"
#include "libpldmresponder/pdr.hpp"

#include <libpldm/platform_oem_ibm.h>
#include <libpldm/pldm.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <iostream>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace responder
{
namespace platform
{
int sendBiosAttributeUpdateEvent(
    uint8_t eid, dbus_api::Requester* requester,
    const std::vector<uint16_t>& handles,
    pldm::requester::Handler<pldm::requester::Request>* handler)
{
    constexpr auto hostStatePath = "/xyz/openbmc_project/state/host0";
    constexpr auto hostStateInterface =
        "xyz.openbmc_project.State.Boot.Progress";
    constexpr auto hostStateProperty = "BootProgress";

    try
    {
        auto propVal = pldm::utils::DBusHandler().getDbusPropertyVariant(
            hostStatePath, hostStateProperty, hostStateInterface);
        const auto& currHostState = std::get<std::string>(propVal);
        if ((currHostState != "xyz.openbmc_project.State.Boot.Progress."
                              "ProgressStages.SystemInitComplete") &&
            (currHostState != "xyz.openbmc_project.State.Boot.Progress."
                              "ProgressStages.OSRunning") &&
            (currHostState != "xyz.openbmc_project.State.Boot.Progress."
                              "ProgressStages.SystemSetup"))
        {
            return PLDM_SUCCESS;
        }
    }
    catch (
        const sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound&
            e)
    {
        /* Exception is expected to happen in the case when state manager is
         * started after pldm, this is expected to happen in reboot case
         * where host is considered to be up. As host is up pldm is expected
         * to send attribute update event to host so this is not an error
         * case */
    }
    catch (const sdbusplus::exception_t& e)
    {
        error(
            "Error in getting current host state, {EXCEP_NAME} Continue sending the bios attribute update event ...",
            "EXCEP_NAME", e.name());
    }

    auto instanceId = requester->getInstanceId(eid);

    std::vector<uint8_t> requestMsg(
        sizeof(pldm_msg_hdr) + sizeof(pldm_bios_attribute_update_event_req) -
            1 + (handles.size() * sizeof(uint16_t)),
        0);

    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_bios_attribute_update_event_req(
        instanceId, PLDM_PLATFORM_EVENT_MESSAGE_FORMAT_VERSION, TERMINUS_ID,
        handles.size(), reinterpret_cast<const uint8_t*>(handles.data()),
        requestMsg.size() - sizeof(pldm_msg_hdr), request);
    if (rc != PLDM_SUCCESS)
    {
        error(
            "BIOS Attribute update event message encode failure. PLDM error code = {RC}",
            "RC", lg2::hex, rc);
        requester->markFree(eid, instanceId);
        return rc;
    }

    auto platformEventMessageResponseHandler =
        [](mctp_eid_t /*eid*/, const pldm_msg* response, size_t respMsgLen) {
        if (response == nullptr || !respMsgLen)
        {
            error("Failed to receive response for platform event message");
            return;
        }
        uint8_t completionCode{};
        uint8_t status{};
        auto rc = decode_platform_event_message_resp(response, respMsgLen,
                                                     &completionCode, &status);
        if (rc || completionCode)
        {
            error(
                "Failed to decode BIOS Attribute update platform_event_message_resp: rc = {RC}, cc= {CC}",
                "RC", rc, "CC", static_cast<unsigned>(completionCode));
        }
    };
    rc = handler->registerRequest(
        eid, instanceId, PLDM_PLATFORM, PLDM_PLATFORM_EVENT_MESSAGE,
        std::move(requestMsg), std::move(platformEventMessageResponseHandler));
    if (rc)
    {
        error(
            "Failed to send BIOS Attribute update the platform event message");
    }

    return rc;
}

} // namespace platform

} // namespace responder

} // namespace pldm
