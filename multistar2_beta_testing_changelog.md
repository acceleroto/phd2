# Multistar2 Beta Testing Changelog

## Beta 1 - 2026-02-06

- Initial beta release for external testing.

## Beta 2 - 2026-02-16

- Added/updated BSD license headers in `multistar2` source files.
- Improved instrumentation to more closely match `multistar` support/debug visibility.
  - Added structured `MultiStar2` debug lines for frame outcome, reject breakdown, and recovery state transitions.
  - Added distance-based success-frame image logging parity and explicit recovery-path logging/reset behavior.
- Added behavioral guardrails to more closely match `multistar` safety/recovery behavior.
- Updated headers for improved self-sufficiency (`multistar2` header now includes required STL headers directly).
