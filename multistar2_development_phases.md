# Multi-star guiding development: Phases A, B, C

This document summarizes the changes made to the PHD2 codebase while developing the experimental multi-star guider (“multistar2”), organized by phases.

It intentionally does **not** cover local build environment setup (CMake/wxWidgets/etc.).

---

## Phase A — Scaffold + opt-in selection (no behavior change)

### Goal

Create a safe, opt-in path for a new multi-star guider implementation without disturbing the existing (classic) guider behavior.

### What we changed in PHD2 (infrastructure/UI)

- **Added a new guider class** (scaffold)
  - Added `src/guider_multistar2.h`
  - Added `src/guider_multistar2.cpp`
  - Implemented `GuiderMultiStar2` as a thin wrapper around `GuiderMultiStar`, primarily to create a separate class boundary for future behavioral changes.

- **Added an Advanced Settings selector**
  - Updated `src/guider_multistar.cpp` / `src/guider_multistar.h` to add a UI drop-down next to “Use multiple stars”:
    - **Classic (multistar)**
    - **Experimental (multistar2)**
  - Persisted selection in the profile at `"/guider/multistar/implementation"` (classic is default when unset).

- **Enabled safe runtime guider swapping**
  - Updated `src/myframe.cpp` / `src/myframe.h` to create the guider via a factory method that selects between classic and multistar2.
  - Added a safe mechanism to destroy/recreate the guider when the selection changes (only when not capturing).

- **Build system integration**
  - Updated `CMakeLists.txt` to include the new multistar2 source files.
  - Updated `src/guiders.h` to include the new guider header.

### What we added in the guider

- `GuiderMultiStar2` existed and could be selected, but it was intentionally behavior-equivalent to classic multi-star.

---

## Phase B — “No single point of failure” (primary-loss failover)

### Goal

Ensure that when multi-star guiding is enabled, the guider does not drop frames / report StarLost just because one star (including the original primary) is lost—**as long as at least one usable star remains**.

### What we changed in PHD2 (supporting refactors)

- Updated `src/guider_multistar.h` to make classic guider internals available to the derived class:
  - Promoted key state (e.g. `m_primaryStar`, `m_guideStars`, and related flags/parameters) to `protected` so multistar2 can implement new logic cleanly.

### What we added/changed in the guider

- Implemented Phase B logic in `GuiderMultiStar2` by overriding:
  - `UpdateCurrentPosition(...)`
  - `CurrentPosition()`
  - `PrimaryStar()`
  - `IsLocked()`
  - `GetStarCount()`
  - `InvalidateCurrentPosition(...)`

- Added internal state to distinguish:
  - **Aggregate “solution” point** (used for guiding offsets)
  - **Display star** (a real found star used for UI/status metrics like SNR/HFD/Mass)

- Behavior outcome:
  - If the original primary is lost but other stars are still found, guiding continues and the guider does **not** emit StarLost.
  - StarLost occurs only when **no stars are found**.

---

## Phase C — Continuity + gating + per-star mass check + UI overlays

### Goal

Make multi-star failover **continuous and stable**:

- No guiding solution “jump” or “drift” when stars are lost or reacquired.
- Reacquired stars should not “pull” the solution.
- Per-star mass-change rejection (bad star does not break guiding).
- Clear UI indicators of contributing vs lost stars and multistar2 status.

### What we added/changed in the guider

- **Continuity-safe solution math**
  - Switched from an “absolute estimated primary” model to a **displacement-from-reference** model:
    - Each star contributes its displacement from its `referencePoint`.
    - Displacements are SNR-weighted and averaged.
    - The aggregate solution point is computed from lock position + aggregate displacement.
  - This makes the solution stable across star membership changes.

- **Reacquire gating (anti-pull)**
  - Implemented per-star gating: a lost star must be “good” for a small number of consecutive frames before it is allowed to contribute again (currently 3 frames).
  - When a star becomes eligible again, its `referencePoint` is pinned so it does not shift the aggregate solution.

- **Per-star mass-change rejection**
  - Implemented per-star mass history tracking (median-based thresholding, exposure-normalized for auto-exposure).
  - If a star fails the mass-change test, it is excluded from contributing (treated as a bad measurement), without generating StarLost unless all stars are unusable.

- **UI overlays and status text (multistar2 visual identity)**
  - Implemented `GuiderMultiStar2::OnPaint(...)` to provide multistar2-specific overlays:
    - Green circles around **contributing** stars
    - Orange dotted circles around **non-contributing/lost** stars
    - Aggregate solution marker: green box with outward ticks (a “hollow +” signature) so users can see they’re using multistar2
    - Bottom-right status text: `Multi-stars: X/Y`
      - `X` = contributing stars now
      - `Y` = max concurrently contributing stars since star selection (session max)

### What we changed in PHD2 (non-guider code)

- macOS/arm64 build robustness: updated `thirdparty/thirdparty.cmake` to avoid linking bundled x86_64-only camera SDKs on Apple Silicon (while keeping arm64-capable SDKs enabled).

---

## Files added/changed (quick index)

### New files
- `src/guider_multistar2.h`
- `src/guider_multistar2.cpp`
- `multistar_description.md`
- `multistar2_description.md`
- `multistar2_phases.md` (this document)

### Updated files (high level)
- `src/guider_multistar.h` (enable derived-class access to classic state)
- `src/guider_multistar.cpp` (Advanced Settings UI selector + persistence)
- `src/myframe.h`, `src/myframe.cpp` (guider selection + safe recreate)
- `src/guiders.h` (include new guider)
- `CMakeLists.txt` (build integration)
- `thirdparty/thirdparty.cmake` (macOS/arm64 camera SDK gating)

