# Contributing to mav-voice-gcs

Thanks for your interest in improving the project! This document covers
everything needed to start developing, submit changes and get them merged.

## Setting up a development environment

Follow the [Installation](README.md#installation) section of the README —
it results in a working build with unit tests passing. In short:

```bash
git clone --recurse-submodules https://github.com/EgorLikhachev/testTask.git
cd testTask
./scripts/setup_wsl.sh      # optional: installs packages + Qt 6.5.3 via aqtinstall
./scripts/build.sh          # configure + build + ctest
```

For WSL2 users: the build directory is moved to the Linux home
automatically; do not fight CMake on `/mnt/*` mounts.

## Before you submit

Run the full local check-list:

```bash
./scripts/build.sh                 # build + unit tests (3 suites)
./scripts/synthetic_test.sh        # synthetic integration test (~40 s, no SITL)
npx markdownlint-cli2 \
  README.md CONTRIBUTING.md SECURITY.md SUPPORT.md CHANGELOG.md CODE_OF_CONDUCT.md docs/*.md
```

The synthetic test requires `python3` with `pymavlink`
(`pip install --user --break-system-packages pymavlink`) and `espeak-ng`.

If your change touches voice phrases or event detection, also skim the
[manual testing methodology](docs/MANUAL_TESTING.md) — reviewers may ask for
a manual confirmation of the affected scenario.

## Branching model

The project uses a simple GitHub Flow:

- branch off `main`;
- one branch per topic, named `<type>/<short-slug>`:
  `feat/piper-tts-backend`, `fix/battery-hysteresis`, `docs/architecture`;
- merge into `main` via pull request after CI is green.

## Commit message convention

[Conventional Commits](https://www.conventionalcommits.org/) are used.
Real examples from this repository:

```text
fix(ci): explicit Qt install dir and QT_DIR for build steps
docs: comprehensive documentation set (English) and repo templates
feat: voice announcements for link lost/regained
```

Format: `<type>(<scope>): <subject>`, where type is one of `feat`, `fix`,
`docs`, `test`, `ci`, `refactor`, `chore`; scope is optional (e.g. `ci`,
`packaging`, `parser`). Keep the subject line ≤ 72 characters, imperative
mood. Add a body paragraph for anything non-obvious.

## Pull request process

1. Fill in the [pull request template](.github/PULL_REQUEST_TEMPLATE.md).
2. Make sure CI passes: build + unit tests + synthetic integration test
   (the `CI` workflow) and documentation checks (the `Docs` workflow).
3. A maintainer reviews; small focused PRs are usually reviewed within days.
4. Squash-merge keeps history readable — your PR title becomes the commit
   message, so make it Conventional-Commits-clean.

## Reporting issues

Use the issue templates ([bug report](.github/ISSUE_TEMPLATE/bug_report.md),
[feature request](.github/ISSUE_TEMPLATE/feature_request.md)).
For bug reports, attach:

- application console/log output (`[announce]`/`[event]` lines),
- the `.tlog` file of the session if available (see `[log]` in the config),
- SITL/vehicle type and ArduPilot version when relevant.

Security problems must not go through public issues — see
[SECURITY.md](SECURITY.md).

## Code style

- C++17. No tabs: 4 spaces. Keep lines readable (soft limit ~100 columns).
- One class per file pair (`Foo.h` + `Foo.cpp`), file named after the class.
- Comments explain *why* and state responsibilities, in the same language
  and density as the surrounding code (Russian is the existing convention
  for source comments; English is fine for new subsystems).
- Layer discipline (see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)):
  `domain` must not include MAVLink headers; `transport` moves bytes only;
  UI never talks to the parser directly.
- User-visible strings in the UI go through `tr()` and are added to
  `translations/mav-voice-gcs_en.ts`.
- New user-facing events must be covered by a unit test (see `tests/`) and,
  where practical, by a synthetic-test check in `scripts/synthetic_driver.py`.
