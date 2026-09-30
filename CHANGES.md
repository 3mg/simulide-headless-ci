# Changes vs upstream

Base: [Arcachofo/SimulIDE-dev](https://github.com/Arcachofo/SimulIDE-dev) at
the commit in [UPSTREAM_SHA.txt](UPSTREAM_SHA.txt).

## 1. Simulation correctness fix

**USI I2C clock-stretch (`src/microsim/cores/avr/avrusi.cpp`, `.h`;
`src/microsim/modules/twi/twimodule.cpp`, `.h`)**

An ATtiny85 USI I2C slave (mode 3) needs to hold SCL low at counter overflow
(clock stretching) until firmware services the overflow interrupt and
writes USISR. Upstream's comment in `AvrUsi::configureA()` already says mode
3 is "SCL held low at counter overflow", but no code implements it — SCL is
never actually pulled down. This matters as soon as a second device shares
the bus (e.g. an OLED): without the stretch, master and slave lose sync and
the OLED display corrupts.

- `avrusi.cpp`/`.h`: adds `m_sclHold`. On counter overflow (`stepCounter()`),
  if `m_mode == 3` and the overflow interrupt is enabled, pulls SCL to GND
  via `IoPin::setExtraSource(0, 1/1e-9)` — an admittance-based pull, not
  `controlPin()` pin capture, so other devices on the bus can still drive
  the line (real open-drain semantics). Released in `configureB()` when
  firmware writes USISR (signals the ISR ran).
- `twimodule.cpp`/`.h`: the master's `runEvent()` requests SCL transitions
  via `IoPin::scheduleState()` and assumed they always succeed. Added
  `m_waitSCL`: after requesting SCL go high, the master now polls at a
  tighter interval and re-checks the pin's *actual* voltage before
  continuing, instead of blindly proceeding with a stale clock-low state
  (which caused it to occasionally re-process the same bit/ack twice while
  a slave was stretching). Zero timing change versus upstream in the
  non-stretched case — the poll only ever fires when a rise-request didn't
  actually take effect.

## 2. Real headless mode: `-nogui-ci`

Upstream's `-nogui` still constructs a real `QApplication`+`MainWindow`
(just hides child panels afterward) — it needs a working display and
doesn't set `QT_QPA_PLATFORM`. `-nogui-ci` is a new, separate flag (added
in `src/main.cpp`, `-nogui`/`-test` are untouched — same argv loop,
same behavior, full backward compatibility):

- Pre-scans argv *before* `QApplication` is constructed, sets
  `QT_QPA_PLATFORM=offscreen` if `-nogui-ci` or `-test-ci` is present.
- Skips `window.show()` entirely under either flag.
- Starts a Unix-socket control server (`SimuServer`, see below) — only
  under `-nogui-ci`, not for plain `-nogui` or a normal launch.

## 3. Self-contained scenario runner: `-test-ci <scenario.json|folder>`

A CI-safe sibling of upstream's `-test <folder>` (also untouched). Where
`-test` relies on "test unit" Components wired into the circuit to
self-report pass/fail, `-test-ci` runs a JSON scenario spec (see
[tests/README.md](tests/README.md)) with assertions on serial output, pin
voltage/state, potentiometer/button control, etc. — entirely inside the
process, no socket, no external driver. Implemented in
`src/gui/testing/scenariorunner.cpp`/`.h`.

## 4. Socket control API

New `src/simuserver.cpp`/`.h` (`QLocalServer` on `/tmp/simulide.sock`) and
`src/simcommand.cpp`/`.h` (the actual command dispatch — `run`/`stop`/
`pause`/`reset`/`status`/`simtime`/`serial`/`serial_clear`/`send_serial`/
`wait_serial`/`set_control`/`pin_voltage`/`pin_state`). `SimCommand` is
deliberately factored out so the socket server (`SimuServer`) and the
in-process runner (`ScenarioRunner`, item 3) share one implementation
rather than duplicating the protocol. Full command reference in
[tests/README.md](tests/README.md).

Supporting changes to make this possible:
- `src/microsim/mcu.cpp`/`.h`: new `Mcu::getUsart(n)`/`usartCount()`,
  implemented against the existing `eMcu::m_transModules` list (no changes
  to `mcucreator.cpp` needed).
- `src/microsim/modules/usart/usartmodule.cpp`/`.h`: in-memory ring-buffer
  serial capture (`serialCapture()`/`serialCaptureClear()`, thread-safe —
  the simulator runs on its own thread) and `receiveByte()`.
- `src/microsim/modules/usart/usartrx.cpp`/`.h`: `UartRx::injectByte()`
  delivers a byte straight into the receive FIFO (a well-formed frame with
  a valid stop bit for the currently configured data bits/parity),
  bypassing pin-level bit timing — used by `send_serial`.
- `src/microsim/mcuuart.cpp` (`McuUsart::frameSent`) and
  `src/microsim/cores/avr/avrusart.cpp` (`AvrUsart::frameSent`): both
  override `frameSent()` further down the hierarchy and previously called
  `printOut()` directly, bypassing the base `UsartModule::frameSent()`
  entirely — silently skipping the new capture buffer for every real AVR
  chip. Changed both to call `UsartModule::frameSent(data)` first.

## 5. Build fixes

- `SimulIDE.pri`: `runLrelease.commands` used unquoted `$$PWD`-based shell
  paths — breaks on any checkout path containing a space. Wrapped in
  `$$shell_quote(...)`.
- `SimulIDE.pri` (macOS): hardcoded `QMAKE_CC`/`CXX`/`LINK` to a specific
  Homebrew `gcc@7` path; that formula has since been removed from Homebrew.
  Switched to system `clang`/`clang++` for macOS builds (Linux is
  unaffected and remains the primary CI target).

## 6. Offscreen-safe modal dialogs

`-nogui-ci`/`-test-ci` inherit `MainWindow`/`CircuitWidget` GUI code largely
as-is, including two blocking `QMessageBox::exec()` calls: the crash-recovery
prompt (`MainWindow` constructor, shown when a `backup.sim2` autosave exists)
and the unsaved-changes prompt (`CircuitWidget::newCircuit()`, shown when
loading a new circuit over a modified one). Under `QT_QPA_PLATFORM=offscreen`
these dialogs are created but nothing can ever click them — the process
hangs forever (0% CPU, no output, no crash). In practice this means a single
prior run that gets killed (CI timeout, crash) leaves `backup.sim2` behind
and poisons every subsequent `-nogui-ci`/`-test-ci` invocation until someone
manually deletes it.

Fixed in `src/mainwindow.cpp` and `src/gui/circuitwidget/circuitwidget.cpp`:
both dialogs are skipped when `QGuiApplication::platformName() == "offscreen"`.
The crash-recovery backup is silently removed instead of offered; unsaved
changes are silently discarded in favor of the freshly-requested circuit.
Plain `-nogui`/`-test`/normal GUI launches (`platformName() != "offscreen"`)
are unaffected — both dialogs behave exactly as upstream there.

## 7. Docker image

`docker/Dockerfile` — multi-stage build (Ubuntu 22.04 build stage, minimal
Qt-runtime-only final stage), entrypoint `simulide -nogui-ci`. See
[tests/README.md](tests/README.md) for the `mcu-defs/` vendoring this image
relies on (SimulIDE 2.0.0 fetches chip/board definitions via a network
Installer by default — not viable for reproducible offline CI, so the
regression suite vendors the handful of chips/boards it actually uses).
