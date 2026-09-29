"""Pair selected-stage original asset/configuration and CPU lifetime checks.

Build both ordinary and ASan/UBSan libraries, freeze sources and archive
identities, require all 30 configurations and 63 fresh/retained/replacement
asset entries, and retain exact commands/results. This is not the complete
4D1025 loader, a rendered ending, leak detection, or Switch acceptance.
"""
from __future__ import annotations
import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading

from check_bom_dual_assets import ROOT, digest, manifest

SUITES = ['configuration', 'assets', 'effects', 'control', 'action', 'ui', 'motion', 'motion-assets',
          'menu-regression', 'presentation', 'scene-presentation', 'auxiliary', 'failures']
PACKS = ['bk3_10', 'bk3_13', 'bk3_03', 'bk3_04', 'fambom']


def selected_manifest():
    sources = manifest()
    probe = ROOT / 'tools/ending_selected_presentation_probe.c'
    sources[str(probe.relative_to(ROOT))] = digest(probe)
    return sources


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('data', type=Path)
    parser.add_argument('--suites', default=','.join(SUITES))
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    suites = args.suites.split(',')
    if (args.jobs < 1 or not suites or len(set(suites)) != len(suites)
            or any(suite not in SUITES for suite in suites)):
        parser.error('jobs must be positive; suites must be distinct: ' + ','.join(SUITES))
    host_python = ROOT / 'local/venv/bin/python'
    asan_python = ROOT / 'build/asan/ending-oracle-python'
    packs = PACKS + (['bk3_02', 'bk3_06'] if 'scene-presentation' in suites else [])
    archives = [args.data / (pack + '.pp') for pack in packs]
    for path in [args.exe, host_python, asan_python, *archives]:
        if not path.is_file():
            parser.error('required input/launcher missing: ' + str(path))
    expected_exe = 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    if digest(args.exe) != expected_exe:
        parser.error('original addresses require the pinned EXE')
    parent = ROOT / 'build/validation'
    parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='ending-selected-assets-', dir=parent))
    sources = selected_manifest()
    inputs = {path.name: digest(path) for path in archives}
    report = dict(passed=False, started_at=datetime.now(timezone.utc).isoformat(),
                  exe_sha256=expected_exe, source_sha256=sources, archive_sha256=inputs,
                  commands=[], checks=[], leak_detection=False, full_loader=False,
                  gpu_or_device_validation=False, scope=__doc__)
    lock = threading.Lock()
    print('Selected assets validation:', output, flush=True)

    def execute(name, command, env=None):
        log_path = output / (name + '.log')
        entry = dict(name=name, argv=[str(value) for value in command],
                     log=str(log_path.relative_to(ROOT)))
        with lock:
            report['commands'].append(entry)
            print('Running', name, flush=True)
        with log_path.open('w') as log:
            process = subprocess.run(command, cwd=ROOT, env=env,
                                     stdout=log, stderr=subprocess.STDOUT)
        entry['exit_code'] = process.returncode
        text = log_path.read_text(errors='replace')
        if (process.returncode or 'runtime error:' in text
                or 'ERROR: AddressSanitizer' in text):
            with lock:
                print('\n'.join(text.splitlines()[-18:]), flush=True)
            raise RuntimeError(f'{name} failed with exit {process.returncode}')
        return text

    try:
        for mode, folder, sanitized, vulkan, kind in [
            ('host', 'build', 'OFF', 'ON', 'RelWithDebInfo'),
            ('asan', 'build/asan', 'ON', 'OFF', 'Debug')]:
            execute('configure-' + mode, ['cmake', '-S', '.', '-B', folder,
                '-DBK_BUILD_TESTS=ON', '-DBK_SANITIZE=' + sanitized,
                '-DBK_WITH_VULKAN=' + vulkan, '-DCMAKE_BUILD_TYPE=' + kind])
            targets = ['model-test']
            if 'scene-presentation' in suites:
                targets.append('ending-selected-presentation-probe')
            execute('build-' + mode, ['cmake', '--build', folder, '--target',
                                     *targets, '--parallel', str(args.jobs)])
        site = execute('python-site', [str(host_python), '-c',
            'import sysconfig; print(sysconfig.get_paths()["purelib"])']).strip()
        if not Path(site).is_dir():
            raise RuntimeError('host Python returned no package directory')

        def run(suite, mode):
            folder, python = ('build', host_python) if mode == 'host' else ('build/asan', asan_python)
            env = dict(os.environ)
            env.update(PYTHONPATH=os.pathsep.join([str(ROOT / 'tests'), str(ROOT / 'tools'), site]),
                       BK3_BUILD_DIR=str(ROOT / folder), ASAN_OPTIONS='detect_leaks=0',
                       UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',
                       PYTHONFAULTHANDLER='1')
            name = suite + '-' + mode
            result_path = output / (name + '.json')
            script = {'configuration': 'original_ending_selected_config_oracle.py',
                      'assets': 'original_ending_selected_assets_oracle.py',
                      'effects': 'original_ending_selected_assets_oracle.py',
                      'motion-assets': 'original_ending_selected_assets_oracle.py',
                      'control': 'original_ending_selected_control_oracle.py',
                      'action': 'original_ending_selected_action_oracle.py',
                      'ui': 'original_ending_selected_ui_oracle.py',
                      'motion': 'original_ending_selected_motion_oracle.py',
                      'menu-regression': 'original_ending_secondary_ui_oracle.py',
                      'presentation': 'original_ending_selected_presentation_oracle.py',
                      'scene-presentation': 'check_ending_selected_presentation_scene.py',
                      'auxiliary': 'original_ending_selected_auxiliary_oracle.py',
                      'failures': 'ending_selected_failures.py'}[suite]
            command = [str(python), str(ROOT / 'tests' / script)]
            if suite != 'failures':
                command.append(str(args.exe.resolve()))
            if suite in ['assets', 'effects', 'motion-assets', 'scene-presentation', 'failures']:
                command.append(str(args.data.resolve()))
            if suite == 'effects':
                command += ['--steps', '0', '--mixed-effects', '48']
            if suite == 'motion-assets':
                command += ['--steps', '0', '--motion-controls', '48']
            command += ['--report' if suite == 'menu-regression' else '--output', str(result_path)]
            text = execute(name, command, env)
            result = json.loads(result_path.read_text())
            if not result.get('passed'):
                raise RuntimeError(name + ' omitted its pass')
            if suite != 'failures' and result.get('exe_sha256') != expected_exe:
                raise RuntimeError(name + ' omitted the pinned EXE identity')
            if suite == 'configuration':
                expected = {(g, v, s) for g in range(5) for v in range(2) for s in range(3)}
                records = result.get('profiles', [])
                if (len(records) != 30 or {(r['group'], r['variant'], r['selection'])
                        for r in records} != expected or result.get('native_background_branches') != 1370
                        or result.get('invalid_rejections') != 9):
                    raise RuntimeError(name + ' omitted a configuration/background branch')
            elif suite in ['assets', 'effects', 'motion-assets']:
                expected = {(g, v, s, mode) for g in range(5) for v in range(2)
                            for s in range(3) for mode in ['fresh', 'retained']}
                expected |= {(1, 0, s, 'replace') for s in range(3)}
                records = result.get('records', [])
                if (len(records) != 63 or {(r['group'], r['variant'], r['selection'], r['mode'])
                        for r in records} != expected or any(r['steps'] != (24 if suite == 'assets' else 0) or
                            r.get('native_initial_sample_windows', 0) < 1 for r in records)):
                    raise RuntimeError(name + ' omitted an actual resource/reload entry')
                counters = result.get('counters', {})
                expected_counts = dict(face_initializations=63, eye_selections=63,
                        rejections=189, dropped_previous_background_owners=33)
                expected_counts.update(dict(effects=1638, requests=126,
                    hidden_updates=252, zero_seconds=567) if suite == 'assets'
                    else dict(effects=3402, requests=0, hidden_updates=0, zero_seconds=0))
                if suite == 'motion-assets':
                    expected_counts['effects'] = 6678
                for key, expected_count in expected_counts.items():
                    if counters.get(key) != expected_count:
                        raise RuntimeError(name + ' omitted required ' + key + ' coverage')
                if suite == 'effects':
                    for record in records:
                        mixed = record.get('mixed_effects', {})
                        required = dict(frames=48, hidden_frames=4, zero_seconds=24,
                                        requests=4, clock_edits=4, material_edits=4, rejections=4)
                        if (any(mixed.get(key) != count for key, count in required.items())
                                or not mixed.get('native_sampling')
                                or not mixed.get('blend_samples')
                                or not mixed.get('held_plain_after_blend')
                                or mixed.get('by_mode') != {'-1':24, '0':8, '1':12, '2':4}):
                            raise RuntimeError(name + ' omitted a mixed effect/clock/cache path')
                if suite == 'motion-assets':
                    for record in records:
                        motion = record.get('motion_controls', {})
                        required = dict(frames=48, pointer_edits=24, drag_edits=24, requests=6,
                                        hidden_frames=8, snapshots=104, rejections=8,
                                        preserved_elapsed=48)
                        if (any(motion.get(key) != count for key, count in required.items())
                                or not motion.get('native_sampling')
                                or motion.get('by_mode') != {'0': 16, '1': 16, '2': 16}):
                            raise RuntimeError(name + ' omitted a real motion descriptor/cache path')
            elif suite == 'control':
                if (result.get('frames') != 16000 or result.get('max_error') != 0
                        or result.get('table_words') != 210
                        or result.get('openings') != [0, 1, 2, 3]
                        or result.get('variants') != [0, 1]
                        or not {1, 2, 3, 4} <= set(result.get('gates', []))
                        or result.get('missing_services') != 16
                        or result.get('bounded_rejections') != 6
                        or result.get('native_edge_cases') != 8
                        or result.get('failure_prefixes', 0) < 400
                        or result.get('live_mutations', 0) < 400):
                    raise RuntimeError(name + ' omitted a selected parent/table/input boundary')
            elif suite == 'action':
                counters = result.get('counters', {})
                required = dict(frames=12000, native_edges=47, completion_copies=30,
                                captured_descriptors=4, portable_rejections=11,
                                continuous_frames=1200, missing_services=21,
                                missing_menu_labels=1)
                services = {'key', 'audio', 'load', 'expression', 'active', 'request',
                            'write', 'target', 'camera', 'random_value', 'clip', 'source',
                            'pointer', 'drag', 'hit', 'clock', 'stop', 'repeat', 'ui_byte',
                            'ui_uv_reset', 'ui_fade'}
                if (any(counters.get(key) != count for key, count in required.items())
                        or set(result.get('missing_service_names', [])) != services
                        or counters.get('failure_prefixes', 0) < 400
                        or counters.get('live_mutations', 0) < 400
                        or result.get('modes') != list(range(8))
                        or not {'pointer', 'drag', 'repeat', 'ui_fade', 'ui_uv_reset',
                                'hit', 'clock'} <= set(result.get('operations', []))
                        or not result.get('numerical_children_unmodified')
                        or result.get('max_error') != 0):
                    raise RuntimeError(name + ' omitted an action mode/alias/timer/record path')
            elif suite == 'scene-presentation':
                required = dict(profiles=30, entries=60, frames=7200, requests=720,
                                automatic_frames=1200, controlled_frames=1800,
                                frozen_frames=1200, retained_backgrounds=30,
                                replacements=3, atomic_rejections=120, failure_prefixes=30)
                pcm = result.get('pcm', {})
                if (any(result.get(key) != count for key, count in required.items())
                        or result.get('pcm_frames', 0) + result.get('fixed_mouth_frames', 0) != 7200
                        or not result.get('fixed_mouth_frames')
                        or pcm.get('frames') != result.get('pcm_frames')
                        or not pcm.get('sampled_pcm_bytes') or pcm.get('max_error') != 0):
                    raise RuntimeError(name + ' omitted a real presentation/audio/lifetime path')
            elif suite == 'ui':
                counters = result.get('counters', {})
                if (counters.get('cases') != 7200 or counters.get('projected') != 7200
                        or counters.get('impossible') != 4 or counters.get('rejected') != 7
                        or result.get('max_error') != 0 or len(result.get('viewports', [])) != 6
                        or not counters.get('bearing_fit') or not counters.get('bearing_fallback')
                        or not counters.get('center_order')
                        or {tuple(z) for z in result.get('zones', [])} != {
                            (event, zone) for event in [1, 2] for zone in range(-1, 9)}):
                    raise RuntimeError(name + ' omitted a selected menu placement/projection path')
            elif suite == 'motion':
                counters = result.get('counters', {})
                if (counters.get('pointer') != 16534 or counters.get('drag') != 17066
                        or counters.get('sequence') != 1600
                        or counters.get('unchanged_descriptor_fields') != 33600
                        or counters.get('rejected') != 32 or result.get('max_error') != 0
                        or result.get('hooks') != []
                        or not counters.get('radial_offset') or not counters.get('overshoots')
                        or not counters.get('ignored_y') or not counters.get('wrapped_coordinates')
                        or result.get('settings') != [-1, 0, 1, 17]
                        or result.get('reversals') != [-1, 0, 1, 17]
                        or not {10, 15, 20, 30, 40, 60, 80} <= set(result.get('durations', []))):
                    raise RuntimeError(name + ' omitted a selected numerical/input path')
            elif suite == 'menu-regression':
                if (result.get('zone_calls') != 7200 or result.get('menu_calls') != 1440
                        or result.get('pick_calls') != 4800 or result.get('max_error') != 0
                        or result.get('impossible_bounded_failures') != 2):
                    raise RuntimeError(name + ' omitted a shared menu/picking regression')
            elif suite == 'presentation':
                if (result.get('frames') != 12000 or result.get('max_error') != 0
                        or result.get('failure_prefixes') != 1091
                        or result.get('missing_services') != 16
                        or result.get('bounded_rejections') != 3
                        or result.get('actors') != [0, 2]
                        or result.get('controlled') != [0, 1, 2]
                        or not result.get('live_mutations')
                        or not result.get('constant_mouth') or not result.get('envelope_mouth')):
                    raise RuntimeError(name + ' omitted a selected presentation branch/service')
            elif suite == 'auxiliary':
                checks = {row['kind']: row for row in result.get('checks', [])}
                if (set(checks) != {'cycle', 'manual'}
                        or any(row.get('frames') != 12000 or not row.get('failure_prefixes')
                               or not row.get('live_mutations') for row in checks.values())
                        or result.get('cue_table_words') != 60
                        or result.get('bounded_rejections', 0) < 7
                        or result.get('max_error') != 0
                        or not checks['cycle'].get('undefined_cue_rejections')
                        or not {3, 4, 5, 6} <= set(checks['cycle'].get('banks', []))
                        or checks['manual'].get('operations') != ['audio', 'request_ten', 'write']):
                    raise RuntimeError(name + ' omitted a selected cycle/manual path')
            else:
                if (len(result.get('cases', [])) != 23 or result.get('original_comparison')
                        or not all(case.get('passed') for case in result['cases'])):
                    raise RuntimeError(name + ' omitted a portable ownership/failure case')
            if suite == 'scene-presentation':
                summary_prefixes = ('PASS selected presentation integration ',)
            else:
                summary_prefixes = (
                    'PASS selected configuration ', 'PASS selected assets TOTAL ',
                    'PASS selected failure contracts ', 'PASS selected presentation ',
                    'PASS selected auxiliary ', 'PASS selected control ',
                    'PASS selected UI ', 'PASS selected motion ',
                    'PASS selected action ')
            summaries = [line for line in text.splitlines()
                         if line.startswith(summary_prefixes)]
            if suite == 'menu-regression':
                summaries = ['PASS shared menu regression zones=7200 menus=1440 picks=4800 bounded=2']
            if len(summaries) != 1:
                raise RuntimeError(name + ' omitted its final coverage summary')
            with lock:
                print(name + ': ' + summaries[0], flush=True)
            return dict(suite=suite, mode=mode, result=result, summary=summaries[0],
                        report=str(result_path.relative_to(ROOT)), python_sha256=digest(python.resolve()))

        rows, errors = [], []
        with ThreadPoolExecutor(max_workers=min(args.jobs, 4)) as pool:
            futures = [pool.submit(run, suite, mode) for suite in suites for mode in ['host', 'asan']]
            for future in futures:
                try:
                    rows.append(future.result())
                except Exception as exc:
                    errors.append(str(exc))
        for suite in suites:
            pair = [row for row in rows if row['suite'] == suite]
            if len(pair) != 2 or pair[0]['result'] != pair[1]['result'] or pair[0]['summary'] != pair[1]['summary']:
                errors.append(suite + ' ordinary/sanitized results did not match')
            else:
                report['checks'].append(dict(name=suite, passed=True, runs=pair))
        if errors:
            raise RuntimeError('; '.join(errors))
        if selected_manifest() != sources:
            raise RuntimeError('source changed during validation')
        if {path.name: digest(path) for path in archives} != inputs or digest(args.exe) != expected_exe:
            raise RuntimeError('original data changed during validation')
        report['library_sha256'] = {}
        for folder in ['build', 'build/asan']:
            candidates = list((ROOT / folder).glob('*model-test.dylib')) + list((ROOT / folder).glob('*model-test.so'))
            if len(candidates) != 1:
                raise RuntimeError('cannot identify tested library in ' + folder)
            report['library_sha256'][str(candidates[0].relative_to(ROOT))] = digest(candidates[0])
        report['passed'] = True
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as exc:
        report['error'] = str(exc)
        print(str(exc), flush=True)
    finally:
        report['finished_at'] = datetime.now(timezone.utc).isoformat()
        path = output / 'result.json'
        path.write_text(json.dumps(report, indent=2) + '\n')
        print('Selected assets result:', path, flush=True)
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
