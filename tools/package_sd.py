"""Package the local NVK development build; game files stay out of source control."""
import argparse
import json
from pathlib import Path
import shutil
from bk3_assets import Archive
from fetch_sdk import ROOT, LOCK, digest


def copy_verified(source, target, replace=False):
    sha = digest(source)
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        if digest(target) == sha:
            return sha
        if not replace:
            raise ValueError(f'Existing data differs; preserving {target}')
    temp = target.with_name(target.name+'.packaging')
    shutil.copy2(source,temp)
    if digest(temp) != sha:
        raise ValueError(f'Copy checksum mismatch: {source}')
    temp.replace(target)
    return sha


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game',type=Path)
    parser.add_argument('--output',type=Path,default=ROOT/'交付/SD卡根目录')
    parser.add_argument('--full-data',action='store_true',help='copy all 22 PP archives for further development')
    parser.add_argument('--complete-data',action='store_true',
                        help='copy every file from the original game Data directory')
    args=parser.parse_args()
    if args.full_data and args.complete_data:
        parser.error('--full-data and --complete-data are mutually exclusive')
    game=args.game.resolve()
    target=args.output.resolve()/'switch/biko3'
    if target.is_relative_to(game) or game.is_relative_to(target):
        raise ValueError('Output must be separate from original game data')
    nro=ROOT/'build-switch/biko3-runtime.nro'
    if not nro.is_file():
        raise ValueError('Run build-switch.sh first')
    # Match the game and diagnostic mounts in runtime/app/application.c.
    packs = ['bk3_00', 'bk3_01', 'bk3_02', 'bk3_03', 'bk3_04',
             'bk3_05', 'bk3_06', 'bk3_07', 'bk3_08', 'bk3_09',
             'bk3_10', 'bk3_11', 'bk3_12', 'bk3_13', 'bk3_14',
             'bk3_15', 'bk3_16', 'bk3_18', 'bk3_19', 'bk3_20', 'fambom']
    data_dir = game/'Data'
    if args.complete_data:
        paths=sorted(p for p in data_dir.iterdir() if p.is_file())
        loose=[]
        data_scope='complete-data-directory'
    else:
        paths=sorted(data_dir.glob('*.pp')) if args.full_data else [data_dir/(name+'.pp') for name in packs]
        loose=sorted(p for p in data_dir.iterdir()
                     if p.is_file() and (p.suffix.lower() in {'.ckp', '.atr', '.fam'}
                                        or p.name.lower() in {'type_s.ftt', 'type_g.ftt'}))
        data_scope='all-pp-archives' if args.full_data else 'runtime-dependency-set'
    # Preflight the whole playable dependency set before replacing an NRO.
    for name in packs:
        if not (game/'Data'/(name+'.pp')).is_file():
            raise ValueError(f'Missing required game archive: {name}.pp')
    if not args.complete_data:
        for suffix in ['.ckp', '.atr', '.fam', '.ftt']:
            if not any(p.suffix.lower() == suffix for p in loose):
                raise ValueError(f'Missing required loose game assets: {suffix}')
    for path in paths:
        if path.suffix.lower() == '.pp':
            Archive(path)
    build_info=json.loads((ROOT/'build-switch/build-manifest.json').read_text())
    if build_info['sha256'] != digest(nro) or build_info['version'] != LOCK['project_version'] or build_info['mesa_commit'] != LOCK['mesa']['commit']:
        raise ValueError('NRO/build metadata does not match dependency lock; rebuild first')
    manifest={'version':build_info['version'], 'status':'development-build',
              'language':'Japanese resources and Shift-JIS save labels',
              'data_scope':data_scope,
              'default_scene':'game', 'first_flow':'original-title',
              'scenes':['title','game','office','camera-track','actor','pause','ending'],
              'implemented_flows':['original title/selection/five introductions/game entry',
                                   'mission opening/gameplay/area handover', 'native rain/snow in game and failure scenes', 'photo', 'pause/resume',
                                   'pause/return-original-title', 'confirmed exit',
                                   'natural failure/dialogue', 'retry/reenter-game', 'retry/return-title',
                                   'area-completion/save prompt; both choices resume next area',
                                   '50-slot checkpoint save/overwrite/load', 'cancel load/retain paused game',
                                   'endings/special scenes', 'action record save/replay', 'persistent unlocks',
                                   'gallery', 'photo album', 'volume settings/save/restore defaults'],
              'retained_process_state':True, 'scene_aspect':[4,3],
              'renderer':'static Mesa NVK Vulkan, no fallback',
              'mesh_skinning_implemented':True, 'fog_implemented':True, 'weather_enabled':True,
              'gameplay_complete':False, 'switch_hardware_tested':False,
              'pending':['latest-build Switch performance/stability validation'],
              'mesa_commit':build_info['mesa_commit'],'files':[]}
    manifest['files'].append({'path':nro.name,'sha256':copy_verified(nro,target/nro.name,replace=True),'size':nro.stat().st_size})
    for path in paths:
        items=[path]
        if not args.complete_data and path.suffix.lower() == '.pp' and path.with_suffix('.tbl').exists():
            items.append(path.with_suffix('.tbl'))
        for source in items:
            relative='game/Data/'+source.name
            manifest['files'].append({'path':relative,'sha256':copy_verified(source,target/relative),'size':source.stat().st_size})
    for source in loose:
        relative='game/Data/'+source.name
        manifest['files'].append({'path':relative,'sha256':copy_verified(source,target/relative),'size':source.stat().st_size})
    for name in ['README.md', 'THIRD_PARTY.md', 'reports/porting.md',
                 'reports/play-session.md', 'reports/play-session-verification.json',
                 'reports/failure-session.md', 'reports/failure-session-verification.json',
                 'reports/pause-background-fix.md',
                 'reports/area-handover.md', 'reports/area-handover-verification.json',
                 'reports/area-session.md', 'reports/area-session-verification.json',
                 'reports/checkpoint-storage.md', 'reports/checkpoint-storage-verification.json',
                 'reports/save-session.md', 'reports/save-session-verification.json',
                 'reports/dialogue-entry.md', 'reports/dialogue-entry-verification.json',
                 'reports/dialogue-session.md', 'reports/dialogue-session-verification.json',
                 'reports/original-front-end.md', 'reports/original-front-end-verification.json',
                 'reports/weather-integration.md', 'reports/weather-integration-verification.json',
                 'docs/architecture.md', 'docs/roadmap.md']:
        copy_verified(ROOT/name,target/name,replace=True)
    staged=target/'manifest.json.packaging'
    staged.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
    staged.replace(target/'manifest.json')
    print(target)
    print(f"Verified {len(manifest['files'])} files; {sum(f['size'] for f in manifest['files']):,} bytes")


if __name__=='__main__':
    main()
