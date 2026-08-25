# Architecture

This document explains how **mav-voice-gcs** is organized internally: the
layer contract, the data flow, the threading model and the reasoning behind
key decisions. For build and usage see the [README](../README.md); for
config keys see [CONFIGURATION.md](CONFIGURATION.md).

## The one-sentence summary

Bytes come in over UDP, get parsed into plain data objects, a small domain
model turns them into events, and the announcer turns events into Russian
phrases that a worker thread speaks through espeak-ng — the receiving path
never waits on the speaking path.

## Layer overview

```text
            ┌─────────────────────────── main thread ───────────────────────────┐
            │                                                                  │
 UDP 14550 │  TRANSPORT           PARSER                DOMAIN           UI     │
 ─────────►│  UdpTransport  ───►  MavlinkParser  ───►  VehicleState  ──► MainWin│
            │  (QUdpSocket)       (c_library_v2)        EventDetector          │
            │       ▲                   │               LinkMonitor            │
            │       │                   │               AntiSpamFilter         │
            │       │                   ▼                      │               │
            │       │              TlogWriter                 ▼               │
            │       │                                   Announcer ──► event log│
            └───────│───────────────────────────────────────│──────────────────┘
                    │ heartbeat, stream requests            │ announce(phrase, priority)
                    │                                       ▼ (queued connection)
            ┌───────┴─────────────────── TTS thread ────────┴──────────────────┐
            │  TtsQueue  ──►  EspeakBackend  ──►  espeak-ng process (WAV/audio)│
            └──────────────────────────────────────────────────────────────────┘
```

## Layers and their contract

| Layer | Files | May include | Must not include |
|---|---|---|---|
| Transport | `src/transport/UdpTransport.*` | QtCore, QtNetwork | anything MAVLink-specific |
| Parser | `src/mavlink/*` | `extern/c_library_v2`, domain DTO headers | Qt UI, TTS |
| Domain | `src/domain/*`, `src/telemetry/*` | QtCore only | MAVLink headers |
| Announcer | `src/announce/*` | domain, config | MAVLink headers, sockets |
| TTS | `src/tts/*` | QtCore | domain logic |
| UI | `src/ui/*` | QtWidgets, domain | parser, transport |
| Wiring | `src/app/Application.*` | everything above | UI |

The point of the contract: the domain model is testable without a socket,
and swapping the speech backend (see `ITtsBackend`) touches nothing else.

## Data flow, step by step

1. **Receive.** `UdpTransport` binds UDP 14550 (`ShareAddress`) and emits
   `datagramReceived(QByteArray)` per datagram. The sender address is
   remembered as the peer so the app can talk back.
2. **Parse.** `MavlinkParser::feed()` runs `mavlink_parse_char()` byte by
   byte — the state machine resynchronizes after garbage on its own. For
   every decoded frame the parser:
   - applies the **sysid filter** (`vehicle_sysid`, auto-lock on the first
     heartbeat whose autopilot ≠ INVALID and type ≠ GCS);
   - emits `rawFrame()` for the tlog recorder;
   - decodes the supported messages into plain structs (`Telemetry.h`) and
   - emits typed signals (`heartbeatReceived`, `batteryReceived`, …).
3. **Record.** `TlogWriter` (if `[log] enabled`) writes each frame with an
   8-byte little-endian microsecond timestamp — self-describing enough for
   replay and offline analysis.
4. **Track.** `VehicleState` keeps the latest snapshot for the UI and the
   status speech (mode, armed, battery %, relative altitude, ground speed,
   message counters, last-message time).
5. **Detect.** `EventDetector` turns the stream into *edges*:
   mode changed; armed toggled (the first heartbeat is taken as the
   baseline and not spoken); battery crossing thresholds — with hysteresis
   (`recover_margin_percent`) so a value hovering at the threshold does not
   machine-gun warnings; STATUSTEXT with severity ≤ WARNING passes through
   verbatim.
6. **Watch the link.** `LinkMonitor` polls `VehicleState::linkAlive()` once
   a second: silence longer than `[link] loss_sec` produces `linkLost()`
   (which also resets the stream-request flag, so a rebooted vehicle gets
   asked for data again), returning traffic produces `linkRegained()`.
7. **Phrase.** `Announcer` maps events to Russian phrases (with correct
   plural forms: «12 метров», «5 метров в секунду»), applies the anti-spam
   filter (`AntiSpamFilter`, per-event-type keys and intervals from the
   config) and emits `announce(phrase, priority)`.
8. **Speak.** The signal crosses into the **TTS thread** (queued
   connection): `TtsQueue` deduplicates identical pending phrases, caps the
   queue (dropping the oldest non-critical items first) and drives
   `EspeakBackend`, which spawns `espeak-ng` per phrase with a fail-safe
   timeout. Speech can take seconds; reception never blocks.

## Threading model

- **Main thread**: transport, parser, domain, announcer, UI. All work is
  event-driven and short — the event loop stays responsive.
- **TTS thread** owns `TtsQueue` + backend. `QProcess`/`QTimer` children
  are created *after* `moveToThread` (see `EspeakBackend::initInWorkerThread`)
  so Qt object affinity rules are never violated.
- Connections between threads are automatic `Qt::QueuedConnection`s —
  there are no locks in the codebase.

## Message set and rates

The app requests its working set explicitly after the first vehicle
heartbeat via `MAV_CMD_SET_MESSAGE_INTERVAL` (2 Hz):

`HEARTBEAT`, `SYS_STATUS`, `BATTERY_STATUS`, `GLOBAL_POSITION_INT`,
`VFR_HUD`, `STATUSTEXT`.

Everything else in the stream is parsed for the tlog but does not enter the
domain model. Long STATUSTEXT messages that arrive chunked (non-zero `id`
field) are reassembled and flushed on a short timeout.

## Anti-spam keys

| Key | Interval key in config | Event |
|---|---|---|
| `mode` | `mode_change_sec` | flight mode change |
| `arm` | `arm_sec` | arm / disarm |
| `battery_warn`, `battery_crit` | `battery_sec` | threshold crossings |
| `st:<first 40 chars>` | `statustext_sec` | each distinct STATUSTEXT text |
| `link_up`, `link_down` | `link_sec` | link state (separate keys on purpose) |
| `status` | `status_hotkey_sec` | hotkey status speech |

The key table is capped at 512 entries (cleared wholesale when full) so a
stream of unique STATUSTEXTs cannot grow memory indefinitely.

## Testing strategy

- **Unit** (`tests/`): parser behavior on hostile input, detector edges,
  anti-spam timing (injectable clock), link monitor state machine, TTS
  queue dedup/overflow — no sockets, no processes.
- **Synthetic integration** (`scripts/synthetic_driver.py`): a real app
  process fed generated MAVLink frames over real UDP; phrase expectations
  checked against the app log and WAV outputs. Runs in CI.
- **Full integration** (`scripts/integration.sh`): the same idea against a
  real ArduCopter SITL, including battery failsafe STATUSTEXT.
- **Manual acceptance** ([MANUAL_TESTING.md](MANUAL_TESTING.md)): the
  human-facing checklist mirroring the original specification.

## Known design trade-offs

- Single vehicle per process by design; a second vehicle on the same port
  is filtered out (`vehicle_sysid`).
- The hotkey works while the window is focused; global X11 hotkeys and a
  tray icon are deliberate non-goals for now.
- espeak-ng is the only shipped backend; the `ITtsBackend` interface exists
  so higher-quality synthesizers can be added without touching the domain.
