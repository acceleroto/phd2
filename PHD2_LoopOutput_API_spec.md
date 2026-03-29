# PHD2 LoopOutput API Spec

## Purpose

This document describes the server-side outputs added by this fork that are
relevant to downstream integrations, especially Cursor + GPT-5.x agents that
need to consume live `PHD2` drift data while looping exposures.

The main new server output in this fork is the event:

- `LoopFrameOffset`

This doc is intentionally practical. It includes:

- the transport/wire format
- the event envelope fields added to every event
- the exact `LoopFrameOffset` payload
- when the event is emitted
- how it relates to existing `PHD2` events and RPCs
- integration gotchas that will save time for downstream coding agents
- repo-specific build notes useful when testing this fork locally

## Scope

This spec focuses on the `PHD2` event server output used by downstream clients.

It does **not** redefine the entire `PHD2` server protocol. Instead, it
documents the new additive behavior in this fork and the nearby existing API
surface a client will usually need when consuming it.

## Executive Summary

The new event is:

- `LoopFrameOffset`

It gives downstream clients per-frame selected-star drift while:

- exposures are looping, or guiding is paused but loop updates still occur
- a valid current star position exists
- a valid lock position exists

The event includes:

- camera-space offsets: `dx`, `dy`
- full-frame star and lock positions: `StarPosition`, `LockPosition`
- calibration status: `Calibrated`
- mount-space offsets when available: `RADistanceRaw`, `DECDistanceRaw`

This solves the gap where existing clients could get mount-axis drift from
`GuideStep` only while guiding was active.

## Transport

`PHD2` event-server messages are sent as:

- one JSON object per message
- UTF-8 encoded
- terminated by `\r\n`

Practical client rule:

- treat the event stream as CRLF-delimited JSON objects
- do not assume pretty-printed JSON
- do not assume fixed field order

## Event Envelope

Every event created through the standard event wrapper includes these fields:

- `Event`
- `Timestamp`
- `Host`
- `Inst`

These are added automatically before event-specific fields.

### Envelope Field Types

- `Event`: string
- `Timestamp`: number, Unix timestamp in UTC seconds with millisecond precision
- `Host`: string, host name from `wxGetHostName()`
- `Inst`: integer, `PHD2` instance number

## Connection Behavior

When a client connects to the event server, `PHD2` immediately sends several
"catch-up" events representing current state. A client should expect some or
all of these before new live frame events begin:

- `Version`
- `LockPositionSet`
- `StarSelected`
- `CalibrationComplete`
- `StartGuiding`
- `StartCalibration`
- `Paused`
- `AppState`

Practical parser rule:

- your client must ignore unknown events cleanly
- your client should not assume the first live event will be `LoopFrameOffset`

## New Event: `LoopFrameOffset`

### Why It Exists

Existing `PHD2` clients can already consume:

- `GuideStep`

But `GuideStep` is emitted only while guiding is active.

For looping-with-star-selected workflows, downstream tools often need drift
without guide corrections mixed in. This fork adds an additive event for that
case instead of changing `GuideStep`.

### Event Name

- `LoopFrameOffset`

### Event Payload

Always present:

- `Event`
- `Timestamp`
- `Host`
- `Inst`
- `Frame`
- `dx`
- `dy`
- `StarPosition`
- `LockPosition`
- `Calibrated`

Conditionally present:

- `RADistanceRaw`
- `DECDistanceRaw`

The mount-axis fields are present only when calibration is valid for that
frame.

### Field Definitions

- `Frame`
  - integer frame number from `pImage->FrameNum`
- `dx`
  - camera-space X offset in pixels
  - value comes from `ofs.cameraOfs.X`
  - formatted to 3 decimal places
- `dy`
  - camera-space Y offset in pixels
  - value comes from `ofs.cameraOfs.Y`
  - formatted to 3 decimal places
- `StarPosition`
  - full-frame selected-star centroid coordinates
  - JSON array: `[x, y]`
- `LockPosition`
  - full-frame lock position coordinates
  - JSON array: `[x, y]`
- `Calibrated`
  - boolean
  - `true` when `ofs.mountOfs.IsValid()`
- `RADistanceRaw`
  - mount-axis raw X/RA-like offset
  - present only when calibrated
  - formatted to 3 decimal places
- `DECDistanceRaw`
  - mount-axis raw Y/Dec-like offset
  - present only when calibrated
  - formatted to 3 decimal places

### Canonical JSON Example

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
  "Calibrated": true,
  "RADistanceRaw": 0.932,
  "DECDistanceRaw": -0.901
}
```

### Uncalibrated Example

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

### Exact Emission Conditions

The event is emitted from `Guider::UpdateGuideState()` after star-position
update logic succeeds.

The current code emits it when all of the following are true:

- `IsLoopingState(m_state)` is true
  - or the guider is paused and `m_state == STATE_GUIDING`
- `CurrentPosition().IsValid()`
- `LockPosition().IsValid()`

In code terms:

```cpp
bool emitLoopFrameOffset =
    (IsLoopingState(m_state) || (IsPaused() && m_state == STATE_GUIDING)) &&
    CurrentPosition().IsValid() &&
    LockPosition().IsValid();
```

This means:

- it is available during normal looping with a selected star
- it can also appear while guiding is paused if loop-frame updates still occur
- it is not emitted when star position or lock position is invalid
- it is not emitted if the frame update failed before reaching this point

### Ordering Relative To `LoopingExposures`

In the current code path:

1. `LoopFrameOffset` is emitted first
2. `LoopingExposures` is emitted afterward when `IsLoopingState(m_state)` is
   true

Practical client advice:

- do not rely on strict cross-event ordering unless your client actually needs
  it
- if you do use ordering, current code emits `LoopFrameOffset` before
  `LoopingExposures` for the same frame

## Relationship To Existing Events

### `GuideStep`

`GuideStep` remains unchanged and is still the source of guiding-time offsets.

Use:

- `GuideStep` while guiding is active
- `LoopFrameOffset` while looping with a selected star and guide corrections are
  not being applied

Important compatibility rule:

- do not reinterpret `GuideStep`
- do not assume `LoopFrameOffset` means guiding is active

### `LoopingExposures`

`LoopingExposures` still exists and is still useful as a general loop-frame
heartbeat, but it does not expose full drift geometry by itself.

Typical downstream use:

- `LoopingExposures` for general loop state
- `LoopFrameOffset` for actual drift data

### `Paused`

Clients may receive `Paused` and still continue to receive `LoopFrameOffset`
events if `PHD2` is in the paused-guiding path where image updates continue.

## Relationship To Existing RPCs

### `get_lock_position`

`get_lock_position` returns the full-frame lock position.

This is useful for state inspection, but `LoopFrameOffset` already includes
`LockPosition`, so a client usually does not need to call this RPC per frame.

### `get_star_image`

Do **not** use `get_star_image.star_pos` to reconstruct the same values as
`LoopFrameOffset`.

Reason:

- `get_star_image.star_pos` is returned in the cropped subimage coordinate
  system
- it is **not** in full-frame coordinates

The implementation explicitly subtracts the crop origin before returning
`star_pos`.

So:

- `get_lock_position` is full-frame
- `get_star_image.star_pos` is crop-local
- subtracting them is wrong

For this fork, use `LoopFrameOffset.StarPosition` and `LoopFrameOffset.dx/dy`
instead.

### `get_calibration_data`

This RPC remains useful if a downstream tool wants additional calibration
context, but for normal live drift consumption a client can simply trust:

- `Calibrated`
- `RADistanceRaw`
- `DECDistanceRaw`

from `LoopFrameOffset`.

## Coordinate Semantics

### Camera-Space

`dx` and `dy` come from:

- `ofs.cameraOfs`

These are camera-space offsets between the current selected star and lock
position in the same full-frame coordinate space used internally by the guider.

### Mount-Space

`RADistanceRaw` and `DECDistanceRaw` come from:

- `ofs.mountOfs`

These use the same raw mount-coordinate concept already used by `GuideStep`.

Practical downstream rule:

- if you already know how to consume `GuideStep.RADistanceRaw` /
  `GuideStep.DECDistanceRaw`, consume `LoopFrameOffset` the same way

## Recommended Client Strategy

### Minimum Robust Parser

1. Connect to the event server.
2. Read CRLF-delimited UTF-8 JSON objects.
3. Dispatch by `Event`.
4. Ignore unknown events.
5. For `LoopFrameOffset`:
   - read `Frame`, `dx`, `dy`, `StarPosition`, `LockPosition`, `Calibrated`
   - read `RADistanceRaw` / `DECDistanceRaw` only if present

### Suggested Event Handling Rules

- treat missing `RADistanceRaw` / `DECDistanceRaw` as "uncalibrated", not as
  zero
- do not poll `get_star_image` to reconstruct drift
- do not require `LoopingExposures` if `LoopFrameOffset` alone is enough
- keep support for both `GuideStep` and `LoopFrameOffset` if your tool must work
  in both guiding and non-guiding modes

### Suggested Data Model

For integration code, normalize to something like:

```json
{
  "source": "LoopFrameOffset",
  "frame": 284,
  "timestamp": 1774205123.412,
  "camera": { "dx": 1.237, "dy": -0.418 },
  "mount": { "ra": 0.932, "dec": -0.901, "valid": true },
  "star": { "x": 642.381, "y": 511.927 },
  "lock": { "x": 641.144, "y": 512.345 }
}
```

That makes downstream plotting and logging simpler than working directly off raw
JSON dictionaries everywhere.

## Practical Gotchas

### 1. `LoopFrameOffset` Is Additive

Older upstream `PHD2` builds will not emit it.

Client rule:

- detect by `Event == "LoopFrameOffset"`
- if absent, fall back to whatever older behavior your tool supports

### 2. `LoopFrameOffset` Is Not Guaranteed On Every Connection Immediately

You may connect while:

- no star is selected
- lock position is invalid
- no frame updates are happening

Client rule:

- wait for live events instead of assuming immediate availability

### 3. `LoopFrameOffset` Does Not Replace All Other Events

A full client may still care about:

- `Version`
- `AppState`
- `StarSelected`
- `LockPositionSet`
- `CalibrationComplete`
- `LoopingExposures`
- `GuideStep`

### 4. Do Not Assume Field Order

The event builder appends fields in a stable order today, but JSON object order
should not be treated as part of the protocol contract.

### 5. `Frame` Is The Best Per-Image Correlation Key

If your client wants to correlate:

- `LoopFrameOffset`
- `LoopingExposures`
- `GuideStep`
- image-related RPC results

use the frame number first and timestamp second.

## Source References

Relevant implementation locations in this fork:

- `src/event_server.cpp`
  - event envelope construction
  - `LoopingExposures`
  - `LoopFrameOffset`
  - `get_lock_position`
  - `get_star_image`
- `src/event_server.h`
  - `NotifyLoopFrameOffset` declaration
- `src/guider.cpp`
  - actual emission condition and call site
- `src/guider.h`
  - `GuiderOffset`

Most important source facts:

- `Ev` automatically adds `Event`, `Timestamp`, `Host`, `Inst`
- `LoopFrameOffset` is emitted from `Guider::UpdateGuideState()`
- `get_star_image.star_pos` is crop-local and should not be used to derive the
  same quantity

## Repo-Specific Build Notes For Contributors

These are not required for a client that only consumes the API, but they are
useful if a coding agent needs to build and test this fork locally.

### Windows Build Notes Observed In This Fork

- `wxWidgets 3.0.5` worked correctly for Unicode event-server builds
- `wxWidgets 3.2.10` caused the generated Windows build to come out as
  `MultiByte`, which broke Unicode-sensitive compilation in this fork's build
  environment
- in a fresh build tree, building `indi` before `phd2` avoids missing
  `libindi/baseclient.h` errors because the external-project dependency is not
  forced by default

### Known-Good Build Sequence

```powershell
cmake -S . -B build/Win32 -A Win32 -DwxWidgets_PREFIX_DIRECTORY=C:/wxWidgets-3.0.5
cmake --build build/Win32 --config Release --target indi
cmake --build build/Win32 --config Release --target phd2
```

## Bottom Line

If you are writing downstream integration code for this fork:

- consume the event socket as CRLF-delimited JSON
- look for `Event == "LoopFrameOffset"`
- use `dx`, `dy`, `StarPosition`, `LockPosition`, and `Calibrated`
- use `RADistanceRaw` / `DECDistanceRaw` only when present
- do not reconstruct this data from `get_star_image.star_pos`

That is the intended, direct, and reliable integration path for the new
looping-with-selected-star server output in this fork.
