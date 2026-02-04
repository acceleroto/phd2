# Multistar guiding (PHD2 `GuiderMultiStar`)

This document describes how PHD2’s **multistar guiding** works (the implementation historically exposed via the “Use multiple stars” setting), and how it compares to baseline **single-star guiding**.

Terminology used below (matching the code):

- **Primary star**: `m_primaryStar` (the “main” guide star in `GuiderMultiStar`)
- **Secondary stars / guide star list**: `m_guideStars` (`std::vector<GuideStar>`)
- **Reference point**: per-star `GuideStar::referencePoint` (the star’s baseline position used to compute its displacement)
- **Offset from primary**: `GuideStar::offsetFromPrimary` (computed during AutoFind; used to predict where a star should be)
- **Search region**: `m_searchRegion` (pixel radius for `Star::Find`)

---

## Big-picture overview

Multistar guiding in PHD2 is best described as:

1. **Always** tracking a single **primary** star every frame (that star is the authoritative “current position”).
2. Optionally using a set of **secondary stars** to **refine** the measured offset (the guide error) when guiding is stable.

Critically: multistar is still **primary-star-centric**. If the primary star cannot be found, multistar guiding behaves like single-star guiding in that it reports a lost-star condition.

---

## How multistar computes the guide error

### 1) Primary-star tracking (always)

Every exposure, PHD2 attempts to re-find `m_primaryStar` near its last known location (within `m_searchRegion`) using `Star::Find(...)`.

If found:

- `CurrentPosition()` is set to the primary star’s centroid.
- The **camera-space error** is computed as:

  - `cameraOfs = primary - lockPos`

If not found:

- The frame is considered a **lost star** frame (and guiding may pause / dead-reckon depending on state).

### 2) Secondary-star refinement (when conditions allow)

Multistar uses secondary stars only in a refinement step (`RefineOffset(...)`), and only when:

- Multi-star mode is enabled
- There are secondary stars in the list
- Guiding is active and not settling
- The system is not in a “stabilizing” phase

The refinement idea is:

- Each secondary star has a `referencePoint` recorded when the star list is built/refreshed.
- When a secondary star is found again, its displacement is computed:

  - `dX = X - referencePoint.X`
  - `dY = Y - referencePoint.Y`

- A weighted average displacement is formed (weights are based on relative SNR).
- If the refined displacement is “better” than the primary-only displacement (smaller distance), multistar replaces the camera offset with the refined one.

### 3) Star list creation (AutoFind)

When AutoFind is used and multi-star is enabled, `GuideStar::AutoFind(...)` populates a list of candidate stars across the full frame (it explicitly does not run on subframes).

The chosen “best” star becomes the **primary** (placed at list index 0), and for each secondary star:

- `offsetFromPrimary` is computed relative to the primary’s position at selection time
- `referencePoint` is set to the star’s position at selection time

---

## Interaction with subframes / ROI (“Use subframes”)

Multistar does **not** use secondary stars when subframes are enabled (because the image content does not contain the full field needed to track multiple stars).

Practically:

- With subframes enabled, multi-star behaves like single-star guiding (primary only).

---

## Pros vs single-star guiding

- **Reduced sensitivity to one-star centroid noise** (in stable conditions): averaging over multiple stars can reduce random centroid jitter.
- **Some robustness to transient issues affecting a secondary star**: multistar can drop individual secondary stars (hot pixels, repeated misses, suspicious excursions) during refinement.
- **Better field sampling** when AutoFind selects stars spread across the ROI (helps average local distortions/noise).

---

## Cons vs single-star guiding

- **More complexity**: more moving parts, more states (stabilizing, star list quality, heuristic drop rules).
- **Field-dependent behavior**: performance depends on having enough suitable stars, and on the distribution/quality of those stars.
- **Hidden “primary is king” coupling**: users may assume “multi-star” means “no single point of failure,” but multistar still depends on the primary being found each frame.

---

## Known limitations / improvement opportunities in multistar

These are behaviors that motivated the multistar2 work:

- **Primary-star single point of failure**  
  If the primary star is lost, multistar produces a lost-star frame, which can pause guiding or trigger dead reckoning. It does not seamlessly fail over to remaining stars.

- **Continuity on star-count change is not guaranteed**  
  Since the primary star is authoritative, losing the primary (or re-selecting/changing the star set) can cause discontinuities in the effective guiding position.

- **Lost-star warnings are primary-centric**  
  “Star lost” semantics are tied to the primary. Losing one secondary star may be handled quietly, but losing the primary is treated as a guiding interruption even if other stars are still present.

- **Subframe mode disables multi-star benefit**  
  With subframes enabled, multistar cannot effectively use multiple stars (by design/limitation of available pixel data).

---

## Summary

Multistar guiding improves guiding stability by optionally refining the primary star’s measured offset with information from secondary stars, but it remains fundamentally **primary-star-centric**. This yields benefits in typical conditions, but also leaves room for improvement in failover behavior and continuity when stars are lost or reacquired.

