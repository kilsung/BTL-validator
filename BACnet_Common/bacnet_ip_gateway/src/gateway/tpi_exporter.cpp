#include "tpi_exporter.h"

#include <iostream>
#include <fstream>

namespace gateway {

const char* TpiExporter::unitsToTpiString(BACSTAC_ENGINEERING_UNITS units)
{
    switch (units) {
        case UNIT_DEGREES_C:           return "degrees-celsius";
        case UNIT_DEGREES_F:           return "degrees-fahrenheit";
        case UNIT_PERCENT:             return "percent";
        case UNIT_HOURS:               return "hours";
        case UNIT_SECONDS:             return "seconds";
        case UNIT_MINUTES:             return "minutes";
        case UNIT_AMPERES:             return "amperes";
        case UNIT_HERTZ:               return "hertz";
        case UNIT_VOLTS:               return "volts";
        case UNIT_REVOLUTIONS_PER_MIN: return "revolutions-per-minute";
        case UNIT_KILOPASCALS:         return "kilopascals";
        case UNIT_BARS:                return "bars";
        case UNIT_CUBIC_METERS_PER_HOUR: return "cubic-meters-per-hour";
        case UNIT_NO_UNITS:
        default:                       return "no-units";
    }
}

bool TpiExporter::exportCurrentDeviceTpi(
    const std::string& filepath,
    const BACnetConfig& bacnet_cfg,
    const std::vector<ObjectMapping*>& mappings,
    const std::string& bip_mode)
{
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) {
        std::cerr << "[TPI Export] Failed to open file for writing: " << filepath << std::endl;
        return false;
    }

    ofs << "PICS 0\n"
        << "BACnet Protocol Implementation Conformance Statement\n\n"
        << "Vendor Name: \"" << bacnet_cfg.vendor_name << "\"\n"
        << "Product Name: \"" << bacnet_cfg.device_name << "\"\n"
        << "Product Model Number: \"" << bacnet_cfg.model_name << "\"\n"
        << "Product Description: \"" << bacnet_cfg.description << "\"\n\n"
        << "BIBBs Supported:\n"
        << "{\n"
        << "        DS-RP-B\n"
        << "        DS-WP-B\n"
        << "        DS-COV-B\n"
        << "        DM-DDB-B\n"
        << "        DM-DOB-B\n"
        << "        DM-DCC-B\n"
        << "}\n\n"
        << "BACnet Standard Application Services Supported:\n"
        << "{\n"
        << "        ReadProperty               Execute\n"
        << "        WriteProperty              Execute\n"
        << "        SubscribeCOV               Execute\n"
        << "        ConfirmedCOVNotification   Initiate\n"
        << "        UnconfirmedCOVNotification Initiate\n"
        << "        DeviceCommunicationControl Execute\n"
        << "        Who-Has                    Execute\n"
        << "        Who-Is                     Execute\n"
        << "        I-Have                     Initiate\n"
        << "        I-Am                       Initiate\n"
        << "}\n\n";

    bool has_ai = false;
    bool has_ao = false;
    bool has_bi = false;
#ifdef BINARY_OUTPUT_SUPPORT
    bool has_bo = false;
#endif
    bool has_bv = false;
    for (const auto* obj : mappings) {
        if (obj != nullptr) {
            if (obj->bacnet_type == OBJ_ANALOG_INPUT)  has_ai = true;
            if (obj->bacnet_type == OBJ_ANALOG_OUTPUT) has_ao = true;
            if (obj->bacnet_type == OBJ_BINARY_INPUT)  has_bi = true;
#ifdef BINARY_OUTPUT_SUPPORT
            if (obj->bacnet_type == OBJ_BINARY_OUTPUT) has_bo = true;
#endif
            if (obj->bacnet_type == OBJ_BINARY_VALUE)  has_bv = true;
        }
    }

    ofs << "Standard Object-Types Supported:\n"
        << "{\n"
        << "        Device\n";
    if (has_ai) ofs << "        Analog Input\n";
    if (has_ao) ofs << "        Analog Output\n";
    if (has_bi) ofs << "        Binary Input\n";
#ifdef BINARY_OUTPUT_SUPPORT
    if (has_bo) ofs << "        Binary Output\n";
#endif
    if (has_bv) ofs << "        Binary Value\n";
    ofs << "        Network Port\n"
        << "}\n\n"
        << "Data Link Layer Option:\n"
        << "{\n"
        << "        BACnet/IP\n"
        << "}\n\n"
        << "Character Sets Supported:\n"
        << "{\n"
        << "        ISO 10646 (UTF-8)\n"
        << "}\n\n"
        << "Special Functionality:\n"
        << "{\n"
        << "        Maximum APDU size in octets: " << bacnet_cfg.max_apdu_length << "\n"
        << "}\n\n"
        << "List of Objects in test device:\n"
        << "{\n"
        << "  {\n"
        << "         object-identifier: (device, " << bacnet_cfg.device_id << ")\n"
        << "         object-name: '" << bacnet_cfg.device_name << "'\n"
        << "         object-type: device\n"
        << "         system-status: operational\n"
        << "         vendor-name: \"" << bacnet_cfg.vendor_name << "\"\n"
        << "         vendor-identifier: " << bacnet_cfg.vendor_id << "\n"
        << "         model-name: \"" << bacnet_cfg.model_name << "\"\n"
        << "         description: \"" << bacnet_cfg.description << "\"\n"
        << "         firmware-revision: \"" << bacnet_cfg.firmware_revision << "\"\n"
        << "         application-software-version: \"" << bacnet_cfg.app_software_version << "\"\n"
        << "         protocol-version: 1\n"
        << "         protocol-revision: " << bacnet_cfg.protocol_revision << "\n"
        << "         protocol-services-supported:\n"
        << "         {\n"
        << "                  F, F,  -- acknowledge-alarm, confirmed-cov-notification,\n"
        << "                  F, F,  -- confirmed-event-notification, get-alarm-summary,\n"
        << "                  F, T,  -- get-enrollment-summary, subscribe-cov,\n"
        << "                  F, F,  -- atomic-read-file, atomic-write-file,\n"
        << "                  F, F,  -- add-list-element, remove-list-element,\n"
        << "                  F, F,  -- create-object, delete-object,\n"
        << "                  T, F,  -- read-property, read-property-conditional,\n"
        << "                  F, T,  -- read-property-multiple, write-property,\n"
        << "                  F, T,  -- write-property-multiple, device-communication-control,\n"
        << "                  F, F,  -- confirmed-private-transfer, confirmed-text-message,\n"
        << "                  F, F,  -- reinitialize-device, vt-open,\n"
        << "                  F, F,  -- vt-close, vt-data,\n"
        << "                  F, F,  -- authenticate, request-key,\n"
        << "                  F, F,  -- i-am, i-have,\n"
        << "                  F, F,  -- unconfirmed-cov-notification, unconfirmed-event-notification,\n"
        << "                  F, F,  -- unconfirmed-private-transfer, unconfirmed-text-message,\n"
        << "                  F, T,  -- time-synchronization, who-has,\n"
        << "                  T, F,  -- who-is, read-range,\n"
        << "                  F, F,  -- utc-time-synchronization, life-safety-operation,\n"
        << "                  F, F,  -- subscribe-cov-property, get-event-information,\n"
        << "                  F, F,  -- write-group, subscribe-cov-property-multiple,\n"
        << "                  F, F,  -- confirmed-cov-notification-multiple, unconfirmed-cov-notification-multiple\n"
        << "                  F, F,  -- confirmed-audit-notification, audit-log-query,\n"
        << "                  F, F,  -- unconfirmed-audit-notification, who-am-i,\n"
        << "                  F      -- you-are\n"
        << "         }\n\n"
        << "         protocol-object-types-supported:\n"
        << "         {\n"
        << "                  " << (has_ai ? "T" : "F") << ", "
                                << (has_ao ? "T" : "F") << ", F, "
                                << (has_bi ? "T" : "F") << ", "
#ifdef BINARY_OUTPUT_SUPPORT
                                << (has_bo ? "T" : "F") << ", "
#else
                                << "F, "
#endif
                                << (has_bv ? "T" : "F") << ",        -- AI, AO, AV, BI, BO, BV\n"
        << "                  F, F,                    -- calendar, command\n"
        << "                  T, F,                    -- device, event enrollment\n"
        << "                  F, F, F,                 -- file, group, loop\n"
        << "                  F, F, F,                 -- MI, MO, notification class\n"
        << "                  F, F,                    -- program, schedule\n"
        << "                  F, F, F,                 -- averaging, multi-state-value, trend-log\n"
        << "                  F, F,                    -- life-safety-point, life-safety-zone\n"
        << "                  F, F,                    -- accumulator, pulse-converter\n"
        << "                  F, F, F,                 -- event-log, global-group, trend-log-multiple\n"
        << "                  F, F, F,                 -- load-control, structured-view, access-door\n"
        << "                  F,                       -- timer,\n"
        << "                  F, F, F,                 -- access-credential, access-point, access-rights,\n"
        << "                  F, F, F,                 -- access-user, access-zone, credential-data-input\n"
        << "                  F,                       -- network-security\n"
        << "                  F, F, F,                 -- bitstring-value, characterstring-value, date-pattern-value\n"
        << "                  F, F, F,                 -- date-value, datetime-pattern-value, datetime-value\n"
        << "                  F, F, F,                 -- integer-value, large-analog-value, octetstring-value\n"
        << "                  F, F, F,                 -- positive-Integer-value, time-pattern-value, time-value\n"
        << "                  F, F,                    -- notification-forwarder, alert-enrollment\n"
        << "                  F, F,                    -- channel,lighting-output\n"
        << "                  F, T,                    -- binary-lighting-output, network-port\n"
        << "                  F, F, F,                 -- elevator-group, escalator, lift\n"
        << "                  F, F, F,                 -- staging, audit-log, audit-reporter\n"
        << "                  F, F                     -- color, color-temperature\n"
        << "         }\n\n"
        << "         object-list: AUTO\n"
        << "         max-APDU-length-accepted: " << bacnet_cfg.max_apdu_length << "\n"
        << "         segmentation-supported: no-segmentation\n"
        << "         max-segments-accepted: 1\n"
        << "         apdu-timeout: " << bacnet_cfg.apdu_timeout_ms << "\n"
        << "         number-of-APDU-retries: " << bacnet_cfg.apdu_retries << "\n"
        << "         device-address-binding: ?\n"
        << "         database-revision: ?\n"
        << "         device-uuid: ?\n"
        << "         daylight-savings-status: F\n"
        << "         active-cov-subscriptions: ?\n"
        << "         local-time: AUTO\n"
        << "         local-date: AUTO\n"
        << "  },\n"
        << "  {\n"
        << "         object-identifier: (network-port, " << bacnet_cfg.network_port_instance << ")\n"
        << "         object-name: \"" << bacnet_cfg.network_port_name << "\"\n"
        << "         object-type: network-port\n"
        << "         status-flags: {F, F, F, F}\n"
        << "         reliability: no-fault-detected\n"
        << "         out-of-service: F\n"
        << "         network-type: ipv4\n"
        << "         protocol-level: bacnet-application\n"
        << "         network-number: 0\n"
        << "         network-number-quality: unknown\n"
        << "         changes-pending: F\n"
        << "         apdu-length: " << bacnet_cfg.max_apdu_length << "\n"
        << "         link-speed: 0.0\n"
        << "         mac-address: ?\n"
        << "         network-interface-name: ?\n"
        << "         ip-address: ?\n"
        << "         ip-subnet-mask: ?\n"
        << "         ip-default-gateway: ?\n"
        << "         bacnet-ip-udp-port: ?\n";

    // BACnet_IP_Mode(normal/foreign-device)와 FD 속성은 모드와 무관하게 존재하며
    // WriteProperty로 즉시 변경할 수 있다(게이트웨이 재시작으로 반영).
    if (bip_mode == "fd") {
        ofs << "         bacnet-ip-mode: foreign-device W\n"
            << "         ip-dhcp-enable: F\n"
            << "         ip-dhcp-lease-time: ?\n"
            << "         ip-dhcp-lease-time-remaining: ?\n"
            << "         ip-dhcp-server: ?\n"
            << "         ip-dns-server: ?\n"
            << "         fd-bbmd-address: ? W\n"
            << "         fd-subscription-lifetime: ? W\n";
    } else if (bip_mode == "bbmd") {
        ofs << "         bacnet-ip-mode: bbmd\n"
            << "         ip-dhcp-enable: F\n"
            << "         ip-dhcp-lease-time: ?\n"
            << "         ip-dhcp-lease-time-remaining: ?\n"
            << "         ip-dhcp-server: ?\n"
            << "         ip-dns-server: ?\n";
    } else {
        ofs << "         bacnet-ip-mode: normal W\n"
            << "         ip-dhcp-enable: F\n"
            << "         ip-dhcp-lease-time: ?\n"
            << "         ip-dhcp-lease-time-remaining: ?\n"
            << "         ip-dhcp-server: ?\n"
            << "         ip-dns-server: ?\n"
            << "         fd-bbmd-address: ? W\n"
            << "         fd-subscription-lifetime: ? W\n";
    }

    ofs << "         routing-table: ?\n"
        << (mappings.empty() ? "  }\n" : "  },\n");

#ifdef OUT_OF_SERVICE
    const std::string oos_str = "         out-of-service: F W\n";
#else
    const std::string oos_str = "         out-of-service: F\n";
#endif

    for (size_t idx = 0; idx < mappings.size(); ++idx) {
        const auto* mapping = mappings[idx];
        if (mapping == nullptr) continue;
        const bool is_last = (idx + 1 == mappings.size());
        const std::string block_end = is_last ? "  }\n" : "  },\n";

        if (mapping->bacnet_type == OBJ_ANALOG_INPUT) {
            ofs << "  {\n"
                << "         object-identifier: (analog-input, " << mapping->instance << ")\n"
                << "         object-name: \"" << mapping->name << "\"\n"
                << "         object-type: analog-input\n"
                << "         present-value: ?\n";
            if (!mapping->description.empty()) {
                ofs << "         description: \"" << mapping->description << "\"\n";
            }
            ofs << "         status-flags: {F, F, F, F}\n"
                << "         event-state: normal\n"
                << oos_str
                << "         units: " << unitsToTpiString(mapping->units) << "\n";
            if (mapping->has_min_value) {
                ofs << "         min-pres-value: " << mapping->min_value << "\n";
            }
            if (mapping->has_max_value) {
                ofs << "         max-pres-value: " << mapping->max_value << "\n";
            }
            ofs << "         cov-increment: " << mapping->cov_increment << " W\n"
                << block_end;
        } else if (mapping->bacnet_type == OBJ_ANALOG_OUTPUT) {
            ofs << "  {\n"
                << "         object-identifier: (analog-output, " << mapping->instance << ")\n"
                << "         object-name: \"" << mapping->name << "\"\n"
                << "         object-type: analog-output\n"
                << "         present-value: ? W\n";
            if (!mapping->description.empty()) {
                ofs << "         description: \"" << mapping->description << "\"\n";
            }
            ofs << "         status-flags: {F, F, F, F}\n"
                << "         event-state: normal\n"
                << oos_str
                << "         units: " << unitsToTpiString(mapping->units) << "\n";
            if (mapping->has_min_value) {
                ofs << "         min-pres-value: " << mapping->min_value << "\n";
            }
            if (mapping->has_max_value) {
                ofs << "         max-pres-value: " << mapping->max_value << "\n";
            }
            ofs << "         cov-increment: " << mapping->cov_increment << " W\n"
                << "         priority-array: ?\n"
                << "         relinquish-default: " << (mapping->has_relinquish_default ? std::to_string(mapping->relinquish_default) : "?") << "\n"
                << block_end;
        } else if (mapping->bacnet_type == OBJ_BINARY_INPUT) {
            ofs << "  {\n"
                << "         object-identifier: (binary-input, " << mapping->instance << ")\n"
                << "         object-name: \"" << mapping->name << "\"\n"
                << "         object-type: binary-input\n"
                << "         present-value: ?\n";
            if (!mapping->description.empty()) {
                ofs << "         description: \"" << mapping->description << "\"\n";
            }
            ofs << "         status-flags: {F, F, F, F}\n"
                << "         event-state: normal\n"
                << oos_str
                << "         polarity: normal W\n"
                << block_end;
#ifdef BINARY_OUTPUT_SUPPORT
        } else if (mapping->bacnet_type == OBJ_BINARY_OUTPUT) {
            ofs << "  {\n"
                << "         object-identifier: (binary-output, " << mapping->instance << ")\n"
                << "         object-name: \"" << mapping->name << "\"\n"
                << "         object-type: binary-output\n"
                << "         present-value: ? W\n";
            if (!mapping->description.empty()) {
                ofs << "         description: \"" << mapping->description << "\"\n";
            }
            ofs << "         status-flags: {F, F, F, F}\n"
                << "         event-state: normal\n"
                << oos_str
                << "         polarity: normal W\n"
                << "         priority-array: ?\n"
                << "         relinquish-default: " << (mapping->has_relinquish_default ? (mapping->relinquish_default != 0.0 ? "active" : "inactive") : "?") << "\n"
                << block_end;
#endif
        } else if (mapping->bacnet_type == OBJ_BINARY_VALUE) {
            ofs << "  {\n"
                << "         object-identifier: (binary-value, " << mapping->instance << ")\n"
                << "         object-name: \"" << mapping->name << "\"\n"
                << "         object-type: binary-value\n"
                << "         present-value: ? W\n";
            if (!mapping->description.empty()) {
                ofs << "         description: \"" << mapping->description << "\"\n";
            }
            ofs << "         status-flags: {F, F, F, F}\n"
                << "         event-state: normal\n"
                << oos_str
                << block_end;
        }
    }

    ofs << "}\n\n"
        << "End of BACnet Protocol Implementation Conformance Statement\n";

    std::cout << "[TPI Export] Successfully generated: " << filepath << " (Total objects: " << (mappings.size() + 2) << ")" << std::endl;
    return true;
}

} // namespace gateway
