# XENON

**Codename:** Music Trinity Engine

XENON is the shared integration and contract layer connecting three independent native applications:

- **EtherPlayer / EtherPlay** — perception, playback, listening state
- **Spiral AI** — reasoning, continuity, musical intent
- **EtherBeat** — composition, arrangement, generation, revision and rendering

Core rule:

> **EtherPlayer perceives. Spiral understands. EtherBeat creates. XENON connects.**

XENON is **not** a DAW, audio renderer, media player, or LLM implementation. It is the stable protocol boundary that lets those systems cooperate without merging codebases.

## Music Trinity loop

```text
EtherPlayer
    │ MusicFrame v1
    ▼
  XENON
    │
    ▼
Spiral AI
    │ ProductionIntent v1
    ▼
  XENON
    │
    ▼
EtherBeat
    │ RenderArtifact v1
    ▼
  XENON
    │
    ▼
EtherPlayer
```

The first proof target is one closed loop:

1. Play a reference in EtherPlayer.
2. Publish `MusicFrameV1` through XENON.
3. Spiral compiles musical intent.
4. Publish `ProductionIntentV1` through XENON.
5. EtherBeat renders a preview.
6. Publish `RenderArtifactV1` through XENON.
7. EtherPlayer auditions the result.
8. Spiral receives revision feedback and changes only requested musical dimensions.

## Repository responsibility

This repository owns:

- versioned cross-application schemas
- capability/tool identifiers
- request/response envelopes
- permission-boundary semantics
- transport-neutral adapter interfaces
- compatibility tests and Trinity end-to-end fixtures

This repository does **not** own:

- EtherPlayer playback internals
- Spiral model/runtime internals
- EtherBeat generation/DSP internals
- application UI

## Canonical repositories

```text
spiraletech/xenon
    shared Music Trinity contracts + integration

spiraletech/spiralos-ai-genius
    CORTEX + ORGANIC + XENON client

spiraletech/ether-beat-ai
    composition/generation + XENON adapter

EtherPlayer / EtherPlay
    polished native playback/perception applications + XENON adapter
```

## Initial protocol namespace

```text
etherplayer.get_current_track
etherplayer.get_library_state
etherplayer.analyze_audio
etherplayer.extract_features
etherplayer.seek
etherplayer.queue_track
etherplayer.set_metadata

etherbeat.get_project_state
etherbeat.get_arrangement
etherbeat.get_stems
etherbeat.analyze_reference
etherbeat.create_arrangement
etherbeat.generate_midi
etherbeat.select_drum_pattern
etherbeat.build_chord_progression
etherbeat.request_stem
etherbeat.apply_etherseam
etherbeat.export_song
```

## Status

`XENON v0.1` begins with protocol/schema stabilization before transport implementation.
