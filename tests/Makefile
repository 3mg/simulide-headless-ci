# SimulIDE automated regression tests.
# Usage:
#   make test
#   make list
#   make scenario-04-wait-serial
#   make SIMULIDE=/path/to/simulide test

SIMULIDE ?= ../executables/SimulIDE_1.1.0-SR2/simulide.app/Contents/MacOS/simulide
PYTHON ?= python3
SCENARIOS := $(sort $(patsubst scenarios/%/scenario.json,%,$(wildcard scenarios/*/scenario.json)))

.PHONY: all test list test1 hardware-test3 $(SCENARIOS:%=scenario-%)

all: test

test:
	@SIMULIDE="$(SIMULIDE)" $(PYTHON) -m unittest -v test_scenarios.py

list:
	@printf '%s\n' $(SCENARIOS)

test1: scenario-04-wait-serial

hardware-test3: scenario-09-subcircuit-serial

$(SCENARIOS:%=scenario-%):
	@echo "=== $(@:scenario-%=%) ==="
	@$(PYTHON) simctl.py --simulide "$(SIMULIDE)" --scenario "scenarios/$(@:scenario-%=%)/scenario.json"
