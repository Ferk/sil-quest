# Compiling Instructions

Sil-Quest currently supports two frontend targets from this repository:

- Terminal frontend via GCU (curses/ncurses) on Linux and macOS
- Web frontend via Emscripten/WebAssembly

The native Windows, Cocoa, and X11 frontends are no longer built from this tree.

## Preparation

The initial step is to download the Sil-Quest source code. You have two options.

**Option 1: Clone the Sil-Quest git repository.**

```shell
# Clone the Sil-Quest repository and change to its directory
$ git clone git@github.com:Ferk/sil-quest.git Sil-Quest && cd Sil-Quest
```

**Option 2: Download the source release archive.**

If you have downloaded a source archive:

- Unpack it into a directory such as `Sil-Quest/`
- Keep the `lib/` directory next to the built executable or web bundle
- Run the build steps below from the top-level project directory

## Terminal frontend (Linux/macOS)

Install the terminal build dependencies. You also need `cmake` and `ninja`.

> Example for Debian/Ubuntu Linux systems:
> `sudo apt-get install gcc cmake ninja-build libncursesw5-dev`
>
> Example for Fedora Linux systems:
> `sudo dnf install -y gcc cmake ninja-build ncurses-devel`
>
> Example for macOS systems:
> `brew install cmake ninja ncurses`

Build Sil-Quest:

```shell
$ cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release
$ cmake --build build
$ cp build/sil sil
```

Run the terminal frontend from the top-level project directory:

```shell
$ ./sil
```

The `lib/` directory must remain next to the executable at runtime.

> NOTE: If you are on macOS 10.15 or later and have not run a locally built
> binary before, Gatekeeper may block `sil` the first time. In `System Settings`
> > `Privacy & Security`, use `Open Anyway` for the blocked binary if needed.

## Web frontend (Emscripten)

Install `cmake`, `ninja`, Python, and the Emscripten SDK. One straightforward
setup is:

```shell
$ git clone https://github.com/emscripten-core/emsdk.git
$ cd emsdk
$ ./emsdk install 4.0.20
$ ./emsdk activate 4.0.20
$ . ./emsdk_env.sh
$ cd ../Sil-Quest
```

Configure and build the web frontend:

```shell
$ emcmake cmake -G Ninja -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release -DBUILD_ENABLE_WERROR=ON
$ cmake --build build-wasm
```

This generates the runtime in `web/lib/` (`sil.js`, `sil.wasm`, `sil.data`).

To run it locally, serve the `web/` directory over HTTP:

```shell
$ python3 -m http.server --directory web 8000
```

Then open `http://localhost:8000` in your browser.
