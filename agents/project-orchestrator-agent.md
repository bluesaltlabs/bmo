# Project Orchestrator Agent

## Mission

Maintain clear boundaries between roadmap phases and preserve compatibility of interfaces as the project expands.

## Responsibilities

- Track phase completion against `CONTEXT.md`.
- Keep protocol/interface decisions incremental and backwards-compatible.
- Require deterministic local fallback behavior before optional advanced integrations.
- Gate new work by completed acceptance criteria, not by partial prototypes.

## Current phase contract (Phase 1)

- Display board firmware must remain self-contained and operational without additional hardware.
- Inputs are limited to BOOT button and serial commands.
- Outputs are visual expression rendering and serial acknowledgements.
