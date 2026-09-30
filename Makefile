SIMULIDE_INI = $(HOME)/Library/Application\ Support/simulide/simulide.ini
APP          = executables/SimulIDE_1.1.0-SR2/simulide.app/Contents/MacOS/simulide
RUNNER       = executables/SimulIDE_1.1.0-SR2/simulide.app/Contents/MacOS/simulide_run
QMAKE        = /usr/local/opt/qt@5/bin/qmake
BUILD_DIR    = build_XX

# Extract last opened circuit from SimulIDE preferences
LAST_CIRCUIT = $(shell grep '^lastCircDir=' "$(HOME)/Library/Application Support/simulide/simulide.ini" 2>/dev/null | cut -d= -f2-)

.PHONY: build run debug clean test _qmake_release _qmake_debug

build: _qmake_release
	$(MAKE) -C $(BUILD_DIR) -j4

debug: _qmake_debug
	$(MAKE) -C $(BUILD_DIR) -j4
	@echo "Launching in debugger..."
	QT_PLUGIN_PATH=/usr/local/opt/qt@5/plugins \
	DYLD_FRAMEWORK_PATH=/usr/local/opt/qt@5/lib \
	lldb -- $(APP) $(if $(LAST_CIRCUIT),"$(LAST_CIRCUIT)")

run: $(RUNNER)
	@if [ -n "$(LAST_CIRCUIT)" ]; then \
		echo "Opening: $(LAST_CIRCUIT)"; \
		"$(RUNNER)" "$(LAST_CIRCUIT)"; \
	else \
		echo "No recent circuit found, opening without file"; \
		"$(RUNNER)"; \
	fi

test: build
	$(MAKE) -C tests test SIMULIDE="$(PWD)/$(APP)"

clean:
	$(MAKE) -C $(BUILD_DIR) clean

$(RUNNER):
	@echo "Creating simulide_run wrapper..."
	@printf '#!/bin/bash\nDIR="$$(cd "$$(dirname "$$0")" && pwd)"\nexport QT_PLUGIN_PATH="/usr/local/opt/qt@5/plugins"\nexport DYLD_FRAMEWORK_PATH="/usr/local/opt/qt@5/lib"\nexec "$$DIR/simulide" "$$@"\n' > $(RUNNER)
	@chmod +x $(RUNNER)

_qmake_release:
	@if [ ! -f $(BUILD_DIR)/Makefile ]; then \
		echo "Running qmake (release)..."; \
		ln -sf "$(PWD)/resources" ../resources 2>/dev/null || true; \
		cd $(BUILD_DIR) && $(QMAKE) ../SimulIDE.pro \
			-spec macx-clang CONFIG+=release CONFIG+=sdk_no_version_check BUILD_DIR=..; \
		mkdir -p ../executables/SimulIDE_1.1.0-SR2/simulide.app/Contents/MacOS/; \
	fi

_qmake_debug:
	@echo "Running qmake (debug)..."
	@ln -sf "$(PWD)/resources" ../resources 2>/dev/null || true
	@cd $(BUILD_DIR) && $(QMAKE) ../SimulIDE.pro \
		-spec macx-clang CONFIG+=debug CONFIG+=sdk_no_version_check BUILD_DIR=..
	@mkdir -p executables/SimulIDE_1.1.0-SR2/simulide.app/Contents/MacOS/
