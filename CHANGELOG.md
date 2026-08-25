# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.2.0] — 2026-08-25

### Added

- Voice announcements for link state: “Связь с бортом установлена”,
  “Потеря связи с бортом”, “Связь с бортом восстановлена”
  (`LinkMonitor`, `[link]` config section, separate anti-spam keys).
- Vehicle filtering by MAVLink system id: `vehicle_sysid` in `[udp]`
  (0 = auto-lock the first valid vehicle; other systems are ignored).
- Session telemetry recording to `.tlog` (raw MAVLink frames prefixed with
  8-byte little-endian microsecond timestamps): `[log]` config section.
- Message-stream request is re-issued after the link is regained (a
  rebooted vehicle used to stay silent for the rest of the session).
- Deduplication in the TTS queue: an identical phrase already queued or
  being spoken is not enqueued again.
- CI (GitHub Actions): build + ctest + synthetic integration test
  (pymavlink-generated frames, no ArduPilot required), AppImage packaging
  job with release attachment on `v*` tags, documentation checks.
- Synthetic integration test without ArduPilot
  (`scripts/synthetic_test.sh`) covering the full pipeline.
- English UI translation (`translations/`, selected by `QLocale`).
- MIT license, this changelog, repository templates and community files.

### Changed

- WAV debug output (`GCS_TTS_WAV_DIR`) now keeps only the last `wav_keep`
  files (default 64) instead of growing indefinitely.
- Anti-spam key table is capped at 512 entries — memory no longer grows on
  long sessions with a stream of unique STATUSTEXT messages.

## [0.1.0] — 2026-08-23

Initial release.

- Layered architecture per spec: UDP transport → MAVLink parser
  (c_library_v2) → domain model → Russian voice announcements (espeak-ng
  via QProcess).
- Announced events: flight mode change, arm/disarm, battery warning and
  critical thresholds with hysteresis, STATUSTEXT of severity WARNING or
  worse verbatim, status speech on a hotkey (altitude / speed / battery).
- Per-event-type anti-spam with intervals from an INI config.
- TTS queue in a worker thread — speech never blocks telemetry reception.
- Qt Widgets window: telemetry, status button, mute, event log.
- Unit tests (parser, event detector, anti-spam) and an automated SITL
  integration scenario (`scripts/integration.sh`).
- Manual acceptance testing methodology: `docs/MANUAL_TESTING.md`.

[Unreleased]: https://github.com/EgorLikhachev/testTask/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/EgorLikhachev/testTask/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/EgorLikhachev/testTask/releases/tag/v0.1.0
