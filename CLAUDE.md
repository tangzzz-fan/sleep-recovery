# CLAUDE.md

This file provides guidance to Claude Code when working with code in this repository.

## Project Overview

Sleep Recovery — a macOS MVP for sleep recovery assessment using public wearable data. Not a medical tool; an end-to-end engineering demo spanning C++, Python, Core ML, and SwiftUI.

See `docs/spec-and-milestones.md` for the authoritative spec and milestone plan.

## Project Conventions

### Language & Terminology

- Use **recovery** over fatigue for the primary output
- Use **night session** for a single analyzed sleep event
- Use **feature package** for the Swift-side inference input
- Use **weak label** when the recovery target is rule-derived
- See `CONTEXT.md` for the full glossary

### Git Commits

- **Do NOT add `Co-Authored-By` or any co-author trailer to commit messages.**
- Commit messages should use Chinese (this repo's working language).
- Keep commits focused and atomic — one logical change per commit.

### Issue Tracker

- Issues and specs are tracked as local markdown files under `.scratch/`.
- See `docs/agents/issue-tracker.md` for conventions.

### Domain Docs

- This repo uses a single-context domain doc layout.
- Before exploring the codebase, read `CONTEXT.md` and `docs/adr/`.
- See `docs/agents/domain.md` for details.

### SwiftPM Resource Loading

- `swift-app/` is a Swift Package and does **not** require a standalone `.xcodeproj`.
- Core ML models used by the Swift app must live under `swift-app/SleepRecovery/Sources/Resources/Models/`.
- Model packages are bundled as SwiftPM resources via `Package.swift` using `.copy("Resources/Models")`.
- Runtime loading must use `Bundle.module`, not `Bundle.main`.
- `.mlpackage` resources should be loaded by compiling them first with `MLModel.compileModel(at:)`, then opening the returned `.mlmodelc`.
- Do not place `.mlpackage` directly under `Sources/` root, or SwiftPM/Xcode may treat it as a source input and trigger unwanted Core ML codegen behavior.

## Architecture

```
C++ CLI       →  data adaptation, normalization, feature extraction
Python/Colab  →  model training, comparison, adapter layer, Core ML export
Core ML       →  independent validation seam between training and macOS app
SwiftUI       →  macOS local inference, chart rendering, result presentation
```

Key constraint: Swift does NOT perform raw feature extraction in MVP — it consumes precomputed feature packages.

## Toolchain (macOS arm64)

| Tool | Version | Status | Notes |
|---|---|---|---|
| clang++ | Apple 17.0.0 | ✅ System | C++ build with Makefile (no CMake needed for MVP) |
| make | — | ✅ System | Simple Makefile, no extra deps |
| Python 3 | 3.9.6 | ✅ System | For local dev; Colab uses its own version |
| uv | 0.11.28 | ✅ Homebrew | Built-in venv (`uv venv`), no manual virtualenv; shared lockfile with Colab |
| Swift | 6.1.2 | ✅ System | With Swift Charts (system framework) |
| Xcode | 16.4 | ✅ System | macOS SDK 15.5, deployment target 14.0 |

### Toolchain Usage

**C++ build:**
```bash
cd cpp && make        # build the CLI
./sleep-recovery-cli --input ../data/raw/ --output ../data/features/
```

**Python (uv):**
```bash
cd python
uv venv                                   # create .venv (once)
uv pip install -r requirements.txt        # install deps
uv run python train.py                    # run script in .venv
uv run pytest tests/                      # run tests
```

**Python workflow:**
Local macOS with uv for fast iteration & unit tests → Google Colab for full training runs.
Same `requirements.txt` must work on both. On Colab, use `!pip install -r requirements.txt`.

**Swift:**
Open `swift-app/` in Xcode 16.4, or build from CLI:
```bash
cd swift-app
swift build
swift run SleepRecovery
xcodebuild -scheme SleepRecovery -destination 'platform=macOS' build
```

For Xcode-built products, the generated binary under DerivedData should load the bundled model without extra configuration.

## Key Files

| File | Purpose |
|---|---|
| `docs/spec-and-milestones.md` | Authoritative MVP spec + milestones |
| `.scratch/sleep-recovery-mvp/spec.md` | Full user stories and implementation decisions |
| `.scratch/sleep-recovery-mvp/issues/` | 7 actionable tickets (01–07) |
| `docs/sleep-recovery-project-plan.md` | Full project plan (MVP + mid + long term) |
| `docs/adr/0001-mvp-data-and-deployment-shape.md` | Architecture Decision Record |
| `CONTEXT.md` | Glossary and architecture shape |
| `data_readme.md` | Dataset selection analysis |
| `Fitabase Data 3.12.16-4.11.16/` | Downloaded Fitbit dataset (11 CSVs) |

## Current State

Planning stage complete, implementation not yet started. No source code exists yet. The downloaded Fitbit dataset has been analyzed: 12 users have both sleep and heart rate data (MVP primary training set). Next step is Ticket 01 — lock data contract and golden sample.
