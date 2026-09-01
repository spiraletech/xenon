# XENON

**Codename:** Music Trinity Engine  
**Binary:** `XENON.exe`  
**Language:** C++20

XENON is the new native Music Trinity creation/runtime program. It supersedes EtherBeat as the active music-generation body while keeping EtherPlayer and Spiral AI independent.

Core rule:

> **EtherPlayer perceives. Spiral understands. XENON creates.**

## Architecture

```text
EtherPlayer / EtherPlay
    │ MusicFrameV1
    ▼
Spiral AI
    │ ProductionIntentV1
    ▼
XENON.exe
    │ RenderArtifactV1
    ▼
EtherPlayer / EtherPlay
```

XENON owns music creation, project revisions, renderer routing, arrangement execution and exported audio. Spiral owns reasoning and continuity. EtherPlayer owns playback and perception.

## v0.1 native foundation

The first implementation intentionally proves the executable and render lifecycle before adding model backends.

```text
ProductionIntentV1
        │
        ▼
    xenon::Engine
        │
        ▼
    WavRenderer
        │
        ▼
<project>_rN.wav
        │
        ▼
RenderArtifactV1
```

The deterministic preview renderer is a temporary native proof backend. BPM, duration, seed, drum density, bass weight and texture grit already alter the generated PCM16 WAV. It will later sit beside real model/DSP/MIDI backends rather than defining XENON's final sound.

## Repository responsibility

XENON owns:

- `XENON.exe`
- Music Trinity schemas
- project/revision state
- arrangement and production execution
- renderer/backend routing
- MIDI and DSP expansion
- exported stems/audio
- Spiral-facing creation commands
- EtherPlayer audition handoff

XENON does not own:

- EtherPlayer playback internals
- Spiral/CORTEX inference internals
- ORGANIC long-term user memory

## Canonical repositories

```text
spiraletech/xenon
    XENON.exe + Music Trinity creation engine

spiraletech/spiralos-ai-genius
    CORTEX + ORGANIC + musical intent compiler

EtherPlayer / EtherPlay
    polished native playback/perception applications

spiraletech/ether-beat-ai
    legacy/prototype generation research; no longer the canonical Trinity executable
```

## Build

```text
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The resulting application target is `XENON` (`XENON.exe` on Windows).

## Current proof target

1. `XENON.exe` boots.
2. It accepts a `ProductionIntentV1`.
3. `xenon::Engine` opens or continues project state.
4. The native preview backend renders revision `rN`.
5. XENON returns `RenderArtifactV1`.
6. A follow-up intent creates `rN+1` without resetting the project.
7. EtherPlayer audition/IPC is added as the next external boundary.

**Status:** `XENON v0.1 native foundation in progress.`
