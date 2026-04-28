# ADBMS6833 driver starter set

This folder mirrors the current `adbms6830_on_core2` driver layout so the project can
keep ADBMS6830 support while exposing a separate ADBMS6833-facing driver surface.

## Current implementation approach

- The ADBMS6833 headers and entrypoints are implemented as a family-compatible
  wrapper around the existing ADBMS6830 Core2 driver set.
- This matches the intent already noted in the existing 6830 driver README:
  the starter set was written with ADBMS6830 / ADBMS6833 family behavior in mind.
- The wrapper keeps code duplication low and lets both 6830 and 6833 builds
  coexist in the same repo.

## Important note

This is a compatibility port, not yet a silicon-specific divergence.
If your NDA ADBMS6833 datasheet confirms any command, PEC, timing, or register
layout differences, update the 6833 wrapper layer or replace it with a dedicated
implementation in this folder.
