# SimulIDE 

Electronic Circuit Simulator


SimulIDE is a simple real time electronic circuit simulator.

It's intended for general purpose electronics and microcontroller simulation, supporting PIC, AVR and Arduino.

This is not an accurate simulator for circuit analisis, it aims to be the fast, simple and easy to use, so this means simple and not very accurate electronic models and limited features.

Intended for hobbist or students to learn and experiment with simple circuits.


SimulIDE also features a code Editor and Debugger.
Editor/Debugger is still in it's firsts stages of development, with basic functionalities, but it is possible to write, compile and basic debugging with breakpoints, watch registers and global variables.


## Building SimulIDE:

Build dependencies:

 - Qt5 dev packages
 - Qt5Core
 - Qt5Gui
 - Qt5Xml
 - Qt5Widgets
 - Qt5Concurrent
 - Qt5svg dev
 - Qt5 Multimedia dev
 - Qt5 Serialport dev
 - Qt5 qmake

 
Once installed go to build_XX folder, then:

```
$ qmake
$ make
```

In folder build_XX/executables/SimulIDE_x.x.x you will find executable and all files needed to run SimulIDE.


## Building on macOS (tested on macOS 15, x86_64)

**Prerequisites:**
```bash
brew install qt@5
```

**Changes already made to SimulIDE.pro** (already applied in this repo):

1. Lines 138-140 (`macx` section): replaced hardcoded `gcc@7` with system clang:
```
QMAKE_CC  = /usr/bin/clang
QMAKE_CXX = /usr/bin/clang++
QMAKE_LINK = /usr/bin/clang++
```

2. `copy2dest` command: added explicit `/data` and `/examples` destination paths so
   `cp -r` copies the folders themselves, not their contents.

**Build steps:**

The build must run from inside `build_XX/`. qmake resolves the icon path as
`../../resources/icons/simulide.icns`, so a temporary symlink is needed one level up:

```bash
# From the SimulIDE_1.1.0-SR2_Sources/ directory:
ln -sf "$(pwd)/resources" ../resources

cd build_XX

# Only needed on first build or after SimulIDE.pro changes:
/usr/local/opt/qt@5/bin/qmake ../SimulIDE.pro \
    -spec macx-clang CONFIG+=release CONFIG+=sdk_no_version_check BUILD_DIR=..

make -j4

# Create MacOS dir if it doesn't exist yet (needed before first link):
mkdir -p ../executables/SimulIDE_1.1.0-SR2/simulide.app/Contents/MacOS/
```

Output binary: `executables/SimulIDE_1.1.0-SR2/simulide.app/Contents/MacOS/simulide`

**Running on macOS:**

`macdeployqt` does not work reliably here (it detects the app as already deployed
and skips framework copy, leaving a broken bundle). Use the wrapper script instead:

```bash
# Run via wrapper (already created by build):
executables/SimulIDE_1.1.0-SR2/simulide.app/Contents/MacOS/simulide_run [circuit.sim1] [--run]
```

The wrapper sets `DYLD_FRAMEWORK_PATH` and `QT_PLUGIN_PATH` to point at the
system Qt from Homebrew. If the wrapper doesn't exist yet, create it:

```bash
cat > executables/SimulIDE_1.1.0-SR2/simulide.app/Contents/MacOS/simulide_run << 'EOF'
#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
export QT_PLUGIN_PATH="/usr/local/opt/qt@5/plugins"
export DYLD_FRAMEWORK_PATH="/usr/local/opt/qt@5/lib"
exec "$DIR/simulide" "$@"
EOF
chmod +x executables/SimulIDE_1.1.0-SR2/simulide.app/Contents/MacOS/simulide_run
```

Also add the cocoa platform plugin (required once):
```bash
mkdir -p executables/SimulIDE_1.1.0-SR2/simulide.app/Contents/PlugIns/platforms
cp /usr/local/opt/qt@5/plugins/platforms/libqcocoa.dylib \
   executables/SimulIDE_1.1.0-SR2/simulide.app/Contents/PlugIns/platforms/

cat > executables/SimulIDE_1.1.0-SR2/simulide.app/Contents/Resources/qt.conf << 'CONF'
[Paths]
Plugins = PlugIns
CONF
```

Output app: `executables/SimulIDE_1.1.0-SR2/simulide.app`



## Running SimulIDE:

Run time dependencies:

 - Qt5Core
 - Qt5Gui
 - Qt5Xml
 - Qt5svg
 - Qt5Widgets
 - Qt5Concurrent
 - Qt5 Multimedia
 - Qt5 Multimedia Plugins
 - Qt5 Serialport


No need for installation, place SimulIDE folder wherever you want and run the executable.


