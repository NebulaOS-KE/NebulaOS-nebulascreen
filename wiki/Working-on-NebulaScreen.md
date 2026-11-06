# Working on NebulaScreen

A contributor's guide: how the code is arranged so that most of it can be tested without a
printer, how to drive the UI without one, and the compatibility contract that stops a mismatched
build from starting.

This complements [Architecture](Architecture), which describes the components and the repository
layout. Read that first for *what is where*; read this for *how to work on it*.

Written against `dee0f6f`.

## The split that makes this testable

The single most useful thing to know about this codebase: **logic that does not need LVGL lives
in files that do not include it.**

Twelve such files sit in `src/`, each with a matching host test:

| Logic file | Test |
|---|---|
| `nebulaos_calibration_flow.cpp` | `test_nebulaos_calibration_flow.cpp` |
| `nebulaos_calibration_status.cpp` | `test_nebulaos_calibration_status.cpp` |
| `nebulaos_camera.cpp` | `test_nebulaos_camera.cpp` |
| `nebulaos_display.cpp` | `test_nebulaos_display.cpp` |
| `nebulaos_image.cpp` | `test_nebulaos_image.cpp` |
| `nebulaos_macros.cpp` | `test_nebulaos_macros.cpp` |
| `nebulaos_network.cpp` | `test_nebulaos_network.cpp` |
| `nebulaos_refusal.cpp` | `test_nebulaos_refusal.cpp` |
| `nebulaos_settings.cpp` | `test_nebulaos_settings.cpp` |
| `nebulaos_system.cpp` | `test_nebulaos_system.cpp` |
| `nebulaos_z_adjust.cpp` | `test_nebulaos_z_adjust.cpp` |
| `plr_status.cpp` | `test_plr_status.cpp` |

Plus `config`, `theme`, `paths`, `print_estimate`, `print_start`, `nebulascreen_fb`,
`touch_beep_client` and the subscription ordering.

The panels themselves - `*_panel.cpp` - are the LVGL layer. They draw and dispatch; the decisions
live in the files above.

**Why it matters when you add something.** A question like *which offer should the screen show
for this power-loss state?* or *is this calibration row due for a redo?* is logic, and it belongs
in a plain file where a test can ask it directly. Put it in the panel and it can only be checked
by looking at a screen. `plr_status.cpp` has 21 tests precisely because the offer logic was kept
out of the panel.

## Running the tests

```sh
make test
```

**Do not pass a custom `LDFLAGS` to `make test`.** It propagates into libhv's own sub-make, where
the relative paths do not resolve, and the run fails in a way that looks like a broken test
suite. (Observed: `cannot find -l:libhv.a`.) Build the simulator with your overrides if you need
them; run the tests plainly.

At `dee0f6f` on a desktop: **18 suites, 262 tests, 0 failures, 2 skips.** The two skips are the
test suite itself detecting that `AF_UNIX` is not permitted in the environment and saying so,
which is the right behaviour.

That is a genuinely good return for a UI codebase, and it exists because of the split above.

## Driving the UI without a printer

Build the SDL simulator - see [Building from source](Building-from-Source) for the exact flags -
then read [Development and simulator](Development-and-Simulator) for running it.

Two facilities worth knowing:

**`sim_*` methods.** Panels expose hooks under the `SIMULATOR` define for putting the UI into a
specific state without a backend: `sim_tap_row(i)`, `sim_show_row(i)`, `sim_keypad()`,
`sim_type(keys)`, `sim_open_graph(i)`, `sim_running(heater)`, `sim_camera_present(bool)`. They
are how you reach a dialog or a mid-calibration state directly.

**`GUPPY_SIM_SHOTS=<dir>`.** Walks every page and state, saves one PPM each, and exits - no
display server needed. Useful beyond screenshots: diffing two commits' captures shows what a
change actually touched. Re-capturing across `94869f7` → `3194104` → `dee0f6f` changed 1 image
then 5, and at `dee0f6f` all five were print-status or print-start screens, matching that
commit's subject.

**What the simulator cannot tell you.** The `machine.nebulaos.*` methods only exist on a real
printer, so the NebulaOS settings panels show errors or empty values. A screen that looks broken
in the simulator may be perfectly fine on hardware, and the Updates page in particular captures
differently run to run because it waits on a Moonraker reply that never comes.

## The compatibility contract

NebulaScreen refuses to run on an image whose UI API level it does not match.

`src/nebulascreen_paths.h` declares the level this build needs. At startup the app reads
`/opt/nebulaos-version.json`, which the image provides, and compares its `nebulaos_ui_api` field;
`ui_api_problem()` in `nebulascreen_paths.cpp` returns a description of any mismatch - including
the cases where the field is missing or unreadable, which count as problems rather than being
assumed compatible.

Once connected, the app reports its own version back through
`machine.nebulaos.screen.report`, which the firmware's update supervisor reads as a health signal
for a downloaded release.

This is the same shape as the Klipper extensions' `required_klipper_symbols` check, and for the
same reason: **a component that can be updated independently of the thing it talks to needs to
state what it expects and refuse to start when it is not met.** A clear refusal at startup is
better than a subtle failure later, and far better than a screen that half works.

Consequently: if you change what the screen expects from the image, change the level. Shipping a
build that silently assumes a new path or method is how you get a printer with a blank display and
no explanation.

## Where the OS boundary is

Everything touching the operating system goes through Moonraker JSON-RPC - the
`machine.nebulaos.*` methods. The app does not run system commands for these itself.

That is worth respecting when adding a feature. If you need something the OS can do and there is
no method for it, the method belongs in the firmware's Moonraker component, not in a shell call
from a panel. The boundary is what lets the logic files above be tested at all, since they deal
in requests and replies rather than in processes.

## Print-state safety

[Architecture](Architecture) lists which panels are blocked, which confirm, and which are always
available during a print. The helpers are in `src/utils.cpp`: `is_printing()`,
`notify_locked()`, `confirm_if_printing()`.

When adding a control, decide deliberately which of the three it is. The default should not be
"always available" simply because that is the least code - `nebulaos_refusal.cpp` exists to make
refusals explicit and is covered by 19 tests.

## Not covered here

- **Hardware behaviour.** Nothing on this page was observed on a printer.
- **LVGL itself.** Upstream LVGL v8 documentation applies; this repository patches it only for
  DPI text scaling.
- **The MIPS release build.** See [Building from source](Building-from-Source) and
  [Releases and deployment](Releases-and-Deployment).
