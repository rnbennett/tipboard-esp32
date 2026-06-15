# Host tests

Native-gcc unit tests for the firmware's **pure** logic — the parts that have no
ESP-IDF dependency and can therefore be exercised off-target. This is the cheap
test seam for a project with no on-target test framework: extract a pure function
into its own translation unit, then assert against it here.

## Run

```bash
cd tools/hosttest
make test
```

(Requires only a C compiler — `cc`/`gcc`/`clang`. Nothing ESP-IDF.)

## What's covered

| Test | Unit under test | Source |
|------|-----------------|--------|
| `test_cal_format` | `format_iso_time()` — ISO-8601 → 12-hour clock | `components/mqtt_client/cal_format.c` |

## Adding a test

1. Extract the pure logic into its own `.c`/`.h` in the relevant component
   (as `cal_format.c` was extracted from the MQTT calendar handler).
2. Add a `test_<name>.c` here that includes the header and asserts.
3. Add a target to the `Makefile` and list it under `test:`.

Good next candidates (noted in the audit ledger): the MQTT topic classifier and
the state-JSON parse/clamp in `persist.c` — both become testable once extracted
into pure functions.
