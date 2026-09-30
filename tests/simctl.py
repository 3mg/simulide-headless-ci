#!/usr/bin/env python3
"""Headless SimulIDE session runner for ad-hoc automation and regression tests."""

from __future__ import annotations

import argparse
import json
import os
import socket
import stat
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any, Dict, List, Optional


DEFAULT_SOCKET = "/tmp/simulide.sock"
DEFAULT_STARTUP_TIMEOUT = 15.0
DEFAULT_SIMULIDE = (
    Path(__file__).resolve().parent.parent
    / "executables/SimulIDE_1.1.0-SR2/simulide.app/Contents/MacOS/simulide"
)


class SimCtlError(RuntimeError):
    pass


class SimulideSession:
    def __init__(
        self,
        simulide: Path,
        circuit: Path,
        socket_path: str = DEFAULT_SOCKET,
        startup_timeout: float = DEFAULT_STARTUP_TIMEOUT,
        headless: bool = True,
        show_simulide_output: bool = False,
    ) -> None:
        self.simulide = simulide.resolve()
        self.circuit = circuit.resolve()
        self.socket_path = socket_path
        self.startup_timeout = startup_timeout
        self.headless = headless
        self.show_simulide_output = show_simulide_output
        self.proc: Optional[subprocess.Popen[str]] = None
        self.log_file: Optional[tempfile.NamedTemporaryFile[str]] = None

    @property
    def launch_cwd(self) -> Path:
        return self.circuit.parent

    def start(self) -> None:
        if not self.simulide.is_file():
            raise SimCtlError(f"simulide binary not found: {self.simulide}")
        if not self.circuit.is_file():
            raise SimCtlError(f"circuit not found: {self.circuit}")

        try:
            os.unlink(self.socket_path)
        except FileNotFoundError:
            pass

        argv = [str(self.simulide)]
        if self.headless:
            argv.append("--headless")
        argv.append(str(self.circuit))

        stdout_target = None
        stderr_target = None
        if not self.show_simulide_output:
            self.log_file = tempfile.NamedTemporaryFile(
                mode="w+",
                encoding="utf-8",
                prefix="simctl-",
                suffix=".log",
                delete=False,
            )
            stdout_target = self.log_file
            stderr_target = subprocess.STDOUT

        self.proc = subprocess.Popen(
            argv,
            cwd=str(self.launch_cwd),
            text=True,
            stdout=stdout_target,
            stderr=stderr_target,
        )
        self._wait_for_socket()

    def _wait_for_socket(self) -> None:
        deadline = time.time() + self.startup_timeout
        while time.time() < deadline:
            if self.proc and self.proc.poll() is not None:
                raise SimCtlError(
                    f"simulide exited early with code {self.proc.returncode}"
                )
            if os.path.exists(self.socket_path) and stat_is_socket(self.socket_path):
                return
            time.sleep(0.1)
        raise SimCtlError(
            f"simulide did not create socket {self.socket_path} within {self.startup_timeout:.1f}s"
        )

    def send_command(self, command: str, timeout: float = 10.0) -> str:
        client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        client.settimeout(timeout)
        try:
            client.connect(self.socket_path)
            client.sendall((command + "\n").encode())
            chunks: List[bytes] = []
            while True:
                data = client.recv(4096)
                if not data:
                    break
                chunks.append(data)
        finally:
            client.close()
        return b"".join(chunks).decode(errors="replace").rstrip("\n")

    def cleanup(self) -> None:
        if not self.proc:
            if self.log_file:
                self.log_file.close()
                self.log_file = None
            return

        if self.proc.poll() is None:
            try:
                self.send_command("stop", timeout=2.0)
            except Exception:
                pass
            time.sleep(0.2)
            try:
                self.send_command("quit", timeout=2.0)
            except Exception:
                pass

            try:
                self.proc.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                self.proc.terminate()
                try:
                    self.proc.wait(timeout=2.0)
                except subprocess.TimeoutExpired:
                    self.proc.kill()
                    self.proc.wait(timeout=2.0)

        self.proc = None
        if self.log_file:
            self.log_file.close()
            self.log_file = None

    def log_tail(self, max_lines: int = 40) -> str:
        if not self.log_file:
            return ""
        self.log_file.flush()
        try:
            with open(self.log_file.name, encoding="utf-8", errors="replace") as handle:
                lines = handle.readlines()
        except OSError:
            return ""
        return "".join(lines[-max_lines:]).strip()


def stat_is_socket(path: str) -> bool:
    return os.path.exists(path) and stat.S_ISSOCK(os.stat(path).st_mode)


def resolve_path(base: Path, value: str) -> Path:
    path = Path(value)
    if not path.is_absolute():
        path = base / path
    return path.resolve()


def discover_scenarios(scenarios_dir: Path) -> List[Path]:
    return sorted(scenarios_dir.glob("*/scenario.json"))


def expand_command(command: str, variables: Dict[str, str]) -> str:
    result = command
    for key, value in variables.items():
        result = result.replace("{" + key + "}", value)
    return result


def parse_response(response: str, parse_as: Optional[str]) -> Any:
    if parse_as == "int":
        return int(response.strip() or "0", 10)
    if parse_as == "float":
        return float(response.strip() or "0")
    return response


def assert_step(step: Dict[str, Any], parsed: Any, variables: Dict[str, Any]) -> None:
    if "equals" in step and parsed != step["equals"]:
        raise SimCtlError(f"expected {step['equals']!r}, got {parsed!r}")

    if "contains" in step and step["contains"] not in str(parsed):
        raise SimCtlError(f"expected response to contain {step['contains']!r}, got {parsed!r}")

    if "not_contains" in step and step["not_contains"] in str(parsed):
        raise SimCtlError(
            f"expected response not to contain {step['not_contains']!r}, got {parsed!r}"
        )

    if "min" in step and parsed < step["min"]:
        raise SimCtlError(f"expected value >= {step['min']}, got {parsed}")

    if "max" in step and parsed > step["max"]:
        raise SimCtlError(f"expected value <= {step['max']}, got {parsed}")

    if "min_from" in step:
        source = step["min_from"]["var"]
        if source not in variables:
            raise SimCtlError(f"unknown captured variable: {source}")
        threshold = variables[source] + step["min_from"]["delta"]
        if parsed < threshold:
            raise SimCtlError(f"expected value >= {threshold}, got {parsed}")


def execute_steps(
    session: SimulideSession,
    steps: List[Dict[str, Any]],
    variables: Dict[str, Any],
    verbose: bool = True,
) -> List[Dict[str, Any]]:
    results: List[Dict[str, Any]] = []
    for index, step in enumerate(steps, start=1):
        if "sleep_ms" in step:
            sleep_ms = int(step["sleep_ms"])
            if verbose:
                print(f"[{index:02d}] sleep {sleep_ms} ms")
            time.sleep(sleep_ms / 1000.0)
            results.append({"index": index, "sleep_ms": sleep_ms})
            continue

        command = expand_command(step["cmd"], {k: str(v) for k, v in variables.items()})
        if verbose:
            print(f"[{index:02d}] {command}")
        response = session.send_command(command, timeout=float(step.get("timeout", 10.0)))
        parse_as = step.get("parse")
        parsed = parse_response(response, parse_as)
        assert_step(step, parsed, variables)

        capture = step.get("capture")
        if capture:
            variables[capture] = parsed

        if verbose:
            print(f"     -> {response!r}")

        results.append(
            {
                "index": index,
                "cmd": command,
                "response": response,
                "parsed": parsed,
                "capture": capture,
            }
        )
    return results


def run_scenario(
    simulide: Path,
    scenario_path: Path,
    socket_path: str,
    startup_timeout: float,
    verbose: bool,
    show_simulide_output: bool = False,
) -> Dict[str, Any]:
    scenario_file = scenario_path.resolve()
    spec = json.loads(scenario_file.read_text())
    circuit = resolve_path(scenario_file.parent, spec["circuit"])

    variables: Dict[str, Any] = {
        "scenario_dir": str(scenario_file.parent),
        "circuit": str(circuit),
        "circuit_dir": str(circuit.parent),
    }

    if verbose:
        print(f"=== {spec.get('name', scenario_file.stem)} ===")
        print(f"circuit: {circuit}")

    session = SimulideSession(
        simulide=simulide,
        circuit=circuit,
        socket_path=socket_path,
        startup_timeout=startup_timeout,
        show_simulide_output=show_simulide_output,
    )
    try:
        session.start()
        results = execute_steps(session, spec["steps"], variables, verbose=verbose)
        return {
            "scenario": str(scenario_file),
            "name": spec.get("name", scenario_file.stem),
            "circuit": str(circuit),
            "results": results,
        }
    except Exception as exc:
        log_tail = session.log_tail()
        if log_tail:
            raise SimCtlError(f"{exc}\n\nSimulIDE log tail:\n{log_tail}") from exc
        raise
    finally:
        session.cleanup()


def run_commands(
    simulide: Path,
    circuit: Path,
    commands: List[str],
    run_for_ms: Optional[int],
    socket_path: str,
    startup_timeout: float,
    json_output: bool,
    show_simulide_output: bool = False,
) -> Dict[str, Any]:
    expanded_commands = list(commands)
    if run_for_ms is not None:
        expanded_commands = ["run", f"wait {run_for_ms}"] + expanded_commands

    session = SimulideSession(
        simulide=simulide,
        circuit=circuit,
        socket_path=socket_path,
        startup_timeout=startup_timeout,
        show_simulide_output=show_simulide_output,
    )
    try:
        session.start()
        results: List[Dict[str, str]] = []
        for command in expanded_commands:
            response = session.send_command(command)
            results.append({"cmd": command, "response": response})
            if not json_output:
                print(f"{command} -> {response}")
        return {
            "circuit": str(circuit.resolve()),
            "commands": results,
        }
    finally:
        session.cleanup()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run SimulIDE headless scenarios or ad-hoc command sequences."
    )
    parser.add_argument(
        "--simulide",
        default=str(DEFAULT_SIMULIDE),
        help="path to SimulIDE binary",
    )
    parser.add_argument("--socket", default=DEFAULT_SOCKET, help="Unix socket path")
    parser.add_argument(
        "--startup-timeout",
        type=float,
        default=DEFAULT_STARTUP_TIMEOUT,
        help="seconds to wait for the socket to appear",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="print machine-readable JSON result",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="suppress step-by-step logging for scenario execution",
    )
    parser.add_argument(
        "--show-simulide-output",
        action="store_true",
        help="forward raw SimulIDE stdout/stderr instead of capturing it",
    )

    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--scenario", help="path to a JSON scenario spec")
    mode.add_argument("--circuit", help="path to a circuit for ad-hoc commands")

    parser.add_argument(
        "--cmd",
        action="append",
        default=[],
        help="command to send to the socket; can be repeated",
    )
    parser.add_argument(
        "--run-for-ms",
        type=int,
        help="convenience option: prepend 'run' and 'wait <ms>' before --cmd commands",
    )
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    simulide = Path(args.simulide)
    try:
        if args.scenario:
            result = run_scenario(
                simulide=simulide,
                scenario_path=Path(args.scenario),
                socket_path=args.socket,
                startup_timeout=args.startup_timeout,
                verbose=not args.quiet,
                show_simulide_output=args.show_simulide_output,
            )
        else:
            if not args.cmd and args.run_for_ms is None:
                raise SimCtlError("ad-hoc mode requires at least one --cmd or --run-for-ms")
            result = run_commands(
                simulide=simulide,
                circuit=Path(args.circuit),
                commands=args.cmd,
                run_for_ms=args.run_for_ms,
                socket_path=args.socket,
                startup_timeout=args.startup_timeout,
                json_output=args.json,
                show_simulide_output=args.show_simulide_output,
            )
    except SimCtlError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Interrupted", file=sys.stderr)
        return 130

    if args.json:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
