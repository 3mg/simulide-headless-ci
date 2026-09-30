#!/usr/bin/env python3
"""unittest wrapper around JSON-based SimulIDE regression scenarios."""

from __future__ import annotations

import os
import re
import unittest
from pathlib import Path

import simctl


TESTS_DIR = Path(__file__).resolve().parent
SIMULIDE = Path(os.environ.get("SIMULIDE", str(simctl.DEFAULT_SIMULIDE)))
SIMCTL_VERBOSE = os.environ.get("SIMCTL_VERBOSE", "").lower() in {"1", "true", "yes"}


class ScenarioTests(unittest.TestCase):
    maxDiff = None


def _scenario_method_name(path: Path) -> str:
    return "test_" + re.sub(r"[^0-9a-zA-Z_]+", "_", path.parent.name)


def _make_test(path: Path):
    def test(self: ScenarioTests) -> None:
        try:
            simctl.run_scenario(
                simulide=SIMULIDE,
                scenario_path=path,
                socket_path=simctl.DEFAULT_SOCKET,
                startup_timeout=simctl.DEFAULT_STARTUP_TIMEOUT,
                verbose=SIMCTL_VERBOSE,
            )
        except Exception as exc:
            self.fail(f"{path.parent.name}: {exc}")

    test.__name__ = _scenario_method_name(path)
    test.__doc__ = f"Scenario: {path.parent.name}"
    return test


for scenario_path in simctl.discover_scenarios(TESTS_DIR / "scenarios"):
    setattr(ScenarioTests, _scenario_method_name(scenario_path), _make_test(scenario_path))


if __name__ == "__main__":
    unittest.main(verbosity=2)
