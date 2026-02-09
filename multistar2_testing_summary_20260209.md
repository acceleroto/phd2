# Multistar2 testing summary (as of 2026-02-09)

This file summarizes **testing performed to date** for the experimental `multistar2` guider (`GuiderMultiStar2`) and how results map to the project’s stated goals (continuity under star loss/reacquire; avoid nuisance “Star lost” unless all stars are lost; usable in real imaging sessions).

---

## Test environment (from logs)

- **PHD2 build**: `2.6.13dev8-multistar2` (Windows)
- **Pixel scale**: `2.55 arc-sec/px`
- **Guide scope focal length**: `162 mm`
- **Camera**: `G3M678M`, 3840×2160, binning 1, exposure typically `1500 ms`
- **Mount**: `DeviceHub -> Gemini Telescope .NET (ASCOM)`
- **RA algorithm**: Predictive PEC; **Dec algorithm**: Resist Switch

---

## Testing performed

### 1) Live guiding sessions using multistar2 (February 2026)

#### 2026-02-05 (GuideLog `PHD2_GuideLog_2026-02-05_012841.txt`)

- **Guiding runs**: 10 (multiple short runs + longer run later in the file)
- **Observed MultiStar2 list sizes**: examples include 1, 2, 4 (from log headers)
- **Star-lost / DROP frames**: 18 total (≈0.29% of all frames in this file)
  - **Top DROP reasons**:
    - 13× `Star lost`
    - 5× `Star lost - low HFD`

**Interpretation vs goals**
- **Avoid nuisance “Star lost”**: mixed; there were some DROP frames, often correlated with HFD constraints (suggesting poor star quality / star-finder issues rather than simple single-star loss).
- **Continuity when membership changes**: not directly visible in GuideLog (membership/contributing-set changes are debug-log-level), but there was no clear evidence in this file of systematic “step” behavior tied to star set changes.

#### 2026-02-07 to 2026-02-08 (GuideLog `PHD2_GuideLog_2026-02-07_173156.txt`)

- **Guiding runs**: 5
- **Observed MultiStar2 list sizes**: 10, 4, 7, 7, 6 (per-run headers)
- **Star-lost / DROP frames**: 0 total (0.00% for the entire file)

Per-run behavior (dx/dy RMS in pixels, computed from GuideLog):
- **Run 1 (list size 10)**: RMS ≈ 0.743 px, no extreme frames
- **Run 2 (list size 4)**: mount safety stop near meridian (reported)
  - 59/60 frames had extreme offsets; pulses were pegged at max
  - This run is treated as **mount-limited behavior**, not a multistar2 failure
- **Run 3 (list size 7)**: RMS ≈ 0.756 px, no extreme frames
- **Run 4 (list size 7)**: RMS ≈ 0.783 px, no extreme frames
- **Run 5 (list size 6)**: RMS ≈ 0.636 px, no extreme frames

**Interpretation vs goals**
- **Avoid nuisance “Star lost”**: strong; no DROP frames across the session, consistent with “only lost when all stars are lost.”
- **Continuity under loss/reacquire**: not directly measurable from GuideLog alone, but the session overall is consistent with stable guiding when the mount is behaving normally.
- **Mount safety stop segment**: highlighted a “guardrails” scenario (when the mount cannot respond, PHD2 will naturally rail pulses). This is useful for robustness testing but should not be conflated with normal multistar2 performance.

---

### 2) Debug-log based multistar2 assessment (February 2026)

This section uses `PHD2_DebugLog_...` files to evaluate multistar2’s goals that are not visible in GuideLog alone (notably: whether guiding continues when the *original primary star* stops contributing, and whether membership changes cause systematic steps).

#### 2026-02-07 (DebugLog `PHD2_DebugLog_2026-02-07_173156.txt`)

The debug log includes multistar2 internal lines of the form:
- `MultiStar2: pool=... found=... used=... primaryContrib=... added=[...] removed=[...] disp=(...) dDisp=(...) ...`

**Highlights**
- **Primary not required for continuity**: `primaryContrib==0` occurred on **6080/13068 (46.53%)** of multistar2 solution lines, and **every one** of those lines still had `used>0` (i.e., the aggregate solution continued using non-primary stars).
- **Reacquire gating exercised**: `MultiStar2: reacquire idx=...` appeared **4047** times, showing that lost/reacquired-star handling was frequently triggered in real data.
- **Membership-change step sizes (from `dDisp`)**:
  - Across **6714** membership-change events (`added` or `removed` non-empty), and excluding obvious outliers (`|dDisp| > 10 px`):
    - median \(|dDisp|\) ≈ **0.524 px**
    - p90 \(|dDisp|\) ≈ **1.294 px**
    - p99 \(|dDisp|\) ≈ **2.819 px**
    - max (non-outlier) \(|dDisp|\) ≈ **6.358 px**
- **Large outliers correlate with mount-limited behavior**: there were **12** membership-change events with \(|dDisp| > 10 px\), with the largest coinciding with the reported mount safety-stop segment near the meridian.

#### 2026-02-09 (DebugLog `PHD2_DebugLog_2026-02-09_153233.txt`)

- This debug log contains startup + shutdown only (no guiding run), and therefore contains **no** multistar2 solution/membership lines (`MultiStar2: pool=...`).

---

### 2) Initial on-sky shakedown (January 2026)

Based on project notes and artifacts:
- At least one on-sky test session occurred under poor seeing, during which:
  - a crash and several small bugs were identified and fixed
  - additional debug logging was added to aid analysis
- A crashlog bundle exists in `_crashlogs/174028/` with:
  - `PHD2_DebugLog_2026-01-28_174028.txt`
  - `PHD2_GuideLog_2026-01-28_174028.txt`

---

## What we have *not* yet validated (or needs more data)

- **Primary-loss failover evidence**: to prove “no single point of failure,” we need to correlate:
  - multistar2 contributing-set changes (debug log `MultiStar2:` membership-change lines)
  - guide error continuity (GuideLog dx/dy) at those same timestamps
- **Quantitative continuity metric**: step size \(\Delta(dx,dy)\) distribution at membership changes, excluding dithers/settling.
- **Behavior under intentionally forced star loss**: e.g., partial occlusions / clouds / deliberate ROI/subframe constraints.

---

## Known anomalies / notes

- **Mount safety stop near meridian (2026-02-07 run 2)** produced pegged pulses and huge dx/dy. This is primarily an external constraint, but it is a good “robustness under mount non-response” scenario.

---

## Next recommended tests (high value)

- Collect matching **DebugLog** files for the sessions and compute:
  - membership-change timeline
  - continuity metrics at each star loss/reacquire
- Repeat on a night with average/better seeing and capture a longer continuous run with multiple natural star drop/reacquire events.

