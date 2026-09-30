"""Run selected real-asset ending checks and compare host/sanitized results.

The Japanese Data directory is read-only. Reports and complete command logs
go to a new build/validation directory for each invocation. These are host
checks; they do not assert a Windows frame match or Switch validation.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SUITES = {
    "cpu": [
        ("secondary-presentation", "ending-secondary-presentation-probe", []),
        ("secondary-controller", "ending-secondary-controller-probe", []),
    ],
    "session": [("secondary-session", "ending-secondary-session-probe", [])],
    "stage": [("stage-reload", "ending-stage-reload-probe", [])],
    "selected-session": [("selected-session", "ending-selected-session-probe", [])],
    "node-bindings": [("node-bindings", "ending-node-bindings-probe", [])],
    "gallery-session": [("gallery-session", "ending-gallery-session-probe", []),
                        ("gallery-mixed", "ending-gallery-session-probe", ["0", "0", "1"])],
    "auxiliary-assets": [("auxiliary-assets", "ending-4d39e6-probe", [])],
    "selected-adapters": [("selected-adapters", "ending-selected-adapters-probe", [])],
    "final-image": [("final-image", "ending-final-image-probe", [])],
    "story": [("story-reload", "ending-story-reload-probe", [])],
    "third-cpu": [("third-scene", "ending-tertiary-scene-probe", [])],
    "third-session": [("third-session", "ending-tertiary-session-probe", [])],
    "third-render": [
        ("third-render", "ending-tertiary-render-probe", []),
        ("dual-bom-render", "bom-dual-render-probe", []),
    ],
    "application": [
        ("normal-application", "ending-exit-probe", ["{output}"]),
        ("third-application", "ending-exit-probe", ["{output}", "--third"]),
    ],
    "gallery-app": [("gallery-application", "ending-gallery-app-probe", ["{output}"])],
    "capture-lifecycle": [
        ("capture-lifecycle", "capture-lifecycle-probe", ["{output}"]),
        ("screenshot", "screenshot-probe", ["{output}"]),
        ("play-flow", "play-flow-probe", ["{output}", "{output}.rgba"]),
    ],
    "special-session": [("special-session", "special-session-probe", ["{output}"])],
    "special-ui-media": [
        ("special-ui-media", "special-ui-media-probe", ["{output}"]),
        ("screenshot", "screenshot-probe", ["{output}"]),
        ("capture-render", "capture-render-probe", []),
    ],
    "render": [
        ("normal-render", "ending-render-probe", []),
        ("normal-dual-render", "ending-render-probe", ["--special"]),
        ("normal-retained-render", "ending-render-probe", ["--special", "--retained"]),
        ("secondary-render", "ending-secondary-render-probe", []),
        ("secondary-retained-render", "ending-secondary-render-probe", ["--retained"]),
    ],
}
CPU_SUITES = {"cpu", "third-cpu"}


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def source_manifest() -> dict[str, str]:
    paths = [ROOT / "CMakeLists.txt", ROOT / "config/dependencies.lock.json", Path(__file__).resolve()]
    paths.extend(p for p in (ROOT / "runtime").rglob("*") if p.is_file())
    paths.extend((ROOT / "tools").glob("ending_*probe.c"))
    paths.extend((ROOT / "tools").glob("ending_*.h"))
    paths.extend(ROOT / "tools" / name for name in [
        "special_session_probe.c", "special_ui_media_probe.c", "screenshot_probe.c", "capture_render_probe.c",
        "capture_lifecycle_probe.c", "play_flow_probe.c"])
    paths.extend(ROOT / "tests" / name for name in [
        "original_ending_selected_session_oracle.py", "original_prop_route_oracle.py",
        "original_matrix_oracle.py", "model_binding.py",
        "original_ending_auxiliary_targets_oracle.py", "test_ending_4d39e6_config.c"])
    return {str(p.relative_to(ROOT)): digest(p) for p in sorted(paths)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("data", type=Path)
    parser.add_argument("--suite", action="append", choices=tuple(SUITES))
    parser.add_argument("--reuse-host-build", action="store_true",
                        help="test-host.sh has already built all host targets")
    parser.add_argument("--original-exe", type=Path, default=os.environ.get("BK3_ORIGINAL_EXE"),
                        help="also compare selected-session loader state to the pinned EXE")
    parser.add_argument("--oracle-python", default=os.environ.get("BK3_TEST_PYTHON", "local/venv/bin/python"))
    parser.add_argument("--jobs", type=int, default=int(os.environ.get("BK_BUILD_JOBS", "8")))
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    data = args.data.resolve()
    if not data.is_dir() or not (data / "bk3_09.pp").is_file():
        parser.error("data must be an extracted Japanese Data directory with bk3_09.pp")
    suites = list(dict.fromkeys(args.suite or SUITES))
    parent = ROOT / "build/validation"
    parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="ending-runtime-", dir=parent))
    manifest = source_manifest()
    packs = set(["bk3_00", "bk3_02", "bk3_15"] if "special-ui-media" in suites else [])
    if "capture-lifecycle" in suites:
        packs.update(["bk3_00", "bk3_01", "bk3_02", "bk3_03", "bk3_04",
                      "bk3_05", "bk3_06", "bk3_07", "bk3_15", "bk3_16", "bk3_20"])
    if "special-session" in suites:
        packs.update(["bk3_00", "bk3_01", "bk3_02", "bk3_03", "bk3_04", "bk3_06",
                      "bk3_08", "bk3_09", "bk3_10", "bk3_11", "bk3_12", "bk3_13",
                      "bk3_14", "bk3_15", "bk3_18", "fambom"])
    archives = {pack: digest(data / (pack + ".pp")) for pack in sorted(packs)}
    report = {
        "started_at": datetime.now(timezone.utc).isoformat(),
        "data": str(data), "suites": suites, "reuse_host_build": args.reuse_host_build,
        "passed": False, "scope": "Host real-asset CPU/Vulkan and ASan/UBSan comparisons; no Switch or full-game acceptance",
        "source_sha256": manifest, "archive_sha256": archives, "commands": [], "checks": [],
    }
    print(f"Ending validation logs: {output}", flush=True)
    env = {**os.environ, "ASAN_OPTIONS": "detect_leaks=0", "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"}
    env.pop("BK_ENDING_STORY_TRACE", None)

    def execute(name: str, command: list[str], check: bool = False) -> str:
        path = output / (name + ".log")
        entry = {"name": name, "argv": command, "log": str(path.relative_to(ROOT))}
        report["commands"].append(entry)
        print(f"Running {name}", flush=True)
        with path.open("w") as log:
            process = subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
        entry["exit_code"] = process.returncode
        text = path.read_text(errors="replace")
        if process.returncode or "runtime error:" in text or "ERROR: AddressSanitizer" in text:
            print("\n".join(text.splitlines()[-20:]), flush=True)
            raise RuntimeError(f"{name} failed; exit {process.returncode}; see {path}")
        if not check:
            return ""
        summaries = [line for line in text.splitlines() if line.startswith("PASS ")]
        if not summaries:
            raise RuntimeError(f"{name} exited successfully without its final coverage assertion")
        print(summaries[-1], flush=True)
        entry["summary"] = summaries[-1]
        return summaries[-1]

    try:
        jobs = [job for suite in suites for job in SUITES[suite]]
        host_targets = sorted({target for _, target, _ in jobs})
        cpu_targets = sorted({target for suite in suites if suite in CPU_SUITES
                              for _, target, _ in SUITES[suite]})
        gpu_targets = sorted(set(host_targets) - set(cpu_targets))
        if not args.reuse_host_build:
            execute("configure-host", ["cmake", "-S", ".", "-B", "build", "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
                                      "-DBK_WITH_VULKAN=ON", "-DBK_BUILD_TESTS=ON", "-DBK_SANITIZE=OFF"])
            execute("build-host", ["cmake", "--build", "build", "--target", *host_targets, "--parallel", str(args.jobs)])
        for label, folder, targets, vulkan in [
            ("asan-cpu", "build/asan", cpu_targets, "OFF"),
            ("asan-vulkan", "build/asan-static", gpu_targets, "ON"),
        ]:
            if targets:
                execute("configure-" + label, ["cmake", "-S", ".", "-B", folder, "-DCMAKE_BUILD_TYPE=Debug",
                                               "-DBK_SANITIZE=ON", "-DBK_BUILD_TESTS=ON", "-DBK_WITH_VULKAN=" + vulkan])
                execute("build-" + label, ["cmake", "--build", folder, "--target", *targets, "--parallel", str(args.jobs)])
        for suite in suites:
            for name, target, flags in SUITES[suite]:
                comparison = {"name": name, "suite": suite, "passed": False, "runs": []}
                report["checks"].append(comparison)
                summaries = []
                for label, folder in [("host", "build"), ("asan", "build/asan" if suite in CPU_SUITES else "build/asan-static")]:
                    binary = ROOT / folder / target
                    arguments = [flag.replace("{output}", str(output / (name + "-" + label + "-files")))
                                 for flag in flags]
                    command = [str(binary), str(data), *arguments]
                    summaries.append(execute(name + "-" + label, command, check=True))
                    comparison["runs"].append({"kind": label, "binary_sha256": digest(binary), "summary": summaries[-1]})
                if summaries[0] != summaries[1]:
                    raise RuntimeError(f"{name}: host and sanitized coverage/hashes differ")
                comparison["passed"] = True
        if "selected-session" in suites and args.original_exe:
            native_report = output / "selected-session-native.json"
            execute("selected-session-native", [args.oracle_python,
                "tests/original_ending_selected_session_oracle.py", str(args.original_exe),
                str(output / "selected-session-host.log"),
                str(output / "selected-session-asan.log"), "--output", str(native_report)])
            native = json.loads(native_report.read_text())
            if not native.get("passed"):
                raise RuntimeError("selected-session native scalar comparison failed")
            report["selected_session_native"] = native
        if "auxiliary-assets" in suites and args.original_exe:
            native_library = output / "auxiliary-targets.dylib"
            execute("build-auxiliary-targets-native", [os.environ.get("CC", "cc"),
                "-dynamiclib" if sys.platform == "darwin" else "-shared", "-fPIC",
                "-Iruntime", "runtime/game/ending_4d39e6_config.c", "-o", str(native_library)])
            native_report = output / "auxiliary-targets-native.json"
            execute("auxiliary-targets-native", [args.oracle_python,
                "tests/original_ending_auxiliary_targets_oracle.py", str(args.original_exe),
                str(native_library), "--logs", str(output / "auxiliary-assets-host.log"),
                str(output / "auxiliary-assets-asan.log"), "--output", str(native_report)])
            native = json.loads(native_report.read_text())
            if not native.get("passed"):
                raise RuntimeError("auxiliary targets native comparison failed")
            report["auxiliary_targets_native"] = native
        if source_manifest() != manifest:
            raise RuntimeError("source changed during validation; results cannot identify one source state")
        if any(digest(data / (pack + ".pp")) != value for pack, value in archives.items()):
            raise RuntimeError("archive changed during validation")
        report["passed"] = True
    except (OSError, RuntimeError) as error:
        report["error"] = str(error)
        print(str(error), flush=True)
    finally:
        report["finished_at"] = datetime.now(timezone.utc).isoformat()
        target = output / "result.json"
        target.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
        print(f"Ending validation report: {target}", flush=True)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
