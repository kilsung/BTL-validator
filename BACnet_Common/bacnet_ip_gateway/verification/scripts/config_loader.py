#!/usr/bin/env python3
import os
import sys
import json
from bacnet_protocol import BACNET_OBJECT_TYPES

def parse_unit_id(uid):
    if isinstance(uid, str):
        if uid.lower().startswith("0x"):
            return int(uid, 16)
        return int(uid)
    return int(uid)

def infer_bacnet_type(obj):
    # 1. If explicit bacnet_type is specified
    b_type = obj.get("bacnet_type")
    if b_type:
        b_type_upper = b_type.upper()
        if b_type_upper in BACNET_OBJECT_TYPES:
            return BACNET_OBJECT_TYPES[b_type_upper]
        if b_type_upper == "AI": return 0
        if b_type_upper == "AO": return 1
        if b_type_upper == "BI": return 3
        if b_type_upper == "BO": return 4
        if b_type_upper in ("BV", "BINARY_VALUE"): return 5

    # 2. Infer from modbus_block and read_only
    block = obj.get("modbus_block", "").upper()
    read_only = obj.get("read_only", False)
    bit_pos = obj.get("bit_position", None)

    if block == "DISCRETE_INPUT":
        return 3 # BINARY_INPUT
    elif block == "INPUT_REGISTER":
        return 0 # ANALOG_INPUT
    elif block == "COIL":
        return 3 if read_only else 5 # BI / BV
    elif block == "HOLDING_REGISTER":
        if bit_pos is not None:
            return 3 if read_only else 5 # BI / BV
        else:
            return 0 if read_only else 1 # AI / AO
    return 0 # Default fallback to AI

def load_json_without_comments(file_path):
    with open(file_path, "r", encoding="utf-8") as f:
        content = f.read()
    
    lines = []
    for line in content.splitlines():
        stripped = line.strip()
        if stripped.startswith("//"):
            continue
        if "//" in line:
            in_string = False
            comment_start = -1
            char_idx = 0
            while char_idx < len(line) - 1:
                c = line[char_idx]
                if c == '"' and (char_idx == 0 or line[char_idx-1] != '\\'):
                    in_string = not in_string
                elif c == '/' and line[char_idx+1] == '/' and not in_string:
                    comment_start = char_idx
                    break
                char_idx += 1
            if comment_start != -1:
                line = line[:comment_start]
        lines.append(line)
    
    clean_content = "\n".join(lines)
    return json.loads(clean_content)

def load_gateway_configs(config_dir):
    """
    단일 장치 모드(Single-Device Mode) 기준 설정 로더.
    - 각 profiles/*.json 파일이 modbus_unit_id 를 가짐
    - 모든 객체 instance 는 전역 flat 번호로 할당됨
    """
    gateway_path = os.path.join(config_dir, "gateway.json")
    if not os.path.exists(gateway_path):
        print(f"Error: Common config file '{gateway_path}' not found.")
        sys.exit(1)

    try:
        g_data = load_json_without_comments(gateway_path)
    except Exception as e:
        print(f"Error parsing '{gateway_path}': {e}")
        sys.exit(1)

    gateway_dev_id = g_data.get("bacnet", {}).get("device_id", 1200)
    database_revision = g_data.get("bacnet", {}).get("database_revision", 1)
    model_name = g_data.get("bacnet", {}).get("model_name", "BACnet-Modbus Gateway")
    description = g_data.get("bacnet", {}).get("description", "Modbus RTU to BACnet IP Gateway")

    profiles_dir = os.path.join(config_dir, "profiles")
    devices = []

    if os.path.exists(profiles_dir):
        for fname in sorted(os.listdir(profiles_dir)):
            if fname.endswith(".json"):
                fpath = os.path.join(profiles_dir, fname)
                prefix = os.path.splitext(fname)[0]
                try:
                    p_data = load_json_without_comments(fpath)
                except Exception as e:
                    print(f"Error parsing profile '{fpath}': {e}")
                    sys.exit(1)

                if "modbus_unit_id" not in p_data:
                    print(f"[FATAL CONFIG ERROR] Profile '{fpath}' is missing required field 'modbus_unit_id'!")
                    sys.exit(1)
                try:
                    modbus_unit_id = parse_unit_id(p_data["modbus_unit_id"])
                except Exception:
                    print(f"[FATAL CONFIG ERROR] Profile '{fpath}' has unparseable modbus_unit_id!")
                    sys.exit(1)
                if not (1 <= modbus_unit_id <= 247):
                    print(f"[FATAL CONFIG ERROR] Profile '{fpath}' modbus_unit_id ({modbus_unit_id}) out of range [1..247]!")
                    sys.exit(1)

                objs = []
                for obj in p_data.get("objects", []):
                    obj_unit_id = modbus_unit_id
                    if "modbus_unit_id" in obj:
                        try:
                            obj_unit_id = parse_unit_id(obj["modbus_unit_id"])
                        except Exception:
                            print(f"[FATAL CONFIG ERROR] Object '{obj.get('name')}' has unparseable modbus_unit_id!")
                            sys.exit(1)
                        if not (1 <= obj_unit_id <= 247):
                            print(f"[FATAL CONFIG ERROR] Object '{obj.get('name')}' modbus_unit_id ({obj_unit_id}) out of range [1..247]!")
                            sys.exit(1)

                    b_type = infer_bacnet_type(obj)
                    read_only = obj.get("read_only", False)
                    is_writeable = not read_only and b_type in [1, 4, 5]

                    min_v = obj.get("min_value")
                    max_v = obj.get("max_value")
                    if min_v is not None and max_v is not None and float(min_v) > float(max_v):
                        print(f"[FATAL CONFIG ERROR] Object '{obj.get('name')}' has min_value ({min_v}) > max_value ({max_v})!")
                        sys.exit(1)
                    raw_rd = obj.get("relinquish_default")
                    cand_rd = float(raw_rd) if raw_rd is not None else None
                    if cand_rd is not None:
                        if min_v is not None and cand_rd < float(min_v):
                            cand_rd = float(min_v)
                        if max_v is not None and cand_rd > float(max_v):
                            cand_rd = float(max_v)
                        if b_type in (4, 5) and cand_rd not in (0.0, 1.0):
                            print(
                                f"[FATAL CONFIG ERROR] Binary Object '{obj.get('name')}' "
                                f"has invalid relinquish_default ({cand_rd}); only 0 or 1 is allowed!")
                            sys.exit(1)

                    scale_val = float(obj.get("scale", 1.0))
                    if scale_val <= 0.0:
                        scale_val = 1.0

                    objs.append({
                        "instance": obj.get("instance"),
                        "name": obj.get('name', 'Unknown'),
                        "type": b_type,
                        "is_writeable": is_writeable,
                        "modbus_unit_id": obj_unit_id,
                        "modbus_block": obj.get("modbus_block"),
                        "offset_address": obj.get("offset_address"),
                        "min_value": min_v,
                        "max_value": max_v,
                        "scale": scale_val,
                        "offset": obj.get("offset", 0.0),
                        "relinquish_default": cand_rd
                    })

                devices.append({
                    "profile_name": prefix,
                    "modbus_unit_id": modbus_unit_id,
                    "objects": objs
                })

    # 인스턴스 번호 자동 할당 로직 적용
    global_used = set()
    for dev in devices:
        for obj in dev['objects']:
            inst = obj.get("instance")
            if inst is not None and inst != 0:
                if inst in global_used:
                    print(f"[Instance] Error: Duplicate instance number {inst} in configuration!")
                    sys.exit(1)
                global_used.add(inst)

    next_candidate = 1
    for dev in devices:
        for obj in dev['objects']:
            inst = obj.get("instance")
            if inst is None or inst == 0:
                while next_candidate in global_used:
                    next_candidate += 1
                obj["instance"] = next_candidate
                global_used.add(next_candidate)
                next_candidate += 1

    return {
        "gateway_device_id": gateway_dev_id,
        "database_revision": database_revision,
        "model_name": model_name,
        "description": description,
        "devices": devices
    }
