# PHD2 Loop Frame Server Output Spec

## Purpose

This document is the handoff/spec for a new `PHD2` fork and a separate Cursor
project/chat whose goal is to extend the `PHD2` server interface so
looping-but-not-guiding operation can expose live mount-axis drift data that is
usable by future downstream tools.

It includes:

- the product/problem context
- what has already been tried and verified
- what is currently missing from the public `PHD2` server API
- the recommended server/API extension
- implementation notes and source locations in `PHD2`
- suggested validation and PR strategy

This should be sufficient context for a fresh project/chat to continue without
having to reconstruct the prior reasoning.

## Project Context

The intended downstream use case is future tooling that needs live mount-axis
drift while `PHD2` is looping exposures with a selected star but guiding is
off.

That operating mode is important for tools that need to observe raw drift
without guide corrections mixed into the measurement stream.

## Why This Fork Is Needed

A downstream client can currently consume live RA/Dec-like error using the
`PHD2` `GuideStep` event.

That works while guiding is active because `GuideStep` contains:

- `dx`
- `dy`
- `RADistanceRaw`
- `DECDistanceRaw`

However, `GuideStep` is only emitted while guiding is active.

When guiding is off but exposures are still looping and a star is selected:

- `PHD2` app state can be `Looping` or `Selected`
- the downstream client's graph/data stream becomes empty
- no unguided RA/Dec drift samples are available through the current server
  event stream

So the present integration is correct for guiding mode, but wrong/incomplete
for non-guiding drift-capture workflows.

## What We Verified

### 1. `GuideStep` is guiding-only

From `PHD2` server/event docs and source:

- `GuideStep` is emitted for each frame while guiding
- it includes both camera-space and mount-space offsets

Relevant source:

- `src/event_server.cpp`

Observed payload construction:

- `dx`, `dy` come from `step.cameraOffset`
- `RADistanceRaw`, `DECDistanceRaw` come from `step.mountOffset`

### 2. `LoopingExposures` exists but does not include offsets

`LoopingExposures` is emitted during looping, but it only gives cadence/frame
state and not the guide-star drift values needed by downstream tooling.

### 3. `get_star_image` is not sufficient

At first glance, a possible workaround looked like:

1. use `LoopingExposures` as the per-frame trigger
2. call `get_star_image`
3. call `get_lock_position`
4. compute `star_pos - lock_position`
5. transform to mount `RA/Dec`

But source inspection shows that `get_star_image.star_pos` is returned in the
cropped star-image subframe coordinate system, not the full guider frame.

Relevant source:

- `src/event_server.cpp`

The implementation:

- gets `CurrentPosition()`
- crops a small rectangle around that star
- then subtracts `rect.GetLeft()` / `rect.GetTop()`
- returns `star_pos` relative to the crop

So:

- `get_lock_position` is full-frame
- `get_star_image.star_pos` is crop-local
- subtracting them does not produce true full-frame drift

Conclusion:

`get_star_image + get_lock_position` is not enough to reconstruct unguided
RA/Dec drift externally.

### 4. We verified the internal camera->mount transform

Source inspection in `PHD2` `src/mount.cpp` found the transform used to convert
camera-space offsets into mount-axis coordinates:

```cpp
double hyp = cameraVectorEndpoint.Distance();
double cameraTheta = cameraVectorEndpoint.Angle();

double xAngle = cameraTheta - m_cal.xAngle;
double yAngle = cameraTheta - (m_cal.xAngle + m_yAngleError);

mountVectorEndpoint.SetXY(cos(xAngle) * hyp, sin(yAngle) * hyp);
```

And:

```cpp
m_yAngleError = norm_angle(cal.xAngle - cal.yAngle + M_PI / 2.);
```

This means the public `get_calibration_data` RPC does expose enough
calibration-state information to describe the transform, but the missing piece
is the correct full-frame current-star position during unguided looping.

### 5. `PHD2` already has the right internal data

The data clearly exists internally:

- full-frame current star position
- full-frame lock position
- camera-space offset
- mount-space offset via the calibration transform

So this is not a math problem inside `PHD2`.
It is a server-output/API-surface gap.

## Product Requirement For The New Output

The new `PHD2` server output must allow a downstream client to:

- receive live sample updates while guiding is `OFF`
- use those samples for live plotting
- use those samples for drift capture/recording
- avoid relying on private/internal `PHD2` implementation assumptions

It must do this without changing the behavior of existing guiding clients that
already consume `GuideStep`.

## Compatibility Requirement

Do **not** change the meaning or emission conditions of `GuideStep`.

Reason:

- existing clients may interpret `GuideStep` as "guiding is active"
- existing logs/tools/automation may rely on current semantics
- reusing `GuideStep` for unguided loop frames would be a compatibility risk

The safe path is an additive extension:

- new event name
- optional new RPC(s)
- no breaking change to existing fields or existing event timing

## Recommended API Design

## New Event

Add a new event dedicated to selected-star loop frames while guide
corrections are not active.

Recommended event name candidates:

- `LoopFrameOffset`
- `SelectedStarLoopFrame`
- `StarOffset`

Preferred name:

- `LoopFrameOffset`

Reason:

- clearly tied to loop frames
- not ambiguous with guiding
- generic enough for wider tool development use

## Event Emission Conditions

Emit the new event when all of the following are true:

- camera exposures are looping
- a guide star is currently selected
- guide corrections are not being applied
- the current star position is valid
- the lock position is valid
- the current frame update succeeded

If calibration is not available:

- still emit the event with camera-space info
- include `Calibrated: false`
- include mount-axis fields only when valid

Paused-guiding behavior:

- if looping exposures continue while guiding is paused, still emit the event
- if exposures are fully paused, no new frame means no event

That gives clients more observability and avoids all-or-nothing behavior.

## Event Payload

Recommended payload:

- `Event`
- `Timestamp`
- `Host`
- `Inst`
- `Frame`
- `dx`
- `dy`
- `RADistanceRaw`
- `DECDistanceRaw`
- `StarPosition`
- `LockPosition`
- `Calibrated`

Field meanings:

- `dx`, `dy`
  - camera-space offset from lock position in full-frame coordinates
- `RADistanceRaw`, `DECDistanceRaw`
  - mount-space offset using the same transform semantics as guiding-time
    `GuideStep`
- `StarPosition`
  - full-frame current star position
- `LockPosition`
  - full-frame lock position
- `Calibrated`
  - whether mount-axis transform was valid for this frame

## Proposed Example Payload

```json
{
  "Event": "LoopFrameOffset",
  "Timestamp": 1774205123.412,
  "Host": "OBS-PC",
  "Inst": 1,
  "Frame": 284,
  "dx": 1.237,
  "dy": -0.418,
  "RADistanceRaw": 0.932,
  "DECDistanceRaw": -0.901,
  "StarPosition": [642.381, 511.927],
  "LockPosition": [641.144, 512.345],
  "Calibrated": true
}
```

If uncalibrated:

```json
{
  "Event": "LoopFrameOffset",
  "Timestamp": 1774205123.412,
  "Host": "OBS-PC",
  "Inst": 1,
  "Frame": 284,
  "dx": 1.237,
  "dy": -0.418,
  "StarPosition": [642.381, 511.927],
  "LockPosition": [641.144, 512.345],
  "Calibrated": false
}
```

## First-Pass Scope

The first implementation should be event-only:

- add `LoopFrameOffset`
- do not add `get_current_star_position` yet

`get_current_star_position` remains a possible follow-up if a polling-oriented
client use case appears later.

## Semantics Requirement

The new event should use the same conceptual raw mount-axis distances as
guiding `GuideStep` where possible.

That means:

- clients can reason about unguided and guided drift in the same coordinate
  space
- downstream tools can switch sources with minimal normalization differences

## Non-Goals

This proposal should **not**:

- change `GuideStep`
- change existing guiding behavior
- change guide log formats
- require any client to adopt the new event
- require clients to compute calibration transforms themselves

## Relevant PHD2 Source Areas

Fresh fork/project should start here:

- `src/event_server.cpp`
  - server RPCs
  - event emission
  - `GuideStep` payload construction
  - `get_lock_position`
  - `get_star_image`
  - `get_calibration_data`

- `src/mount.cpp`
  - `TransformCameraCoordinatesToMountCoordinates`
  - calibration geometry
  - `xAngle`, `yAngle`, parity, rates

- `src/guider.cpp`
  - star selection/current position/lock position behavior
  - looping vs guiding state transitions

## Likely Implementation Shape In PHD2

High-level approach:

1. Identify the loop-frame code path where a valid selected-star position is
   available even when guiding is off.
2. Compute the full-frame camera-space offset:
   - `current_star_position - lock_position`
3. If calibration is valid:
   - transform camera offset to mount `RA/Dec` using the existing internal
     transform
4. Emit new event to event-server clients

Important:

- do not reconstruct this from `get_star_image`
- use the same full-frame positions that `PHD2` already uses internally

## Suggested Internal API/Code Notes

If possible, reuse existing internal data structures rather than duplicating
transform logic in a new code path.

Ideal:

- one helper that produces:
  - camera offset
  - mount offset
  - frame number
  - timestamp
- called from:
  - guiding event path
  - looping-selected-star event path

That reduces the chance that guiding and unguided semantics drift apart later.

## Proposed Validation Plan

### Functional Validation

1. Start `PHD2`
2. Connect camera and mount
3. Select a star
4. Begin looping exposures
5. Keep guiding off
6. Verify new event is emitted each loop frame
7. Confirm:
   - `dx`/`dy` change as the star drifts
   - `RADistanceRaw`/`DECDistanceRaw` also change when calibrated
8. Clear calibration and confirm:
   - the event still emits
   - `Calibrated` is `false`
   - `RADistanceRaw` / `DECDistanceRaw` are omitted
9. Pause guiding while looping continues and confirm the event still emits

### Backward Compatibility Validation

1. Repeat standard guiding workflow
2. Verify `GuideStep` output is unchanged
3. Verify existing automation/logging clients behave normally

### Sanity Validation

Compare:

- unguided loop-frame mount offsets from the new event
- guiding-time `GuideStep` raw mount offsets

for a controlled scenario where the star is moved in a known direction.

The values do not need to be identical in all contexts, but the coordinate
system and signs should be consistent.

## Suggested Upstream Framing

This should be framed as:

- an additive event-server enhancement
- useful for future tool development, diagnostics, and other non-guiding
  tooling
- intentionally non-breaking for current guiding clients

It should **not** be framed as:

- changing how guiding clients work
- replacing `GuideStep`

## Decision Summary

What we have decided:

- `GuideStep` must remain untouched
- a new event is the preferred solution
- `get_star_image` is not sufficient for this use case
- the right unguided quantity is:
  - full-frame current star position
  - minus lock position
  - transformed to mount coordinates
- `PHD2` already has the required internal information
- this appears to be a server-output gap, not a fundamental algorithm gap

## Recommended Deliverables For The Fork

Minimum:

1. new unguided loop-frame event
2. docs update for event-server API
3. validation on real `PHD2`

Nice-to-have:

1. `get_current_star_position` RPC
2. small refactor to share offset-building logic between guiding and looping
   code paths
3. unit/integration test coverage if the `PHD2` codebase supports it cleanly

## Handoff Note For A New Cursor Project/Chat

If starting a fresh `PHD2` fork in a separate Cursor project:

- begin by reading this file first
- then inspect:
  - `src/event_server.cpp`
  - `src/mount.cpp`
  - `src/guider.cpp`
- preserve backward compatibility of `GuideStep`
- prefer additive server output
- do not use `get_star_image.star_pos` as the live unguided drift source

The key engineering target is:

Add a new event that emits unguided selected-star loop-frame offsets in the
same mount-coordinate space as `GuideStep`, without altering existing guiding
API behavior.
