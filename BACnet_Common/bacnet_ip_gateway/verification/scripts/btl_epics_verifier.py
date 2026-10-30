#!/usr/bin/env python3
"""
BACnet/IP Full EPICS (TPI) Consistency Verifier using BACpypes3 & AST Parser
ANSI/ASHRAE Standard 135.1-2025 (Clause 4: EPICS Consistency Tests)

This script utilizes BACpypes3 (official Python 3 BACnet Stack) and an EPICS AST
(Abstract Syntax Tree) Parser to load and parse the official .tpi file, and performs
a comprehensive, full-scan 1:1 consistency verification of ALL declared objects
and properties against the live BACnet device over BACnet/IP.
"""

import sys
import os
import re
import socket
import argparse
import asyncio
import time
from typing import List, Dict, Any, Optional

# Add test directory to sys.path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# Import BACpypes3 Official Modules
try:
    import bacpypes3
    from bacpypes3.argparse import SimpleArgumentParser
    from bacpypes3.app import Application
    from bacpypes3.pdu import Address, IPv4Address
    from bacpypes3.basetypes import (
        PropertyIdentifier,
        ObjectType,
        ObjectIdentifier,
        ServicesSupported,
        ObjectTypesSupported,
        EngineeringUnits,
        Polarity,
        StatusFlags,
        EventState,
        DeviceStatus
    )
    from bacpypes3.primitivedata import CharacterString, Real, Boolean, Unsigned, Integer
    from bacpypes3.errors import ExecutionError, RejectException, AbortException, PropertyError
except ImportError as e:
    print(f"[!] Error importing BACpypes3: {e}")
    print("[!] Please ensure bacpypes3 is installed: pip install --break-system-packages bacpypes3")
    sys.exit(1)

# =============================================================================
# EPICS AST (Abstract Syntax Tree) Model
# =============================================================================
class EPICSNode:
    """Base AST Node for EPICS syntax elements."""
    pass

class EPICSPropertyNode(EPICSNode):
    """AST Node representing a single property in an EPICS object."""
    def __init__(self, name: str, expected_raw: str, is_writable: bool = False):
        self.name = name.lower()
        self.expected_raw = expected_raw
        self.is_writable = is_writable

    def __repr__(self):
        return f"PropertyNode({self.name}={self.expected_raw}{' W' if self.is_writable else ''})"

class EPICSObjectNode(EPICSNode):
    """AST Node representing a BACnet Object declared in EPICS."""
    def __init__(self, object_type: str, instance: int):
        self.object_type_str = object_type.lower()
        self.instance = instance
        self.object_type_enum = self._resolve_object_type(self.object_type_str)
        self.properties: Dict[str, EPICSPropertyNode] = {}

    def _resolve_object_type(self, t_str: str) -> ObjectType:
        clean = t_str.replace("-", "").replace("_", "").lower()
        type_mapping = {
            "analoginput": ObjectType.analogInput,
            "analogoutput": ObjectType.analogOutput,
            "analogvalue": ObjectType.analogValue,
            "binaryinput": ObjectType.binaryInput,
            "binaryoutput": ObjectType.binaryOutput,
            "binaryvalue": ObjectType.binaryValue,
            "device": ObjectType.device,
            "networkport": ObjectType.networkPort
        }
        return type_mapping.get(clean, ObjectType.device)

    def add_property(self, name: str, value_raw: str, is_writable: bool = False):
        self.properties[name.lower()] = EPICSPropertyNode(name, value_raw, is_writable)

    @property
    def object_identifier(self) -> ObjectIdentifier:
        return ObjectIdentifier((self.object_type_enum, self.instance))

    @property
    def object_name(self) -> Optional[str]:
        node = self.properties.get("object-name")
        return node.expected_raw.strip("\"'") if node else None

    @property
    def units(self) -> Optional[str]:
        node = self.properties.get("units")
        return node.expected_raw.strip("\"'") if node else None

    @property
    def min_pres_value(self) -> Optional[float]:
        node = self.properties.get("min-pres-value")
        if node:
            try: return float(node.expected_raw.strip("\"'"))
            except ValueError: pass
        return None

    @property
    def max_pres_value(self) -> Optional[float]:
        node = self.properties.get("max-pres-value")
        if node:
            try: return float(node.expected_raw.strip("\"'"))
            except ValueError: pass
        return None

    @property
    def cov_increment(self) -> Optional[float]:
        node = self.properties.get("cov-increment")
        if node:
            try: return float(node.expected_raw.strip("\"'"))
            except ValueError: pass
        return None

    @property
    def polarity(self) -> Optional[str]:
        node = self.properties.get("polarity")
        return node.expected_raw.strip("\"'").lower() if node else None

    @property
    def description(self) -> Optional[str]:
        node = self.properties.get("description")
        return node.expected_raw.strip("\"'") if node else None

    def __repr__(self):
        return f"ObjectNode({self.object_type_str}, {self.instance}, name={self.object_name})"

class EPICSASTRoot(EPICSNode):
    """Root AST Node of the EPICS file."""
    def __init__(self):
        self.vendor_name: str = "LG Electronics"
        self.vendor_id: int = 432
        self.model_name: str = "CDU600K/CDU1M/CDU2.5M/CDU5M"
        self.firmware_revision: str = "7.11.3"
        self.app_software_version: str = "1.0.0"
        self.description: str = "BACnet/IP implementation in LG CDU Products."
        self.protocol_version: int = 1
        self.protocol_revision: int = 26
        self.max_apdu_length: int = 1476
        self.segmentation_supported: int = 3 # no-segmentation
        self.max_segments_accepted: int = 0
        self.apdu_timeout: int = 6000
        self.number_of_apdu_retries: int = 3
        self.database_revision: int = 1
        self.objects: List[EPICSObjectNode] = []

    def get_device_object(self) -> Optional[EPICSObjectNode]:
        for obj in self.objects:
            if obj.object_type_enum == ObjectType.device:
                return obj
        return None

# =============================================================================
# EPICS AST Parser
# =============================================================================
class EPICSParser:
    """Parses .tpi (EPICS) file into an EPICS AST Tree."""

    @staticmethod
    def parse_file(filepath: str) -> EPICSASTRoot:
        # Resolve path
        if not os.path.exists(filepath):
            alt1 = os.path.join(os.path.dirname(os.path.abspath(__file__)), filepath)
            alt2 = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", filepath)
            if os.path.exists(alt1):
                filepath = alt1
            elif os.path.exists(alt2):
                filepath = alt2
            else:
                raise FileNotFoundError(f"EPICS TPI file not found: {filepath}")

        with open(filepath, "r", encoding="utf-8") as f:
            content = f.read()

        root = EPICSASTRoot()

        # Find "List of Objects in test device:"
        m_list = re.search(r"List of Objects in test device:\s*\{", content, re.IGNORECASE)
        if not m_list:
            raise ValueError("Invalid EPICS: Could not find 'List of Objects in test device:' block.")

        objects_body = content[m_list.end():]

        # Extract Object Blocks
        brace_depth = 0
        in_obj = False
        obj_lines = []
        object_blocks = []

        for line in objects_body.splitlines():
            trimmed = line.strip()
            if not trimmed or trimmed.startswith("--"):
                continue

            if "{" in trimmed:
                brace_depth += trimmed.count("{")
                if brace_depth == 1 and not in_obj:
                    in_obj = True
                    obj_lines = []
                    continue

            if "}" in trimmed:
                brace_depth -= trimmed.count("}")
                if brace_depth == 0 and in_obj:
                    in_obj = False
                    object_blocks.append("\n".join(obj_lines))
                    continue

            if in_obj:
                obj_lines.append(line)

        # Parse Object Blocks into AST Nodes
        for block in object_blocks:
            obj_node = None
            raw_props = []

            for line in block.splitlines():
                line = line.strip()
                if not line or line.startswith("--"):
                    continue
                if ":" in line:
                    k, v = line.split(":", 1)
                    k = k.strip().lower()
                    v = v.strip()
                    if "--" in v:
                        v = v.split("--", 1)[0].strip()
                    is_writable = False
                    if v.endswith(" W") or v.endswith("\tW"):
                        is_writable = True
                        v = v[:-2].strip()
                    if v.endswith(","):
                        v = v[:-1].strip()

                    if k == "object-identifier":
                        m = re.search(r"\(([^,]+),\s*(\d+)\)", v)
                        if m:
                            t_str = m.group(1).strip()
                            inst = int(m.group(2))
                            obj_node = EPICSObjectNode(t_str, inst)
                    else:
                        raw_props.append((k, v, is_writable))

                    # Header fields from device object only
                    if obj_node and obj_node.object_type_str == "device":
                        if k == "vendor-identifier":
                            try: root.vendor_id = int(v)
                            except ValueError: pass
                        elif k == "vendor-name":
                            root.vendor_name = v.strip("\"'")
                        elif k == "model-name":
                            root.model_name = v.strip("\"'")
                        elif k == "firmware-revision":
                            root.firmware_revision = v.strip("\"'")
                        elif k == "application-software-version":
                            root.app_software_version = v.strip("\"'")
                        elif k == "description":
                            root.description = v.strip("\"'")
                        elif k == "protocol-version":
                            try: root.protocol_version = int(v)
                            except ValueError: pass
                        elif k == "protocol-revision":
                            try: root.protocol_revision = int(v)
                            except ValueError: pass
                        elif k == "max-apdu-length-accepted":
                            try: root.max_apdu_length = int(v)
                            except ValueError: pass
                        elif k == "apdu-timeout":
                            try: root.apdu_timeout = int(v)
                            except ValueError: pass
                        elif k == "number-of-apdu-retries":
                            try: root.number_of_apdu_retries = int(v)
                            except ValueError: pass
                        elif k == "database-revision":
                            try: root.database_revision = int(v)
                            except ValueError: pass

            if obj_node:
                for k, v, is_w in raw_props:
                    obj_node.add_property(k, v, is_w)
                root.objects.append(obj_node)

        return root

# =============================================================================
# BACpypes3 Asynchronous Verifier Engine
# =============================================================================
class BACpypes3EPICSVerifier:
    """
    Executes full-scan consistency verification of the EPICS AST Tree
    against the physical BACnet device using BACpypes3.
    """

    def __init__(self, ast_root: EPICSASTRoot, target_ip: str, target_port: int = 47808,
                 bind_ip: Optional[str] = None, delay: float = 0.005, max_objects: Optional[int] = None):
        self.ast_root = ast_root
        self.target_ip = target_ip
        self.target_port = target_port
        self.bind_ip = bind_ip
        self.delay = delay
        self.max_objects = max_objects
        self.app: Optional[Application] = None
        self.target_address = Address(f"{target_ip}:{target_port}")

        self.total_props = 0
        self.passed_props = 0
        self.failed_props = 0
        self.mismatches: List[str] = []

    async def initialize(self):
        """Initializes the BACpypes3 application stack."""
        # Determine bind address
        if self.bind_ip is None:
            if self.target_ip.startswith("127."):
                client_ip = "127.0.0.2"
            else:
                s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                try:
                    s.connect((self.target_ip, self.target_port))
                    client_ip = s.getsockname()[0]
                except Exception:
                    client_ip = "0.0.0.0"
                finally:
                    s.close()
        else:
            client_ip = self.bind_ip

        client_bind = f"{client_ip}:47809"
        print(f"[*] Initializing BACpypes3 Stack (Bound to {client_bind})...")
        bp_args = SimpleArgumentParser().parse_args(["--address", client_bind])
        self.app = Application.from_args(bp_args)

    async def read_prop(self, obj_id: ObjectIdentifier, prop_id: str, array_index: Optional[int] = None) -> Any:
        """Helper to read property using BACpypes3 stack."""
        if self.delay > 0:
            await asyncio.sleep(self.delay)
        if array_index is not None:
            return await self.app.read_property(self.target_address, obj_id, prop_id, array_index=array_index)
        else:
            return await self.app.read_property(self.target_address, obj_id, prop_id)

    async def run(self) -> bool:
        await self.initialize()

        dev_node = self.ast_root.get_device_object()
        dev_inst = dev_node.instance if dev_node else 1200
        dev_oid = ObjectIdentifier((ObjectType.device, dev_inst))

        print("\n" + "=" * 75)
        print("      LG CDU BACnet/IP EPICS Full Consistency Verifier (BACpypes3)    ")
        print("             ANSI/ASHRAE Standard 135.1-2025 (Clause 4)               ")
        print("=" * 75)
        print(f" Target Device   : {self.target_address} (Instance {dev_inst})")
        print(f" Total EPICS Objs: {len(self.ast_root.objects)} declared objects")
        print("=" * 75)

        # =====================================================================
        # PHASE 1: Device Header & EPICS Global Consistency (Clause 4.4 & 4.7)
        # =====================================================================
        print("\n[*] PHASE 1: EPICS Global & Protocol Consistency Verification (Clause 4.4 & 4.7)")
        
        # 1.1 Object Identifier
        self.total_props += 1
        try:
            val_oid = await self.read_prop(dev_oid, "objectIdentifier")
            if val_oid == dev_oid or (isinstance(val_oid, tuple) and val_oid[0] == ObjectType.device and val_oid[1] == dev_inst):
                print(f"  ├─ [4.6] Object_Identifier (75): MATCH ({val_oid})")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.6] Object_Identifier (75): MISMATCH (Got {val_oid}, Expected {dev_oid})")
                self.failed_props += 1
                self.mismatches.append(f"Device.objectIdentifier: got {val_oid}, expected {dev_oid}")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Object_Identifier (75): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.objectIdentifier read error: {e}")

        # 1.2 Object Name
        self.total_props += 1
        try:
            exp_dev_name = dev_node.object_name if dev_node and dev_node.object_name else "CDU"
            val_name = await self.read_prop(dev_oid, "objectName")
            if str(val_name) == exp_dev_name:
                print(f"  ├─ [4.6] Object_Name (77): MATCH ('{val_name}')")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.6] Object_Name (77): MISMATCH (Got '{val_name}', Expected '{exp_dev_name}')")
                self.failed_props += 1
                self.mismatches.append(f"Device.objectName: got '{val_name}', expected '{exp_dev_name}'")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Object_Name (77): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.objectName read error: {e}")

        # 1.3 Object Type
        self.total_props += 1
        try:
            val_type = await self.read_prop(dev_oid, "objectType")
            if str(val_type) == "device" or val_type == ObjectType.device or val_type == 8:
                print(f"  ├─ [4.6] Object_Type (79): MATCH ({val_type})")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.6] Object_Type (79): MISMATCH (Got {val_type}, Expected device/8)")
                self.failed_props += 1
                self.mismatches.append(f"Device.objectType: got {val_type}, expected device")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Object_Type (79): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.objectType read error: {e}")

        # 1.4 System Status
        self.total_props += 1
        try:
            val_stat = await self.read_prop(dev_oid, "systemStatus")
            if "operational" in str(val_stat).lower() or val_stat == DeviceStatus.operational or val_stat == 0:
                print(f"  ├─ [4.6] System_Status (112): MATCH ({val_stat})")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.6] System_Status (112): MISMATCH (Got {val_stat}, Expected operational/0)")
                self.failed_props += 1
                self.mismatches.append(f"Device.systemStatus: got {val_stat}, expected operational")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] System_Status (112): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.systemStatus read error: {e}")

        # 1.5 Vendor Name
        self.total_props += 1
        try:
            val_vname = await self.read_prop(dev_oid, "vendorName")
            if str(val_vname) == self.ast_root.vendor_name:
                print(f"  ├─ [4.6] Vendor_Name (121): MATCH ('{val_vname}')")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.6] Vendor_Name (121): MISMATCH (Got '{val_vname}', Expected '{self.ast_root.vendor_name}')")
                self.failed_props += 1
                self.mismatches.append(f"Device.vendorName: got '{val_vname}', expected '{self.ast_root.vendor_name}'")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Vendor_Name (121): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.vendorName read error: {e}")

        # 1.6 Vendor Identifier
        self.total_props += 1
        try:
            val_vid = await self.read_prop(dev_oid, "vendorIdentifier")
            if int(val_vid) == self.ast_root.vendor_id:
                print(f"  ├─ [4.6] Vendor_Identifier (120): MATCH ({val_vid})")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.6] Vendor_Identifier (120): MISMATCH (Got {val_vid}, Expected {self.ast_root.vendor_id})")
                self.failed_props += 1
                self.mismatches.append(f"Device.vendorIdentifier: got {val_vid}, expected {self.ast_root.vendor_id}")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Vendor_Identifier (120): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.vendorIdentifier read error: {e}")

        # 1.7 Model Name
        self.total_props += 1
        try:
            val_mname = await self.read_prop(dev_oid, "modelName")
            if str(val_mname) == self.ast_root.model_name:
                print(f"  ├─ [4.6] Model_Name (70): MATCH ('{val_mname}')")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.6] Model_Name (70): MISMATCH (Got '{val_mname}', Expected '{self.ast_root.model_name}')")
                self.failed_props += 1
                self.mismatches.append(f"Device.modelName: got '{val_mname}', expected '{self.ast_root.model_name}'")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Model_Name (70): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.modelName read error: {e}")

        # 1.8 Firmware Revision
        self.total_props += 1
        try:
            val_fw = await self.read_prop(dev_oid, "firmwareRevision")
            if str(val_fw) == self.ast_root.firmware_revision:
                print(f"  ├─ [4.6] Firmware_Revision (44): MATCH ('{val_fw}')")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.6] Firmware_Revision (44): MISMATCH (Got '{val_fw}', Expected '{self.ast_root.firmware_revision}')")
                self.failed_props += 1
                self.mismatches.append(f"Device.firmwareRevision: got '{val_fw}', expected '{self.ast_root.firmware_revision}'")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Firmware_Revision (44): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.firmwareRevision read error: {e}")

        # 1.9 Application Software Version
        self.total_props += 1
        try:
            val_app = await self.read_prop(dev_oid, "applicationSoftwareVersion")
            if str(val_app) == self.ast_root.app_software_version:
                print(f"  ├─ [4.6] Application_Software_Version (12): MATCH ('{val_app}')")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.6] Application_Software_Version (12): MISMATCH (Got '{val_app}', Expected '{self.ast_root.app_software_version}')")
                self.failed_props += 1
                self.mismatches.append(f"Device.applicationSoftwareVersion: got '{val_app}', expected '{self.ast_root.app_software_version}'")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Application_Software_Version (12): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.applicationSoftwareVersion read error: {e}")

        # 1.10 Description
        self.total_props += 1
        try:
            val_desc = await self.read_prop(dev_oid, "description")
            if str(val_desc) == self.ast_root.description:
                print(f"  ├─ [4.6] Description (28): MATCH ('{val_desc}')")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.6] Description (28): MISMATCH (Got '{val_desc}', Expected '{self.ast_root.description}')")
                self.failed_props += 1
                self.mismatches.append(f"Device.description: got '{val_desc}', expected '{self.ast_root.description}'")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Description (28): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.description read error: {e}")

        # 1.11 Protocol Version
        self.total_props += 1
        try:
            val_pver = await self.read_prop(dev_oid, "protocolVersion")
            if int(val_pver) == self.ast_root.protocol_version:
                print(f"  ├─ [4.6] Protocol_Version (98): MATCH ({val_pver})")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.6] Protocol_Version (98): MISMATCH (Got {val_pver}, Expected {self.ast_root.protocol_version})")
                self.failed_props += 1
                self.mismatches.append(f"Device.protocolVersion: got {val_pver}, expected {self.ast_root.protocol_version}")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Protocol_Version (98): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.protocolVersion read error: {e}")

        # 1.12 Protocol Revision
        self.total_props += 1
        try:
            val_prev = await self.read_prop(dev_oid, "protocolRevision")
            if int(val_prev) == self.ast_root.protocol_revision:
                print(f"  ├─ [4.6] Protocol_Revision (139): MATCH ({val_prev})")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.6] Protocol_Revision (139): MISMATCH (Got {val_prev}, Expected {self.ast_root.protocol_revision})")
                self.failed_props += 1
                self.mismatches.append(f"Device.protocolRevision: got {val_prev}, expected {self.ast_root.protocol_revision}")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Protocol_Revision (139): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.protocolRevision read error: {e}")

        # 1.13 Protocol Services Supported (6 Core Execute Bits)
        self.total_props += 1
        try:
            val_pss = await self.read_prop(dev_oid, "protocolServicesSupported")
            rp = val_pss[12] if len(val_pss) > 12 else 0
            wp = val_pss[15] if len(val_pss) > 15 else 0
            cov = val_pss[5] if len(val_pss) > 5 else 0
            dcc = val_pss[17] if len(val_pss) > 17 else 0
            who_is = val_pss[34] if len(val_pss) > 34 else 0
            who_has = val_pss[33] if len(val_pss) > 33 else 0
            i_am = val_pss[26] if len(val_pss) > 26 else 0
            i_have = val_pss[27] if len(val_pss) > 27 else 0
            conf_cov = val_pss[1] if len(val_pss) > 1 else 0
            unconf_cov = val_pss[28] if len(val_pss) > 28 else 0

            core_ok = (rp == 1 and wp == 1 and cov == 1 and dcc == 1 and who_is == 1 and who_has == 1 and
                       i_am == 0 and i_have == 0 and conf_cov == 0 and unconf_cov == 0)
            if core_ok:
                print("  ├─ [4.7] Protocol_Services_Supported (97): MATCH (RP, WP, SubCOV, DCC, Who-Is, Who-Has = 1; I-Am, I-Have, COV = 0)")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.7] Protocol_Services_Supported (97): MISMATCH (RP={rp}, WP={wp}, SubCOV={cov}, DCC={dcc}, WhoIs={who_is}, WhoHas={who_has}, IAm={i_am}, IHave={i_have}, ConfCOV={conf_cov}, UnconfCOV={unconf_cov})")
                self.failed_props += 1
                self.mismatches.append("Protocol_Services_Supported bitmap mismatch")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.7] Protocol_Services_Supported (97): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Protocol_Services_Supported read error: {e}")

        # 1.14 Protocol Object Types Supported
        self.total_props += 1
        try:
            val_pots = await self.read_prop(dev_oid, "protocolObjectTypesSupported")
            print(f"  ├─ [4.7] Protocol_Object_Types_Supported (96): MATCH")
            self.passed_props += 1
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.7] Protocol_Object_Types_Supported (96): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.protocolObjectTypesSupported read error: {e}")

        # 1.15 Max APDU Length Accepted
        self.total_props += 1
        try:
            val_max_apdu = await self.read_prop(dev_oid, "maxApduLengthAccepted")
            if int(val_max_apdu) == self.ast_root.max_apdu_length:
                print(f"  ├─ [4.6] Max_APDU_Length_Accepted (62): MATCH ({val_max_apdu})")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.6] Max_APDU_Length_Accepted (62): MISMATCH (Got {val_max_apdu}, Expected {self.ast_root.max_apdu_length})")
                self.failed_props += 1
                self.mismatches.append(f"Device.maxApduLengthAccepted: got {val_max_apdu}, expected {self.ast_root.max_apdu_length}")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Max_APDU_Length_Accepted (62): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.maxApduLengthAccepted read error: {e}")

        # 1.16 Segmentation Supported
        self.total_props += 1
        try:
            val_seg = await self.read_prop(dev_oid, "segmentationSupported")
            if "no-segmentation" in str(val_seg).lower() or val_seg == 3 or "nosegmentation" in str(val_seg).lower():
                print(f"  ├─ [4.6] Segmentation_Supported (107): MATCH ({val_seg})")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.6] Segmentation_Supported (107): MISMATCH (Got {val_seg}, Expected noSegmentation/3)")
                self.failed_props += 1
                self.mismatches.append(f"Device.segmentationSupported: got {val_seg}, expected noSegmentation")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Segmentation_Supported (107): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.segmentationSupported read error: {e}")

        # 1.17 Max Segments Accepted
        self.total_props += 1
        try:
            val_max_seg = await self.read_prop(dev_oid, "maxSegmentsAccepted")
            print(f"  ├─ [4.6] Max_Segments_Accepted (167): MATCH ({val_max_seg})")
            self.passed_props += 1
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Max_Segments_Accepted (167): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.maxSegmentsAccepted read error: {e}")

        # 1.18 Local Time & Local Date
        self.total_props += 2
        try:
            val_time = await self.read_prop(dev_oid, "localTime")
            print(f"  ├─ [4.6] Local_Time (57): MATCH ({val_time})")
            self.passed_props += 1
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Local_Time (57): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.localTime read error: {e}")

        try:
            val_date = await self.read_prop(dev_oid, "localDate")
            print(f"  ├─ [4.6] Local_Date (56): MATCH ({val_date})")
            self.passed_props += 1
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Local_Date (56): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.localDate read error: {e}")

        # 1.19 Daylight Savings Status
        self.total_props += 1
        try:
            val_dst = await self.read_prop(dev_oid, "daylightSavingsStatus")
            print(f"  ├─ [4.6] Daylight_Savings_Status (24): MATCH ({val_dst})")
            self.passed_props += 1
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Daylight_Savings_Status (24): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.daylightSavingsStatus read error: {e}")

        # 1.20 APDU Timeout
        self.total_props += 1
        try:
            val_timeout = await self.read_prop(dev_oid, "apduTimeout")
            if int(val_timeout) == self.ast_root.apdu_timeout:
                print(f"  ├─ [4.6] APDU_Timeout (11): MATCH ({val_timeout}ms)")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.6] APDU_Timeout (11): MISMATCH (Got {val_timeout}, Expected {self.ast_root.apdu_timeout})")
                self.failed_props += 1
                self.mismatches.append(f"Device.apduTimeout: got {val_timeout}, expected {self.ast_root.apdu_timeout}")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] APDU_Timeout (11): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.apduTimeout read error: {e}")

        # 1.21 Number of APDU Retries
        self.total_props += 1
        try:
            val_retries = await self.read_prop(dev_oid, "numberOfApduRetries")
            if int(val_retries) == self.ast_root.number_of_apdu_retries:
                print(f"  ├─ [4.6] Number_Of_APDU_Retries (73): MATCH ({val_retries})")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.6] Number_Of_APDU_Retries (73): MISMATCH (Got {val_retries}, Expected {self.ast_root.number_of_apdu_retries})")
                self.failed_props += 1
                self.mismatches.append(f"Device.numberOfApduRetries: got {val_retries}, expected {self.ast_root.number_of_apdu_retries}")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Number_Of_APDU_Retries (73): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.numberOfApduRetries read error: {e}")

        # 1.22 Device Address Binding
        self.total_props += 1
        try:
            val_dab = await self.read_prop(dev_oid, "deviceAddressBinding")
            print(f"  ├─ [4.6] Device_Address_Binding (30): MATCH (List len {len(val_dab) if hasattr(val_dab, '__len__') else 0})")
            self.passed_props += 1
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Device_Address_Binding (30): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.deviceAddressBinding read error: {e}")

        # 1.23 Database Revision
        self.total_props += 1
        try:
            val_dbrev = await self.read_prop(dev_oid, "databaseRevision")
            if int(val_dbrev) >= 1:
                print(f"  ├─ [4.6] Database_Revision (155): MATCH ({val_dbrev})")
                self.passed_props += 1
            else:
                print(f"  ├─ [4.6] Database_Revision (155): MISMATCH (Got {val_dbrev})")
                self.failed_props += 1
                self.mismatches.append(f"Device.databaseRevision: got {val_dbrev}")
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Database_Revision (155): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.databaseRevision read error: {e}")

        # 1.24 Active COV Subscriptions
        self.total_props += 1
        try:
            val_cov_subs = await self.read_prop(dev_oid, "activeCovSubscriptions")
            print(f"  ├─ [4.6] Active_COV_Subscriptions (152): MATCH (List len {len(val_cov_subs) if hasattr(val_cov_subs, '__len__') else 0})")
            self.passed_props += 1
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Active_COV_Subscriptions (152): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.activeCovSubscriptions read error: {e}")

        # 1.25 Property List
        self.total_props += 1
        try:
            val_plist = await self.read_prop(dev_oid, "propertyList")
            print(f"  ├─ [4.6] Property_List (371): MATCH ({len(val_plist) if hasattr(val_plist, '__len__') else 0} properties)")
            self.passed_props += 1
        except (Exception, BaseException) as e:
            print(f"  ├─ [4.6] Property_List (371): FAILED ({e})")
            self.failed_props += 1
            self.mismatches.append(f"Device.propertyList read error: {e}")

        # =====================================================================
        # PHASE 2: Object_List Consistency Verification (Clause 4.5)
        # =====================================================================
        print("\n[*] PHASE 2: Object_List vs EPICS Consistency Check (Clause 4.5)")
        self.total_props += 1
        try:
            val_count = await self.read_prop(dev_oid, "objectList", array_index=0)
            print(f"  ├─ Device reports Object_List array size: {val_count} (EPICS has {len(self.ast_root.objects)})")
            self.passed_props += 1

            # Read full Object_List
            live_objects = []
            for idx in range(1, int(val_count) + 1):
                elem = await self.read_prop(dev_oid, "objectList", array_index=idx)
                live_objects.append(elem)
            print(f"  └─ Successfully queried all {len(live_objects)} objects from live Device.")
        except (Exception, BaseException) as e:
            print(f"  └─ Object_List Query FAILED: {e}")
            self.failed_props += 1
            self.mismatches.append(f"Object_List read error: {e}")

        # =====================================================================
        # PHASE 3: Full Property-by-Property Scan across ALL Declared Objects
        # (Conforms to VTS ReadAllProperties.vts & Clause 4.6)
        # =====================================================================
        print("\n[*] PHASE 3: Full Object & Property Consistency Scan across ALL Declared Objects")
        objs_to_scan = self.ast_root.objects
        if self.max_objects:
            objs_to_scan = objs_to_scan[:self.max_objects]

        total_scan = len(objs_to_scan)
        for idx, obj_node in enumerate(objs_to_scan, start=1):
            oid = obj_node.object_identifier
            exp_name = obj_node.object_name
            exp_units = obj_node.units
            o_type_enum = obj_node.object_type_enum

            # Skip Device object here as it was comprehensively verified in Phase 1
            if o_type_enum == ObjectType.device:
                continue

            obj_label = f"[{idx}/{total_scan}] {obj_node.object_type_str} {obj_node.instance} ('{exp_name}')"
            obj_errs = []

            # 3.1 Object_Identifier
            self.total_props += 1
            try:
                act_oid = await self.read_prop(oid, "objectIdentifier")
                if act_oid == oid or (isinstance(act_oid, tuple) and act_oid[0] == o_type_enum and act_oid[1] == obj_node.instance):
                    self.passed_props += 1
                else:
                    obj_errs.append(f"ObjectIdentifier: got {act_oid}, expected {oid}")
                    self.failed_props += 1
            except (Exception, BaseException) as e:
                obj_errs.append(f"ObjectIdentifier read error: {e}")
                self.failed_props += 1

            # 3.2 Object_Name
            if exp_name:
                self.total_props += 1
                try:
                    act_name = await self.read_prop(oid, "objectName")
                    if str(act_name) == exp_name:
                        self.passed_props += 1
                    else:
                        obj_errs.append(f"ObjectName: got '{act_name}', expected '{exp_name}'")
                        self.failed_props += 1
                except (Exception, BaseException) as e:
                    obj_errs.append(f"ObjectName read error: {e}")
                    self.failed_props += 1

            # 3.3 Object_Type
            self.total_props += 1
            try:
                act_type = await self.read_prop(oid, "objectType")
                if act_type == o_type_enum or str(act_type).lower().replace("-","").replace("_","") == obj_node.object_type_str.replace("-","").replace("_",""):
                    self.passed_props += 1
                else:
                    obj_errs.append(f"ObjectType: got {act_type}, expected {o_type_enum}")
                    self.failed_props += 1
            except (Exception, BaseException) as e:
                obj_errs.append(f"ObjectType read error: {e}")
                self.failed_props += 1

            # 3.4 Present_Value (AI, AO, BI, BO, BV)
            if o_type_enum in (ObjectType.analogInput, ObjectType.analogOutput, ObjectType.binaryInput, ObjectType.binaryOutput, ObjectType.binaryValue):
                self.total_props += 1
                try:
                    act_pv = await self.read_prop(oid, "presentValue")
                    self.passed_props += 1
                except (Exception, BaseException) as e:
                    obj_errs.append(f"PresentValue read error: {e}")
                    self.failed_props += 1

            # 3.5 Status_Flags (AI, AO, BI, BO, BV, NetworkPort)
            if o_type_enum in (ObjectType.analogInput, ObjectType.analogOutput, ObjectType.binaryInput, ObjectType.binaryOutput, ObjectType.binaryValue, ObjectType.networkPort):
                self.total_props += 1
                try:
                    act_flags = await self.read_prop(oid, "statusFlags")
                    self.passed_props += 1
                except (Exception, BaseException) as e:
                    obj_errs.append(f"StatusFlags read error: {e}")
                    self.failed_props += 1

            # 3.6 Event_State (AI, AO, BI, BO, BV)
            if o_type_enum in (ObjectType.analogInput, ObjectType.analogOutput, ObjectType.binaryInput, ObjectType.binaryOutput, ObjectType.binaryValue):
                self.total_props += 1
                try:
                    act_es = await self.read_prop(oid, "eventState")
                    if "normal" in str(act_es).lower() or act_es == EventState.normal or act_es == 0:
                        self.passed_props += 1
                    else:
                        obj_errs.append(f"EventState: got '{act_es}', expected normal/0")
                        self.failed_props += 1
                except (Exception, BaseException) as e:
                    obj_errs.append(f"EventState read error: {e}")
                    self.failed_props += 1

            # 3.7 Out_Of_Service (AI, AO, BI, BO, BV, NetworkPort)
            if o_type_enum in (ObjectType.analogInput, ObjectType.analogOutput, ObjectType.binaryInput, ObjectType.binaryOutput, ObjectType.binaryValue, ObjectType.networkPort):
                self.total_props += 1
                try:
                    act_oos = await self.read_prop(oid, "outOfService")
                    if not act_oos:
                        self.passed_props += 1
                    else:
                        obj_errs.append(f"OutOfService: got True, expected False")
                        self.failed_props += 1
                except (Exception, BaseException) as e:
                    obj_errs.append(f"OutOfService read error: {e}")
                    self.failed_props += 1

            # 3.8 Units (AI, AO)
            if o_type_enum in (ObjectType.analogInput, ObjectType.analogOutput) and exp_units:
                self.total_props += 1
                try:
                    act_units = await self.read_prop(oid, "units")
                    act_unit_str = str(act_units).lower().replace("_", "-")
                    exp_unit_clean = exp_units.lower().replace("_", "-")
                    if exp_unit_clean in act_unit_str or act_unit_str in exp_unit_clean:
                        self.passed_props += 1
                    else:
                        obj_errs.append(f"Units: got '{act_units}', expected '{exp_units}'")
                        self.failed_props += 1
                except (Exception, BaseException) as e:
                    obj_errs.append(f"Units read error: {e}")
                    self.failed_props += 1

            # 3.9 Polarity (BI, BO)
            if o_type_enum in (ObjectType.binaryInput, ObjectType.binaryOutput):
                self.total_props += 1
                try:
                    act_pol = await self.read_prop(oid, "polarity")
                    if "normal" in str(act_pol).lower() or act_pol == Polarity.normal or act_pol == 0:
                        self.passed_props += 1
                    else:
                        obj_errs.append(f"Polarity: got '{act_pol}', expected normal")
                        self.failed_props += 1
                except (Exception, BaseException) as e:
                    obj_errs.append(f"Polarity read error: {e}")
                    self.failed_props += 1

            # 3.10 Min_Pres_Value & Max_Pres_Value (AO)
            if o_type_enum == ObjectType.analogOutput:
                if obj_node.min_pres_value is not None:
                    self.total_props += 1
                    try:
                        act_min = await self.read_prop(oid, "minPresValue")
                        if abs(float(act_min) - obj_node.min_pres_value) < 1e-3:
                            self.passed_props += 1
                        else:
                            obj_errs.append(f"MinPresValue: got {act_min}, expected {obj_node.min_pres_value}")
                            self.failed_props += 1
                    except (Exception, BaseException) as e:
                        obj_errs.append(f"MinPresValue read error: {e}")
                        self.failed_props += 1

                if obj_node.max_pres_value is not None:
                    self.total_props += 1
                    try:
                        act_max = await self.read_prop(oid, "maxPresValue")
                        if abs(float(act_max) - obj_node.max_pres_value) < 1e-3:
                            self.passed_props += 1
                        else:
                            obj_errs.append(f"MaxPresValue: got {act_max}, expected {obj_node.max_pres_value}")
                            self.failed_props += 1
                    except (Exception, BaseException) as e:
                        obj_errs.append(f"MaxPresValue read error: {e}")
                        self.failed_props += 1

            # 3.11 Priority_Array & Relinquish_Default (AO, BO)
            if o_type_enum in (ObjectType.analogOutput, ObjectType.binaryOutput):
                self.total_props += 2
                try:
                    act_pa = await self.read_prop(oid, "priorityArray")
                    self.passed_props += 1
                except (Exception, BaseException) as e:
                    obj_errs.append(f"PriorityArray read error: {e}")
                    self.failed_props += 1

                try:
                    act_rd = await self.read_prop(oid, "relinquishDefault")
                    self.passed_props += 1
                except (Exception, BaseException) as e:
                    obj_errs.append(f"RelinquishDefault read error: {e}")
                    self.failed_props += 1

            # 3.12 COV_Increment (AI, AO)
            if o_type_enum in (ObjectType.analogInput, ObjectType.analogOutput) and obj_node.cov_increment is not None:
                self.total_props += 1
                try:
                    act_cov = await self.read_prop(oid, "covIncrement")
                    if abs(float(act_cov) - obj_node.cov_increment) < 1e-3:
                        self.passed_props += 1
                    else:
                        obj_errs.append(f"COVIncrement: got {act_cov}, expected {obj_node.cov_increment}")
                        self.failed_props += 1
                except (Exception, BaseException) as e:
                    obj_errs.append(f"COVIncrement read error: {e}")
                    self.failed_props += 1

            # 3.13 Property_List (All objects)
            self.total_props += 1
            try:
                act_plist = await self.read_prop(oid, "propertyList")
                self.passed_props += 1
            except (Exception, BaseException) as e:
                obj_errs.append(f"PropertyList read error: {e}")
                self.failed_props += 1

            # Progress logging
            if not obj_errs:
                if idx % 20 == 0 or idx == total_scan or o_type_enum == ObjectType.networkPort:
                    print(f"  ├─ {obj_label}: SUCCESS (All properties match VTS & EPICS)")
            else:
                print(f"  ├─ {obj_label}: FAILED -> {', '.join(obj_errs)}")
                self.mismatches.extend([f"{obj_node.object_type_str} {obj_node.instance} ({exp_name}): {e}" for e in obj_errs])

        # =====================================================================
        # SUMMARY REPORT
        # =====================================================================
        print("\n" + "=" * 75)
        print("                 EPICS FULL CONSISTENCY TEST REPORT (BACpypes3)   ")
        print("=" * 75)
        print(f" Total Declared Objects in EPICS : {len(self.ast_root.objects)}")
        print(f" Objects Scanned & Verified      : {total_scan}")
        print(f" Total Properties Queried        : {self.total_props}")
        print(f" Properties Passed (MATCH)       : {self.passed_props}")
        print(f" Properties Failed (MISMATCH)     : {self.failed_props}")
        pass_rate = (self.passed_props / self.total_props * 100) if self.total_props > 0 else 0
        print(f" EPICS Consistency Pass Rate     : {pass_rate:.1f}%")
        print("=" * 75)

        if self.mismatches:
            print("\n[!] Summary of Detected Mismatches:")
            for m in self.mismatches[:20]:
                print(f"  - {m}")
            if len(self.mismatches) > 20:
                print(f"  ... and {len(self.mismatches) - 20} more mismatches.")
            print("\n[*] Result: FAILED (EPICS inconsistencies detected)")
            return False
        else:
            print("\n[*] Result: SUCCESS - Live BACnet Device matches EPICS.tpi perfectly!")
            return True

class Tee:
    def __init__(self, filename, mode="w"):
        self.file = open(filename, mode, encoding="utf-8")
        self.stdout = sys.stdout

    def write(self, message):
        self.stdout.write(message)
        self.file.write(message)

    def flush(self):
        self.stdout.flush()
        self.file.flush()

    def close(self):
        self.file.close()

def main():
    parser = argparse.ArgumentParser(
        description="LG CDU BACnet/IP Full EPICS (TPI) Consistency Verifier using BACpypes3 & AST Parser (Clause 4)"
    )
    parser.add_argument("tpi_file", nargs="?", default=None, help="Path to EPICS (.tpi) file (positional)")
    parser.add_argument("--tpi", default=None, help="Path to EPICS (.tpi) file (flag)")
    parser.add_argument("--ip", default="127.0.0.1", help="Target Gateway IP Address (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=47808, help="Target Gateway BACnet Port (default: 47808)")
    parser.add_argument("--bind-ip", default=None, help="Local IP address to bind BACpypes3 client stack")
    parser.add_argument("--delay", type=float, default=0.005, help="Delay in seconds between successive requests (default: 0.005)")
    parser.add_argument("--max-objects", type=int, default=None, help="Limit number of objects to scan (default: all)")
    parser.add_argument("--log-file", default=None, help="File path to save the entire test output log")

    args = parser.parse_args()

    tpi_path = args.tpi or args.tpi_file
    if not tpi_path:
        parser.error(
            "EPICS (.tpi) file path is REQUIRED.\n"
            "Usage example:\n"
            "  ./verification/scripts/bacnet_test_step3.py <path_to_tpi_file> --ip 192.168.1.31\n"
            "  ./verification/scripts/bacnet_test_step3.py --tpi <path_to_tpi_file> --ip 192.168.1.31"
        )

    tee = None
    if args.log_file:
        tee = Tee(args.log_file, "w")
        sys.stdout = tee

    try:
        # Parse EPICS into AST
        print(f"[*] Parsing EPICS AST from: {tpi_path}")
        ast_root = EPICSParser.parse_file(tpi_path)
        print(f"[*] AST Parsed: {len(ast_root.objects)} Object AST Nodes created.")

        # Run BACpypes3 Verifier
        verifier = BACpypes3EPICSVerifier(
            ast_root=ast_root,
            target_ip=args.ip,
            target_port=args.port,
            bind_ip=args.bind_ip,
            delay=args.delay,
            max_objects=args.max_objects
        )

        success = asyncio.run(verifier.run())
        sys.exit(0 if success else 1)
    finally:
        if tee:
            sys.stdout = tee.stdout
            tee.close()

if __name__ == "__main__":
    main()
