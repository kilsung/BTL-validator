#!/usr/bin/env python3
"""Execute the SRS-traced BACnet/IP gateway test catalog."""

from __future__ import annotations

import argparse
import json
import os
import socket
import subprocess
import sys
import time
from pathlib import Path

from tc_environment import ES_ROOT, GATEWAY_ROOT, VERIFICATION_ROOT, RunOptions, TestEnvironment
from tc_framework import BLOCKED, FAIL, execute_case, select_cases, validate_registry, write_reports
import tc_cases  # noqa: F401 - importing registers all TC handlers


def default_local_ip():
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect(("192.0.2.1", 9))
        return probe.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        probe.close()


def parse_args():
    parser = argparse.ArgumentParser(description="Run all SRS/RTM BACnet gateway test cases")
    parser.add_argument("--suite", choices=("quick", "full", "audit", "soak", "all"), default="quick")
    parser.add_argument("--case", action="append", default=[], dest="patterns",
                        help="TC ID or prefix wildcard, repeatable (example: TC-CFG-* )")
    parser.add_argument("--mode", choices=("managed", "live"), default="managed")
    parser.add_argument("--ip", default=None, help="BACnet target/bind IP")
    parser.add_argument("--port", type=int, default=47808)
    parser.add_argument("--device-id", type=int, default=1200)
    parser.add_argument("--config-dir", type=Path, default=None,
                        help="gateway configuration directory (alias for --gateway-config-dir)")
    parser.add_argument("--gateway-config-dir", type=Path, default=None,
                        help="gateway configuration directory; managed copies it to full-conf, live reads it directly")
    parser.add_argument("--vport-config-path", type=Path, default=None,
                        help="vport configuration file path (default: vport_release/vport.cfg)")
    parser.add_argument("--gateway-bin", type=Path, default=None,
                        help="bacnet_ip_gateway executable (prompted when a managed TC needs it)")
    parser.add_argument("--vport-bin", type=Path, default=None,
                        help="vport executable (prompted when a managed TC needs it)")
    parser.add_argument("--vport-test-bin", type=Path, default=None,
                        help="vport_test executable for TC-VPORT-004..009")
    parser.add_argument("--unit-test-bin", type=Path, default=None,
                        help="host unit-test executable for TC-MAP-004..006")
    parser.add_argument("--tpi", type=Path, default=None,
                        help="authoritative EPICS/TPI file for TC-BTL-003/004/006")
    parser.add_argument("--pics-doc", type=Path, default=None,
                        help="BTL functionality checklist / PICS document (docx or md) for TC-BTL-003")
    parser.add_argument("--python-bin", type=Path, default=None,
                        help="Python interpreter used by compatibility adapters")
    parser.add_argument("--bbmd-endpoint", default=None, metavar="HOST:PORT",
                        help="external BBMD endpoint for TC-BIP-002 and TC-BAC-020")
    parser.add_argument("--bbmd-auto", action="store_true", default=False,
                        help="automatically launch embedded BBMD server if no external BBMD endpoint is specified")
    parser.add_argument("--response-time-ms", type=int, default=0,
                        help="expected Main slave response delay in milliseconds (passed to simulator in managed mode and used for performance scaling)")
    parser.add_argument("--performance-policy", type=Path,
                        default=VERIFICATION_ROOT / "scripts" / "performance_policy.json",
                        help="approved scenario performance policy JSON")
    parser.add_argument("--results-dir", type=Path, default=VERIFICATION_ROOT / "results")
    parser.add_argument("--long", action="store_true", help="enable 60s/10min duration cases")
    parser.add_argument("--soak", action="store_true", help="enable long-term reliability soak test (e.g. 12 hours)")
    parser.add_argument("--soak-hours", type=float, default=12.0,
                        help="soak test duration in hours (default: 12.0)")
    parser.add_argument("--soak-interval", type=float, default=15.0,
                        help="soak test cycle polling interval in seconds (default: 15.0)")
    parser.add_argument(
        "--allow-live-mutation",
        action="store_true",
        help="allow BACnet WriteProperty/DCC/SubscribeCOV in live mode",
    )
    parser.add_argument(
        "--ssh-enable",
        action="store_true",
        help="enable remote SSH process monitoring (gateway & vport) in live mode",
    )
    parser.add_argument(
        "--ssh-user",
        default="root",
        help="remote SSH username (default: root)",
    )
    parser.add_argument(
        "--ssh-port",
        type=int,
        default=22,
        help="remote SSH port (default: 22)",
    )
    parser.add_argument(
        "--ssh-pass",
        default=os.environ.get("SSH_PASS"),
        help="remote SSH password (optional, if password auth used)",
    )
    parser.add_argument("--allow-build", action="store_true",
                        help="allow TC-ENV-001 host build (makeIMX is always prohibited)")
    parser.add_argument("--list", action="store_true", help="list selected cases without running")
    parser.add_argument("--approve-baseline", type=Path, default=None, metavar="RESULT_JSON",
                        help="explicitly approve performance metrics from a reviewed JSON result")
    parser.add_argument("--baseline-scenario", default=None,
                        help="scenario ID when approving a performance result")
    parser.add_argument("--non-interactive", action="store_true",
                        help="do not prompt for missing dependencies; affected cases become BLOCKED")
    return parser.parse_args()


def _path_value(value, default):
    candidate = value if value is not None else default
    return Path(candidate).expanduser().resolve()


def _is_executable(path):
    return path.is_file() and path.stat().st_mode & 0o111 != 0


def _is_file(path):
    return path.is_file()


def _has_bacpypes3(path):
    if not _is_executable(path):
        return False
    try:
        probe = subprocess.run(
            [str(path), "-c", "import bacpypes3"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            timeout=10.0, check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        return False
    return probe.returncode == 0


def _find_python_bacpypes3(requested: Path | None) -> Path:
    if requested is not None and _has_bacpypes3(requested):
        return requested.expanduser().resolve()
    current = Path(sys.executable).resolve()
    if _has_bacpypes3(current):
        return current
    venv_py = VERIFICATION_ROOT / ".venv" / "bin" / "python"
    if _has_bacpypes3(venv_py):
        return venv_py.resolve()
    import shutil
    sys_py = shutil.which("python3")
    if sys_py:
        sys_py_path = Path(sys_py).resolve()
        if _has_bacpypes3(sys_py_path):
            return sys_py_path
    return requested or current


def _prompt_path(label, candidate, predicate, non_interactive):
    if predicate(candidate):
        return candidate
    if non_interactive or not sys.stdin.isatty():
        return candidate
    print(f"{label}을 찾을 수 없습니다: {candidate}")
    print("이번 실행에서 사용할 경로를 입력하십시오 (빈 입력은 해당 TC를 BLOCKED 처리합니다): ",
          end="", flush=True)
    try:
        value = input().strip()
    except EOFError:
        return candidate
    return Path(value).expanduser().resolve() if value else candidate


def _parse_endpoint(value):
    if value is None:
        return None
    text = str(value).strip()
    if text.startswith("[") and "]" in text:
        host, separator, port_text = text[1:].partition("]")
        if separator:
            port_text = port_text.lstrip(":")
    else:
        host, separator, port_text = text.rpartition(":")
    if not separator or not host or not port_text.isdigit():
        return None
    port = int(port_text)
    if not 1 <= port <= 65535:
        return None
    return host, port


def _prompt_endpoint(candidate, non_interactive):
    parsed = _parse_endpoint(candidate)
    if parsed is not None:
        return parsed
    if non_interactive or not sys.stdin.isatty():
        return None
    print("외부 BBMD endpoint가 필요합니다 (host:port, 빈 입력은 TC-BIP-002/TC-BAC-020 BLOCKED): ",
          end="", flush=True)
    try:
        value = input().strip()
    except EOFError:
        return None
    return _parse_endpoint(value)


def resolve_inputs(args, selected):
    required = set()
    for spec in selected:
        required.update(TestEnvironment._case_dependencies(spec.tc_id, args.mode))
    interactive = not args.non_interactive

    gateway = _path_value(args.gateway_bin, GATEWAY_ROOT / "bacnet_ip_gateway")
    vport_default = ES_ROOT / "vport_release" / "vport"
    if not vport_default.is_file() and (ES_ROOT / "vport" / "vport").is_file():
        vport_default = ES_ROOT / "vport" / "vport"
    vport_test_default = ES_ROOT / "vport_release" / "vport_test"
    if not vport_test_default.is_file() and (ES_ROOT / "vport" / "vport_test").is_file():
        vport_test_default = ES_ROOT / "vport" / "vport_test"

    vport = _path_value(args.vport_bin, vport_default)
    vport_test = _path_value(args.vport_test_bin, vport_test_default)
    unit_test = _path_value(args.unit_test_bin, GATEWAY_ROOT / "verification" / "unit" / "run_tests")
    python_bin = _find_python_bacpypes3(args.python_bin)
    
    tpi = args.tpi.expanduser().resolve() if args.tpi else None
    if tpi is None:
        cdu_tpi = GATEWAY_ROOT / "model" / "cdu" / "[BTL]cdu.tpi"
        if cdu_tpi.is_file():
            tpi = cdu_tpi.resolve()

    if "gateway_bin" in required:
        gateway = _prompt_path("bacnet_ip_gateway", gateway, _is_executable, args.non_interactive)
    if "vport_bin" in required:
        vport = _prompt_path("vport", vport, _is_executable, args.non_interactive)
    if "vport_test_bin" in required:
        vport_test = _prompt_path("vport_test", vport_test, _is_executable, args.non_interactive)
    if "unit_test_bin" in required:
        unit_test = _prompt_path("verification/unit/run_tests", unit_test,
                                 _is_executable, args.non_interactive)
    if "tpi_path" in required and (tpi is None or not _is_file(tpi)):
        if interactive and sys.stdin.isatty():
            print("EPICS/TPI 파일이 필요합니다 (빈 입력은 BTL TC를 BLOCKED 처리합니다): ",
                  end="", flush=True)
            try:
                value = input().strip()
            except EOFError:
                value = ""
            tpi = Path(value).expanduser().resolve() if value else None
    if "python_bacpypes3" in required:
        python_bin = _prompt_path("bacpypes3용 Python", python_bin,
                                  _has_bacpypes3, args.non_interactive)

    bbmd_endpoint = None
    if "bbmd_endpoint" in required:
        if args.bbmd_endpoint:
            bbmd_endpoint = _parse_endpoint(args.bbmd_endpoint)
        elif args.bbmd_auto:
            bbmd_endpoint = ("127.0.0.1", 47809)
        else:
            bbmd_endpoint = _prompt_endpoint(args.bbmd_endpoint, args.non_interactive)

    policy = _path_value(args.performance_policy, VERIFICATION_ROOT / "scripts" / "performance_policy.json")
    if "performance_policy" in required:
        policy = _prompt_path("performance policy", policy, _is_file, args.non_interactive)
    return {
        "gateway_bin": gateway,
        "vport_bin": vport,
        "vport_test_bin": vport_test,
        "unit_test_bin": unit_test,
        "python_bin": python_bin,
        "tpi_path": tpi,
        "performance_policy": policy,
        "bbmd_endpoint": bbmd_endpoint,
    }


def approve_baseline(result_path: Path, policy_path: Path, scenario=None):
    payload = json.loads(result_path.read_text(encoding="utf-8"))
    results = payload.get("results", [])
    if not isinstance(results, list):
        raise ValueError("baseline JSON results must be a list")
    if scenario is None:
        scenario = payload.get("performance_scenario")
    if scenario is None:
        scenario = payload.get("metadata", {}).get("performance_scenario")
    performance_results = [
        result for result in results
        if isinstance(result, dict)
        and str(result.get("tc_id", "")).startswith("TC-PERF-")
    ]
    if scenario is None:
        if len(performance_results) != 1:
            raise ValueError(
                "baseline scenario is required when result JSON contains multiple "
                "or no PERF cases")
        scenario = performance_results[0].get("tc_id")

    selected_result = next(
        (result for result in performance_results if result.get("tc_id") == scenario),
        None,
    )
    if selected_result is not None:
        if selected_result.get("status") != "PASS":
            raise ValueError(
                f"baseline source {scenario} must be PASS, "
                f"got {selected_result.get('status')}")
        metrics = selected_result.get("details", {}).get("performance_metrics")
    else:
        # Keep compatibility with a standalone result payload that contains the
        # metrics at the top level, but never silently borrow another scenario's
        # metrics from a multi-case report.
        metrics = payload.get("performance_metrics") or payload.get("metrics")
        if metrics is None:
            metrics = payload.get("metadata", {}).get("performance_metrics")
        if performance_results:
            raise ValueError(f"baseline scenario {scenario} was not found in result JSON")
    required = ("success_rate", "throughput_rps", "p95_latency_ms", "p99_latency_ms")
    if not metrics or any(key not in metrics for key in required):
        raise ValueError(f"baseline JSON must contain metrics: {', '.join(required)}")
    now = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    if policy_path.is_file():
        policy = json.loads(policy_path.read_text(encoding="utf-8"))
    else:
        policy = {}
    scenario_policy = {
        "approved": True,
        "approved_at": now,
        "source_result": str(result_path),
        "baseline": {key: float(metrics[key]) for key in required},
        "limits": {
            "minimum_success_rate": min(1.0, float(metrics["success_rate"]) * 0.99),
            "minimum_throughput_rps": float(metrics["throughput_rps"]) * 0.90,
            "maximum_p95_latency_ms": float(metrics["p95_latency_ms"]) * 1.20,
            "maximum_p99_latency_ms": float(metrics["p99_latency_ms"]) * 1.20,
        },
        "note": "Reviewed baseline: throughput -10%, latency +20%, success-rate -1% tolerances.",
    }
    policy.setdefault("scenarios", {})[scenario] = scenario_policy
    policy["approved"] = bool(policy.get("approved", False))
    if scenario == "TC-PERF-001":
        policy.update({key: scenario_policy[key] for key in
                       ("approved_at", "source_result", "baseline", "limits", "note")})
        policy["approved"] = True
    policy_path.write_text(json.dumps(policy, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print(f"Approved performance baseline for {scenario}: {policy_path}")


def main():
    args = parse_args()
    if args.approve_baseline is not None:
        approve_baseline(args.approve_baseline.resolve(),
                         args.performance_policy.expanduser().resolve(),
                         args.baseline_scenario)
        return 0
    rtm = VERIFICATION_ROOT / "docs" / "specs" / "bacnet_gateway_rtm.csv"
    if not rtm.is_file():
        rtm = VERIFICATION_ROOT / "docs" / "bacnet_gateway_rtm.csv"
    if not rtm.is_file():
        rtm = None
    dependency_manifest = VERIFICATION_ROOT / "scripts" / "dependencies.json"
    json.loads(dependency_manifest.read_text(encoding="utf-8"))
    rtm_validated = validate_registry(rtm)
    suite = "all" if (args.patterns and args.suite == "quick") else args.suite
    selected = select_cases(suite, args.patterns)
    if not selected:
        print("No TC matched the selection.", file=sys.stderr)
        return 2
    if args.list:
        for spec in selected:
            print(f"{spec.tc_id}\t{spec.srs_id}\t{spec.title}")
        print(f"TOTAL={len(selected)}")
        return 0

    resolved = resolve_inputs(args, selected)

    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    output_dir = args.results_dir.resolve() / stamp
    # Managed runs contain the BACnet client and Gateway on the same host. Use
    # loopback by default so a container, host-only VM, or machine with a
    # non-bindable route does not make BACstac startup fail before a TC runs.
    # An explicit --ip still selects a real interface when that is required.
    if args.ip is not None:
        target_ip = args.ip
    elif args.mode == "managed":
        target_ip = "127.0.0.1"
    else:
        target_ip = default_local_ip()
    options = RunOptions(
        mode=args.mode,
        output_dir=output_dir,
        gateway_bin=resolved["gateway_bin"],
        vport_bin=resolved["vport_bin"],
        vport_test_bin=resolved["vport_test_bin"],
        unit_test_bin=resolved["unit_test_bin"],
        python_bin=resolved["python_bin"],
        tpi_path=resolved["tpi_path"],
        performance_policy=resolved["performance_policy"],
        bbmd_endpoint=resolved["bbmd_endpoint"],
        target_ip=target_ip,
        target_port=args.port,
        device_id=args.device_id,
        config_dir=(args.gateway_config_dir or args.config_dir).resolve() if (args.gateway_config_dir or args.config_dir) else None,
        gateway_config_dir=(args.gateway_config_dir or args.config_dir).resolve() if (args.gateway_config_dir or args.config_dir) else None,
        vport_config_path=args.vport_config_path.resolve() if args.vport_config_path else None,
        allow_build=args.allow_build,
        long_tests=args.long,
        allow_live_mutation=args.allow_live_mutation,
        non_interactive=args.non_interactive,
        expected_response_time_ms=args.response_time_ms,
        pics_doc=args.pics_doc.resolve() if args.pics_doc else None,
        soak=args.soak,
        soak_hours=args.soak_hours,
        soak_interval=args.soak_interval,
        ssh_enable=args.ssh_enable,
        ssh_user=args.ssh_user,
        ssh_port=args.ssh_port,
        ssh_pass=args.ssh_pass,
    )
    env = TestEnvironment(options)
    preflight = env.preflight(selected)
    metadata = {
        "suite": args.suite,
        "mode": args.mode,
        "target": f"{target_ip}:{args.port}",
        "device_id": args.device_id,
        "started_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "rtm_sha256": env.checksum(rtm) if rtm is not None else None,
        "rtm_validated": rtm_validated,
        "dependency_manifest_sha256": env.checksum(dependency_manifest),
        "selected_count": len(selected),
        "allow_live_mutation": args.allow_live_mutation,
        "capabilities": sorted(env.capabilities),
        "preflight": preflight,
        "preflight_path": str(output_dir / "preflight.json"),
    }
    results = []
    bbmd_proc = None
    if args.bbmd_auto and resolved.get("bbmd_endpoint") == ("127.0.0.1", 47809):
        bbmd_script = VERIFICATION_ROOT / "scripts" / "bbmd_server.py"
        if bbmd_script.is_file():
            try:
                bbmd_proc = subprocess.Popen(
                    [sys.executable, str(bbmd_script), "--ip", "127.0.0.1", "--port", "47809"],
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
                )
                time.sleep(0.3)
            except Exception as exc:
                print(f"[WARN] Failed to start embedded BBMD server: {exc}", file=sys.stderr)

    try:
        env.prepare()
        print(f"Result directory: {output_dir}")
        if env.options.mode == "live" and getattr(env.options, "ssh_enable", False):
            try:
                tc_cases._fetch_remote_configs(env)
            except Exception as exc:
                print(f"[WARN] 원격 설정 SSH 동기화: {exc}", file=sys.stderr)
        for index, spec in enumerate(selected, 1):
            print(f"[{index:02d}/{len(selected):02d}] {spec.tc_id} ...", flush=True)
            result = execute_case(spec, env)
            results.append(result)
            print(f"[{index:02d}/{len(selected):02d}] {spec.tc_id} => "
                  f"{result.status}"
                  + (f" - {result.message}" if result.message else ""), flush=True)
    finally:
        env.close()
        if bbmd_proc is not None:
            try:
                bbmd_proc.terminate()
                bbmd_proc.wait(timeout=1.0)
            except Exception:
                try:
                    bbmd_proc.kill()
                except Exception:
                    pass
    counts = write_reports(results, output_dir, metadata)
    print((output_dir / "summary.log").read_text(encoding="utf-8"))
    print(f"Step report: {output_dir / 'steps.log'}")
    print(f"Step CSV:    {output_dir / 'steps.csv'}")
    print(f"JSON report: {output_dir / 'results.json'}")
    if counts[FAIL]:
        return 1
    if counts[BLOCKED]:
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
