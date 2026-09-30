#!/bin/sh
set -eu
cd "$(dirname "$0")"
./build-host.sh --vulkan
ctest --test-dir build --output-on-failure
build/texture-update-probe
build/gpu-transfer-probe
build/depth-clear-probe
build/actor-view-probe build/actor-view-fixture
python3 -m unittest discover -s tests -p 'test_*.py'
cmake -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug -DBK_SANITIZE=ON -DBK_WITH_VULKAN=OFF -DBK_BUILD_TESTS=ON
cmake --build build/asan --target test-ending-confirm test-ending-input test-ending-target test-ending-auxiliary-state1 test-ending-auxiliary-sequence ending-secondary-probe --parallel "${BK_BUILD_JOBS:-8}"
cmake --build build/asan --target test-ending-state ending-state-probe test-ending-ui-frame test-ending-ui-tail test-ending-ui-select test-ending-ui-pick test-ending-ui-hints test-ending-ui-geometry test-ending-stage-ui test-ending-reload ending-reload-audio-probe test-ending-ui test-ending-sound ending-sound-probe test-ending-special ending-special-scene-probe test-ending-normal ending-normal-probe test-bom-motion bom-assets-probe test-material-animation material-animation-probe test-fixed-animation fixed-animation-probe test-bom-deform test-node-reference bom-deform-probe test-ending-entry test-bom ending-face-probe test-ending-auxiliary ending-audio-probe ending-auxiliary-probe clip-edits-probe test-ending-control ending-camera-probe test-ending-camera test-ending-frame test-ending-record test-unlock-file test-unlock-switch-fs test-dialogue-entry test-dialogue-ui test-dialogue-media dialogue-audio-probe test-dialogue-backdrop test-switch-file-replace dialogue-actor-probe avi-probe test-avi test-avi-clock selection-actor-probe test-selection-ui test-x-pose menu-camera-probe test-menu-camera test-title-menu test-save-menu-control test-save-menu-view test-checkpoint-file area-entry-probe test-checkpoint-prompt test-retry failure-session-probe test-failure-camera test-failure-hud test-pause test-flow-loading player-hotkeys-probe test-player-hotkeys test-capture-file test-player-hud outcome-audio-probe game-frame-probe npc-event-audio-probe player-idle-probe prop-interaction-probe test-area-boundary area-audio-probe test-font test-message item-feedback-probe test-item entry-forest-probe pcm-probe audio-probe test-pcm test-audio test-audio-output test-footsteps test-core test-static test-camera test-follow-camera test-route test-npc-motion test-player-movement test-player-interaction test-player-view test-player-events test-lighting-pass test-rain test-actor-forest test-frame-tree test-draw-order test-draw-dispatch test-prop test-player-wall test-edge-math test-collision test-material-pose test-voice test-face test-face-assets test-eye-assets test-eye-pose test-morph test-skin test-triangles test-animation test-clip test-store fuzz-assets fuzz-model collision-probe prop-collision-probe prop-probe background-probe entry-probe face-probe skin-probe --parallel "${BK_BUILD_JOBS:-8}"
ctest --test-dir build/asan --output-on-failure
if [ -n "${BK3_ORIGINAL_EXE:-}" ]; then
    if [ "$#" -ge 1 ]; then
        python3 tests/check_ending_gallery_cpu.py "$BK3_ORIGINAL_EXE" --host-python "${BK3_TEST_PYTHON:-local/venv/bin/python}" --data "$1/Data"
    else
        python3 tests/check_ending_gallery_cpu.py "$BK3_ORIGINAL_EXE" --host-python "${BK3_TEST_PYTHON:-local/venv/bin/python}"
    fi
fi
if [ -n "${BK3_ORIGINAL_EXE:-}" ] && [ "$#" -lt 1 ]; then
    python3 tests/check_ending_tertiary_cpu.py "$BK3_ORIGINAL_EXE" --host-python "${BK3_TEST_PYTHON:-local/venv/bin/python}"
fi
if [ "$#" -ge 1 ]; then
    python=${BK3_TEST_PYTHON:-local/venv/bin/python}
    if [ -n "${BK3_ORIGINAL_EXE:-}" ]; then
        "$python" tests/check_ending_tertiary_cpu.py "$BK3_ORIGINAL_EXE" --host-python "$python" --data "$1/Data"
        "$python" tests/check_bom_dual_assets.py "$BK3_ORIGINAL_EXE" "$1/Data" --with-ending-assets
        "$python" tests/check_ending_selected_assets.py "$BK3_ORIGINAL_EXE" "$1/Data"
    fi
    "$python" tests/audit_game.py "$1/Data"
    "$python" tests/audit_models.py "$1/Data"
    build/render-probe "$1/Data/bk3_00.pp" op_00.bmp build/title-readback.rgba
    build/render-probe "$1/Data/bk3_00.pp" op_01.tga build/button-readback.rgba
    build/material-probe "$1/Data/bk3_03.pp" m01_04.x
    build/material-render-probe "$1/Data/bk3_03.pp" m01_04.x
    build/biko3-preview "$1" build/preview-readback.rgba --frames 16
    BK_SHOW_FPS=1 "$python" tests/check_game_application.py build/biko3-preview "$1"
    "$python" tests/check_game_application.py build/biko3-preview "$1" --replay-hz 30
    "$python" tests/check_game_application.py build/biko3-preview "$1" --replay-hz 15
    if [ -f "$1/Data/bk3_18.pp" ]; then
        build/avi-probe "$1/Data/bk3_18.pp"
        build/avi-render-probe "$1/Data/bk3_18.pp"
        ASAN_OPTIONS=detect_leaks=0 build/asan/avi-probe "$1/Data/bk3_18.pp"
    fi
    if [ -n "${BK3_DIALOGUE_AUDIO_TRACE:-}" ]; then
        build/dialogue-audio-probe "$1/Data" "$BK3_DIALOGUE_AUDIO_TRACE"
        ASAN_OPTIONS=detect_leaks=0 build/asan/dialogue-audio-probe "$1/Data" "$BK3_DIALOGUE_AUDIO_TRACE"
    fi
    if [ -n "${BK3_ENDING_RELOAD_TRACE:-}" ]; then
        build/ending-reload-audio-probe "$1/Data" "$BK3_ENDING_RELOAD_TRACE"
        ASAN_OPTIONS=detect_leaks=0 build/asan/ending-reload-audio-probe "$1/Data" "$BK3_ENDING_RELOAD_TRACE"
    fi
    if [ -n "${BK3_ENDING_SOUND_TRACE:-}" ]; then
        build/ending-sound-probe "$1/Data" "$BK3_ENDING_SOUND_TRACE" build/ending-sound-fixture
        ASAN_OPTIONS=detect_leaks=0 build/asan/ending-sound-probe "$1/Data" "$BK3_ENDING_SOUND_TRACE" build/ending-sound-fixture-asan
    fi
    if [ -n "${BK3_ENDING_AUDIO_TRACE:-}" ]; then
        build/ending-audio-probe "$1/Data" "$BK3_ENDING_AUDIO_TRACE"
        ASAN_OPTIONS=detect_leaks=0 build/asan/ending-audio-probe "$1/Data" "$BK3_ENDING_AUDIO_TRACE"
    fi
    if [ -n "${BK3_BOM_FIXTURE:-}" ]; then
        build/bom-deform-probe "$BK3_BOM_FIXTURE"
        ASAN_OPTIONS=detect_leaks=0 build/asan/bom-deform-probe "$BK3_BOM_FIXTURE"
    fi
    build/ending-special-scene-probe "$1/Data"
    ASAN_OPTIONS=detect_leaks=0 build/asan/ending-special-scene-probe "$1/Data"
    build/ending-state-probe "$1/Data"
    ASAN_OPTIONS=detect_leaks=0 build/asan/ending-state-probe "$1/Data"
    build/ending-normal-probe "$1/Data"
    ASAN_OPTIONS=detect_leaks=0 build/asan/ending-normal-probe "$1/Data"
    build/ending-secondary-probe "$1/Data"
    ASAN_OPTIONS=detect_leaks=0 build/asan/ending-secondary-probe "$1/Data"
    python3 tests/check_ending_runtime.py "$1/Data" --reuse-host-build
    build/ending-normal-scene-probe "$1/Data" 0
    build/ending-normal-scene-probe "$1/Data" 1
    build/ending-normal-scene-probe "$1/Data" 0 --story
    build/ending-normal-scene-probe "$1/Data" 1 --story
    build/ending-normal-scene-probe "$1/Data" 0 --confirmation
    build/ending-viewport-probe "$1/Data"
    build/ending-framing-probe "$1/Data"
    build/ending-draw-event-probe "$1/Data"
    build/ending-exit-probe "$1/Data" build/ending-exit-output
    build/ending-normal-probe "$1/Data" --background
    ASAN_OPTIONS=detect_leaks=0 build/asan/ending-normal-probe "$1/Data" --background
    build/ending-face-probe "$1/Data"
    ASAN_OPTIONS=detect_leaks=0 build/asan/ending-face-probe "$1/Data"
    build/ending-auxiliary-probe "$1/Data"
    ASAN_OPTIONS=detect_leaks=0 build/asan/ending-auxiliary-probe "$1/Data"
    build/ending-auxiliary-probe "$1/Data" --cycle
    ASAN_OPTIONS=detect_leaks=0 build/asan/ending-auxiliary-probe "$1/Data" --cycle
    cmake --build build/asan --target ending-presentation-probe ending-opening-probe --parallel "${BK_BUILD_JOBS:-8}"
    build/ending-opening-probe "$1/Data"
    ASAN_OPTIONS=detect_leaks=0 build/asan/ending-opening-probe "$1/Data"
    build/ending-presentation-probe "$1/Data" build/ending-presentation-host.pcmtrace
    ASAN_OPTIONS=detect_leaks=0 build/asan/ending-presentation-probe "$1/Data" build/asan/ending-presentation-asan.pcmtrace
    cmp build/ending-presentation-host.pcmtrace build/asan/ending-presentation-asan.pcmtrace
    build/bom-assets-probe "$1/Data"
    build/bom-render-probe "$1/Data"
    build/ending-ui-batch-probe "$1/Data"
    build/ending-ui-tail-probe "$1/Data"
    build/ending-ui-hints-render-probe "$1/Data"
    build/ending-ui-hints-render-probe "$1/Data" --cursors
    build/ending-ui-hints-render-probe "$1/Data" --selectors
    build/ending-stage-ui-render-probe "$1/Data" build/ending-stage-ui-fixture
    build/ending-stage-ui-render-probe "$1/Data" build/ending-stage-ui-fixture --lines
    build/ending-ui-render-probe "$1/Data" build/ending-ui-fixture
    build/bom-assets-probe "$1/Data" --motion
    build/bom-render-probe "$1/Data" --motion
    build/bom-assets-probe "$1/Data" --seconds
    build/bom-render-probe "$1/Data" --seconds
    ASAN_OPTIONS=detect_leaks=0 build/asan/bom-assets-probe "$1/Data" --seconds
    ASAN_OPTIONS=detect_leaks=0 build/asan/bom-assets-probe "$1/Data" --motion
    ASAN_OPTIONS=detect_leaks=0 build/asan/bom-assets-probe "$1/Data"
    build/material-animation-render-probe "$1/Data"
    build/material-animation-probe "$1/Data"
    ASAN_OPTIONS=detect_leaks=0 build/asan/material-animation-probe "$1/Data"
    build/fixed-animation-probe "$1/Data"
    ASAN_OPTIONS=detect_leaks=0 build/asan/fixed-animation-probe "$1/Data"
    build/clip-edits-probe "$1/Data"
    ASAN_OPTIONS=detect_leaks=0 build/asan/clip-edits-probe "$1/Data"
    build/ending-camera-probe "$1/Data"
    ASAN_OPTIONS=detect_leaks=0 build/asan/ending-camera-probe "$1/Data"
    build/test-dialogue-entry "$1/Data"
    ASAN_OPTIONS=detect_leaks=0 build/asan/test-dialogue-entry "$1/Data"
    build/dialogue-session-probe "$1/Data"
    build/dialogue-ui-render-probe "$1/Data"
    build/dialogue-backdrop-probe "$1/Data"
    build/dialogue-actor-probe "$1/Data"
    ASAN_OPTIONS=detect_leaks=0 build/asan/dialogue-actor-probe "$1/Data"
    build/dialogue-actor-render-probe "$1/Data"
    build/selection-actor-probe "$1/Data"
    build/asan/selection-actor-probe "$1/Data"
    build/menu-camera-probe "$1/Data"
    build/asan/menu-camera-probe "$1/Data"
    build/selection-ui-probe "$1/Data" build/selection-ui.rgba
    build/title-menu-probe "$1/Data" build/title-menu.rgba
    build/save-menu-probe "$1/Data" build/save-menu.rgba
    BK_NATIVE_GAME_CLOCK=1 build/save-flow-probe "$1/Data" build/save-flow-output build/save-flow.rgba
    BK_NATIVE_GAME_CLOCK=1 BK_REPEAT_SAVE=1 build/save-flow-switch-fs-probe "$1/Data" build/save-switch-fs-output build/save-switch-fs.rgba
    build/weather-flow-probe "$1/Data" build/weather-texture-fixture
    build/area-flow-probe "$1/Data" build/area-flow-captures build/area-flow.rgba
    build/area-entry-probe "$1/Data"
    build/asan/area-entry-probe "$1/Data"
    build/failure-session-probe "$1/Data"
    build/asan/failure-session-probe "$1/Data"
    build/failure-flow-probe "$1/Data" build/failure-captures build/failure-flow.rgba
    BK_NATIVE_GAME_CLOCK=1 BK_CAR_IMPACT=1 build/failure-flow-probe "$1/Data" build/car-failure-captures build/car-failure.rgba
    BK_CAR_IMPACT=1 build/prop-probe "$1/Data" --audio
    ASAN_OPTIONS=detect_leaks=0 BK_CAR_IMPACT=1 build/asan/prop-probe "$1/Data" --audio
    BK_NATIVE_GAME_CLOCK=1 build/front-flow-probe "$1/Data" build/front-flow-captures build/front-flow.rgba
    BK_NATIVE_GAME_CLOCK=1 BK_FRONT_CONTROLLER=1 build/front-flow-probe "$1/Data" build/front-controller-captures build/front-controller.rgba
    BK_NATIVE_GAME_CLOCK=1 BK_FRONT_CONTROLLER=1 BK_FRONT_UNLOCKS=1 build/front-flow-probe "$1/Data" build/front-unlock-captures build/front-unlock.rgba
    BK_NATIVE_GAME_CLOCK=1 BK_FRONT_RAIN=1 build/front-flow-probe "$1/Data" build/front-rain-captures build/front-rain.rgba
    python3 tests/check_front_application.py build/biko3-preview "$1"
    BK_NATIVE_GAME_CLOCK=1 build/play-flow-probe "$1/Data" build/play-flow-captures build/play-flow.rgba
    build/scene-probe "$1" build/scene-readback
    build/entry-probe "$1/Data"
    build/asan/entry-probe "$1/Data"
    build/face-probe "$1/Data"
    build/actor-render-probe "$1/Data"
    build/asan/face-probe "$1/Data"
    build/gpu-skin-probe "$1"/Data/*.pp
    build/skin-probe "$1/Data/bk3_01.pp" h00_80.x h01_80.x h02_80.x h03_80.x h04_80.x h05_80.x
    build/asan/skin-probe "$1/Data/bk3_01.pp" h00_80.x h01_80.x h02_80.x h03_80.x h04_80.x h05_80.x
    for atr in "$1"/Data/*.atr; do
        scene=$(basename "$atr" .atr)
        build/collision-probe "$1/Data" "$scene"
        build/asan/collision-probe "$1/Data" "$scene"
    done
fi
