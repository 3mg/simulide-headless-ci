# SimulIDE Headless Regression Tests

`tests/` serves three purposes:

1. Regression coverage for the patched SimulIDE build.
2. A reusable headless automation entrypoint for hardware-oriented tests.
3. A single `unittest` runner for the full regression suite.

## Quick start

Run the full regression suite:

```bash
cd tests
make test
```

```bash
make list
make scenario-09-subcircuit-serial
```

`make test` runs `test_scenarios.py`, which maps each `scenarios/*/scenario.json` file to one `unittest` test case.
Raw SimulIDE stdout/stderr is hidden by default there, so the output looks like a normal test framework run.
For debugging, use `SIMCTL_VERBOSE=1` and/or pass `--show-simulide-output` to `simctl.py`.

## Ad-hoc automation

`simctl.py` launches SimulIDE in headless mode (`-nogui-ci`), waits for the Unix socket, sends commands, and cleans up the process automatically.

Example:

```bash
python3 tests/simctl.py \
  --circuit tests/scenarios/04-wait-serial/test.sim1 \
  --cmd status \
  --cmd run \
  --cmd "wait_serial mega328-4 hello 5000" \
  --cmd "serial mega328-4"
```

If you only need to run a circuit for a fixed amount of simulation time and collect results:

```bash
python3 tests/simctl.py \
  --circuit tests/scenarios/03-wait-simtime/test.sim1 \
  --run-for-ms 100 \
  --cmd simtime \
  --json
```

### Self-contained alternative: `-test-ci`

Every scenario here can also run entirely inside the SimulIDE process, with
no Python driver and no socket, via `-test-ci`:

```bash
build_XX/executables/SimulIDE_2.0.0-/simulide -test-ci tests/scenarios
```

Prints a pass/fail summary per scenario and exits 0 (all passed) or 1 (any
failed). Both paths share the same command-dispatch code (`SimCommand`), so
they're guaranteed to agree — this is how the Docker image runs the suite,
and how `-nogui-ci`/`-test-ci` themselves get cross-checked against each
other.

## Scenario format

Each scenario is a JSON file with:

- `circuit`: path to the `.sim1` file, relative to the scenario file
- `steps`: ordered list of actions

Supported step forms:

- `{ "cmd": "status", "equals": "stopped" }`
- `{ "cmd": "serial mega328-1", "contains": "hello" }`
- `{ "cmd": "simtime", "parse": "int", "capture": "t0" }`
- `{ "cmd": "simtime", "parse": "int", "min_from": { "var": "t0", "delta": 100000 } }`
- `{ "cmd": "pin_voltage Pot-26 PinM", "parse": "float", "min": 2.0, "max": 3.0 }`
- `{ "cmd": "pin_state mega328-31 PORTD4", "equals": "high" }`
- `{ "cmd": "send_serial mega328-31 H", "equals": "ok" }`
- `{ "sleep_ms": 200 }`

## Current coverage

The suite currently includes 31 scenarios; 30 pass. The one known failure
(`11-task3-local-copy`) references a firmware path outside this repo
(`hardware-tests/test3-attiny85-encoder-i2c-slave/...`) that only exists in
a full project checkout — unrelated to the SimulIDE version.

Coverage is grouped roughly like this:

1. `01-10`: core socket API, lifecycle, `wait`, `load`, reset, error handling
2. `11-20`: copied real-world setups, UART links, multi-MCU boot, Mega+Nano I2C
3. `21-31`: analog controls, button input, GPIO verification, Wokwi-parity patterns

Representative checks include:

- `wait_serial` on simple and multi-MCU circuits
- `set_control` for `Potentiometer` and `Push`
- `pin_voltage` / `pin_state` for analog and GPIO assertions
- `send_serial` to inject bytes into a target's UART RX (Wokwi `write-serial` parity)
- boot banners, re-banners after reset, and timed output
- combined scenarios mixing UART, GPIO, buttons, and analog input

## Socket API reference

| Command | Description |
|---|---|
| `status` / `run` / `stop` / `pause` / `reset` / `quit` | Lifecycle control |
| `simtime` | Simulated time in µs |
| `serial <circId> [uartN]` | Full in-memory serial capture buffer |
| `serial_clear <circId> [uartN]` | Clear the capture buffer |
| `wait_serial <circId> [uartN] <pattern> <timeout_ms>` | Block until `pattern` appears on TX |
| `send_serial <circId> [uartN] <text>` | Inject `text` into the target's UART RX (Wokwi `write-serial` parity) |
| `set_control <circId> <value>` | Drive a `Potentiometer` (ohms) or `Push` (`press`/`release`) |
| `pin_voltage <circId> <pinSuffix>` | Raw voltage at a named pin |
| `pin_state <circId> <pinSuffix>` | `high`/`low`, thresholded at 2.5V (Wokwi `expect-pin` parity) |

`send_serial` delivers bytes directly into the USART receive FIFO (see
`UartRx::injectByte` in `src/microsim/modules/usart/usartrx.cpp`)
rather than bit-banging pin voltages, so it is independent of baud rate and wiring.

All command logic lives once, in `SimCommand`
(`src/simcommand.cpp`) — the socket server
(`SimuServer`) and the in-process `-test-ci` runner (`ScenarioRunner`) are
both thin callers of it.

## MCU/board definitions (`mcu-defs/`)

SimulIDE 2.0.0 no longer bundles chip definitions (`mega328.mcu` and
friends) with the app — they're fetched on demand through a network
Installer into `~/Library/Application Support/simulide/components/` (or
the Linux equivalent), with an "installed" flag in `QSettings`. That's a
bad fit for reproducible CI (network dependency, state outside the repo),
so every scenario folder has a `data` symlink to the shared
[`mcu-defs/`](mcu-defs) folder instead, which vendors exactly the chips this
suite needs (`mega328`, `mega2560`, `tiny85`) plus the two Arduino board
presets (`Mega`, `Nano`) used by subcircuit-based scenarios. This relies
entirely on SimulIDE's own existing circuit-relative fallback lookup
(`<circuit-dir>/data/<device>/<device>.mcu` in `Mcu::Mcu()`,
`<circuit-dir>/data/<device>/<device>.sim1` in `SubCircuit::construct()`) —
no code changes were needed to support it.

Note that a subcircuit board (e.g. `Mega/Mega.sim1`) becomes the new
"circuit directory" for anything nested inside it, so `mcu-defs/Mega/` and
`mcu-defs/Nano/` each carry their own `data` symlink pointing back up to
`mcu-defs/` itself — otherwise the mega2560 chip nested inside the Mega
board definition can't find its own `.mcu` file.

If you add a scenario that needs a chip not yet in `mcu-defs/`, download it
once via SimulIDE's own Installer UI, then copy the resulting
`<device>/<device>.mcu` (+ its referenced `avr/`, `<family>/` subfolders,
+ `<device>.package`) into `mcu-defs/<device>/`, matching the existing
layout for `mega328`/`mega2560`/`tiny85`.

## Fixtures

Reusable assets live in:

- [firmware-assets/hello328.hex](firmware-assets/hello328.hex)
- [fixtures/hello328-src](fixtures/hello328-src)
