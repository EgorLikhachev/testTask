# Configuration reference

The application reads a single INI file. **Every key is optional** —
missing keys fall back to the built-in defaults listed here. The reference
config with comments lives at [`config/gcs-tts.ini`](../config/gcs-tts.ini);
a faster-paced variant for automated testing at
[`config/gcs-tts-integration.ini`](../config/gcs-tts-integration.ini).

## Config file lookup

First existing path wins:

1. `--config <path>` command-line argument;
2. `<binary dir>/gcs-tts.ini`;
3. `<binary dir>/../config/gcs-tts.ini`;
4. `<binary dir>/../../config/gcs-tts.ini`;
5. `<binary dir>/../share/mav-voice-gcs/gcs-tts.ini` (AppImage layout).

Invalid values are clamped into the documented range (e.g. a battery
warning of 150 % becomes 99 %), never rejected.

## `[udp]` — transport

| Key | Default | Range | Description |
|---|---|---|---|
| `host` | `127.0.0.1` | — | informational: expected telemetry source |
| `port` | `14550` | 1–65535 | UDP port to listen on |
| `sysid` | `255` | 1–255 | our GCS MAVLink system id |
| `compid` | `190` | 1–255 | our GCS MAVLink component id |
| `vehicle_sysid` | `0` | 0–255 | which vehicle to listen to: `0` = auto-lock the first valid one (heartbeat with `autopilot ≠ INVALID`, `type ≠ GCS`); `1..255` = only that system id |

## `[battery]` — thresholds

| Key | Default | Range | Description |
|---|---|---|---|
| `warn_percent` | `25` | 2–99 | warning threshold, % of charge |
| `critical_percent` | `15` | 1 .. warn−1 | critical threshold |
| `recover_margin_percent` | `5` | 0–50 | hysteresis: an alert clears only when charge rises `threshold + margin`; re-entering the zone after recovery speaks again |

## `[link]` — connection watchdog

| Key | Default | Range | Description |
|---|---|---|---|
| `loss_sec` | `4` | 1–3600 | no messages for this long → “Потеря связи с бортом”; traffic returning → “Связь с бортом восстановлена” and the data-stream request is re-issued |

## `[antispam]` — repeat intervals (seconds)

| Key | Default | Description |
|---|---|---|
| `default_sec` | `8` | fallback for event types without a dedicated key |
| `mode_change_sec` | `10` | flight mode change |
| `arm_sec` | `5` | arm / disarm |
| `battery_sec` | `30` | battery threshold crossings (per zone) |
| `statustext_sec` | `15` | per distinct STATUSTEXT text |
| `link_sec` | `10` | link lost / regained (separate keys) |
| `status_hotkey_sec` | `2` | hotkey status speech |

Set an interval to `0` to disable suppression for that event type.

## `[tts]` — speech

| Key | Default | Description |
|---|---|---|
| `enabled` | `true` | master switch; `false` logs phrases without speaking |
| `backend` | `espeak` | `espeak` or `piper` (neural, see `scripts/setup_piper.sh`) |
| `program` | `espeak-ng` | espeak backend: synthesizer executable in `PATH` |
| `voice` | `ru` | espeak-ng voice |
| `speed` | `150` | espeak-ng words per minute (80–400) |
| `queue_limit` | `16` | max queued phrases; overflow drops the oldest non-critical first |
| `wav_keep` | `64` | in `GCS_TTS_WAV_DIR` mode keep only the last N WAV files (`1`–`10000`) |
| `piper_bin` | `piper` | piper backend: executable |
| `piper_model` | — | piper backend: path to the `.onnx` voice model (required) |
| `piper_play` | `paplay` | piper backend: WAV playback command, may include arguments (`pw-play -q`) |
| `piper_length_scale` | `1.0` | piper backend: speech rate, `0.5` faster … `2.0` slower (clamped) |

## `[log]` — session recording

| Key | Default | Description |
|---|---|---|
| `enabled` | `true` | record every parsed frame to `<dir>/tlog-<date>-<time>.tlog` |
| `dir` | `logs` | directory (created if missing), relative to the working directory |

`.tlog` format: for each MAVLink frame — 8 bytes little-endian microseconds
since the Unix epoch, then the raw frame. Frames are self-synchronizing,
so the file can be replayed through any MAVLink parser.

## `[hotkey]` — status speech

| Key | Default | Description |
|---|---|---|
| `status_key` | `F2` | key sequence accepted by `QKeySequence` (`F2`, `Ctrl+S`, …); works while the window is focused |
| `global` | `true` | also grab the key globally via X11/XWayland (`XGrabKey`); without X11 or if the key is already grabbed, falls back to the window-only hotkey with a log message |

## `[ui]` — window and tray

| Key | Default | Description |
|---|---|---|
| `hide_on_close` | `true` | closing the window hides it to the system tray (when a tray is available); quit via the tray menu. Without a tray, closing exits as usual |

## Environment variables

| Variable | Effect |
|---|---|
| `GCS_TTS_WAV_DIR` | when set, every phrase is additionally rendered into a WAV file in this directory — for hosts without audio output (CI, WSL without sound). Retention controlled by `wav_keep` |
| `QT_QPA_PLATFORM` | Qt platform plugin; `offscreen` for headless runs |
