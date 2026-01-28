# Experimental multi-star guiding (PHD2 `GuiderMultiStar2`)

This document describes **multistar2**, the experimental multi-star guider implementation added as an opt-in alternative to the classic multi-star guider.

Multistar2 is selected via Advanced Settings, and is intentionally implemented as a separate guider class so it can evolve without destabilizing the classic guider.

---

## Big-picture overview

Multistar2 keeps the same overall user intent (“guide using multiple stars”) but changes the *core tracking model* to address classic multi-star’s main shortcomings:

- **No primary-star single point of failure**: guiding can continue when the originally selected primary star is lost, as long as at least one usable guide star remains.
- **Continuity when star membership changes**: the guiding solution is computed in a way that prevents systematic jumps/drift when stars are lost or reacquired.
- **Reduced nuisance lost-star handling**: “StarLost” conditions are only produced when **all** stars are lost, not when any single star drops out.

Multistar2 still respects the existing “Use subframes” behavior: if the camera is operating with subframes/ROI, multi-star operation is effectively constrained (similar to classic behavior).

---

## Compatibility goals

Multistar2 aims to be compatible and intuitive for existing users and integrations:

- **External clients (server/event mode)**: guide error streams (`dx/dy`, RA/Dec distances) remain tied to the **guiding solution** (aggregate), which is what downstream graphs should represent.
- **Star profile panel**: star image metrics (HFD/SNR/Mass/Peak) should remain tied to a **real measured star** (not a synthetic aggregate point), since those metrics describe a single star’s profile.

---

## Feature-by-feature description (implemented)

### 1) Opt-in implementation selection (Classic vs Experimental)

Multistar2 is exposed as an opt-in choice in Advanced Settings (alongside “Use multiple stars”). The profile stores the selection so users can switch back and forth.

- **Similar to classic**: the user still enables multi-star guiding from Advanced Settings.
- **Different from classic**: the multi-star algorithm can be switched between implementations without rebuilding.

### 2) Aggregate guiding solution (solution point)

Multistar2 maintains an internal **aggregate solution point** that represents the “effective primary” star position used for guiding (i.e., for computing the lock error).

- This aggregate point is what drives camera-space offsets and therefore what external guiding error graphs represent.
- The aggregate point is drawn as the primary marker in the guider image.

### 3) Continuity-safe multi-star math (no jump / no drift on star set changes)

Classic multi-star refines the primary’s offset using secondary star displacements, but remains anchored to the primary. Multistar2 instead computes the guiding solution using:

- per-star displacement from that star’s **reference point**
- weighted averaging (SNR-weighted)

This means if one star disappears (or reappears), the guiding solution does not systematically jump to a new “absolute” pointing just because membership changed.

### 4) Reacquire gating (prevent “pull” when stars come back)

When a previously-lost star reappears, multistar2 does not immediately let it affect the solution. Instead it requires a small run of good detections (currently 3 frames) before the star can contribute again.

When the star becomes eligible again, its reference is pinned so it does not pull the solution to a new value.

- **Improvement vs classic**: avoids subtle drift or steps caused by newly reacquired stars entering the averaging set.

### 5) Per-star mass-change rejection

Classic mass-change handling is primary-centric. Multistar2 applies a mass-change check **per star**, and if a star’s mass appears inconsistent it is excluded from contributing (treated as a bad measurement).

- **Improvement vs classic**: one “bad” star does not force a lost-star frame; it simply stops contributing.

### 6) Lost-star semantics

Multistar2 only produces a lost-star condition when **no stars are available**. Losing an individual star does not cause StarLost.

This directly supports the “continue guiding unless all guide stars are lost” requirement.

### 7) UI overlays

Multistar2 provides additional visual cues in the guider image panel:

- **Contributing stars**: green circles
- **Non-contributing / lost stars**: orange dotted circles at last-known positions
- **Primary marker**: green box around the aggregate solution, with outward ticks (to visually distinguish multistar2 from classic)
- **Status text**: bottom-right “Multi-stars: X/Y” where:
  - `X` = number of stars contributing now
  - `Y` = maximum number of concurrently contributing stars seen in this guiding session

### 8) Star profile panel behavior

The star profile panel continues to show meaningful star metrics by using a **display star** (the best contributing measured star) for:

- Mass
- SNR
- HFD / FWHM
- Peak

Meanwhile, the guiding solution (and therefore the error stream) is based on the aggregate solution point.

This preserves user expectations: the profile is about a star image, not about a synthetic aggregate.

---

## Similarities to classic and baseline guiders

- **Similar to classic multi-star**:
  - Uses a list of guide stars and tracks them frame-to-frame.
  - Uses the same underlying star finding (`Star::Find`) and the same general configuration knobs (search region, HFD constraints, etc.).
  - Respects the “Use subframes” limitation.

- **Similar to baseline single-star guiding**:
  - Ultimately produces a per-frame camera-space offset relative to the lock position.
  - Integrates with the same guider state machine and event reporting.

---

## Summary of improvements vs classic multi-star

- Continues guiding when the original primary star is lost (no single point of failure).
- Ensures continuity of the guiding solution when stars drop in/out.
- Suppresses lost-star behavior unless all stars are lost.
- Adds clearer UI overlays and a status count indicating how many stars are contributing.

