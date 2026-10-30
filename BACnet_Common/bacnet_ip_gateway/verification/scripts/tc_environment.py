#!/usr/bin/env python3
"""Owned-process environment for the BACnet gateway TC runner."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import signal
import socket
import subprocess
import tempfile
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

from config_loader import load_json_without_comments
from modbus_rtu_slave import check_crc
from tc_framework import blocked, require


TEST_DIR = Path(__file__).resolve().parent
VERIFICATION_ROOT = TEST_DIR.parent
GATEWAY_ROOT = VERIFICATION_ROOT.parent
ES_ROOT = GATEWAY_ROOT.parent
FULL_CONFIG_CASES = frozenset(("TC-BTL-004", "TC-BTL-006"))


@dataclass
class RunOptions:
    mode: str
    output_dir: Path
    gateway_bin: Path
    vport_bin: Path
    vport_test_bin: Path
    unit_test_bin: Path
    python_bin: Path
    tpi_path: Optional[Path]
    performance_policy: Path
    bbmd_endpoint: Optional[Tuple[str, int]]
    target_ip: str
    target_port: int
    device_id: int
    config_dir: Optional[Path] = None
    gateway_config_dir: Optional[Path] = None
    vport_config_path: Optional[Path] = None
    allow_build: bool = False
    long_tests: bool = False
    allow_live_mutation: bool = False
    non_interactive: bool = False
    expected_response_time_ms: int = 0
    pics_doc: Optional[Path] = None
    soak: bool = False
    soak_hours: float = 12.0
    soak_interval: float = 15.0
    ssh_enable: bool = False
    ssh_user: str = "root"
    ssh_port: int = 22
    ssh_pass: Optional[str] = None


class OwnedProcess:
    def __init__(self, name: str, command, cwd: Path, log_path: Path,
                 env: Optional[Dict[str, str]] = None):
        self.name = name
        self.command = [str(item) for item in command]
        self.log_path = log_path
        self._stream = log_path.open("ab", buffering=0)
        self.process = subprocess.Popen(
            self.command,
            cwd=str(cwd),
            stdout=self._stream,
            stderr=subprocess.STDOUT,
            start_new_session=True,
            env=env,
        )

    def stop(self, timeout: float = 3.0):
        try:
            if self.process.poll() is None:
                try:
                    os.killpg(self.process.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                try:
                    self.process.wait(timeout=timeout)
                except subprocess.TimeoutExpired:
                    try:
                        os.killpg(self.process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    self.process.wait(timeout=2.0)
        finally:
            self._stream.close()


class TestEnvironment:
    """Manages only processes and files created by this test invocation."""

    def __init__(self, options: RunOptions):
        self.options = options
        if self.options.gateway_config_dir is None and self.options.config_dir is not None:
            self.options.gateway_config_dir = self.options.config_dir
        elif self.options.config_dir is None and self.options.gateway_config_dir is not None:
            self.options.config_dir = self.options.gateway_config_dir
        self.output_dir = options.output_dir
        self.current_case = None
        self.evidence = None
        self.processes: Dict[str, OwnedProcess] = {}
        self.workspace: Optional[Path] = None
        self.dev_dir: Optional[Path] = None
        self.config_dir: Optional[Path] = None
        self.fixture_config_dir: Optional[Path] = None
        self.modbus_config_dir: Optional[Path] = None
        self.full_config_dir: Optional[Path] = None
        self.control_socket: Optional[Path] = None
        self._process_generations = {}
        self._created_workspace = False
        self._step_number = 0
        self._recorded_modbus_transactions = set()
        self._preflight_cases = {}
        self._preflight_summary = {}
        self._evidence_lock = threading.Lock()

    @property
    def managed(self):
        return self.options.mode == "managed"

    @property
    def capabilities(self):
        """Capabilities available to the selected execution environment."""
        capabilities = {"bacnet_observe"}
        if self.managed:
            capabilities.update({
                "main_control", "main_fault_injection", "modbus_trace",
                "process_lifecycle", "bacnet_mutate",
            })
        elif self.options.allow_live_mutation:
            capabilities.add("bacnet_mutate")
        return capabilities

    def has_capability(self, capability: str):
        return capability in self.capabilities

    def require_capability(self, capability: str, reason: str = ""):
        if self.has_capability(capability):
            return
        detail = reason or f"required capability is unavailable: {capability}"
        blocked(f"{detail} (mode={self.options.mode})")

    def prepare(self):
        self.output_dir.mkdir(parents=True, exist_ok=True)
        if not self.managed:
            self.config_dir = self.options.config_dir or (GATEWAY_ROOT / "conf")
            return
        self.workspace = Path(tempfile.mkdtemp(prefix="bacnet-tc-"))
        self._created_workspace = True
        self.dev_dir = self.workspace / "dev"
        self.fixture_config_dir = self.workspace / "fixture-conf"
        self.config_dir = self.fixture_config_dir
        (self.fixture_config_dir / "profiles").mkdir(parents=True)
        self.modbus_config_dir = self.workspace / "modbus-conf"
        (self.modbus_config_dir / "profiles").mkdir(parents=True)
        self.dev_dir.mkdir()
        self.control_socket = self.workspace / "main-control.sock"
        self._write_fixture()
        if self._config_source_ready(self.options.config_dir):
            self.full_config_dir = self.workspace / "full-conf"
            self._copy_full_config(self.options.config_dir, self.full_config_dir)

    @staticmethod
    def _config_source_ready(path: Optional[Path]) -> bool:
        if path is None or not path.is_dir():
            return False
        gateway_path = path / "gateway.json"
        profiles_dir = path / "profiles"
        return gateway_path.is_file() and profiles_dir.is_dir() and any(
            item.is_file() and item.suffix == ".json" for item in profiles_dir.iterdir()
        )

    @classmethod
    def _config_source_issue(cls, path: Optional[Path]) -> Optional[str]:
        if path is None:
            return "managed full profile에는 --config-dir가 필요합니다"
        if not path.is_dir():
            return f"configuration directory가 없습니다: {path}"
        if not (path / "gateway.json").is_file():
            return f"configuration directory에 gateway.json이 없습니다: {path}"
        profiles_dir = path / "profiles"
        if not profiles_dir.is_dir():
            return f"configuration directory에 profiles 디렉터리가 없습니다: {path}"
        if not any(item.is_file() and item.suffix == ".json"
                   for item in profiles_dir.iterdir()):
            return f"configuration directory에 profiles/*.json이 없습니다: {path}"
        return None

    @staticmethod
    def _case_needs_gateway(tc_id: str) -> bool:
        """Return whether a managed case starts or directly exercises Gateway."""
        if tc_id.startswith(("TC-START-", "TC-AVL-", "TC-PERF-", "TC-OBS-", "TC-MOD-")):
            return True
        if tc_id.startswith("TC-BAC-") or tc_id.startswith("TC-BIP-"):
            return True
        if tc_id in {
            "TC-BTL-001", "TC-BTL-002", "TC-BTL-004", "TC-BTL-005", "TC-BTL-006",
            "TC-CFG-002", "TC-CFG-003", "TC-CFG-006", "TC-CFG-007", "TC-CFG-008",
            "TC-CFG-009", "TC-IFCFG-002", "TC-MAP-007", "TC-MAP-008", "TC-MAP-009",
            "TC-ENV-002",
        }:
            return True
        return False

    @classmethod
    def _case_dependencies(cls, tc_id: str, mode: str) -> List[str]:
        dependencies = []
        managed_gateway = mode == "managed" and cls._case_needs_gateway(tc_id)
        if managed_gateway:
            dependencies.extend(("gateway_bin", "vport_bin", "gateway_server"))
        if mode == "managed":
            if tc_id in {
                "TC-VPORT-004", "TC-VPORT-005", "TC-VPORT-006", "TC-VPORT-007",
                "TC-VPORT-008", "TC-VPORT-009", "TC-VPORT-010", "TC-VPORT-012",
                "TC-VPORT-013",
            }:
                dependencies.append("vport_test_bin")
            if tc_id in {"TC-VPORT-001", "TC-VPORT-002", "TC-VPORT-003", "TC-VPORT-011", "TC-VPORT-014"}:
                dependencies.append("vport_bin")
            if tc_id in {"TC-MAP-004", "TC-MAP-005", "TC-MAP-006"}:
                dependencies.append("unit_test_bin")
        if tc_id in {"TC-BTL-003", "TC-BTL-004", "TC-BTL-006"}:
            dependencies.append("tpi_path")
        if tc_id in {"TC-BTL-004", "TC-BTL-006"}:
            dependencies.append("python_bacpypes3")
            if mode == "managed":
                dependencies.append("full_config_dir")
        if tc_id in {
            "TC-BAC-005", "TC-BAC-006", "TC-BAC-016", "TC-BAC-017", "TC-BAC-018",
            "TC-BIP-003", "TC-BTL-002", "TC-BTL-005", "TC-BTL-004", "TC-BTL-006",
        }:
            dependencies.append("python_runtime")
        if tc_id.startswith("TC-PERF-"):
            dependencies.append("performance_policy")
        if tc_id in {"TC-BIP-002", "TC-BAC-020"}:
            dependencies.append("bbmd_endpoint")
        return dependencies

    @staticmethod
    def _path_is_executable(path: Optional[Path]) -> bool:
        return path is not None and path.is_file() and os.access(path, os.X_OK)

    @staticmethod
    def _path_is_file(path: Optional[Path]) -> bool:
        return path is not None and path.is_file()

    def _dependency_status(self, dependency: str, tc_id: str = "") -> Optional[str]:
        options = self.options
        if dependency == "gateway_bin":
            if not self._path_is_executable(options.gateway_bin):
                return f"bacnet_ip_gateway 실행 파일이 없습니다: {options.gateway_bin}"
        elif dependency == "vport_bin":
            if not self._path_is_executable(options.vport_bin):
                return f"vport 실행 파일이 없습니다: {options.vport_bin}"
        elif dependency == "vport_test_bin":
            if not self._path_is_executable(options.vport_test_bin):
                return f"vport_test 실행 파일이 없습니다: {options.vport_test_bin}"
        elif dependency == "unit_test_bin":
            if not self._path_is_executable(options.unit_test_bin):
                return f"host unit-test 실행 파일이 없습니다: {options.unit_test_bin}"
        elif dependency == "gateway_server":
            script = TEST_DIR / "gateway_server.sh"
            if not self._path_is_executable(script):
                return f"Main simulator wrapper가 없습니다: {script}"
        elif dependency == "tpi_path":
            if not self._path_is_file(options.tpi_path):
                return "EPICS/TPI 파일이 없습니다. --tpi 또는 실행 중 입력으로 지정하십시오"
        elif dependency == "full_config_dir":
            return self._config_source_issue(options.config_dir)
        elif dependency == "python_bacpypes3":
            if not self._path_is_executable(options.python_bin):
                return f"Python interpreter가 없습니다: {options.python_bin}"
            try:
                probe = subprocess.run(
                    [str(options.python_bin), "-c", "import bacpypes3"],
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                    text=True, timeout=10.0, check=False,
                )
            except (OSError, subprocess.TimeoutExpired) as exc:
                return f"bacpypes3 import 점검 실패: {exc}"
            if probe.returncode != 0:
                detail = probe.stderr.strip().splitlines()[-1] if probe.stderr.strip() else "import 실패"
                return f"{options.python_bin}에 bacpypes3가 없습니다: {detail}"
        elif dependency == "python_runtime":
            if not self._path_is_executable(options.python_bin):
                return f"Python interpreter가 없습니다: {options.python_bin}"
        elif dependency == "performance_policy":
            if not self._path_is_file(options.performance_policy):
                return f"performance policy가 없습니다: {options.performance_policy}"
            try:
                policy = json.loads(options.performance_policy.read_text(encoding="utf-8"))
            except (OSError, ValueError) as exc:
                return f"performance policy를 읽을 수 없습니다: {exc}"
            scenarios = policy.get("scenarios", {})
            scenario_policy = scenarios.get(tc_id) if scenarios else None
            if scenario_policy is None and tc_id == "TC-PERF-001" and policy.get("approved"):
                scenario_policy = policy
            if not isinstance(scenario_policy, dict) or not scenario_policy.get("approved"):
                return f"{tc_id}에 승인된 performance scenario policy가 없습니다"
            limits = scenario_policy.get("limits", {})
            if any(limits.get(key) is None for key in (
                    "minimum_success_rate", "minimum_throughput_rps",
                    "maximum_p95_latency_ms", "maximum_p99_latency_ms")):
                return f"{tc_id} performance policy의 기준값이 불완전합니다"
        elif dependency == "bbmd_endpoint":
            if options.bbmd_endpoint is None:
                return "외부 BBMD host:port가 지정되지 않았습니다"
        return None

    def preflight(self, selected: Sequence) -> Dict:
        """Check only dependencies used by selected cases and persist the result."""
        self.output_dir.mkdir(parents=True, exist_ok=True)
        cases = {}
        for spec in selected:
            required = self._case_dependencies(spec.tc_id, self.options.mode)
            issues = []
            for dependency in required:
                issue = self._dependency_status(dependency, spec.tc_id)
                if issue is not None:
                    issues.append(issue)
            if not self.options.long_tests and spec.tc_id == "TC-OBS-003":
                issues.append("TC-OBS-003은 실제 10분 시험이므로 --long이 필요합니다")
            self._preflight_cases[spec.tc_id] = issues
            cases[spec.tc_id] = {
                "required": required,
                "missing": issues,
                "ready": not issues,
            }
        inputs = {
            "gateway_bin": str(self.options.gateway_bin),
            "vport_bin": str(self.options.vport_bin),
            "vport_test_bin": str(self.options.vport_test_bin),
            "unit_test_bin": str(self.options.unit_test_bin),
            "python_bin": str(self.options.python_bin),
            "config_dir": str(self.options.config_dir) if self.options.config_dir else None,
            "gateway_config_dir": str(self.options.gateway_config_dir) if self.options.gateway_config_dir else None,
            "vport_config_path": str(self.options.vport_config_path) if self.options.vport_config_path else None,
            "tpi_path": str(self.options.tpi_path) if self.options.tpi_path else None,
            "performance_policy": str(self.options.performance_policy),
            "bbmd_endpoint": list(self.options.bbmd_endpoint)
            if self.options.bbmd_endpoint else None,
            "non_interactive": self.options.non_interactive,
        }
        self._preflight_summary = {"inputs": inputs, "cases": cases}
        preflight_path = self.output_dir / "preflight.json"
        preflight_path.write_text(
            json.dumps(self._preflight_summary, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        return self._preflight_summary

    def require_preflight(self, tc_id: str):
        issues = self._preflight_cases.get(tc_id, [])
        if issues:
            blocked("preflight: " + "; ".join(issues))

    @property
    def preflight_summary(self):
        return self._preflight_summary

    def _write_fixture(self):
        if self.fixture_config_dir is None:
            raise RuntimeError("managed fixture directory is not prepared")
        gateway = {
            "bacnet": {
                "database_revision": 1,
                "description": "Managed TC fixture",
                "device_id": self.options.device_id,
                "device_name": "BACnet_TC_Device",
                "model_name": "TC",
                "network_port_name": "NetworkPort_1",
                "vendor_id": 432,
                "vendor_name": "LG Electronics",
            },
            "modbus": {
                "hardware_echo": False,
                "rtu_timeout_ms": 500,
                "serial_baudrate": 19200,
                "serial_data_bits": 8,
                "serial_device": str(self.dev_dir / "ttyMASTER2"),
                "serial_parity": "none",
                "serial_stop_bits": 1,
                "cache_sec": 0,
                "cov_polling": 0.1,
            },
        }
        profile = {
            "modbus_unit_id": 1,
            "objects": [
                {"name": "TC_AI", "full_address": 30001, "modbus_block": "INPUT_REGISTER",
                 "bacnet_type": "ANALOG_INPUT", "instance": 1, "data_type": "UINT16",
                 "scale": 1.0, "offset": 0.0, "min_value": 0.0, "max_value": 100.0},
                {"name": "TC_AO_JSON_IN", "full_address": 40001, "modbus_block": "HOLDING_REGISTER",
                 "bacnet_type": "ANALOG_OUTPUT", "instance": 2, "data_type": "UINT16",
                 "scale": 1.0, "offset": 0.0, "min_value": 10.0, "max_value": 90.0,
                 "relinquish_default": 50.0},
                {"name": "TC_AO_JSON_LOW", "full_address": 40002, "modbus_block": "HOLDING_REGISTER",
                 "bacnet_type": "ANALOG_OUTPUT", "instance": 3, "data_type": "UINT16",
                 "scale": 1.0, "offset": 0.0, "min_value": 10.0, "max_value": 90.0,
                 "relinquish_default": -1.0},
                {"name": "TC_AO_JSON_HIGH", "full_address": 40003, "modbus_block": "HOLDING_REGISTER",
                 "bacnet_type": "ANALOG_OUTPUT", "instance": 4, "data_type": "UINT16",
                 "scale": 1.0, "offset": 0.0, "min_value": 10.0, "max_value": 90.0,
                 "relinquish_default": 101.0},
                {"name": "TC_AO_MAIN", "full_address": 40004, "modbus_block": "HOLDING_REGISTER",
                 "bacnet_type": "ANALOG_OUTPUT", "instance": 5, "data_type": "UINT16",
                 "scale": 1.0, "offset": 0.0, "min_value": 10.0, "max_value": 90.0},
                {"name": "TC_AO_MAIN_HIGH", "full_address": 40005, "modbus_block": "HOLDING_REGISTER",
                 "bacnet_type": "ANALOG_OUTPUT", "instance": 8, "data_type": "UINT16",
                 "scale": 1.0, "offset": 0.0, "min_value": 10.0, "max_value": 90.0},
                {"name": "TC_BI", "full_address": 10001, "modbus_block": "DISCRETE_INPUT",
                 "bacnet_type": "BINARY_INPUT", "instance": 6},
                {"name": "TC_BV", "full_address": 1, "modbus_block": "COIL",
                 "bacnet_type": "BINARY_VALUE", "instance": 7},
            ],
        }
        self.write_json(self.fixture_config_dir / "gateway.json", gateway)
        self.write_json(self.fixture_config_dir / "profiles" / "fixture.json", profile)
        if self.modbus_config_dir is not None:
            modbus_profile = dict(profile)
            modbus_profile["objects"] = list(profile["objects"]) + [
                {"name": "TC_AO_FLOAT", "full_address": 40010, "modbus_block": "HOLDING_REGISTER",
                 "bacnet_type": "ANALOG_OUTPUT", "instance": 9, "data_type": "FLOAT32",
                 "scale": 1.0, "offset": 0.0, "min_value": 0.0, "max_value": 100.0,
                 "relinquish_default": 25.0},
            ]
            self.write_json(self.modbus_config_dir / "gateway.json", gateway)
            self.write_json(self.modbus_config_dir / "profiles" / "fixture.json", modbus_profile)
        self.write_bip_config("normal")
        (self.workspace / "vport.cfg").write_text("DEFAULT_SLAVE_TIMEOUT_MS=500\n", encoding="utf-8")

    def _copy_full_config(self, source_dir: Path, target_dir: Path):
        """Copy a product config into an isolated full-profile config tree."""
        target_dir.mkdir(parents=True, exist_ok=True)
        profiles_dir = target_dir / "profiles"
        profiles_dir.mkdir(parents=True, exist_ok=True)
        source_gateway = source_dir / "gateway.json"
        gateway = load_json_without_comments(str(source_gateway))
        gateway.setdefault("modbus", {})["serial_device"] = str(self.dev_dir / "ttyMASTER2")
        self.write_json(target_dir / "gateway.json", gateway)
        for source_profile in sorted((source_dir / "profiles").glob("*.json")):
            shutil.copy2(source_profile, profiles_dir / source_profile.name)

    def select_case_config(self, tc_id: str):
        """Select the isolated config tree appropriate for the current TC."""
        if not self.managed:
            self.config_dir = self.options.config_dir or (GATEWAY_ROOT / "conf")
            selected_kind = "live-config-dir" if self.options.config_dir else "project-conf"
        elif tc_id in FULL_CONFIG_CASES:
            if self.full_config_dir is None:
                blocked(f"{tc_id} requires a valid --config-dir full profile")
            self.config_dir = self.full_config_dir
            selected_kind = "full-conf"
        elif tc_id == "TC-MOD-002":
            if self.modbus_config_dir is None:
                raise RuntimeError("managed modbus directory is not prepared")
            self.config_dir = self.modbus_config_dir
            selected_kind = "modbus-conf"
        else:
            if self.fixture_config_dir is None:
                raise RuntimeError("managed fixture directory is not prepared")
            self.config_dir = self.fixture_config_dir
            selected_kind = "fixture-conf"
        if self.evidence is not None:
            self.evidence.details["config_selection"] = {
                "case": tc_id,
                "kind": selected_kind,
                "source": str(self.options.config_dir)
                if self.options.config_dir is not None else "generated fixture",
                "active": str(self.config_dir),
            }

    @staticmethod
    def write_json(path: Path, payload):
        path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    def write_bip_config(self, mode: str, bbmd_endpoint: Optional[Tuple[str, int]] = None, ttl: int = 60):
        """Write one isolated BACnet/IP mode configuration for the next Gateway start."""
        if self.workspace is None:
            raise RuntimeError("managed workspace is not prepared")
        selected = str(mode).lower()
        if selected not in {"normal", "fd", "bbmd"}:
            raise ValueError(f"unsupported BACnet/IP mode: {mode}")
        endpoint = bbmd_endpoint or self.options.bbmd_endpoint
        header = "# Generated for this test case; the gateway loads bip.cfg from the parent of its config dir."
        normal_lines = [
            header, "mode=normal",
            f"port={self.options.target_port}",
            f"ip={self.options.target_ip}",
            "subnet=255.255.255.0",
        ]
        # bip.cfg is a symlink to bip.cfg.normal / bip.cfg.fd, like the product layout.
        # The gateway re-links it when BACnet_IP_Mode is written.
        (self.workspace / "bip.cfg.normal").write_text("\n".join(normal_lines) + "\n", encoding="utf-8")
        if endpoint is not None:
            host, port = endpoint
            fd_lines = [
                header, "mode=fd",
                f"port={self.options.target_port}",
                f"ip={self.options.target_ip}",
                "subnet=255.255.255.0",
                f"fd.ip={self.options.target_ip}",
                f"fd.port={self.options.target_port}",
                f"fd.bbmd_ip={host}",
                f"fd.bbmd_port={port}",
                f"fd.ttl={ttl}",
                "fd.reg_timeout=3000",
                "fd.reg_attempts=0",
            ]
            (self.workspace / "bip.cfg.fd").write_text("\n".join(fd_lines) + "\n", encoding="utf-8")
        link = self.workspace / "bip.cfg"
        if link.is_symlink() or link.exists():
            link.unlink()
        if selected in ("normal", "fd"):
            if selected == "fd" and endpoint is None:
                raise ValueError("FD mode requires an external BBMD endpoint")
            link.symlink_to("bip.cfg." + selected)
            return
        lines = list(normal_lines)
        lines[1] = f"mode={selected}"
        if selected == "bbmd":
            if endpoint is None:
                raise ValueError("BBMD mode requires an external BBMD endpoint")
            host, port = endpoint
            lines.extend([
                f"bbmd.bdt={host}/32:{port}",
                "bbmd.fdt_present=true",
                "bbmd.nat_supported=false",
                "bbmd.nat_enabled=false",
                "bbmd.max_fdt_entries=32",
                "bbmd.nat_ip=",
                f"bbmd.nat_udp={self.options.target_port}",
                "bbmd.resolv_timeout=3600",
            ])
        (self.workspace / "bip.cfg").write_text("\n".join(lines) + "\n", encoding="utf-8")

    def add_evidence(self, path: Path):
        if self.evidence is not None:
            self.evidence.add(path)

    @staticmethod
    def _endpoint_value(endpoint):
        if endpoint is None:
            return None
        if isinstance(endpoint, (tuple, list)):
            return list(endpoint)
        return str(endpoint)

    def record_packet(self, channel, direction, packet=None, endpoint=None,
                      fields=None, status="received", note=""):
        """Record one protocol packet/event without using text output as an oracle."""
        item = {
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S.000Z", time.gmtime()),
            "channel": str(channel),
            "direction": str(direction),
            "status": str(status),
            "endpoint": self._endpoint_value(endpoint),
            "raw_hex": packet.hex(" ") if isinstance(packet, (bytes, bytearray)) else None,
            "fields": fields or {},
        }
        if note:
            item["note"] = str(note)
        if self.evidence is not None:
            with self._evidence_lock:
                self.evidence.details.setdefault("packet_exchanges", []).append(item)
                case_id = self.current_case.tc_id.lower() if self.current_case is not None else "run"
                trace_path = self.output_dir / f"{case_id}_packet_exchanges.jsonl"
                with trace_path.open("a", encoding="utf-8") as stream:
                    stream.write(json.dumps(item, ensure_ascii=False, sort_keys=True) + "\n")
                self.evidence.add(trace_path)
        return item

    @staticmethod
    def _step_value(value):
        if isinstance(value, str):
            return value
        try:
            return json.dumps(value, ensure_ascii=False, sort_keys=True, default=str)
        except (TypeError, ValueError):
            return repr(value)

    def reset_steps(self):
        self._step_number = 0

    def record_step(self, label, expected, actual, passed=True, message="",
                    result_status=None):
        """Store and print one auditable expected/actual test step."""
        self._step_number += 1
        status = result_status or ("PASS" if passed else "FAIL")
        item = {
            "step": self._step_number,
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "status": status,
            "label": str(label),
            "expected": expected,
            "actual": actual,
        }
        if message:
            item["message"] = str(message)
        if self.evidence is not None:
            self.evidence.details.setdefault("steps", []).append(item)
        case_id = self.current_case.tc_id if self.current_case is not None else "RUN"
        text = (f"    [{case_id}][STEP-{self._step_number:02d}][{status}] {label}"
                f" | 기대값: {self._step_value(expected)}"
                f" | 수신값: {self._step_value(actual)}")
        if message:
            text += f" | {message}"
        print(text, flush=True)

    def require_managed(self, reason: Optional[str] = None):
        if not self.managed:
            blocked(reason or "이 TC는 프로세스 수명주기 제어가 필요하므로 --mode managed에서 실행해야 합니다")

    def require_executable(self, path: Path, label: str):
        if not path.is_file() or not os.access(path, os.X_OK):
            blocked(f"{label} 실행 파일이 없습니다: {path}")

    def start_process(self, name: str, command, cwd: Optional[Path] = None,
                      env: Optional[Dict[str, str]] = None):
        if name in self.processes and self.processes[name].process.poll() is None:
            return self.processes[name]
        prefix = self.current_case.tc_id.lower() if self.current_case is not None else "setup"
        generation_key = (prefix, name)
        generation = self._process_generations.get(generation_key, 0) + 1
        self._process_generations[generation_key] = generation
        suffix = "" if generation == 1 else f"_{generation}"
        log_path = self.output_dir / f"{prefix}_{name}{suffix}.log"
        proc = OwnedProcess(name, command, cwd or self.workspace or GATEWAY_ROOT,
                            log_path, env=env)
        self.processes[name] = proc
        self.add_evidence(log_path)
        return proc

    def start_vport(self, count: int = 3):
        self.require_managed()
        self.require_executable(self.options.vport_bin, "vport")
        env = dict(os.environ)
        env["VPORT_LOG"] = "1"
        return self.start_process("vport", [
            self.options.vport_bin, str(count), "--link-dir", self.dev_dir,
            "--config", self.workspace / "vport.cfg",
        ], env=env)

    def start_simulator(self):
        self.require_managed()
        prefix = self.current_case.tc_id.lower() if self.current_case is not None else "setup"
        simulator_log = self.output_dir / f"{prefix}_main_simulator_transactions.log"
        # Do not treat an old pathname as readiness.  A previous simulator can
        # still be closing its listener while the next case is being prepared.
        # The process stop path waits for the managed PID, and this unlink also
        # makes the readiness check below unambiguous.
        try:
            self.control_socket.unlink()
        except FileNotFoundError:
            pass
        command = [
            TEST_DIR / "gateway_server.sh", self.dev_dir / "ttySLAVE", "1", "19200",
            "--control-socket", self.control_socket,
            "--log-file", simulator_log,
        ]
        if self.options.expected_response_time_ms > 0:
            command.extend(["--delay", f"{self.options.expected_response_time_ms / 1000.0:.3f}"])
        proc = self.start_process("main_simulator", command)

        def simulator_ready():
            if proc.process.poll() is not None:
                raise AssertionError(
                    f"main simulator exited with {proc.process.returncode}")
            if not self.control_socket.exists():
                return False
            try:
                self.simulator({"op": "ping"})
                return True
            except (OSError, AssertionError, ValueError, json.JSONDecodeError):
                # The listener may exist just before its serving thread is
                # ready. Retry the handshake instead of classifying the race
                # as a TC protocol failure.
                return False

        self.wait_for(simulator_ready, 5.0, "simulator control socket")
        self.add_evidence(simulator_log)
        return proc

    def start_gateway(self):
        self.require_managed()
        self.require_executable(self.options.gateway_bin, "bacnet_ip_gateway")
        gateway_env = os.environ.copy()
        gateway_env["BACNET_LOG"] = "1"
        return self.start_process("gateway", [self.options.gateway_bin, self.config_dir],
                                  env=gateway_env)

    def stack_is_running(self):
        """Return true only when the complete managed stack is ready."""
        if not self.managed:
            return False
        for name in ("vport", "main_simulator", "gateway"):
            proc = self.processes.get(name)
            if proc is None or proc.process.poll() is not None:
                return False
        if self.control_socket is None or not self.control_socket.exists():
            return False
        return "BACnet IP Gateway started" in self.gateway_log()

    def start_stack(self, initial_main_value: int = 40):
        self.start_vport(3)
        self.wait_for(lambda: (self.dev_dir / "ttySLAVE").exists(), 5.0, "vport links")
        self.start_simulator()
        self.simulator({"op": "set", "block": "holding", "address": 3,
                        "value": initial_main_value})
        self.simulator({"op": "set", "block": "holding", "address": 4,
                        "value": 110})
        self.start_gateway()
        ready = self.wait_gateway_ready()
        # Capture the actual RTU exchanges performed during initial sync. The
        # stats request is evidence collection, not a visible test step, so it
        # does not change the protocol procedure numbering.
        self.simulator({"op": "stats"}, record_step=False)
        self.record_step("[Modbus RTU/process] managed stack 기동", True, ready,
                         passed=bool(ready))
        return ready

    def wait_gateway_ready(self, timeout: Optional[float] = None):
        delay_sec = (self.options.expected_response_time_ms or 0) / 1000.0
        effective_timeout = timeout if timeout is not None else max(15.0, 15.0 + delay_sec * 150.0)
        gateway = self.processes.get("gateway")
        def ready():
            if gateway is not None and gateway.process.poll() is not None:
                raise AssertionError(f"gateway exited with {gateway.process.returncode}")
            if not gateway:
                return False
            return "BACnet IP Gateway started" in gateway.log_path.read_text(
                encoding="utf-8", errors="replace")
        self.wait_for(ready, effective_timeout, "gateway startup")
        return True

    @staticmethod
    def wait_for(predicate, timeout: float, description: str, interval: float = 0.05):
        deadline = time.monotonic() + timeout
        last_error = None
        while time.monotonic() < deadline:
            try:
                if predicate():
                    return
            except Exception as exc:
                last_error = exc
            time.sleep(interval)
        suffix = f" ({last_error})" if last_error else ""
        raise AssertionError(f"timeout waiting for {description}{suffix}")

    def simulator(self, request, record_step=True):
        self.require_capability("main_control", "Main simulator Unix control socket is required")
        if self.control_socket is None or not self.control_socket.exists():
            blocked("Main simulator control socket is not available")
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
            client.settimeout(3.0)
            client.connect(str(self.control_socket))
            client.sendall((json.dumps(request) + "\n").encode("utf-8"))
            data = b""
            while b"\n" not in data:
                chunk = client.recv(65536)
                if not chunk:
                    break
                data += chunk
        require(bool(data), "Main simulator returned no control response")
        response = json.loads(data.split(b"\n", 1)[0].decode("utf-8"))
        require(response.get("ok") is True, f"simulator operation failed: {response}")
        self.record_packet(
            "Unix control socket", "Runner -> Main simulator",
            fields=request, status="request", note="environment control; not Modbus/BACnet",
        )
        self.record_packet(
            "Unix control socket", "Main simulator -> Runner",
            fields=response, status="response", note="environment control; not Modbus/BACnet",
        )
        if request.get("op") == "stats":
            self._record_modbus_transactions(response.get("transactions", []))
        if record_step:
            self.record_step("[Unix control socket] Main simulator", request, response)
        return response

    def _record_modbus_transactions(self, transactions):
        """Persist simulator-observed RTU request/response frames as evidence."""
        for transaction in transactions:
            identity = (
                transaction.get("timestamp"), transaction.get("request_hex"),
                transaction.get("response_hex"), transaction.get("action"),
            )
            if identity in self._recorded_modbus_transactions:
                continue
            self._recorded_modbus_transactions.add(identity)
            request_hex = transaction.get("request_hex", "")
            response_hex = transaction.get("response_hex", "")
            request = bytes.fromhex(request_hex) if request_hex else None
            response = bytes.fromhex(response_hex) if response_hex else None
            base_fields = {
                "unit_id": transaction.get("unit_id"),
                "function": transaction.get("fc"),
                "address": transaction.get("address"),
                "action": transaction.get("action"),
            }
            request_fields = {**base_fields, "crc_valid": check_crc(request) if request else False}
            response_fields = {**base_fields, "crc_valid": check_crc(response) if response else False}
            self.record_packet(
                "Modbus RTU", "Gateway -> Main simulator", request,
                endpoint=f"slave:{transaction.get('unit_id')}",
                fields=request_fields, status="request",
            )
            self.record_packet(
                "Modbus RTU", "Main simulator -> Gateway", response,
                endpoint=f"slave:{transaction.get('unit_id')}",
                fields=response_fields, status="response",
            )

    def gateway_log(self):
        proc = self.processes.get("gateway")
        return "" if proc is None else proc.log_path.read_text(encoding="utf-8", errors="replace")

    def stop_process(self, name: str):
        proc = self.processes.pop(name, None)
        if proc is not None:
            proc.stop()

    def force_kill_process(self, name: str):
        """Forcefully stop an owned process and forget its generation."""
        proc = self.processes.pop(name, None)
        if proc is None:
            return None
        try:
            if proc.process.poll() is None:
                try:
                    os.killpg(proc.process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                proc.process.wait(timeout=3.0)
            return proc.process.returncode
        finally:
            proc._stream.close()

    def run_command(self, name: str, command, timeout: float = 120.0, cwd: Path = GATEWAY_ROOT):
        prefix = self.current_case.tc_id.lower() if self.current_case is not None else "setup"
        log_path = self.output_dir / f"{prefix}_{name}.log"
        with log_path.open("wb") as stream:
            completed = subprocess.run(
                [str(item) for item in command], cwd=str(cwd), stdout=stream,
                stderr=subprocess.STDOUT, timeout=timeout, check=False)
        self.add_evidence(log_path)
        return completed.returncode, log_path.read_text(encoding="utf-8", errors="replace")

    def run_profile_tester(self, config_dir: Optional[Path] = None):
        """Execute BACnet/IP object profile R/W/Restore verifier (bacnet_profile_tester.py)."""
        script = TEST_DIR / "bacnet_profile_tester.py"
        target_conf = config_dir or self.config_dir or self.options.config_dir or (GATEWAY_ROOT / "conf")
        command = [
            self.options.python_bin, script,
            "--ip", self.options.target_ip,
            "--port", str(self.options.target_port),
            "--config-dir", target_conf,
        ]
        return self.run_command("bacnet_profile_tester", command, timeout=900.0)

    def run_epics_verifier(self, tpi_path: Optional[Path] = None):
        """Execute BTL EPICS 350-point full conformance scanner (btl_epics_verifier.py)."""
        script = TEST_DIR / "btl_epics_verifier.py"
        target_tpi = tpi_path or self.options.tpi_path
        if target_tpi is None or not Path(target_tpi).is_file():
            blocked("BTL EPICS 검증에 필요한 TPI 파일이 없습니다")
        command = [
            self.options.python_bin, script, target_tpi,
            "--ip", self.options.target_ip,
            "--port", str(self.options.target_port),
        ]
        return self.run_command("btl_epics_verifier", command, timeout=900.0)

    def legacy_suite(self, step: int):
        """Backward-compatibility wrapper for legacy step invocation."""
        if step == 2:
            return self.run_profile_tester()
        if step == 3:
            return self.run_epics_verifier()
        raise ValueError(f"unsupported step: {step}")

    def checksum(self, path: Path):
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        return digest

    def after_case(self, _spec, _status):
        if self.control_socket is not None and self.control_socket.exists():
            try:
                self.simulator({"op": "clear_faults"})
            except Exception:
                pass
        if self.managed:
            for name in reversed(list(self.processes)):
                self.stop_process(name)
            if self.config_dir is not None:
                self._write_fixture()

    def close(self):
        for name in reversed(list(self.processes)):
            try:
                self.stop_process(name)
            except Exception:
                pass
        if self._created_workspace and self.workspace is not None:
            shutil.rmtree(self.workspace, ignore_errors=True)
