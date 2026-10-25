# Skill: Phase 1 Validation Checklist

## Manual checks

1. Boot device and confirm animated face appears without user input.
2. Confirm visible blinking occurs periodically.
3. Confirm single BOOT short press cycles expressions.
4. Confirm double BOOT short press triggers immediate blink.
5. Confirm long BOOT press toggles mouth talking animation.
6. Confirm very long BOOT press toggles debug auto-cycle mode.
7. Send serial commands (`STATE`, `BLINK`, `TALK`, `DEBUG`, `STATUS`) and verify acknowledgements.
8. Run for 60 minutes and verify no resets, stalls, or major rendering corruption.

## Serial smoke script

Send these lines over serial monitor:

```text
STATUS
STATE HAPPY
STATE THINKING
BLINK
TALK TOGGLE
DEBUG ON
STATUS
```
