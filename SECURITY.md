# Security Policy

## Supported versions

| Version | Supported |
|---|---|
| 0.2.x | yes |
| 0.1.x | no — please upgrade |

## Reporting a vulnerability

Please do **not** report security problems through public GitHub issues.

Use one of these channels instead:

1. **GitHub private vulnerability reporting** (preferred):
   [Report a vulnerability](https://github.com/EgorLikhachev/testTask/security/advisories/new).
2. Email the maintainer (see the commit author contact in the repository).
   If your email client supports it, encrypt the report and request a key.

Please include:

- a description of the issue and its impact;
- steps or a proof-of-concept to reproduce it;
- affected version (see the `CI`/`Docs` badge builds or `CHANGELOG.md`).

You will receive an acknowledgement within 7 days. Fixes for accepted
issues are released as promptly as practical and credited in the
changelog unless you prefer to remain anonymous.

## Scope notes

- The application is a **ground-side receiver**: it parses untrusted
  network input (MAVLink over UDP) by design. Parser robustness against
  malformed frames is covered by unit tests; crashes on hostile input are
  treated as security-relevant.
- The application does **not** control the vehicle — it only listens
  (plus heartbeats/stream requests). Reporting an issue because the app
  cannot *command* a vehicle is out of scope.
- The `.tlog` recording contains raw network traffic; treat logs as
  potentially sensitive when sharing them.
