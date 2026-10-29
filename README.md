# QSapecNG

> **Note:** this README is for a fork of QSapecNG.

QSapecNG v3.0.0  
Copyright (C) 2009, Michele Caini  
Copyright (C) 2026, Wojciech Kaczmarski  
GNU General Public License v3

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.


## What is it?

QSapecNG is a Qt-based program for symbolic analysis of linear analog circuits.
In fact, it consists of two independent parts: the SapecNG framework engine
and the GUI application QSapecNG.

It is an open source, multi-platform project, continuously enhanced by students
and researchers of the Department of Electronics and Telecommunications (DET) of
the University of Florence. It comes as a continuation of SapWin for Windows, in
order to give the project full compatibility with other platforms.

Through SapecNG/QSapecNG users can draw, solve and analyze analog circuits as
well as manage them. Since version 2.0.0~beta1 there is also full support for
sub-circuits that can be created, saved, reopened and added as part of other
circuits.

SapecNG/QSapecNG is freely available on [SourceForge.net](https://sourceforge.net/projects/qsapecng/).
A previous version of SapecNG is available too, on [SourceForge.net](https://sourceforge.net/projects/sapec-ng/).
Pre-compiled packages will also be available from the projects' pages.


## What's new in 3.0

Version 3.0 brings QSapecNG back to life on current systems and makes its
results trustworthy. The major changes since 2.1.1 are below, the
[CHANGELOG](CHANGELOG) lists everything.

**A modern program again**

* Ported to Qt 6, Qwt 6.2/6.3 and C++17, with a modern CMake build - it
  builds with current GCC, Clang, MSVC and MinGW on Linux and Windows.
* A new look: scalable icons, tabbed documents, light and dark themes,
  IEC/DIN or ANSI resistor symbols, and one Configure dialog for all of it.

**Correct results**

Several bugs gave wrong answers rather than crashes:

* Every circuit with a mutual inductance read its coil values as whole
  numbers (0.001 H became 0 H), so its result was wrong.
* A netlist with a gap in its node numbers was solved as 0.
* Terms cancelling out in the transfer function could crash the solver.
* Plots evaluated the result with values collected again from the parts,
  where a name not found counted as 0 - they now plot exactly what the
  Result panel shows.
* Circuits without a unique solution (a loop of voltage sources, a floating
  part, an op-amp without feedback) are reported as such, with the usual
  causes, instead of showing 0 or 1/0.

**Verified solver**

* The solver is cross-checked against an independent numeric solver
  (modified nodal analysis) on thousands of random circuits with every
  component type, including the poles, zeros and plotted frequency response.
* The SPICE export is checked against ngspice the same way.
* Both run with `ctest` (build with `-DQSAPECNG_BUILD_TESTS=ON`).

**Faster**

* Resolve gives the numeric result and the plots at once, even for big
  circuits: a 101-part ladder takes 8 s.
* The symbolic result is hundreds of times as fast as in 2.1.1, even on one
  processor core (a 25-part ladder: 0.3 s instead of 138 s on the same
  computer), uses all the cores, and needs far less memory (a 33-part
  ladder: 1.4 GB). Its size still grows quickly with the circuit, so a big
  circuit's symbolic result is worked out in the background, with the time
  it will take shown first. Symbolic solver for a circuit too large for the
  computer's memory is stopped, with the numeric result and the plots complete.
* Changing only values keeps the symbolic result quick: it is made from the
  last one's terms instead of being worked out again (a 33-part ladder: 2 s
  instead of 26 s).

**Drawing circuits**

* Wires drawn as in KiCad - parts and wires stay connected while
  dragged - click a pin to start a wire there.
* Parts hang on the pointer by their pin or centre, rotate (R) and flip
  (H, V) while placed. Values can use a standard notation (4k7, 100n, 1meg).
* Parameters in a dialog (double-click). Real R, L, and C with parasitics,
  real op-amp with open-loop gain and gain-bandwidth product.
* Text labels, a configurable grid, export to SVG, PNG or PDF, and example
  circuits in File > Open Example.

**Results and plots**

* A Result panel with the transfer function as real fractions (symbolic,
  raw and numeric), with Copy and Copy LaTeX.
* Several outputs per circuit: every Out marker, voltmeter and ammeter is
  solved, with a tab each, and can be renamed (Vout, Vb, Ve, ...).
* A Plots window with magnitude, phase, Bode, pole-zero and Smith chart
  plots (impedance and admittance circles), smooth curves, and measuring
  markers: peaks, -3 dB points and differences from a reference marker.

**New parts and export**

* Nullor (nullator and norator), three winding transformer (ideal, or with
  inductance and coupling), single-pin voltage source.
* File > Export SPICE Netlist for ngspice, LTspice and similar simulators.
* File > Open Netlist reads SapecNG .crc netlists and draws them as a
  schematic.

Files from 2.x open in 3.0. Older versions can open 3.0 files, but they
leave out or simplify the new parts: a nullor opens as an op-amp, a
three-winding transformer is skipped, and only the first of several Out
markers is read.


## Quick start

1. Draw the circuit: pick parts on the Components toolbar (or with their keys),
   W draws wires, mark the output with Out (O). Several Out markers,
   voltmeters or ammeters give one result each.
2. Press F8 (Run > Resolve): the transfer function appears in the Result
   panel.
3. Run > Plots (F9) opens a Bode plot (then shows the plots); Plot > New
   Plot (Ctrl+F9) adds others. Click a plot to place markers.


## Installation

* Windows: open the setup executable and follow the instructions.
* GNU/Linux
  - binaries: install with the package manager of your distribution
  - sources: see the section "Building and Installing" below
* macOS: open the dmg file and copy the application bundle to the Applications folder.


## Requirements for building

* CMake 3.16 or newer
* A C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
* Qt 6.2 or newer (Core, Gui, Widgets, Svg, PrintSupport)
* Qwt 6.2 or newer, built against Qt 6
* Boost 1.66 or newer (headers only)
* [NSIS](https://nsis.sourceforge.io/) (optional, Windows installer)


## Building and Installing

See [BUILDING.md](BUILDING.md) for step-by-step instructions for Linux and Windows.
In short:

```sh
cmake -S . -B build [-DQWT_ROOT_DIR=/path/to/qwt]
cmake --build build
cmake --install build        # optional
```

The executable is written to `build/bin/`.


## Options

Options are passed to cmake with `-D`:

| Option | Meaning |
|--------|---------|
| `QWT_ROOT_DIR=/path` | Qwt install prefix, if not in a standard place |
| `BOOST_ROOT=/path` | Boost headers (an unpacked Boost tree is fine) |
| `CMAKE_PREFIX_PATH=/path` | Qt 6 install prefix, if CMake can't find it |
| `CMAKE_INSTALL_PREFIX=/path` | installation prefix |
| `QSAPECNG_DEPLOY_AFTER_BUILD` | (Windows) copy Qwt and run windeployqt after building, default `ON` |


## Uninstalling

* **From sources**  
  If the build tree has not been deleted from the filesystem,
  you can execute `make uninstall` on Unix systems to remove the installed files,
  or you can build the project "uninstall" in Visual Studio.

* **From binary packages**
  - If you have installed from a (g|b)zipped binary file, you can simply delete
    every file you have unzipped.
  - If you have installed from the Windows setup file, you can find the Uninstall
    link in Start → QSapecNG → Uninstall.
  - If you have installed from a deb or rpm package, you can remove the program
    using the package manager of your GNU/Linux distribution.
  - If you have installed from the dmg file on macOS, simply drag the program icon
    from the Applications directory to the Trash.


## Note for Debian-based distributions

The sources are provided with the `debian` subfolder, and QSapecNG can be built
on a Debian-based system using the common Debian building tools (`devscripts`).

To build, execute (in the source folder):

```sh
debuild binary
dpkg -i ../qsapecng_${version}_${arch}.deb
```

where `${version}` is the current version of QSapecNG and `${arch}` is
your system architecture.

To generate the 'orig' tarball, execute `cmake --build build --target package_source`.


## Note for Slackware/Debian/RedHat-based distributions

You can use `checkinstall` instead of `make install` to install
the program, so that a Slackware/deb/rpm package will be generated
and installed.

If you have downloaded an unstable version from the repository, I suggest using
checkinstall with the following command:

```sh
checkinstall --default \
  --pkgname qsapecng \
  --pkgversion 3.0.0 \
  --pkggroup electronics
```


## Packages

There are two additional targets for the `make` command:

- `package_source`
- `package`

The first creates compressed archives with all the source files, without
the debian subfolder; the second creates a binary compressed package on
Unix systems, a setup executable on Windows (if [NSIS](https://nsis.sourceforge.io/)
is installed on the system) and a .dmg file with the application bundle on macOS.

* Debian users may execute (as root) `cpack -G DEB` instead of
  `make package` to build a .deb binary file.

* RedHat users may execute (as root) `cpack -G RPM` instead of
  `make package` to build a .rpm binary file (this command has not
  been tested).
