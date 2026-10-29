# Building QSapecNG

QSapecNG 3.0 builds with current toolchains:

| Dependency | Version            | Notes                                         |
|------------|--------------------|-----------------------------------------------|
| CMake      | 3.16 or newer      |                                               |
| C++        | C++17 compiler     | GCC 9+, Clang 10+, MSVC 2019+                 |
| Qt         | 6.2 or newer       | modules: Core, Gui, Widgets, Svg, PrintSupport |
| Qwt        | 6.2 or newer       | must be built against Qt 6                    |
| Boost      | 1.66 or newer      | headers only, no compiled Boost libraries     |

The executable ends up in `build/bin/`.


## Linux (Ubuntu / Xubuntu)

Ubuntu packages Qwt for Qt 6 (`libqwt-qt6-dev`) only from 26.10 onwards.
Ubuntu 26.04 LTS and older ship Qwt for Qt 5 only, so Qwt is built from
source there first (it takes a couple of minutes).

### Ubuntu / Xubuntu 26.04 LTS (and 24.04)

```sh
sudo apt install build-essential cmake ninja-build qmake6 \
    qt6-base-dev qt6-svg-dev libgl-dev libboost-dev

# 1. Qwt 6.3 for Qt 6 (installs to /usr/local/qwt-6.3.0)
wget https://downloads.sourceforge.net/project/qwt/qwt/6.3.0/qwt-6.3.0.tar.bz2
tar xf qwt-6.3.0.tar.bz2
cd qwt-6.3.0
qmake6 qwt.pro
make -j"$(nproc)"
sudo make install
cd ..

# 2. QSapecNG
cmake -S . -B build -G Ninja -DQWT_ROOT_DIR=/usr/local/qwt-6.3.0
cmake --build build
./build/bin/qsapecng

# optional: install into /usr/local (binary, .desktop file, icons)
sudo cmake --install build
```

The Qwt library path is stored in the executable's RPATH, both in the build
tree and after `cmake --install`, so no `LD_LIBRARY_PATH` is needed.

### Ubuntu 26.10 and newer, Debian 13

```sh
sudo apt install build-essential cmake ninja-build \
    qt6-base-dev qt6-svg-dev libgl-dev libqwt-qt6-dev libboost-dev

cmake -S . -B build -G Ninja
cmake --build build
./build/bin/qsapecng
```


## Windows

### Option A: MSYS2 (quickest)

Install [MSYS2](https://www.msys2.org), open the **MSYS2 UCRT64** shell and run:

```sh
pacman -S --needed git \
    mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-qt6-base \
    mingw-w64-ucrt-x86_64-qt6-svg mingw-w64-ucrt-x86_64-qt6-tools \
    mingw-w64-ucrt-x86_64-qwt-qt6 mingw-w64-ucrt-x86_64-boost

cmake -S . -B build -G Ninja
cmake --build build
./build/bin/QSapecNG.exe
```

After each build, the Qwt DLL is copied and `windeployqt` is run, so the Qt
DLLs and plugins sit next to the executable. MSYS2's Qt also needs a few
non-Qt DLLs (ICU, zlib, ...) from `C:\msys64\ucrt64\bin`: running from the
UCRT64 shell works as is; to start it from Explorer, add that folder to
`PATH`. For a self-contained redistributable, prefer option B.

### Option B: Visual Studio + official Qt

1. Install Visual Studio 2022 (workload "Desktop development with C++").
2. Install Qt 6 with the [Qt Online Installer](https://www.qt.io/download-qt-installer),
   selecting the **MSVC 2022 64-bit** kit (it includes Qt SVG).
3. Download Boost from <https://www.boost.org> and unzip it, e.g. to
   `C:\boost_1_xx_0`. Nothing needs compiling.
4. Build Qwt (see "Qwt from source" below) in the
   **x64 Native Tools Command Prompt for VS 2022**. On Windows it installs to
   `C:\Qwt-6.3.0` by default.
5. Configure and build QSapecNG (same command prompt):

```bat
cmake -S . -B build -G "Visual Studio 17 2022" ^
    -DCMAKE_PREFIX_PATH=C:/Qt/6.x.y/msvc2022_64 ^
    -DQWT_ROOT_DIR=C:/Qwt-6.3.0 ^
    -DBOOST_ROOT=C:/boost_1_xx_0
cmake --build build --config Release
build\bin\Release\QSapecNG.exe
```

`build\bin\Release` is self-contained after the build (Qt, Qwt and the
runtime are deployed there). `cmake --build build --config Release --target package`
creates a ZIP, plus an installer if [NSIS](https://nsis.sourceforge.io) is installed.


## Qwt from source

Download `qwt-6.3.0.zip` (or `.tar.bz2`) from <https://sourceforge.net/projects/qwt/files/qwt/6.3.0/>
and unpack it. Then build it with the Qt 6 `qmake` of the kit you build
QSapecNG with:

```sh
qmake6 qwt.pro         # Windows: C:\Qt\6.x.y\msvc2022_64\bin\qmake.exe qwt.pro
make && make install   # Windows (VS prompt): nmake && nmake install
```

The install prefix is `QWT_INSTALL_PREFIX` in `qwtconfig.pri`
(`/usr/local/qwt-6.3.0` on Linux, `C:/Qwt-6.3.0` on Windows). The Qt Designer
plugin is skipped automatically when Qt Designer isn't installed; to skip it
anyway, comment out `QWT_CONFIG += QwtDesigner` in that file.


## CMake options

| Option | Meaning |
|--------|---------|
| `QWT_ROOT_DIR` | Qwt install prefix when it is not in a standard location |
| `BOOST_ROOT` | Boost headers (an unpacked Boost source tree is fine) |
| `CMAKE_PREFIX_PATH` | where Qt 6 is installed, if CMake does not find it |
| `CMAKE_INSTALL_PREFIX` | install location for `cmake --install` |
| `QSAPECNG_BUILD_TESTS` | build the solver regression tests, run with `ctest --test-dir build` (default `OFF`) |
| `QSAPECNG_DEPLOY_AFTER_BUILD` | Windows only: copy Qwt + run `windeployqt` after building (default `ON`) |

Define `QSAPECNG_PARSER_DEBUG` to get the Boost.Spirit trace of the netlist
parser on stdout.


## Release packages and CI

`.github/workflows/build.yml` builds on every push, pull request and tag:

| Platform | Package | Built with |
|----------|---------|------------|
| Linux | `QSapecNG-<version>-x86_64.AppImage` | Ubuntu 22.04, Qt 6.8 LTS, linuxdeploy |
| Windows | `QSapecNG-<version>-win64.zip` and `.exe` installer | MSVC, Qt 6.8 LTS, CPack (ZIP + NSIS) |

Both jobs run the regression tests. The packages are downloadable from the
workflow run's *Artifacts*; pushing a tag such as `v3.0.0` also publishes
them as a GitHub Release (a tag containing `-`, e.g. `v3.0.0-beta1`, makes a
pre-release).

The same steps work locally:

```sh
# Linux: Qwt for the Qt you bundle, then the AppImage (written to dist/)
QMAKE=/path/to/qt6/bin/qmake packaging/build-qwt.sh "$PWD/qwt"
QMAKE=/path/to/qt6/bin/qmake packaging/build-appimage.sh "$PWD/qwt" dist
```

```powershell
# Windows (PowerShell): Qwt with MSVC, then build and package
$env:QMAKE = 'C:\Qt\6.8.3\msvc2022_64\bin\qmake.exe'
packaging\build-qwt.ps1 -Prefix C:\Qwt-6.3.0
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 `
      -DQWT_ROOT_DIR=C:/Qwt-6.3.0 -DBOOST_ROOT=C:/boost_1_xx_0
cmake --build build --config Release
cd build; cpack -C Release
```


## Appearance

*Settings → Appearance* chooses between following the system colour scheme,
or forcing a light or dark theme. The forced themes use Qt's Fusion style and
help on desktops (or Qt versions) that do not report a dark preference to
Qt. The schematic sheet always stays white.
