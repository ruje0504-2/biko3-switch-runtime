"""Real selected presentation resources and independently executed PCM windows.

The C fixture uses30 Japanese model configurations with fresh and retained
backgrounds, real ANIM/MATA/MORP,4968CB, face/eye bindings and queued audio.
Requests/modes and clocks are explicit test inputs, not a natural controller
playthrough. Original4ad363/4ad5a4 consumes every recorded voice window.
The original494015 dispatch and actual model samplers have separate oracles;
this does not claim one continuous native model/frame replay, UI/GPU output,
the complete4D1025 loader, leak detection, or Switch hardware validation.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from unittest.mock import patch

from check_bom_dual_assets import ROOT
import original_ending_voice_oracle as voice


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('data', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--profiles', type=int, default=30)
    args = parser.parse_args()
    if not 1 <= args.profiles <= 30:
        parser.error('profiles must be within1..30')
    exe_hash = hashlib.sha256(args.exe.read_bytes()).hexdigest()
    if exe_hash != 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e':
        parser.error('original PCM addresses require the pinned EXE')
    build = Path(os.environ.get('BK3_BUILD_DIR', ROOT / 'build'))
    records = args.output.with_suffix('.pcm.bin')
    raw = args.output.with_suffix('.raw.json')
    subprocess.run([str(build / 'ending-selected-presentation-probe'), str(args.data),
                    str(raw), str(records), str(args.profiles)], check=True)
    result = json.loads(raw.read_text())
    with patch.object(sys, 'argv', ['original_ending_voice_oracle.py', str(args.exe),
                                   '--records', str(records)]):
        voice.main()
    pcm = json.loads(records.with_suffix('.verification.json').read_text())
    if (not result.get('passed') or not pcm.get('passed') or pcm['max_error'] != 0
            or pcm['frames'] != result['pcm_frames'] or pcm['exe_sha256'] != exe_hash):
        raise AssertionError('actual consumed PCM did not match the original voice filter')
    #Keep full file locations in their individual records; pair semantic
    #results independently of host/ASan output paths and binary layouts.
    result.update(exe_sha256=exe_hash, scope=__doc__, pcm={
        key: pcm[key] for key in ['frames', 'records_sha256', 'sampled_pcm_bytes',
                                 'inactive', 'max_error']},
        full_loader=False, gpu_or_device_validation=False)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('PASS selected presentation integration', json.dumps(result, sort_keys=True), flush=True)


if __name__ == '__main__':
    main()
