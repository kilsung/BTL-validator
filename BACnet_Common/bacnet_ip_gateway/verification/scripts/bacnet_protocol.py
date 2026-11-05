#!/usr/bin/env python3
import socket
import time
import struct
import sys
import math as _math

# ===========================================================================
# BACnet Protocol Constants & Mappings
# ===========================================================================
BACNET_OBJECT_TYPES = {
    "ANALOG_INPUT": 0,
    "ANALOG_OUTPUT": 1,
    "BINARY_INPUT": 3,
    "BINARY_OUTPUT": 4,
    "BINARY_VALUE": 5
}
BACNET_OBJECT_NAMES = {v: k for k, v in BACNET_OBJECT_TYPES.items()}

BACNET_ERROR_CLASSES = {
    0: "DEVICE",
    1: "OBJECT",
    2: "PROPERTY",
    3: "RESOURCES",
    4: "SECURITY",
    5: "SERVICES"
}

BACNET_ERROR_CODES = {
    0: "OTHER",
    2: "CONFIGURATION_IN_PROGRESS",
    3: "DEVICE_BUSY",
    7: "INVALID_DATA_TYPE",
    9: "INCONSISTENT_PARAMETERS",
    14: "OTHER",
    20: "NO_SPACE_TO_WRITE_PROPERTY",
    25: "OPERATIONAL_PROBLEM",
    26: "NO_SPACE_FOR_OBJECT",
    27: "NO_SPACE_TO_ADD_LIST",
    28: "OBJECT_DELETION_NOT_ALLOWED",
    31: "OBJECT_IDENTIFIER_ALREADY_EXISTS",
    32: "UNKNOWN_PROPERTY",
    36: "READ_ACCESS_DENIED",
    37: "VALUE_OUT_OF_RANGE",
    40: "WRITE_ACCESS_DENIED",
    42: "INVALID_ARRAY_INDEX",
    50: "PROPERTY_IS_NOT_AN_ARRAY",
    73: "PARAMETER_OUT_OF_RANGE"
}

# ===========================================================================
# Network Helper Functions
# ===========================================================================
def get_local_ip_for_target(target_ip):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect((target_ip, 47808))
        local_ip = s.getsockname()[0]
        s.close()
        return local_ip
    except Exception:
        return ""

def build_bacnet_ip_packet(dnet, dadr, invoke_id, apdu):
    """
    Constructs a complete BACnet/IP packet (BVLC + NPDU + APDU).
    If dnet is specified, constructs routed NPDU (control = 0x24 / 0x20).
    If dnet is None (Single-Device mode), constructs direct NPDU (control = 0x04 / 0x00).
    """
    is_confirmed = (len(apdu) > 0 and (apdu[0] & 0xf0) == 0x00)
    is_broadcast = (dnet == 65535 or dadr == 255)

    if dnet is not None:
        control = 0x24 if is_confirmed else 0x20
        dadr_bytes = bytes([dadr]) if isinstance(dadr, int) else bytes(dadr)
        dadr_len = len(dadr_bytes) if not is_broadcast else 0
        
        npdu = bytearray([
            0x01,                   # Protocol Version
            control,                # Control Byte
            (dnet >> 8) & 0xff,     # DNET High
            dnet & 0xff,            # DNET Low
            dadr_len                # DADR Length
        ])
        if not is_broadcast:
            npdu.extend(dadr_bytes)
        npdu.append(255)            # Hop Count
    else:
        control = 0x04 if is_confirmed else 0x00
        npdu = bytearray([
            0x01,                   # Protocol Version
            control                 # Control Byte: Direct Unicast
        ])

    npdu_apdu = bytes(npdu) + apdu
    total_len = 4 + len(npdu_apdu)
    
    bvlc = bytes([
        0x81,                   # BVLC Type: BACnet/IP
        0x0B if is_broadcast else 0x0A, # Function: Broadcast or Original-Unicast-NPDU
        (total_len >> 8) & 0xff,
        total_len & 0xff
    ])
    return bvlc + npdu_apdu

# ===========================================================================
# APDU Builders
# ===========================================================================
def encode_context_tag_1_prop_id(property_id):
    if property_id <= 255:
        return bytes([0x19, property_id & 0xff])
    else:
        return bytes([0x1A, (property_id >> 8) & 0xff, property_id & 0xff])

def build_read_property_apdu(object_type, instance, invoke_id, property_id=85, array_index=None):
    """
    Builds ConfirmedRequest APDU for ReadProperty.
    property_id default: 85 (Present_Value)
    array_index: optional Unsigned integer for array index (0 = count, 1..N = element)
    """
    packed = (object_type << 22) | (instance & 0x3fffff)
    apdu = bytearray([
        0x00, # PDU Type: Confirmed Request
        0x05, # Max Segments / Max APDU (1476 bytes)
        invoke_id & 0xff,
        12,   # Service Choice: ReadProperty
        0x0c, # Context tag 0, length 4 (Object Identifier)
        (packed >> 24) & 0xff, (packed >> 16) & 0xff, (packed >> 8) & 0xff, packed & 0xff
    ]) + bytearray(encode_context_tag_1_prop_id(property_id))

    if array_index is not None:
        if array_index <= 255:
            apdu.extend([0x29, array_index & 0xff])
        elif array_index <= 65535:
            apdu.extend([0x2A, (array_index >> 8) & 0xff, array_index & 0xff])
        else:
            apdu.extend([0x2C, (array_index >> 24) & 0xff, (array_index >> 16) & 0xff, (array_index >> 8) & 0xff, array_index & 0xff])

    return bytes(apdu)

def build_write_property_apdu(object_type, instance, invoke_id, value, is_binary, priority=16):
    """
    Builds ConfirmedRequest APDU for WriteProperty (Present_Value).
    If value is None, encodes as Null (0x00) for Relinquish operation.
    """
    packed = (object_type << 22) | (instance & 0x3fffff)
    apdu = bytearray([
        0x00, # PDU Type: Confirmed Request
        0x05, # Max Segments / Max APDU
        invoke_id & 0xff,
        15,   # Service Choice: WriteProperty
        0x0c, # Context tag 0, length 4 (Object Identifier)
        (packed >> 24) & 0xff, (packed >> 16) & 0xff, (packed >> 8) & 0xff, packed & 0xff,
        0x19, # Context tag 1, length 1 (Property Identifier)
        85,   # Property ID 85: Present_Value
        0x3e  # Context tag 3 (Opening tag)
    ])

    if value is None:
        apdu.append(0x00) # Null (Relinquish)
    elif is_binary:
        apdu.extend([0x91, 0x01 if value else 0x00]) # Enumerated
    else:
        apdu.append(0x44) # Real (Float32)
        apdu.extend(struct.pack('>f', float(value)))

    apdu.append(0x3f) # Context tag 3 (Closing tag)

    if priority is not None:
        apdu.extend([
            0x49, # Context tag 4, length 1 (Priority)
            priority & 0xff
        ])

    return bytes(apdu)

def build_write_property_raw_apdu(object_type, instance, invoke_id, property_id, value_bytes):
    """ConfirmedRequest WriteProperty whose property value is pre-encoded ASN.1 (no priority)."""
    packed = (object_type << 22) | (instance & 0x3fffff)
    return bytes([
        0x00, 0x05, invoke_id & 0xff, 15,
        0x0c, (packed >> 24) & 0xff, (packed >> 16) & 0xff, (packed >> 8) & 0xff, packed & 0xff,
    ]) + encode_context_tag_1_prop_id(property_id) + bytes([0x3e]) + bytes(value_bytes) + bytes([0x3f])


def build_write_property_generic_apdu(object_type, instance, invoke_id, property_id, value, tag_type, priority=None):
    """
    Builds ConfirmedRequest APDU for WriteProperty with generic property ID and ASN.1 tag type.
    tag_type: 9 = Enumerated (1 byte), 4 = Real (4 bytes), 1 = Boolean, None/0 = Null
    """
    packed = (object_type << 22) | (instance & 0x3fffff)
    apdu = bytes([
        0x00, # PDU Type: Confirmed Request
        0x05, # Max Segments / Max APDU
        invoke_id & 0xff,
        15,   # Service Choice: WriteProperty
        0x0c, # Context tag 0, length 4 (Object Identifier)
        (packed >> 24) & 0xff, (packed >> 16) & 0xff, (packed >> 8) & 0xff, packed & 0xff
    ]) + encode_context_tag_1_prop_id(property_id) + bytes([0x3e]) # Context tag 3 (Opening tag)

    if value is None:
        val_bytes = bytes([0x00]) # Null
    elif tag_type == 9: # Enumerated
        val_bytes = bytes([0x91, int(value) & 0xff])
    elif tag_type == 4: # Real
        val_bytes = b'\x44' + struct.pack('>f', float(value))
    elif tag_type == 1: # Boolean
        val_bytes = bytes([0x11 if value else 0x10])
    elif tag_type == 2 or tag_type == 0x30: # Signed Integer
        v = int(value)
        if -128 <= v <= 127:
            val_bytes = bytes([0x31, v & 0xff])
        elif -32768 <= v <= 32767:
            val_bytes = bytes([0x32, (v >> 8) & 0xff, v & 0xff])
        else:
            val_bytes = bytes([0x34, (v >> 24) & 0xff, (v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff])
    else:
        val_bytes = bytes([0x91, int(value) & 0xff])

    apdu += val_bytes + bytes([0x3f]) # Context tag 3 (Closing tag)

    if priority is not None:
        apdu += bytes([
            0x49, # Context tag 4, length 1 (Priority)
            priority & 0xff
        ])

    return apdu

def build_subscribe_cov_apdu(monitored_obj_type, monitored_instance, subscriber_process_id, invoke_id, issue_confirmed=True, lifetime=60, cancel=False):
    """
    Builds ConfirmedRequest APDU for SubscribeCOV (Service Choice 5).
    If cancel is True, issueConfirmedNotifications and lifetime are omitted as per BACnet spec.
    """
    packed_obj = (monitored_obj_type << 22) | (monitored_instance & 0x3fffff)
    apdu = bytearray([
        0x00, # PDU Type: Confirmed Request
        0x05, # Max Segments / Max APDU (1476 bytes)
        invoke_id & 0xff,
        5     # Service Choice: SubscribeCOV
    ])

    # Context tag 0: subscriberProcessIdentifier (Unsigned)
    if subscriber_process_id < 256:
        apdu.extend([0x09, subscriber_process_id & 0xff])
    elif subscriber_process_id < 65536:
        apdu.extend([0x0A, (subscriber_process_id >> 8) & 0xff, subscriber_process_id & 0xff])
    else:
        apdu.extend([0x0C, (subscriber_process_id >> 24) & 0xff, (subscriber_process_id >> 16) & 0xff, (subscriber_process_id >> 8) & 0xff, subscriber_process_id & 0xff])

    # Context tag 1: monitoredObjectIdentifier (BACnetObjectIdentifier)
    apdu.extend([
        0x1C,
        (packed_obj >> 24) & 0xff, (packed_obj >> 16) & 0xff, (packed_obj >> 8) & 0xff, packed_obj & 0xff
    ])

    if not cancel:
        # Context tag 2: issueConfirmedNotifications (Boolean, len=1)
        apdu.extend([0x29, 0x01 if issue_confirmed else 0x00])

        # Context tag 3: lifetime (Unsigned)
        if lifetime < 256:
            apdu.extend([0x39, lifetime & 0xff])
        elif lifetime < 65536:
            apdu.extend([0x3A, (lifetime >> 8) & 0xff, lifetime & 0xff])
        else:
            apdu.extend([0x3C, (lifetime >> 24) & 0xff, (lifetime >> 16) & 0xff, (lifetime >> 8) & 0xff, lifetime & 0xff])

    return bytes(apdu)

def build_subscribe_cov_property_apdu(monitored_obj_type, monitored_instance, subscriber_process_id, invoke_id,
                                      property_id=85, issue_confirmed=True, lifetime=0, cov_increment=None, cancel=False):
    """
    Builds ConfirmedRequest APDU for SubscribeCOVProperty (Service Choice 28).
    If cancel is True, issueConfirmedNotifications, lifetime, and covIncrement are omitted as per BACnet spec.
    """
    packed_obj = (monitored_obj_type << 22) | (monitored_instance & 0x3fffff)
    apdu = bytearray([
        0x00, # PDU Type: Confirmed Request
        0x05, # Max Segments / Max APDU (1476 bytes)
        invoke_id & 0xff,
        28    # Service Choice: SubscribeCOVProperty
    ])

    # Context tag 0: subscriberProcessIdentifier (Unsigned)
    if subscriber_process_id < 256:
        apdu.extend([0x09, subscriber_process_id & 0xff])
    elif subscriber_process_id < 65536:
        apdu.extend([0x0A, (subscriber_process_id >> 8) & 0xff, subscriber_process_id & 0xff])
    else:
        apdu.extend([0x0C, (subscriber_process_id >> 24) & 0xff, (subscriber_process_id >> 16) & 0xff, (subscriber_process_id >> 8) & 0xff, subscriber_process_id & 0xff])

    # Context tag 1: monitoredObjectIdentifier (BACnetObjectIdentifier)
    apdu.extend([
        0x1C,
        (packed_obj >> 24) & 0xff, (packed_obj >> 16) & 0xff, (packed_obj >> 8) & 0xff, packed_obj & 0xff
    ])

    if not cancel:
        # Context tag 2: issueConfirmedNotifications (Boolean, len=1)
        apdu.extend([0x29, 0x01 if issue_confirmed else 0x00])

        # Context tag 3: lifetime (Unsigned)
        if lifetime < 256:
            apdu.extend([0x39, lifetime & 0xff])
        elif lifetime < 65536:
            apdu.extend([0x3A, (lifetime >> 8) & 0xff, lifetime & 0xff])
        else:
            apdu.extend([0x3C, (lifetime >> 24) & 0xff, (lifetime >> 16) & 0xff, (lifetime >> 8) & 0xff, lifetime & 0xff])

    # Context tag 4: monitoredPropertyIdentifier (BACnetPropertyReference: Opening 0x4E ... Closing 0x4F)
    apdu.append(0x4E)
    if property_id < 256:
        apdu.extend([0x09, property_id & 0xff])
    elif property_id < 65536:
        apdu.extend([0x0A, (property_id >> 8) & 0xff, property_id & 0xff])
    else:
        apdu.extend([0x0C, (property_id >> 24) & 0xff, (property_id >> 16) & 0xff, (property_id >> 8) & 0xff, property_id & 0xff])
    apdu.append(0x4F)

    if not cancel and cov_increment is not None:
        inc_bytes = struct.pack('>f', float(cov_increment))
        apdu.extend([0x5C, inc_bytes[0], inc_bytes[1], inc_bytes[2], inc_bytes[3]])

    return bytes(apdu)

def build_rpm_apdu(objects, invoke_id):
    """
    Builds ConfirmedRequest APDU for ReadPropertyMultiple.
    objects: list of tuples (object_type, instance)
    """
    apdu = bytes([
        0x00, # PDU Type: Confirmed Request
        0x05, # Max Segments / Max APDU (1476 bytes)
        invoke_id & 0xff,
        14    # Service Choice: ReadPropertyMultiple
    ])

    for obj_type, instance in objects:
        packed = (obj_type << 22) | (instance & 0x3fffff)
        apdu += bytes([
            0x0c, # Context tag 0, length 4 (Object Identifier)
            (packed >> 24) & 0xff, (packed >> 16) & 0xff, (packed >> 8) & 0xff, packed & 0xff,
            0x1e, # Context tag 1 (Opening tag: listOfPropertyReferences)
            0x09, 85, # Context tag 0, length 1 (Property ID 85: Present_Value)
            0x1f  # Context tag 1 (Closing tag)
        ])
    return apdu

def build_wpm_apdu(writes, invoke_id):
    """
    Builds ConfirmedRequest APDU for WritePropertyMultiple.
    writes: list of dicts:
      [{"type": obj_type, "instance": inst, "prop_id": 85, "value": val, "is_binary": bool, "priority": 16}]
    """
    apdu = bytearray([
        0x00, # PDU Type: Confirmed Request
        0x05, # Max Segments / Max APDU
        invoke_id & 0xff,
        16    # Service Choice: WritePropertyMultiple (16)
    ])

    for w in writes:
        packed = (w['type'] << 22) | (w['instance'] & 0x3fffff)
        apdu.extend([
            0x0c, # Context tag 0, len 4 (Object Identifier)
            (packed >> 24) & 0xff, (packed >> 16) & 0xff, (packed >> 8) & 0xff, packed & 0xff,
            0x1e  # Context tag 1 (Opening tag: listOfProperties)
        ])
        prop_id = w.get('prop_id', 85)
        apdu.extend([
            0x09, prop_id & 0xff, # Context tag 0, len 1 (Property Identifier)
            0x2e  # Context tag 2 (Opening tag: propertyValue)
        ])
        val = w.get('value')
        if val is None:
            apdu.append(0x00) # Null (Relinquish)
        elif w.get('is_binary', False):
            apdu.extend([0x91, 0x01 if val else 0x00]) # Enumerated
        else:
            apdu.append(0x44) # Real
            apdu.extend(struct.pack('>f', float(val)))
        apdu.append(0x2f) # Context tag 2 (Closing tag)

        priority = w.get('priority')
        if priority is not None:
            apdu.extend([0x39, priority & 0xff]) # Context tag 3, len 1 (Priority)
        apdu.append(0x1f) # Context tag 1 (Closing tag: listOfProperties)

    return bytes(apdu)

def build_dcc_apdu(enable_state, invoke_id, time_duration=None, password=None):
    """
    Builds ConfirmedRequest APDU for DeviceCommunicationControl (Service Choice 17).
    enable_state: 0 = ENABLE, 1 = DISABLE, 2 = DISABLE_INITIATION
    time_duration: optional minutes (Unsigned)
    password: optional string (CharacterString)
    """
    apdu = bytearray([
        0x00, # PDU Type: Confirmed Request
        0x05, # Max Segments / Max APDU (1476 bytes)
        invoke_id & 0xff,
        17    # Service Choice: DeviceCommunicationControl (17)
    ])

    if time_duration is not None:
        if time_duration < 256:
            apdu.extend([0x09, time_duration & 0xff]) # Context tag 0, len 1
        else:
            apdu.extend([0x0A, (time_duration >> 8) & 0xff, time_duration & 0xff]) # Context tag 0, len 2

    apdu.extend([0x19, enable_state & 0xff]) # Context tag 1, len 1 (enable-disable: Enumerated)

    if password is not None:
        pwd_bytes = password.encode('utf-8')
        apdu.extend([0x25, len(pwd_bytes) + 1, 0x00]) # Context tag 2, encoding UTF-8 (0)
        apdu.extend(pwd_bytes)

    return bytes(apdu)

def build_who_is_apdu(low_limit=None, high_limit=None):
    """
    Builds UnconfirmedRequest APDU for Who-Is (Service Choice 8).
    If limits are None, sends global Who-Is.
    """
    apdu = bytearray([
        0x10, # PDU Type: Unconfirmed Request
        0x08  # Service Choice: Who-Is
    ])
    if low_limit is not None and high_limit is not None:
        if low_limit < 65536:
            apdu.extend([0x0A, (low_limit >> 8) & 0xff, low_limit & 0xff])
        else:
            apdu.extend([0x0C, (low_limit >> 24) & 0xff, (low_limit >> 16) & 0xff, (low_limit >> 8) & 0xff, low_limit & 0xff])
        if high_limit < 65536:
            apdu.extend([0x1A, (high_limit >> 8) & 0xff, high_limit & 0xff])
        else:
            apdu.extend([0x1C, (high_limit >> 24) & 0xff, (high_limit >> 16) & 0xff, (high_limit >> 8) & 0xff, high_limit & 0xff])
    return bytes(apdu)

def build_who_has_apdu(object_name):
    """
    Builds UnconfirmedRequest APDU for Who-Has by Object Name (Service Choice 7).
    """
    name_bytes = object_name.encode('utf-8')
    apdu = bytearray([
        0x10, # PDU Type: Unconfirmed Request
        0x07, # Service Choice: Who-Has
        0x3D, len(name_bytes) + 1, 0x00 # Context tag 3, len = length + 1, character string (encoding 0 = UTF-8)
    ])
    apdu.extend(name_bytes)
    return bytes(apdu)

# ===========================================================================
# APDU Response Parsers
# ===========================================================================
def read_enumerated(data, idx):
    if idx >= len(data): return 0, idx
    tag = data[idx]
    length = tag & 0x07
    val = 0
    for i in range(length):
        val = (val << 8) | data[idx + 1 + i]
    return val, idx + 1 + length

def parse_rpm_response(resp, expected_invoke_id):
    """
    Parses ReadPropertyMultiple ComplexACK response.
    Returns: list of (status, value) tuples in the same order as requested.
    """
    if len(resp) < 6 or resp[0] != 0x81:
        return [("ERROR_INVALID_PACKET", "Not a BACnet/IP packet")]

    control = resp[5]
    idx = 6
    if control & 0x80: return [("ERROR_NETWORK_LAYER", "Network layer msg")]
    if control & 0x20:
        if idx + 2 > len(resp): return [("ERROR_NPDU", "Truncated")]
        idx += 2
        dadr_len = resp[idx]
        idx += 1 + dadr_len + 1
    if control & 0x08:
        if idx + 2 > len(resp): return [("ERROR_NPDU", "Truncated")]
        idx += 2
        sadr_len = resp[idx]
        idx += 1 + sadr_len

    if idx >= len(resp): return [("ERROR_NO_APDU", "No APDU")]

    pdu_type = resp[idx]
    if (pdu_type & 0xf0) != 0x30:
        return [("ERROR_NOT_COMPLEX_ACK", f"PDU Type: 0x{pdu_type:02X}")]

    service_choice = resp[idx + 2]
    if service_choice != 14:
        return [("ERROR_UNEXPECTED_SERVICE", f"Expected RPM ACK (14), got {service_choice}")]

    apdu_data = resp[idx + 3:]
    results = []
    p = 0
    while p < len(apdu_data):
        if p + 5 > len(apdu_data): break
        if apdu_data[p] != 0x0c: break
        p += 5
        if p >= len(apdu_data) or apdu_data[p] != 0x1e: break
        p += 1

        while p < len(apdu_data) and apdu_data[p] != 0x1f:
            if (apdu_data[p] & 0xf8) == 0x08:
                prop_tag = apdu_data[p]
                prop_len = prop_tag & 0x07
                p += 1 + (prop_len if prop_len != 5 else 1 + apdu_data[p + 1])
            else:
                p += 1
                continue

            if p >= len(apdu_data): break

            if apdu_data[p] == 0x4e:
                p += 1
                val_start = p
                close_pos = apdu_data.find(0x4f, val_start)
                if close_pos == -1:
                    results.append(("ERROR_TRUNCATED_VAL", "No closing tag"))
                    break
                val_bytes = apdu_data[val_start:close_pos]
                p = close_pos + 1
                if not val_bytes:
                    results.append(("SUCCESS", None))
                else:
                    vtag = val_bytes[0]
                    if vtag == 0x44 and len(val_bytes) >= 5:
                        results.append(("SUCCESS", struct.unpack('>f', val_bytes[1:5])[0]))
                    elif (vtag & 0xf0) == 0x90:
                        results.append(("SUCCESS", val_bytes[1] if len(val_bytes) >= 2 else 0))
                    elif (vtag & 0xf0) == 0x20:
                        l = vtag & 0x0f
                        v = 0
                        for b in val_bytes[1:1+l]: v = (v << 8) | b
                        results.append(("SUCCESS", v))
                    elif vtag == 0x00:
                        results.append(("SUCCESS", None))
                    else:
                        results.append(("SUCCESS", f"TAG_0x{vtag:02X}"))
            elif apdu_data[p] == 0x5e:
                p += 1
                close_pos = apdu_data.find(0x5f, p)
                err_bytes = apdu_data[p:close_pos] if close_pos != -1 else b""
                p = close_pos + 1 if close_pos != -1 else len(apdu_data)
                err_class, next_i = read_enumerated(err_bytes, 0)
                err_code, _ = read_enumerated(err_bytes, next_i)
                results.append(("BACNET_ERROR_PDU", f"Class {err_class}, Code {err_code}"))

        if p < len(apdu_data) and apdu_data[p] == 0x1f:
            p += 1

    return results

def parse_wpm_response(resp, expected_invoke_id):
    """
    Parses WritePropertyMultiple SimpleACK or Error response.
    Returns: (STATUS_STRING, PARSED_VALUE_OR_ERROR_DESC)
    """
    if len(resp) < 6 or resp[0] != 0x81:
        return "ERROR_INVALID_PACKET", "Not a BACnet/IP packet"

    control = resp[5]
    idx = 6
    if control & 0x80: return "ERROR_NETWORK_LAYER", "Network layer message"
    if control & 0x20:
        if idx + 2 > len(resp): return "ERROR_NPDU", "Truncated"
        idx += 2
        dadr_len = resp[idx]
        idx += 1 + dadr_len + 1
    if control & 0x08:
        if idx + 2 > len(resp): return "ERROR_NPDU", "Truncated"
        idx += 2
        sadr_len = resp[idx]
        idx += 1 + sadr_len

    if idx >= len(resp): return "ERROR_NO_APDU", "No APDU"

    pdu_type = resp[idx]

    if (pdu_type & 0xf0) == 0x20:
        resp_inv = resp[idx + 1]
        if resp_inv != expected_invoke_id:
            return "ERROR_INVOKE_ID_MISMATCH", f"Expected {expected_invoke_id}, got {resp_inv}"
        srv = resp[idx + 2]
        if srv == 16:
            return "SUCCESS", "ACK"
        return "ERROR_UNEXPECTED_SERVICE", f"Expected WPM ACK (16), got {srv}"

    elif (pdu_type & 0xf0) == 0x50:
        resp_inv = resp[idx + 1]
        if resp_inv != expected_invoke_id:
            return "ERROR_INVOKE_ID_MISMATCH", f"Expected {expected_invoke_id}, got {resp_inv}"
        err_bytes = resp[idx + 3:]
        err_class, next_idx = read_enumerated(err_bytes, 0)
        err_code, _ = read_enumerated(err_bytes, next_idx)
        return "BACNET_ERROR_PDU", f"Class {err_class} ({BACNET_ERROR_CLASSES.get(err_class, 'UNKNOWN')}), Code {err_code} ({BACNET_ERROR_CODES.get(err_code, 'UNKNOWN')})"

    elif (pdu_type & 0xf0) == 0x60:
        reason = resp[idx + 2] if idx + 2 < len(resp) else 0
        return "BACNET_REJECT_PDU", f"Reason code: {reason}"

    return "ERROR_UNKNOWN_PDU_TYPE", f"APDU PDU Type: 0x{pdu_type:02X}"

def parse_dcc_response(resp, expected_invoke_id):
    """
    Parses DeviceCommunicationControl SimpleACK / Error response.
    Returns: (STATUS_STRING, PARSED_VALUE_OR_ERROR_DESC)
    """
    if len(resp) < 6 or resp[0] != 0x81:
        return "ERROR_INVALID_PACKET", "Not a BACnet/IP packet"

    control = resp[5]
    idx = 6
    if control & 0x80: return "ERROR_NETWORK_LAYER", "Network layer message"
    if control & 0x20:
        if idx + 2 > len(resp): return "ERROR_NPDU", "Truncated"
        idx += 2
        dadr_len = resp[idx]
        idx += 1 + dadr_len + 1
    if control & 0x08:
        if idx + 2 > len(resp): return "ERROR_NPDU", "Truncated"
        idx += 2
        sadr_len = resp[idx]
        idx += 1 + sadr_len

    if idx >= len(resp): return "ERROR_NO_APDU", "No APDU"

    pdu_type = resp[idx]

    if (pdu_type & 0xf0) == 0x20:
        resp_inv = resp[idx + 1]
        if resp_inv != expected_invoke_id:
            return "ERROR_INVOKE_ID_MISMATCH", f"Expected {expected_invoke_id}, got {resp_inv}"
        srv = resp[idx + 2]
        if srv == 17:
            return "SUCCESS", "ACK"
        return "ERROR_UNEXPECTED_SERVICE", f"Expected DCC ACK (17), got {srv}"

    elif (pdu_type & 0xf0) == 0x50:
        resp_inv = resp[idx + 1]
        err_bytes = resp[idx + 3:]
        err_class, next_idx = read_enumerated(err_bytes, 0)
        err_code, _ = read_enumerated(err_bytes, next_idx)
        return "BACNET_ERROR_PDU", f"Class {err_class} ({BACNET_ERROR_CLASSES.get(err_class, 'UNKNOWN')}), Code {err_code} ({BACNET_ERROR_CODES.get(err_code, 'UNKNOWN')})"

    return "ERROR_UNKNOWN_PDU_TYPE", f"APDU PDU Type: 0x{pdu_type:02X}"

def parse_i_am_response(resp):
    """
    Parses Unconfirmed I-Am response (Service Choice 0).
    Returns: ("SUCCESS", device/object identity plus Max APDU, segmentation and
    vendor fields) or ("ERROR_...", details).
    """
    if len(resp) < 6 or resp[0] != 0x81:
        return "ERROR_INVALID_PACKET", "Not a BACnet/IP packet"

    control = resp[5]
    idx = 6
    if control & 0x80: return "ERROR_NETWORK_LAYER", "Network layer message"
    if control & 0x20:
        if idx + 2 > len(resp): return "ERROR_NPDU", "Truncated NPDU"
        idx += 2
        dadr_len = resp[idx]
        idx += 1 + dadr_len + 1
    if control & 0x08:
        if idx + 2 > len(resp): return "ERROR_NPDU", "Truncated NPDU"
        idx += 2
        sadr_len = resp[idx]
        idx += 1 + sadr_len

    if idx >= len(resp): return "ERROR_NO_APDU", "No APDU present"

    pdu_type = resp[idx]
    if (pdu_type & 0xf0) != 0x10:
        return "ERROR_NOT_UNCONFIRMED_REQ", f"APDU PDU Type: 0x{pdu_type:02X}"
    
    if idx + 1 >= len(resp): return "ERROR_TRUNCATED", "Truncated APDU"
    service_choice = resp[idx + 1]
    if service_choice != 0:
        return "ERROR_UNEXPECTED_SERVICE", f"Expected I-Am (0), got {service_choice}"

    apdu_data = resp[idx + 2:]
    if len(apdu_data) < 12:
        return "ERROR_TRUNCATED_I_AM", "I-Am payload too short"

    if apdu_data[0] != 0xc4:
        return "ERROR_INVALID_TAG", f"Expected ObjectIdentifier tag 0xc4, got 0x{apdu_data[0]:02X}"

    dev_packed = struct.unpack('>I', apdu_data[1:5])[0]
    dev_type = (dev_packed >> 22) & 0x3ff
    dev_inst = dev_packed & 0x3fffff

    # I-Am continues with Max_APDU_Length_Accepted (Unsigned),
    # Segmentation_Supported (Enumerated), and Vendor_ID (Unsigned).  Keep
    # the parser tolerant of older fixtures while exposing the fields when
    # they are present for packet-level assertions.
    cursor = 5

    def read_application_value(expected_tag):
        nonlocal cursor
        if cursor >= len(apdu_data):
            return None
        tag = apdu_data[cursor]
        cursor += 1
        length = tag & 0x07
        if length == 5:
            if cursor >= len(apdu_data):
                return None
            length = apdu_data[cursor]
            cursor += 1
        if (tag & 0xF8) != expected_tag or cursor + length > len(apdu_data):
            return None
        value = 0
        for byte in apdu_data[cursor:cursor + length]:
            value = (value << 8) | byte
        cursor += length
        return value

    max_apdu = read_application_value(0x20)
    segmentation = read_application_value(0x90)
    vendor_id = read_application_value(0x20)

    return "SUCCESS", {
        "device_type": dev_type,
        "device_instance": dev_inst,
        "max_apdu_length": max_apdu,
        "segmentation": segmentation,
        "vendor_id": vendor_id,
    }

def parse_i_have_response(resp):
    """
    Parses Unconfirmed I-Have response (Service Choice 1).
    Returns: ("SUCCESS", {"device_instance": int, "object_type": int, "object_instance": int, "object_name": str}) or ("ERROR_...", details)
    """
    if len(resp) < 6 or resp[0] != 0x81:
        return "ERROR_INVALID_PACKET", "Not a BACnet/IP packet"

    control = resp[5]
    idx = 6
    if control & 0x80: return "ERROR_NETWORK_LAYER", "Network layer message"
    if control & 0x20:
        if idx + 2 > len(resp): return "ERROR_NPDU", "Truncated NPDU"
        idx += 2
        dadr_len = resp[idx]
        idx += 1 + dadr_len + 1
    if control & 0x08:
        if idx + 2 > len(resp): return "ERROR_NPDU", "Truncated NPDU"
        idx += 2
        sadr_len = resp[idx]
        idx += 1 + sadr_len

    if idx >= len(resp): return "ERROR_NO_APDU", "No APDU present"

    pdu_type = resp[idx]
    if (pdu_type & 0xf0) != 0x10:
        return "ERROR_NOT_UNCONFIRMED_REQ", f"APDU PDU Type: 0x{pdu_type:02X}"

    if idx + 1 >= len(resp): return "ERROR_TRUNCATED", "Truncated APDU"
    service_choice = resp[idx + 1]
    if service_choice != 1:
        return "ERROR_UNEXPECTED_SERVICE", f"Expected I-Have (1), got {service_choice}"

    apdu_data = resp[idx + 2:]
    if len(apdu_data) < 13:
        return "ERROR_TRUNCATED_I_HAVE", "I-Have payload too short"

    if apdu_data[0] != 0xc4:
        return "ERROR_INVALID_TAG", f"Expected DeviceIdentifier tag 0xc4, got 0x{apdu_data[0]:02X}"

    dev_packed = struct.unpack('>I', apdu_data[1:5])[0]
    dev_inst = dev_packed & 0x3fffff

    if apdu_data[5] != 0xc4:
        return "ERROR_INVALID_TAG", f"Expected ObjectIdentifier tag 0xc4, got 0x{apdu_data[5]:02X}"

    obj_packed = struct.unpack('>I', apdu_data[6:10])[0]
    obj_type = (obj_packed >> 22) & 0x3ff
    obj_inst = obj_packed & 0x3fffff

    obj_name = ""
    if len(apdu_data) > 10:
        tag = apdu_data[10]
        if (tag & 0xf0) == 0x70:
            length = tag & 0x0f
            cursor = 11
            if length == 5:
                if cursor >= len(apdu_data):
                    return "ERROR_TRUNCATED_I_HAVE", "Extended object name length is missing"
                length = apdu_data[cursor]
                cursor += 1
            if length < 1 or cursor + length > len(apdu_data):
                return "ERROR_TRUNCATED_I_HAVE", "Object name payload is truncated"
            # CharacterString content starts with the character-set byte.
            cursor += 1
            obj_name = apdu_data[cursor:cursor + length - 1].decode(
                'utf-8', errors='ignore')

    return "SUCCESS", {
        "device_instance": dev_inst,
        "object_type": obj_type,
        "object_instance": obj_inst,
        "object_name": obj_name
    }

def parse_bacnet_response(resp, expected_invoke_id):
    """
    Parses generic BACnet/IP response (ComplexACK, SimpleACK, Error, Reject, Abort).
    Returns: (STATUS_STRING, PARSED_VALUE_OR_ERROR_DESC)
    """
    if len(resp) < 6:
        return "ERROR_SHORT_PACKET", "Response length too short"

    if resp[0] != 0x81:
        return "ERROR_NOT_BACNET_IP", f"Unknown BVLC Type: 0x{resp[0]:02X}"

    bvlc_len = (resp[2] << 8) | resp[3]
    if len(resp) < bvlc_len:
        return "ERROR_TRUNCATED", f"Packet truncated: expected {bvlc_len}, got {len(resp)}"

    control = resp[5]
    idx = 6

    if control & 0x80:
        return "ERROR_NETWORK_LAYER_MSG", "Network layer message received"

    if control & 0x20:
        if idx + 2 > len(resp): return "ERROR_PARSE_NPDU", "Truncated NPDU"
        idx += 2 # Skip DNET
        dadr_len = resp[idx]
        idx += 1 + dadr_len
        idx += 1 # Skip Hop Count

    if control & 0x08:
        if idx + 2 > len(resp): return "ERROR_PARSE_NPDU", "Truncated NPDU"
        idx += 2 # Skip SNET
        sadr_len = resp[idx]
        idx += 1 + sadr_len

    if idx >= len(resp):
        return "ERROR_NO_APDU", "No APDU present in packet"

    pdu_type = resp[idx]

    # 1. Complex ACK (ReadProperty Response)
    if (pdu_type & 0xf0) == 0x30:
        if idx + 3 > len(resp): return "ERROR_PARSE_APDU", "Truncated APDU"
        resp_invoke_id = resp[idx + 1]
        if resp_invoke_id != expected_invoke_id:
            return "ERROR_INVOKE_ID_MISMATCH", f"Expected Invoke ID {expected_invoke_id}, got {resp_invoke_id}"
        
        service_choice = resp[idx + 2]
        if service_choice != 12:
            return "ERROR_UNEXPECTED_SERVICE", f"Unexpected service choice: {service_choice}"

        apdu_data = resp[idx + 3:]
        try:
            tag1_pos = -1
            for p in range(5, len(apdu_data) - 1):
                if (apdu_data[p] & 0xf8) == 0x18:
                    tag1_pos = p
                    break
            
            if tag1_pos != -1:
                tag_byte = apdu_data[tag1_pos]
                val_len = tag_byte & 0x07
                if val_len == 5:
                    prop_id_len = apdu_data[tag1_pos + 1]
                    start_search = tag1_pos + 2 + prop_id_len
                else:
                    start_search = tag1_pos + 1 + val_len
                open_idx = apdu_data.index(0x3e, start_search)
            else:
                open_idx = apdu_data.index(0x3e)

            # ReadProperty-ACK: PropertyValue is always the last context tag (Tag 3).
            # The closing tag 0x3F is guaranteed to be at the very end of apdu_data.
            if apdu_data and apdu_data[-1] == 0x3f:
                close_idx = len(apdu_data) - 1
            else:
                close_idx = apdu_data.rfind(0x3f)
                if close_idx == -1:
                    close_idx = len(apdu_data)
        except ValueError:
            return "ERROR_VALUE_TAGS_NOT_FOUND", "Opening tag 0x3E for PropertyValue not found"
        
        val_data = apdu_data[open_idx + 1 : close_idx]
        if not val_data:
            return "ERROR_EMPTY_VALUE", "No value data inside tag 3"

        val_tag = val_data[0]

        # Float32 (Real: Application Tag 4)
        if (val_tag & 0xf0) == 0x40 and len(val_data) >= 5:
            float_val = struct.unpack('>f', val_data[1:5])[0]
            return "SUCCESS", float_val

        # Float64 (Double: Application Tag 5)
        elif (val_tag & 0xf0) == 0x50 and len(val_data) >= 9:
            double_val = struct.unpack('>d', val_data[1:9])[0]
            return "SUCCESS", float(double_val)

        # Unsigned
        elif (val_tag & 0xf0) == 0x20:
            length = val_tag & 0x0f
            if 1 <= length <= len(val_data) - 1:
                val = 0
                for b in val_data[1 : 1 + length]:
                    val = (val << 8) | b
                return "SUCCESS", val
            return "SUCCESS", 0

        # Signed Integer (Application Tag 3 = 0x30)
        elif (val_tag & 0xf0) == 0x30:
            length = val_tag & 0x0f
            if 1 <= length <= len(val_data) - 1:
                raw_bytes = val_data[1 : 1 + length]
                val = int.from_bytes(raw_bytes, byteorder='big', signed=True)
                return "SUCCESS", val
            return "SUCCESS", 0

        # Null (Omitted Optional)
        elif val_tag == 0x00:
            return "SUCCESS", None

        # Enumerated
        elif (val_tag & 0xf0) == 0x90:
            length = val_tag & 0x07
            if 1 <= length <= len(val_data) - 1:
                val = 0
                for b in val_data[1 : 1 + length]:
                    val = (val << 8) | b
                return "SUCCESS", val
            return "ERROR_INVALID_ENUM_LEN", "Invalid enumerated length"

        # Boolean
        elif (val_tag & 0xfc) == 0x10:
            val = val_tag & 0x01
            return "SUCCESS", bool(val)

        # OctetString (Application Tag 6 = 0x60)
        elif (val_tag & 0xf0) == 0x60:
            length = val_tag & 0x07
            if length == 5 and len(val_data) >= 2:
                length = val_data[1]
                raw_bytes = val_data[2 : 2 + length]
            else:
                raw_bytes = val_data[1 : 1 + length]
            
            if len(raw_bytes) == 4:
                # IPv4 Address formatting
                return "SUCCESS", f"{raw_bytes[0]}.{raw_bytes[1]}.{raw_bytes[2]}.{raw_bytes[3]}"
            elif len(raw_bytes) == 6:
                # MAC Address formatting
                return "SUCCESS", ':'.join(f"{b:02X}" for b in raw_bytes)
            return "SUCCESS", raw_bytes.hex()

        # Host_NPort is encoded by BACstac as a constructed host-address
        # choice followed by a context-tagged port.  The IPv4 form returned
        # by Network Port FD_BBMD_Address is:
        #   0E 1C <4-byte IPv4> 0F 1A <2-byte UDP port>
        # (Some stacks use application tag 13/D0 for the same six-byte
        # payload, so that form is accepted below as well.)
        elif val_tag == 0x0E:
            if (len(val_data) >= 10 and val_data[1] == 0x1C and
                    val_data[6] == 0x0F and val_data[7] == 0x1A):
                ip_bytes = val_data[2:6]
                port = (val_data[8] << 8) | val_data[9]
                ip = ".".join(str(value) for value in ip_bytes)
                return "SUCCESS", f"{ip}:{port}"
            return "SUCCESS", val_data.hex()

        # Host_NPort application tag 13.  The BACnet/IP form is an IPv4
        # address followed by a two-octet UDP port.
        elif (val_tag & 0xf0) == 0xD0:
            length = val_tag & 0x07
            value_start = 1
            if length == 5 and len(val_data) >= 2:
                length = val_data[1]
                value_start = 2
            raw_bytes = val_data[value_start:value_start + length]
            if len(raw_bytes) == 6:
                ip = ".".join(str(value) for value in raw_bytes[:4])
                port = (raw_bytes[4] << 8) | raw_bytes[5]
                return "SUCCESS", f"{ip}:{port}"
            return "SUCCESS", raw_bytes.hex()

        # Character String
        elif (val_tag & 0xf0) == 0x70:
            length = val_tag & 0x0f
            if length == 5 and len(val_data) >= 2:
                length = val_data[1]
                str_bytes = val_data[3 : 2 + length]
            else:
                str_bytes = val_data[2 : 1 + length]
            try:
                return "SUCCESS", str_bytes.decode('utf-8', errors='ignore')
            except Exception:
                return "SUCCESS", str_bytes.decode('latin-1', errors='ignore')

        # BitString (Application Tag 8 = 0x80)
        elif (val_tag & 0xf0) == 0x80:
            length = val_tag & 0x07
            if length == 5 and len(val_data) >= 3:
                # Extended length: val_data[1] is length, val_data[2] is unused_bits
                total_len = val_data[1]
                unused_bits = val_data[2]
                bit_bytes = val_data[3 : 2 + total_len]
            elif length >= 2:
                unused_bits = val_data[1]
                bit_bytes = val_data[2 : 1 + length]
            else:
                unused_bits = 0
                bit_bytes = b""

            bits = []
            for b in bit_bytes:
                for bit_idx in range(7, -1, -1):
                    bits.append((b >> bit_idx) & 1)
            if unused_bits > 0 and len(bits) >= unused_bits:
                bits = bits[:-unused_bits]
            return "SUCCESS", bits

        # Object Identifier (BACnetObjectIdentifier: Application Tag 12 = 0xc4)
        elif val_tag == 0xc4 and len(val_data) >= 5:
            packed_oid = struct.unpack('>I', val_data[1:5])[0]
            obj_type = (packed_oid >> 22) & 0x3ff
            inst_num = packed_oid & 0x3fffff
            return "SUCCESS", (obj_type, inst_num)

        # Date (Application Tag 10 = 0xA4)
        elif (val_tag & 0xf0) == 0xA0 and len(val_data) >= 5:
            year = val_data[1] + 1900
            month = val_data[2]
            day = val_data[3]
            wday = val_data[4]
            return "SUCCESS", (year, month, day, wday)

        # Time (Application Tag 11 = 0xB4)
        elif (val_tag & 0xf0) == 0xB0 and len(val_data) >= 5:
            hour = val_data[1]
            minute = val_data[2]
            second = val_data[3]
            hundredth = val_data[4]
            return "SUCCESS", (hour, minute, second, hundredth)

        return "ERROR_UNSUPPORTED_DATA_TAG", f"App Tag: 0x{val_tag:02X}"

    # 2. Simple ACK (WriteProperty Response / SubscribeCOV / DCC)
    elif (pdu_type & 0xf0) == 0x20:
        if idx + 3 > len(resp): return "ERROR_PARSE_APDU", "Truncated APDU"
        resp_invoke_id = resp[idx + 1]
        if resp_invoke_id != expected_invoke_id:
            return "ERROR_INVOKE_ID_MISMATCH", f"Expected Invoke ID {expected_invoke_id}, got {resp_invoke_id}"
        
        service_choice = resp[idx + 2]
        if service_choice in (5, 15, 17, 28): # 5: SubscribeCOV, 15: WriteProperty, 17: DCC, 28: SubscribeCOVProperty
            return "SUCCESS", "ACK"
        return "ERROR_UNEXPECTED_SERVICE", f"Unexpected service choice: {service_choice}"

    # 3. Error PDU
    elif (pdu_type & 0xf0) == 0x50:
        if idx + 3 > len(resp): return "ERROR_PARSE_APDU", "Truncated APDU"
        resp_invoke_id = resp[idx + 1]
        if resp_invoke_id != expected_invoke_id:
            return "ERROR_INVOKE_ID_MISMATCH", f"Expected Invoke ID {expected_invoke_id}, got {resp_invoke_id}"
        
        error_bytes = resp[idx + 3:]
        err_class, next_idx = read_enumerated(error_bytes, 0)
        err_code, _ = read_enumerated(error_bytes, next_idx)

        err_class_name = BACNET_ERROR_CLASSES.get(err_class, "UNKNOWN")
        err_code_name = BACNET_ERROR_CODES.get(err_code, "UNKNOWN")

        return "BACNET_ERROR_PDU", f"Class {err_class} ({err_class_name}), Code {err_code} ({err_code_name})"

    # 4. Reject PDU
    elif (pdu_type & 0xf0) == 0x60:
        reason = resp[idx + 2] if idx + 2 < len(resp) else 0
        return "BACNET_REJECT_PDU", f"Reason code: {reason}"

    # 5. Abort PDU
    elif (pdu_type & 0xf0) == 0x70:
        reason = resp[idx + 2] if idx + 2 < len(resp) else 0
        return "BACNET_ABORT_PDU", f"Reason code: {reason}"

    return "ERROR_UNKNOWN_PDU_TYPE", f"APDU PDU Type: 0x{pdu_type:02X}"

def parse_active_cov_subscriptions(resp, expected_invoke_id):
    """
    Parses ReadProperty response for Active_COV_Subscriptions (Property ID 152).
    Returns: ("SUCCESS", list_of_subscription_dicts) or ("ERROR_...", error_details)
    """
    if len(resp) < 6:
        return "ERROR_SHORT_PACKET", "Response length too short"

    if resp[0] != 0x81:
        return "ERROR_NOT_BACNET_IP", f"Unknown BVLC Type: 0x{resp[0]:02X}"

    bvlc_len = (resp[2] << 8) | resp[3]
    if len(resp) < bvlc_len:
        return "ERROR_TRUNCATED", f"Packet truncated: expected {bvlc_len}, got {len(resp)}"

    control = resp[5]
    idx = 6
    if control & 0x80:
        return "ERROR_NETWORK_LAYER_MSG", "Network layer message received"
    if control & 0x20:
        if idx + 2 > len(resp): return "ERROR_PARSE_NPDU", "Truncated NPDU"
        idx += 2
        dadr_len = resp[idx]
        idx += 1 + dadr_len + 1
    if control & 0x08:
        if idx + 2 > len(resp): return "ERROR_PARSE_NPDU", "Truncated NPDU"
        idx += 2
        sadr_len = resp[idx]
        idx += 1 + sadr_len

    if idx >= len(resp):
        return "ERROR_NO_APDU", "No APDU present in packet"

    pdu_type = resp[idx]
    if (pdu_type & 0xf0) != 0x30:
        status, val = parse_bacnet_response(resp, expected_invoke_id)
        return status, val

    resp_invoke_id = resp[idx + 1]
    if resp_invoke_id != expected_invoke_id:
        return "ERROR_INVOKE_ID_MISMATCH", f"Expected Invoke ID {expected_invoke_id}, got {resp_invoke_id}"

    service_choice = resp[idx + 2]
    if service_choice != 12:
        return "ERROR_UNEXPECTED_SERVICE", f"Unexpected service choice: {service_choice}"

    apdu_data = resp[idx + 3:]
    try:
        tag1_pos = -1
        for p in range(5, len(apdu_data) - 1):
            if (apdu_data[p] & 0xf8) == 0x18:
                tag1_pos = p
                break
        
        if tag1_pos != -1:
            tag_byte = apdu_data[tag1_pos]
            val_len = tag_byte & 0x07
            start_search = tag1_pos + 1 + (val_len if val_len != 5 else 1 + apdu_data[tag1_pos + 1])
            open_idx = apdu_data.index(0x3e, start_search)
        else:
            open_idx = apdu_data.index(0x3e)

        # ReadProperty-ACK: PropertyValue is always the last context tag (Tag 3).
        # The closing tag 0x3F is guaranteed to be at the very end of apdu_data.
        if apdu_data and apdu_data[-1] == 0x3f:
            close_idx = len(apdu_data) - 1
        else:
            close_idx = apdu_data.rfind(0x3f)
            if close_idx == -1:
                close_idx = len(apdu_data)
    except ValueError:
        return "ERROR_VALUE_TAGS_NOT_FOUND", "Opening/closing tags for Active_COV_Subscriptions not found"

    val_data = apdu_data[open_idx + 1 : close_idx]
    if not val_data:
        return "SUCCESS", []

    subscriptions = []

    def _read_tlv(data, offset):
        """Return (tag, value_start, value_end) for one BACnet TLV."""
        if offset >= len(data):
            raise ValueError("truncated tag")
        tag = data[offset]
        length_code = tag & 0x07
        value_start = offset + 1
        if length_code == 5:
            if value_start >= len(data):
                raise ValueError("missing extended length")
            length = data[value_start]
            value_start += 1
        elif length_code == 6:
            if value_start + 1 >= len(data):
                raise ValueError("missing two-octet length")
            length = (data[value_start] << 8) | data[value_start + 1]
            value_start += 2
        elif length_code == 7:
            if value_start + 3 >= len(data):
                raise ValueError("missing four-octet length")
            length = 0
            for byte in data[value_start:value_start + 4]:
                length = (length << 8) | byte
            value_start += 4
        else:
            length = length_code
        value_end = value_start + length
        if value_end > len(data):
            raise ValueError("truncated value")
        return tag, value_start, value_end

    p = 0
    while p < len(val_data):
        start_p = p
        sub_info = {}
        try:
            # 1. RecipientProcess: Opening Tag 0x0E ... Closing Tag 0x0F
            if p < len(val_data) and val_data[p] == 0x0E:
                p += 1
                # RecipientProcess.recipient is itself [0].  The previous
                # parser skipped this wrapper, so an address byte could be
                # mistaken for processIdentifier data when the address/port
                # happened to contain a tag-looking octet.
                recipient_wrapper = False
                if p < len(val_data) and val_data[p] == 0x0E:
                    recipient_wrapper = True
                    p += 1

                # Recipient: device [0] or address [1]
                if p < len(val_data) and val_data[p] == 0x1E:
                    p += 1
                    net_tag, net_start, net_end = _read_tlv(val_data, p)
                    if (net_tag & 0xf8) != 0x20:
                        raise ValueError("invalid BACnet address network number")
                    net_num = 0
                    for byte in val_data[net_start:net_end]:
                        net_num = (net_num << 8) | byte
                    p = net_end

                    mac_tag, mac_start, mac_end = _read_tlv(val_data, p)
                    if (mac_tag & 0xf8) != 0x60:
                        raise ValueError("invalid BACnet address MAC")
                    mac_bytes = bytes(val_data[mac_start:mac_end])
                    p = mac_end
                    sub_info['net_num'] = net_num
                    sub_info['mac'] = mac_bytes.hex()
                    if p >= len(val_data) or val_data[p] != 0x1F:
                        raise ValueError("missing BACnet address closing tag")
                    p += 1
                elif p < len(val_data) and val_data[p] in (0x0C, 0xC4):
                    oid_tag, oid_start, oid_end = _read_tlv(val_data, p)
                    if oid_tag not in (0x0C, 0xC4) or oid_end - oid_start != 4:
                        raise ValueError("invalid BACnet recipient device identifier")
                    dev_oid = struct.unpack('>I', val_data[oid_start:oid_end])[0]
                    sub_info['device_id'] = dev_oid & 0x3fffff
                    p = oid_end

                if recipient_wrapper:
                    if p >= len(val_data) or val_data[p] != 0x0F:
                        raise ValueError("missing BACnet recipient closing tag")
                    p += 1
                
                # processIdentifier (Context Tag 1)
                process_tag, process_start, process_end = _read_tlv(val_data, p)
                if (process_tag & 0xf8) not in (0x10, 0x18):
                    raise ValueError("invalid BACnet process identifier")
                proc_id = 0
                for byte in val_data[process_start:process_end]:
                    proc_id = (proc_id << 8) | byte
                sub_info['process_id'] = proc_id
                p = process_end

                if p >= len(val_data) or val_data[p] != 0x0F:
                    raise ValueError("missing BACnet recipient process closing tag")
                p += 1

            # 2. MonitoredPropertyReference: Opening Tag 0x1E ... Closing Tag 0x1F
            if p < len(val_data) and val_data[p] == 0x1E:
                p += 1
                oid_tag, oid_start, oid_end = _read_tlv(val_data, p)
                if oid_tag not in (0x0C, 0xC4) or oid_end - oid_start != 4:
                    raise ValueError("invalid BACnet monitored object identifier")
                oid = struct.unpack('>I', val_data[oid_start:oid_end])[0]
                sub_info['obj_type'] = (oid >> 22) & 0x3ff
                sub_info['instance'] = oid & 0x3fffff
                p = oid_end

                property_tag, property_start, property_end = _read_tlv(val_data, p)
                if (property_tag & 0xf8) not in (0x10, 0x18):
                    raise ValueError("invalid BACnet monitored property identifier")
                if property_end > property_start:
                    prop_id = 0
                    for byte in val_data[property_start:property_end]:
                        prop_id = (prop_id << 8) | byte
                    sub_info['property_id'] = prop_id
                p = property_end

                if p < len(val_data) and (val_data[p] & 0xf8) in (0x20, 0x28):
                    _, array_start, array_end = _read_tlv(val_data, p)
                    arr_idx = 0
                    for byte in val_data[array_start:array_end]:
                        arr_idx = (arr_idx << 8) | byte
                    sub_info['array_index'] = arr_idx
                    p = array_end
                if p >= len(val_data) or val_data[p] != 0x1F:
                    raise ValueError("missing BACnet monitored property closing tag")
                p += 1

            # 3. IssueConfirmedNotifications (Context Tag 2)
            confirmed_tag, confirmed_start, confirmed_end = _read_tlv(val_data, p)
            if (confirmed_tag & 0xf8) not in (0x20, 0x28) or confirmed_end - confirmed_start != 1:
                raise ValueError("invalid BACnet confirmed-notification flag")
            sub_info['confirmed'] = bool(val_data[confirmed_start])
            p = confirmed_end

            # 4. TimeRemaining (Context Tag 3)
            time_tag, time_start, time_end = _read_tlv(val_data, p)
            if (time_tag & 0xf8) not in (0x30, 0x38):
                raise ValueError("invalid BACnet subscription time remaining")
            if time_end > time_start:
                rem = 0
                for byte in val_data[time_start:time_end]:
                    rem = (rem << 8) | byte
                sub_info['time_remaining'] = rem
            p = time_end

            # 5. COVIncrement (Context Tag 4)
            if p < len(val_data) and (val_data[p] & 0xf8) in (0x40, 0x48):
                cov_tag, cov_start, cov_end = _read_tlv(val_data, p)
                if cov_end - cov_start != 4:
                    raise ValueError("invalid BACnet COV increment")
                cov_inc = struct.unpack('>f', val_data[cov_start:cov_end])[0]
                sub_info['cov_increment'] = cov_inc
                p = cov_end

            if "obj_type" in sub_info or "instance" in sub_info or "property_id" in sub_info:
                subscriptions.append(sub_info)
            elif "process_id" in sub_info and subscriptions:
                # Merge if process_id was parsed prior to object reference
                subscriptions[-1]['process_id'] = sub_info['process_id']
        except Exception:
            break

        # 무한 루프 방지: p가 증가하지 않았다면 강제로 1바이트 전진
        if p <= start_p:
            p = start_p + 1

    return "SUCCESS", subscriptions

# ===========================================================================
# Socket & Synchronization Utilities
# ===========================================================================
def ack_and_drain_cov_notifications(sock, dnet=None, dadr=None, target_ip=None, target_port=None, duration=0.1):
    """
    Listens briefly for any immediate ConfirmedCOVNotification and sends SimpleACK,
    and drains any UnconfirmedCOVNotification to keep the socket state completely clear.
    """
    raw_sock = getattr(sock, 's', sock)
    orig_timeout = raw_sock.gettimeout()
    raw_sock.settimeout(0.05)
    start_time = time.time()
    try:
        while time.time() - start_time < duration:
            try:
                resp, _ = raw_sock.recvfrom(2048)
            except socket.timeout:
                continue
            except Exception:
                break

            if len(resp) < 6 or resp[0] != 0x81:
                continue

            control = resp[5]
            idx = 6
            if control & 0x80: continue
            if control & 0x20:
                if idx + 2 > len(resp): continue
                idx += 2
                dadr_len = resp[idx]
                idx += 1 + dadr_len + 1
            if control & 0x08:
                if idx + 2 > len(resp): continue
                idx += 2
                sadr_len = resp[idx]
                idx += 1 + sadr_len

            if idx >= len(resp): continue
            pdu_type = resp[idx]

            # ConfirmedCOVNotification -> reply SimpleACK
            if (pdu_type & 0xf0) == 0x00 and idx + 4 <= len(resp):
                inv = resp[idx + 2]
                srv = resp[idx + 3]
                if srv == 1 and target_ip and target_port:
                    ack_apdu = bytes([0x20, inv, 1])
                    raw_sock.sendto(build_bacnet_ip_packet(dnet, dadr, inv, ack_apdu), (target_ip, target_port))
    finally:
        raw_sock.settimeout(orig_timeout)

def recv_matching_response(sock, expected_invoke_id, dnet=None, dadr=None, target_ip=None, target_port=None, timeout=3.0):
    """
    Receives packets from socket until a response matching expected_invoke_id is found,
    automatically acknowledging ConfirmedCOVNotifications and discarding UnconfirmedCOVNotifications.
    """
    raw_sock = getattr(sock, 's', sock)
    orig_timeout = raw_sock.gettimeout()
    raw_sock.settimeout(0.3)
    start_time = time.time()
    last_retry_time = start_time
    try:
        while time.time() - start_time < timeout:
            try:
                resp, _ = raw_sock.recvfrom(2048)
            except socket.timeout:
                if time.time() - last_retry_time >= 1.0 and getattr(sock, 'last_sent_packet', None) and getattr(sock, 'last_sent_address', None):
                    raw_sock.sendto(sock.last_sent_packet, sock.last_sent_address)
                    last_retry_time = time.time()
                continue
            except Exception:
                continue

            if len(resp) < 6 or resp[0] != 0x81:
                continue

            control = resp[5]
            idx = 6
            if control & 0x80: continue
            if control & 0x20:
                if idx + 2 > len(resp): continue
                idx += 2
                dadr_len = resp[idx]
                idx += 1 + dadr_len + 1
            if control & 0x08:
                if idx + 2 > len(resp): continue
                idx += 2
                sadr_len = resp[idx]
                idx += 1 + sadr_len

            if idx >= len(resp): continue
            pdu_type = resp[idx]

            # ConfirmedCOVNotification -> reply SimpleACK
            if (pdu_type & 0xf0) == 0x00 and idx + 4 <= len(resp):
                inv = resp[idx + 2]
                srv = resp[idx + 3]
                if srv == 1 and target_ip and target_port:
                    ack_apdu = bytes([0x20, inv, 1])
                    raw_sock.sendto(build_bacnet_ip_packet(dnet, dadr, inv, ack_apdu), (target_ip, target_port))
                    continue

            # UnconfirmedCOVNotification -> discard & keep waiting
            if (pdu_type & 0xf0) == 0x10 and idx + 2 <= len(resp):
                srv = resp[idx + 1]
                if srv == 2:
                    continue

            # Response PDU
            if (pdu_type & 0xf0) in (0x20, 0x30, 0x50, 0x60, 0x70):
                if idx + 2 <= len(resp):
                    resp_inv = resp[idx + 1]
                    if resp_inv == expected_invoke_id:
                        return resp
        return b""
    finally:
        raw_sock.settimeout(orig_timeout)

def drain_socket(sock, duration=0.1):
    """
    Drains any lingering packets from the socket buffer to ensure a clean state.
    """
    raw_sock = getattr(sock, 's', sock)
    orig_timeout = raw_sock.gettimeout()
    raw_sock.settimeout(0.01)
    start_time = time.time()
    try:
        while time.time() - start_time < duration:
            try:
                raw_sock.recvfrom(2048)
            except socket.timeout:
                continue
            except Exception:
                break
    finally:
        raw_sock.settimeout(orig_timeout)

# ===========================================================================
# SocketWrapper Class
# ===========================================================================
class SocketWrapper:
    """
    Robust socket wrapper with automatic retry, delay injection,
    COV notification filtering/acknowledging, and invoke ID tracking.
    """
    def __init__(self, s, d, target_ip, target_port, apdu_timeout=1.0, apdu_retries=2,
                 unconfirmed_timeout=1.0):
        self.s = s
        self.d = d
        self.target_ip = target_ip
        self.target_port = target_port
        self.last_sent_packet = None
        self.last_sent_address = None
        self.expected_invoke_id = None
        # Confirmed 요청: 시도당 apdu_timeout 초 대기, 최대 apdu_retries 회 재전송
        # (BACnet 5.4.4.3, IUT 의 APDU_Timeout / Number_Of_APDU_Retries 로 설정 권장)
        # Unconfirmed 요청(Who-Has 등): 시도당 unconfirmed_timeout 초 대기
        self.apdu_timeout = apdu_timeout
        self.apdu_retries = apdu_retries
        self.unconfirmed_timeout = unconfirmed_timeout
        self.retry_count = 0       # 재전송한 패킷 수
        self.recovered_count = 0   # 재전송 후 응답을 받은 요청 수

    def _get_apdu_offset_and_type(self, data):
        if len(data) < 6 or data[0] != 0x81:
            return None, None
        control = data[5]
        idx = 6
        if control & 0x80:
            return None, None
        if control & 0x20:
            if idx + 2 > len(data): return None, None
            idx += 2 # DNET
            d_len = data[idx]
            idx += 1 + d_len + 1 # DADR len + DADR + Hop Count
        if control & 0x08:
            if idx + 2 > len(data): return None, None
            idx += 2 # SNET
            s_len = data[idx]
            idx += 1 + s_len # SADR len + SADR
        if idx >= len(data):
            return None, None
        return idx, data[idx]

    def sendto(self, data, address):
        self.last_sent_packet = data
        self.last_sent_address = address
        self.expected_invoke_id = None
        idx, pdu_type = self._get_apdu_offset_and_type(data)
        if idx is not None and (pdu_type & 0xf0) == 0x00 and idx + 3 <= len(data):
            self.expected_invoke_id = data[idx + 2]

        if self.d > 0.0:
            time.sleep(self.d)
        return self.s.sendto(data, address)

    def recvfrom(self, bufsize):
        confirmed = self.expected_invoke_id is not None
        per_attempt = self.apdu_timeout if confirmed else self.unconfirmed_timeout
        total_attempts = 1 + max(0, int(self.apdu_retries))
        orig_timeout = self.s.gettimeout()
        try:
            return self._recv_with_retry(bufsize, per_attempt, total_attempts)
        finally:
            self.s.settimeout(orig_timeout)

    def _recv_with_retry(self, bufsize, per_attempt, total_attempts):
        for attempt in range(total_attempts):
            try:
                start_time = time.time()
                while True:
                    remaining = per_attempt - (time.time() - start_time)
                    if remaining <= 0:
                        break
                    self.s.settimeout(remaining)
                    try:
                        resp, addr = self.s.recvfrom(bufsize)
                    except socket.timeout:
                        break

                    idx, pdu_type = self._get_apdu_offset_and_type(resp)
                    if idx is not None:
                        # ConfirmedCOVNotification (Confirmed-Request: 0x00, service: 1) -> reply SimpleACK
                        if (pdu_type & 0xf0) == 0x00 and idx + 4 <= len(resp):
                            inv = resp[idx + 2]
                            srv = resp[idx + 3]
                            if srv == 1:
                                ack_apdu = bytes([0x20, inv, 1])
                                self.s.sendto(build_bacnet_ip_packet(None, None, inv, ack_apdu), (self.target_ip, self.target_port))
                                continue

                        # UnconfirmedCOVNotification (Unconfirmed-Request: 0x10, service: 2) -> discard & keep waiting
                        if (pdu_type & 0xf0) == 0x10 and idx + 2 <= len(resp):
                            srv = resp[idx + 1]
                            if srv == 2:
                                continue

                        # Response PDU (SimpleACK 0x20, ComplexACK 0x30, Error 0x50, Reject 0x60, Abort 0x70)
                        if (pdu_type & 0xf0) in (0x20, 0x30, 0x50, 0x60, 0x70):
                            if idx + 2 <= len(resp) and self.expected_invoke_id is not None:
                                resp_inv = resp[idx + 1]
                                if resp_inv != self.expected_invoke_id:
                                    continue

                        self.expected_invoke_id = None
                        if attempt:
                            self.recovered_count += 1
                        return resp, addr
                    else:
                        self.expected_invoke_id = None
                        if attempt:
                            self.recovered_count += 1
                        return resp, addr

                raise socket.timeout()

            except socket.timeout:
                if attempt == total_attempts - 1:
                    raise
                if self.last_sent_packet and self.last_sent_address:
                    sys.stdout.write(f" (Retry {attempt+1})")
                    sys.stdout.flush()
                    if self.d > 0.0:
                        time.sleep(self.d)
                    self.retry_count += 1
                    self.s.sendto(self.last_sent_packet, self.last_sent_address)

    def __getattr__(self, name):
        return getattr(self.s, name)
