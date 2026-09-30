# SimulIDE Headless Regression Tests

`tools/SimulIDE_1.1.0-SR2_Sources/tests/` now serves three purposes:

1. Regression coverage for the patched SimulIDE build.
2. A reusable headless automation entrypoint for hardware-oriented tests.
3. A single `unittest` runner for the full regression suite.

## Quick start

Run the full regression suite:

```bash
cd tools/SimulIDE_1.1.0-SR2_Sources
make test
```

Run from the tests directory directly:

```bash
cd tools/SimulIDE_1.1.0-SR2_Sources/tests
make test
make list
make scenario-09-subcircuit-serial
```

`make test` runs [test_scenarios.py](/Users/nickolay/workspace/PlatformIO/Projects/Matrix%20Midi%20Controller%20Sandbox/tools/SimulIDE_1.1.0-SR2_Sources/tests/test_scenarios.py), which maps each `scenarios/*/scenario.json` file to one `unittest` test case.
Raw SimulIDE stdout/stderr is hidden by default there, so the output looks like a normal test framework run.
For debugging, use `SIMCTL_VERBOSE=1` and/or pass `--show-simulide-output` to `simctl.py`.

## Ad-hoc automation

`simctl.py` launches SimulIDE in headless mode, waits for the Unix socket, sends commands, and cleans up the process automatically.

Example:

```bash
python3 tools/SimulIDE_1.1.0-SR2_Sources/tests/simctl.py \
  --circuit tools/SimulIDE_1.1.0-SR2_Sources/tests/scenarios/04-wait-serial/test.sim1 \
  --cmd status \
  --cmd run \
  --cmd "wait_serial mega328-4 hello 5000" \
  --cmd "serial mega328-4"
```

If you only need to run a circuit for a fixed amount of simulation time and collect results:

```bash
python3 tools/SimulIDE_1.1.0-SR2_Sources/tests/simctl.py \
  --circuit tools/SimulIDE_1.1.0-SR2_Sources/tests/scenarios/03-wait-simtime/test.sim1 \
  --run-for-ms 100 \
  --cmd simtime \
  --json
```

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

The suite currently includes 31 scenarios.

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
`UartRx::injectByte` in `src/microsim/modules/usart/usartrx.cpp`) rather than
bit-banging pin voltages, so it is independent of baud rate and wiring.

## Fixtures

There is no separate legacy shell-based `test1-headless-serial` test anymore.
Reusable assets live in:

- [firmware-assets/hello328.hex](/Users/nickolay/workspace/PlatformIO/Projects/Matrix%20Midi%20Controller%20Sandbox/tools/SimulIDE_1.1.0-SR2_Sources/tests/firmware-assets/hello328.hex)
- [fixtures/hello328-src](/Users/nickolay/workspace/PlatformIO/Projects/Matrix%20Midi%20Controller%20Sandbox/tools/SimulIDE_1.1.0-SR2_Sources/tests/fixtures/hello328-src)
