#!/usr/bin/env python3
"""Small dependency-free TC registry, executor, and evidence reporters."""

from __future__ import annotations

import csv
import json
import time
import traceback
import uuid
import xml.etree.ElementTree as ET
from collections import deque
from dataclasses import asdict, dataclass, field
import itertools
from pathlib import Path
import shutil
from typing import Callable, Dict, Iterable, List, Optional, Sequence, Tuple


PASS = "PASS"
FAIL = "FAIL"
SKIP = "SKIP"
BLOCKED = "BLOCKED"


class CaseFailure(AssertionError):
    pass


class CaseBlocked(RuntimeError):
    pass


class CaseSkipped(RuntimeError):
    pass


@dataclass(frozen=True)
class CaseSpec:
    tc_id: str
    srs_id: str
    title: str
    suites: Tuple[str, ...]
    timeout_sec: float
    handler: Callable
    requirements: Tuple[str, ...] = ()


@dataclass
class CaseResult:
    tc_id: str
    srs_id: str
    title: str
    status: str
    started_at: str
    duration_sec: float
    message: str = ""
    evidence: List[str] = field(default_factory=list)
    details: Dict = field(default_factory=dict)


REGISTRY: Dict[str, CaseSpec] = {}


def tc_case(tc_id: str, srs_id: str, title: str, suites: Sequence[str],
            timeout_sec: float = 30.0, requirements: Sequence[str] = ()):
    def decorate(handler):
        if tc_id in REGISTRY:
            raise RuntimeError(f"duplicate TC registration: {tc_id}")
        REGISTRY[tc_id] = CaseSpec(
            tc_id=tc_id,
            srs_id=srs_id,
            title=title,
            suites=tuple(suites),
            timeout_sec=float(timeout_sec),
            handler=handler,
            requirements=tuple(requirements),
        )
        return handler
    return decorate


def require(condition: bool, message: str):
    if not condition:
        raise CaseFailure(message)


def blocked(message: str):
    raise CaseBlocked(message)


def skipped(message: str):
    raise CaseSkipped(message)


def expected_tc_map(rtm_path: Path) -> Dict[str, str]:
    expected = {}
    with rtm_path.open("r", encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream):
            srs_id = row["Requirement_ID"]
            for field_name in row:
                if field_name.startswith("TC_"):
                    tc_id = row[field_name].strip()
                    if not tc_id:
                        continue
                    if tc_id in expected:
                        raise RuntimeError(f"RTM contains duplicate TC ID: {tc_id}")
                    expected[tc_id] = srs_id
    return expected


def validate_registry(rtm_path: Optional[Path]):
    """Validate the TC registry, cross-checking the RTM when it is available.

    Release packages built without the quality traceability artifacts do not ship
    `bacnet_gateway_rtm.csv`.  In that case the RTM cross-check is skipped and only
    the registry-internal invariant is enforced, so the test suite still runs.
    """
    if rtm_path is None or not rtm_path.is_file():
        if len(REGISTRY) < 87:
            raise RuntimeError(f"TC registry must contain at least 87 IDs, found {len(REGISTRY)}")
        return False

    expected = expected_tc_map(rtm_path)
    actual = set(REGISTRY)
    missing = sorted(set(expected) - actual)
    extra = sorted(actual - set(expected))
    mismatched = sorted(
        tc_id for tc_id in actual & set(expected)
        if REGISTRY[tc_id].srs_id != expected[tc_id]
    )
    if missing or extra or mismatched:
        raise RuntimeError(
            "TC registry/RTM mismatch: "
            f"missing={missing}, extra={extra}, srs_mismatch={mismatched}")
    if len(actual) != len(expected):
        raise RuntimeError(f"TC registry must contain {len(expected)} IDs, found {len(actual)}")
    return True


def select_cases(suite: str, patterns: Sequence[str]) -> List[CaseSpec]:
    selected = []
    for tc_id in sorted(REGISTRY):
        spec = REGISTRY[tc_id]
        if suite not in spec.suites:
            continue
        if patterns and not any(_matches(tc_id, pattern) for pattern in patterns):
            continue
        selected.append(spec)
    return selected


def _matches(tc_id: str, pattern: str) -> bool:
    for sub in pattern.split(","):
        sub = sub.strip()
        if not sub:
            continue
        if sub.endswith("*"):
            if tc_id.startswith(sub[:-1]):
                return True
        elif tc_id == sub:
            return True
    return False


class Evidence:
    def __init__(self):
        self.paths: List[str] = []
        self.details: Dict = {}

    def add(self, path):
        text = str(path)
        if text not in self.paths:
            self.paths.append(text)


def _step_text(value):
    if isinstance(value, str):
        return value
    try:
        return json.dumps(value, ensure_ascii=False, sort_keys=True, default=str)
    except (TypeError, ValueError):
        return repr(value)


def check_step(context, label, expected, actual, condition, message=None):
    """Record a visible expected/actual assertion and raise on failure."""
    diagnostic = (message or "") if not condition else ""
    context.record_step(label, expected, actual, passed=bool(condition), message=diagnostic)
    if not condition:
        raise CaseFailure(message or f"{label}: expected {expected!r}, actual {actual!r}")


def execute_case(spec: CaseSpec, context) -> CaseResult:
    started_wall = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    started = time.monotonic()
    evidence = Evidence()
    status = PASS
    message = ""
    try:
        context.current_case = spec
        context.evidence = evidence
        context.reset_steps()
        evidence.details["capabilities"] = sorted(context.capabilities)
        if spec.requirements:
            evidence.details["required_capabilities"] = list(spec.requirements)
            for capability in spec.requirements:
                context.require_capability(
                    capability,
                    f"{spec.tc_id} requires capability '{capability}'",
                )
        context.record_step("TC 시작", "handler 실행", spec.title)
        # Preflight is case-scoped: a missing optional executable or external
        # service blocks only the cases that declare that dependency.  The
        # context method is optional so the small framework remains reusable
        # by standalone unit tests.
        if hasattr(context, "require_preflight"):
            context.require_preflight(spec.tc_id)
        if hasattr(context, "select_case_config"):
            context.select_case_config(spec.tc_id)
        spec.handler(context)
    except CaseBlocked as exc:
        status, message = BLOCKED, str(exc)
        context.record_step("선행조건", "실행 가능", message, passed=False,
                            message=message, result_status=BLOCKED)
    except CaseSkipped as exc:
        status, message = SKIP, str(exc)
        context.record_step("TC 건너뜀", "SKIP 조건", message, passed=False,
                            message=message, result_status=SKIP)
    except (CaseFailure, AssertionError) as exc:
        status, message = FAIL, str(exc)
        context.record_step("TC 예외", "모든 assertion PASS", message, passed=False, message=message)
    except Exception as exc:
        status = FAIL
        message = f"{type(exc).__name__}: {exc}"
        evidence.details["traceback"] = traceback.format_exc()
        context.record_step("예외", "예외 없음", message, passed=False, message=message)
    finally:
        try:
            context.after_case(spec, status)
        except Exception as exc:
            if status == PASS:
                status = FAIL
                message = f"cleanup failed: {exc}"
            evidence.details["cleanup_error"] = str(exc)
    duration = time.monotonic() - started
    if duration > spec.timeout_sec and status == PASS:
        status = FAIL
        message = f"case exceeded limit: {duration:.3f}s > {spec.timeout_sec:.3f}s"
        context.record_step("실행시간", f"<= {spec.timeout_sec:.3f}s", duration,
                           passed=False, message=message)
    context.record_step("TC 판정", status, status, passed=(status == PASS),
                        message=message, result_status=status)
    context.current_case = None
    return CaseResult(
        tc_id=spec.tc_id,
        srs_id=spec.srs_id,
        title=spec.title,
        status=status,
        started_at=started_wall,
        duration_sec=round(duration, 6),
        message=message,
        evidence=evidence.paths,
        details=evidence.details,
    )


def _allure_status(status: str) -> str:
    if status == PASS:
        return "passed"
    if status == FAIL:
        return "failed"
    if status == BLOCKED:
        return "broken"
    return "skipped"


def write_allure_results(results: Sequence[CaseResult], output_dir: Path, metadata: Dict):
    now_ms = int(time.time() * 1000)
    for result in results:
        tc_uuid = str(uuid.uuid4())
        duration_ms = max(int(result.duration_sec * 1000), 1)
        start_ms = now_ms
        stop_ms = now_ms + duration_ms

        allure_steps = []
        for index, item in enumerate(result.details.get("steps", []), 1):
            step_number = item.get("step", index)
            step_status = _allure_status(item.get("status", PASS))
            params = [
                {"name": "expected", "value": _step_text(item.get("expected"))},
                {"name": "actual", "value": _step_text(item.get("actual"))},
            ]
            step_obj = {
                "name": f"STEP-{step_number:02d}: {item.get('label', '')}",
                "status": step_status,
                "stage": "finished",
                "start": start_ms,
                "stop": stop_ms,
                "parameters": params,
            }
            if item.get("message"):
                step_obj["statusDetails"] = {"message": item["message"]}
            allure_steps.append(step_obj)

        attachments = []
        for ev in result.evidence:
            ev_path = Path(ev)
            if ev_path.is_file():
                mime = "application/json" if ev_path.suffix == ".json" else "text/plain"
                attachments.append({
                    "name": ev_path.name,
                    "source": ev_path.name,
                    "type": mime,
                })

        allure_case = {
            "uuid": tc_uuid,
            "historyId": result.tc_id,
            "name": f"{result.tc_id}: {result.title}",
            "fullName": f"bacnet_ip_gateway.{result.srs_id}.{result.tc_id}",
            "status": _allure_status(result.status),
            "stage": "finished",
            "start": start_ms,
            "stop": stop_ms,
            "labels": [
                {"name": "parentSuite", "value": "BACnet/IP Gateway Verification"},
                {"name": "suite", "value": result.srs_id},
                {"name": "feature", "value": result.srs_id},
                {"name": "package", "value": "verification.scripts"},
            ],
            "steps": allure_steps,
            "attachments": attachments,
        }
        if result.message:
            allure_case["statusDetails"] = {"message": result.message}

        case_file = output_dir / f"{result.tc_id.lower()}-result.json"
        case_file.write_text(json.dumps(allure_case, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def convert_existing_results_to_allure(results_dir: Path):
    results_json = results_dir / "results.json"
    if not results_json.is_file():
        return
    try:
        data = json.loads(results_json.read_text(encoding="utf-8"))
        metadata = data.get("metadata", {})
        case_results = []
        for item in data.get("results", []):
            case_results.append(CaseResult(
                tc_id=item.get("tc_id", ""),
                srs_id=item.get("srs_id", ""),
                title=item.get("title", ""),
                status=item.get("status", PASS),
                started_at=item.get("started_at", ""),
                duration_sec=float(item.get("duration_sec", 0.0)),
                message=item.get("message", ""),
                evidence=item.get("evidence", []),
                details=item.get("details", {}),
            ))
        write_allure_results(case_results, results_dir, metadata)
    except Exception:
        pass


def prepare_allure_results(source_dir: Path, out_results_dir: Path):
    """Convert results.json to allure results and copy evidence with smart size optimization."""
    has_results = any(source_dir.glob("*-result.json"))
    if not has_results:
        convert_existing_results_to_allure(source_dir)

    out_results_dir.mkdir(parents=True, exist_ok=True)
    max_preview_bytes = 150 * 1024  # 150KB limit for full inline preview in web viewer

    copied_sizes = {}
    for ext in ("*.log", "*.jsonl", "*.txt", "*.csv"):
        for src in source_dir.glob(ext):
            dst = out_results_dir / src.name
            try:
                size = src.stat().st_size
                if size <= max_preview_bytes:
                    shutil.copy2(src, dst)
                    copied_sizes[src.name] = size
                else:
                    size_mb = size / (1024 * 1024)
                    with open(src, "r", encoding="utf-8", errors="ignore") as sf:
                        head = list(itertools.islice(sf, 100))
                        tail = list(deque(sf, maxlen=100))

                    banner = (
                        f"\n\n=======================================================\n"
                        f" [대용량 증적 미리보기 안내]\n"
                        f" - 원본 파일명 : {src.name}\n"
                        f" - 원본 크기   : {size_mb:.1f} MB\n"
                        f" - 안내        : 웹 브라우저 속도 최적화를 위해 처음 100줄과\n"
                        f"                 마지막 100줄을 표시합니다.\n"
                        f"                 전체 원본 파일은 우측 상단의 [Download] 버튼을 눌러 다운로드하십시오.\n"
                        f"=======================================================\n\n"
                    )
                    with open(dst, "w", encoding="utf-8") as df:
                        df.writelines(head)
                        df.write(banner)
                        df.writelines(tail)
                    copied_sizes[src.name] = dst.stat().st_size
            except Exception:
                try:
                    shutil.copy2(src, dst)
                except Exception:
                    pass

    # Copy *-result.json and ensure attachments have lightweight mime and sizes
    for f in source_dir.glob("*-result.json"):
        dst = out_results_dir / f.name
        try:
            content = f.read_text(encoding="utf-8")
            data = json.loads(content)
            for att in data.get("attachments", []):
                src_name = att.get("source", "")
                if src_name.endswith(".jsonl") or att.get("name", "").endswith(".jsonl"):
                    att["type"] = "text/plain"
                if src_name in copied_sizes:
                    att["size"] = copied_sizes[src_name]
            dst.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        except Exception:
            try:
                shutil.copy2(f, dst)
            except Exception:
                pass


def patch_allure_report(report_dir: Path, source_results_dir: Optional[Path] = None):
    """Patch Allure static assets: replace tooltip with Download, link full raw attachments, and ensure instant preview speed."""
    import os
    import re

    # 1. Symlink full raw evidence files so clicking Download fetches the 100% complete original file
    if source_results_dir and source_results_dir.is_dir():
        raw_dir = report_dir / "raw_attachments"
        raw_dir.mkdir(parents=True, exist_ok=True)
        for f in source_results_dir.iterdir():
            if f.is_file():
                link_target = raw_dir / f.name
                try:
                    if link_target.exists() or link_target.is_symlink():
                        link_target.unlink()
                    os.symlink(f.resolve(), link_target)
                except Exception:
                    try:
                        shutil.copy2(f, link_target)
                    except Exception:
                        pass

    # 2. Patch static JavaScript bundles
    for p in report_dir.rglob("*.js"):
        try:
            txt = p.read_text(encoding="utf-8")
            modified = False
            if "Open attachment in new tab" in txt:
                txt = txt.replace("Open attachment in new tab", "Download")
                modified = True

            # Lower syntax highlighting limit from 2MB to 50KB to prevent browser freeze on preview
            if "2097152" in txt:
                txt = txt.replace("2097152", "51200")
                modified = True

            if modified:
                p.write_text(txt, encoding="utf-8")
        except Exception:
            pass

    # 3. Patch index.html with styles and capturing download interceptor
    index_file = report_dir / "index.html"
    if index_file.is_file():
        try:
            html = index_file.read_text(encoding="utf-8")
            patch_script = """<style>
/* High performance attachment preview styles: avoid layout reflow lag */
.attachment-preview__code,
pre.attachment-preview,
.attachment-row__content pre,
.attachment__text-container pre {
    white-space: pre !important;
    word-break: normal !important;
    overflow-x: auto !important;
    max-height: 550px !important;
}
</style>
<script>
(function() {
    // Intercept download button clicks in the CAPTURING phase.
    // This stops event propagation before Allure's onAttachmentClick can ever fire,
    // completely eliminating the flickering toggle/expansion.
    ['click', 'mousedown', 'pointerdown'].forEach(function(evtName) {
        document.addEventListener(evtName, function(e) {
            var dlTarget = e.target.closest('.attachment-row__link, [data-download], .attachment-preview__download');
            if (!dlTarget) return;

            // Stop propagation immediately so attachment row NEVER expands/collapses
            e.stopPropagation();
            e.stopImmediatePropagation();

            if (evtName === 'click') {
                e.preventDefault();

                var row = dlTarget.closest('.attachment-row');
                var nameEl = row ? (row.querySelector('.attachment-row__name') || row.querySelector('[class*="name"]')) : null;
                var filename = (nameEl ? nameEl.textContent.trim() : '') ||
                               (dlTarget.dataset.download ? dlTarget.dataset.download.split('/').pop() : '') ||
                               (dlTarget.getAttribute('download') || '') ||
                               'attachment';

                var rawUrl = 'raw_attachments/' + encodeURIComponent(filename);

                // Download the full, original file directly
                var a = document.createElement('a');
                a.style.display = 'none';
                a.href = rawUrl;
                a.download = filename;
                document.body.appendChild(a);
                a.click();
                setTimeout(function() {
                    if (a.parentNode) a.parentNode.removeChild(a);
                }, 300);
            }
        }, true);
    });

    // Ensure hover tooltip says 'Download'
    document.addEventListener('mouseover', function(e) {
        var el = e.target.closest('[data-tooltip="Open attachment in new tab"]');
        if (el) el.setAttribute('data-tooltip', 'Download');
    }, true);
})();
</script>"""
            if "</head>" in html:
                html = html.replace("</head>", f"{patch_script}\n</head>")
                index_file.write_text(html, encoding="utf-8")
        except Exception:
            pass


def write_reports(results: Iterable[CaseResult], output_dir: Path, metadata: Dict):
    result_list = list(results)
    output_dir.mkdir(parents=True, exist_ok=True)
    counts = {status: 0 for status in (PASS, FAIL, SKIP, BLOCKED)}
    step_counts = {status: 0 for status in (PASS, FAIL, SKIP, BLOCKED)}
    for result in result_list:
        counts[result.status] += 1
        steps = result.details.get("steps", [])
        per_case = {status: 0 for status in (PASS, FAIL, SKIP, BLOCKED)}
        for item in steps:
            item_status = item.get("status", FAIL)
            if item_status not in step_counts:
                item_status = FAIL
            step_counts[item_status] += 1
            per_case[item_status] += 1
        result.details["step_summary"] = {
            "total": len(steps),
            **per_case,
        }

    payload = {
        "metadata": metadata,
        "summary": {"total": len(result_list), **counts,
                     "steps": {"total": sum(step_counts.values()), **step_counts}},
        "results": [asdict(result) for result in result_list],
    }
    (output_dir / "results.json").write_text(
        json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")

    write_allure_results(result_list, output_dir, metadata)

    suite = ET.Element("testsuite", {
        "name": "bacnet_ip_gateway",
        "tests": str(len(result_list)),
        "failures": str(counts[FAIL]),
        "errors": str(counts[BLOCKED]),
        "skipped": str(counts[SKIP]),
        "time": f"{sum(result.duration_sec for result in result_list):.6f}",
    })
    properties = ET.SubElement(suite, "properties")
    for key, value in sorted(metadata.items()):
        ET.SubElement(properties, "property", {"name": str(key), "value": str(value)})
    for result in result_list:
        case = ET.SubElement(suite, "testcase", {
            "classname": result.srs_id,
            "name": result.tc_id,
            "time": f"{result.duration_sec:.6f}",
        })
        if result.status == FAIL:
            ET.SubElement(case, "failure", {"message": result.message}).text = result.message
        elif result.status == BLOCKED:
            ET.SubElement(case, "error", {"message": result.message}).text = result.message
        elif result.status == SKIP:
            ET.SubElement(case, "skipped", {"message": result.message})
        if result.evidence or result.details:
            ET.SubElement(case, "system-out").text = json.dumps({
                "evidence": result.evidence,
                "details": result.details,
            }, ensure_ascii=False, indent=2)
    ET.ElementTree(suite).write(output_dir / "junit.xml", encoding="utf-8", xml_declaration=True)

    lines = [
        "BACnet/IP Gateway TC Summary",
        "=" * 72,
    ]
    for result in result_list:
        suffix = f" - {result.message}" if result.message else ""
        step_summary = result.details.get("step_summary", {})
        step_text = (f" steps={step_summary.get('total', 0)}"
                     f"(PASS:{step_summary.get(PASS, 0)}"
                     f" FAIL:{step_summary.get(FAIL, 0)}"
                     f" BLOCKED:{step_summary.get(BLOCKED, 0)}"
                     f" SKIP:{step_summary.get(SKIP, 0)})")
        lines.append(f"[{result.status:7}] {result.tc_id} {result.title}"
                     f" ({result.duration_sec:.3f}s){step_text}{suffix}")
    lines.extend([
        "=" * 72,
        " ".join(f"{key}={value}" for key, value in ({"TOTAL": len(result_list), **counts}).items()),
        "STEP_TOTAL=" + str(sum(step_counts.values())) + " "
        + " ".join(f"STEP_{key}={value}" for key, value in step_counts.items()),
    ])
    (output_dir / "summary.log").write_text("\n".join(lines) + "\n", encoding="utf-8")

    step_lines = [
        "BACnet/IP Gateway TC Step Report",
        "=" * 96,
        "status | tc_id | step | label | expected | actual | message",
    ]
    for result in result_list:
        step_lines.append(f"[{result.status:7}] {result.tc_id} {result.title}")
        for index, item in enumerate(result.details.get("steps", []), 1):
            step_number = item.get("step", index)
            step_lines.append(
                f"  [{item.get('status', '?'):7}] STEP-{step_number:02d} {item.get('label', '')}"
                f" | expected={_step_text(item.get('expected'))}"
                f" | actual={_step_text(item.get('actual'))}"
                + (f" | {item['message']}" if item.get("message") else ""))
        step_lines.append("")
    (output_dir / "steps.log").write_text("\n".join(step_lines), encoding="utf-8")

    # utf-8-sig(BOM): 한글 Windows Excel이 CP949로 오인해 한글이 깨지는 것을 방지
    with (output_dir / "steps.csv").open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(("tc_id", "srs_id", "tc_status", "step", "status", "label",
                         "expected", "actual", "message"))
        for result in result_list:
            for index, item in enumerate(result.details.get("steps", []), 1):
                writer.writerow((
                    result.tc_id,
                    result.srs_id,
                    result.status,
                    item.get("step", index),
                    item.get("status", ""),
                    item.get("label", ""),
                    _step_text(item.get("expected")),
                    _step_text(item.get("actual")),
                    item.get("message", ""),
                ))
    return counts
