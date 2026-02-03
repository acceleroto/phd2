## Q: Would it be possible to have a test set of images to test the different multistar guiders against each other?

Yes — and there are a couple practical levels of “test set” you can do, from simple to very controlled.

### Option A (practical, closest to real life): recorded guider frames + replay
- **What**: collect a folder of guider frames (your new **ImageLogger** “CameraFrames…” output is already most of this), plus the key metadata needed to interpret them (pixel size, binning, exposure, search region, lock position, calibration transform, etc.).
- **How you’d compare**: run an offline “replay” mode that feeds the same sequence of images to:
  - classic multistar
  - multistar2
  and logs each frame’s computed $dx,dy$, contributing-star set, and any discontinuities.
- **Pros**: uses real data, captures real star loss/reacquire conditions.
- **Cons**: needs some scaffolding (a harness to load frames and call the guider logic deterministically).

### Option B (controlled unit/regression tests): synthetic images
- **What**: generate synthetic star fields with known star PSFs, noise, drift, and controlled occlusions (turn stars on/off), then render image sequences.
- **How you’d compare**: same as above, but now you *know the ground truth* and can assert “no step when star N drops/reappears”.
- **Pros**: deterministic, can systematically sweep scenarios.
- **Cons**: more work up front; synthetic images may miss real-world weirdness.

### Option C (fastest “comparison” without a full replay harness): use logs as the test vector
- **What**: treat the debug log + guide log + new `MultiStar2:` membership-change lines as your dataset.
- **How you’d compare**:
  - For multistar2: detect each membership change and compute $\Delta dx,\Delta dy$ at that frame.
  - For classic: you’d need comparable membership-change logging (or infer it imperfectly).
- **Pros**: minimal engineering.
- **Cons**: not apples-to-apples unless both guiders report the same internal signals.

### The key thing you’d want to log/compare
To directly answer “what happens when stars are added/lost”, the comparison metric should be something like:
- **Step response at membership change**: $\Delta(dx,dy)$ right at the change
- **Short-term drift**: slope of $dx,dy$ over N frames after the change
- **RMS error** in windows before/after changes
- **Membership timeline**: which stars were used each frame

### Feasibility
**Absolutely feasible**, but PHD2 doesn’t currently have a turnkey “guider replay” test runner. The clean approach is to add a small offline harness that:
- loads a directory of saved frames (FITS)
- initializes a guider in a known state (lock pos, star list, search region, etc.)
- runs `UpdateCurrentPosition()` frame-by-frame
- dumps per-frame outputs to CSV for both guiders

If you want, we can sketch what minimum metadata we need to capture in the test bundle so the replay is truly comparable (e.g. lock position and per-star reference points matter a lot for multistar2 continuity).

