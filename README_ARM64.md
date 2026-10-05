# arm64 Build Handoff

## Purpose

This branch is an in-progress effort to build Licode's native Erizo stack for
arm64. The first target is a native Apple Silicon macOS build on the local
machine. The work is not complete and the changes are currently uncommitted.

The baseline is commit `b7325834` (`Merge remote-tracking branch
'upstream/master'`). The working branch is `support/arm64`.

## Intended Build Order

The build is staged. Do not jump directly to `erizoAPI`; each stage depends on
the previous one being complete.

1. Run `scripts/installMacDeps.sh` to install the macOS toolchain and shared
   native dependencies.
2. Run `scripts/installErizo.sh` to install Conan and build Erizo's more
   direct C++ dependencies, then build Erizo itself.
3. Once Erizo is working, install and build `erizoAPI`, the C++ Node.js addon.

### Stage 1: macOS dependencies

`installMacDeps.sh` is responsible for the platform/dependency foundation:

- Homebrew packages: `pkg-config`, `glib`, `nasm`, `gettext`, and `coreutils`.
- Node.js through the repository's NVM configuration.
- Conan 1.62.0.
- A project-local CMake 3.31.12 under `build/libdeps/build`.
- OpenSSL, built for the host architecture.
- libsrtp and libnice.
- FFmpeg 6.1.1, plus Opus and libvpx; x264 is added when GPL media support is
  enabled.

The Apple Silicon requirement at this stage is that Homebrew, Node.js, CMake,
and every native library built here are arm64 and that the shell is not under
Rosetta.

### Stage 2: Erizo and direct C++ dependencies

`installErizo.sh` first invokes Conan and the CMake generation/build flow. The
direct dependency set is:

- Conan packages: `libiconv/1.17`, `boost/1.84.0`, `apr/1.7.4`,
  `apr-util/1.6.1`, and `abseil/20211102.0`.
- The local `IncludePathsGenerator` Conan package, which produces compiler
  include-path arguments.
- Apache log4cxx 0.11.0, built as a CMake ExternalProject using APR,
  APR-util, and Iconv.
- The in-tree WebRTC static library.
- The `nicer` ExternalProject at tag `1.6_fix`, including nrappkit.
- GoogleTest 1.8.0 when the test targets are generated.
- Erizo itself, including the FFmpeg-backed media code.

The Conan settings must resolve to arm64 (`arch=armv8` in the current Conan 1.x
configuration), and all ExternalProjects must inherit the same compiler and
architecture.

### Stage 3: erizoAPI

`erizoAPI` is a separate C++ addon for Node.js. It is not the same build as the
Erizo CMake project. After Erizo and its libraries build successfully:

```sh
cd erizoAPI
npm install
./build.sh
```

`build.sh` runs the addon lint step and uses `node-gyp rebuild` unless the old
`node-waf` command is available. The addon depends on `nan` and `node-gyp`,
and its native build must use the same arm64 Node.js installation and native
Erizo library paths as the earlier stages.

## Current State

The host is native arm64:

- `uname -m` reports `arm64`.
- The active Apple Clang reports target `arm64-apple-darwin`.
- Homebrew is installed at `/opt/homebrew`.
- The Homebrew FFmpeg libraries currently found by CMake are arm64 Mach-O
  libraries.
- Debug and release CMake cache files were generated and found Homebrew GLib
  and FFmpeg dependencies.
- The WebRTC static archive exists in the debug build tree.
- No `erizo` executable has been produced yet.

The existing generated Makefiles currently cannot be used because they refer
to this missing executable:

```text
/Users/pedro/Documents/workspace/Projects/webrtc/licodemac/build/libdeps/build/bin/cmake
```

The first build attempt was:

```sh
make -C erizo/build/debug -j2
```

It stopped during CMake's build-system check with:

```text
make: .../build/libdeps/build/bin/cmake: No such file or directory
make: *** [cmake_check_build_system] Error 1
```

The generated caches also show `CMAKE_OSX_ARCHITECTURES` is empty. The host
compiler is arm64, but the build does not yet explicitly request
`-DCMAKE_OSX_ARCHITECTURES=arm64`; this should be made deliberate and then
verified in the generated compile and link commands.

## Changes Already Made

### Dependency and build scripts

- `Dockerfile` now starts from Ubuntu 22.04 instead of 20.04.
- `erizo/conanfile.txt` updates APR from 1.7.0 to 1.7.4.
- Generated Conan finder and metadata files are ignored in `.gitignore`.
- `erizo/generateProject.sh` uses `CMAKE_CMD` instead of assuming `cmake` is
  on `PATH`, and logs the selected command.
- `scripts/installErizo.sh`:
  - Prefers the project-local CMake installed under `build/libdeps/build`.
  - Maps `arm64`/`aarch64` to Conan `arch=armv8` and x86_64 to
    `arch=x86_64`.
  - Selects a Conan-compatible Apple Clang version, with an override through
    `CONAN_APPLE_CLANG_VERSION`.
  - Passes the project-local CMake executable to Conan's CMake toolchain.
- `scripts/installMacDeps.sh`:
  - Detects the Homebrew prefix, including `/opt/homebrew` on Apple Silicon.
  - Adds the Homebrew `bin` and `pkgconfig` directories to the environment.
  - Warns when the shell is running under Rosetta.
  - Builds and installs CMake 3.31.12 into `build/libdeps/build`.
  - Uses `nasm` rather than `yasm`.
  - Selects the arm64 OpenSSL target on arm64 macOS.
  - Replaces the old libav 11.11 build with FFmpeg 6.1.1, built as shared
    libraries and with programs disabled.
  - Updates Homebrew MongoDB to `mongodb-community@8.0`.
- `scripts/installUbuntuDeps.sh` has corresponding dependency modernization:
  - Ubuntu release-specific GCC selection.
  - arm64 MongoDB installation from the MongoDB apt repository.
  - More complete apt dependencies, including `nasm`, `xz-utils`, and GnuPG.
  - OpenSSL `lib64` contents are copied into the layout expected by Erizo.
  - FFmpeg 6.1.1 replaces libav 11.11.
- `scripts/checkNvm.sh` falls back to `$HOME/.nvm/nvm.sh` when the project-local
  NVM checkout is unavailable.

### CMake and third-party discovery

- CMake minimum versions in the Erizo, test, and WebRTC subprojects were
  raised to 3.5.
- `/opt/homebrew/include` and `/opt/homebrew/lib` were added to GLib and
  general macOS discovery paths.
- The log4cxx ExternalProject setup now extracts the first path from Conan's
  potentially multi-path APR, APR-util, and Iconv variables before passing
  them to `configure`.
- The nicer dependency now uses the `1.6_fix` tag.
- FFmpeg libraries are described as FFmpeg (`avutil`, `avcodec`, and
  `avformat`) rather than the old libav dependency.

### FFmpeg API modernization

The native media code was updated for the newer FFmpeg headers and libraries.
The main files are:

- `erizo/src/erizo/media/ExternalInput.cpp/.h`
- `erizo/src/erizo/media/ExternalOutput.cpp/.h`
- `erizo/src/erizo/media/MediaProcessor.cpp/.h`
- `erizo/src/erizo/media/codecs/AudioCodec.cpp/.h`
- `erizo/src/erizo/media/codecs/VideoCodec.cpp/.h`
- `erizo/src/erizo/media/codecs/FFmpegCompat.h`

The changes include:

- Replacing removed global registration calls with the current FFmpeg API.
- Replacing stack `AVPacket` usage and `av_init_packet`/`av_free_packet` with
  allocated packets and `av_packet_free`/`av_packet_unref`.
- Replacing the deprecated encode/decode functions with the send/receive API.
- Replacing `AVStream::codec` access with `codecpar` and, for decoding, copying
  codec parameters into a newly allocated codec context.
- Updating codec pointers to `const AVCodec*`.
- Updating pixel format, key-frame, URL, I/O close, and channel-layout APIs.
- Adding `FFmpegCompat.h` to bridge old and new channel-layout fields based on
  the FFmpeg major version.
- Replacing FFmpeg's `AV_RB16` dependency in the H.264 RTP parser with a small
  local big-endian reader.
- Adding checks for missing media streams and several allocation failures.

## Important Review Notes

- `package-lock.json` is also modified, with a large npm lockfile
  normalization/deduplication-style diff. It is not obviously required for
  the arm64 native build. Decide whether it belongs in this work before
  committing.
- `erizo/generateProject.sh` still has an unconditional
  `generateVersion release` at the end, so release generation can happen even
  when another option was requested. This is pre-existing behavior but is
  relevant when reproducing builds.
- `scripts/installErizo.sh` currently wraps the Conan install command in a
  Darwin-only conditional. The Linux branch appears to skip `conan install`
  altogether and should be checked before relying on the Ubuntu path.
- `scripts/installErizo.sh` uses an unusual `-no_debug11` argument in the
  release Erizo API npm install path. Verify whether this is intentional or a
  typo before treating the script changes as complete.
- The current CMake caches are stale artifacts: they point at the missing
  project-local CMake executable and should be regenerated after dependencies
  are restored.
- The FFmpeg migration has not yet been validated by a successful Erizo build,
  unit test run, media encode/decode test, or end-to-end Licode run.
- The compatibility code supports both older and newer FFmpeg channel-layout
  APIs, but the actual dependency version and all runtime paths still need to
  be tested.
- Avoid treating a successful CMake configure as proof that all libraries are
  arm64. Check every linked library and generated binary with `file` and
  inspect link paths with `otool -L`.

## Suggested Continuation

1. Complete Stage 1 first. Restore or rebuild the project-local dependency
   tree so `build/libdeps/build/bin/cmake` exists. The intended entry point is:

   ```sh
   ./scripts/installMacDeps.sh --unattended --disable-services --fast
   ```

   Confirm that the script is run from the repository checkout and that it
   uses a native arm64 shell, not Rosetta.

2. Make architecture selection explicit before Stage 2. Prefer passing
   `-DCMAKE_OSX_ARCHITECTURES=arm64` from the generation flow rather than
   relying only on the host compiler, and ensure Conan and all locally built
   dependencies use the same architecture.

3. Start Stage 2 through `installErizo.sh`, after checking/fixing its current
   Conan conditional and CMake-path behavior. For Erizo alone, use its Erizo
   option rather than the no-argument mode, which continues into controller
   and Spine installation. For example:

   ```sh
   ./scripts/installErizo.sh -e
   ```

   Remove the stale generated Erizo build directories before retrying. Do not
   reuse caches generated against a missing or different dependency prefix.

4. Build Erizo and record the first real compiler or linker failure. The lower
   level commands, when needed for iteration, must run from `erizo/`:

   ```sh
   (cd erizo && ./generateProject.sh -d && ./buildProject.sh -j2)
   ```

   Adjust the command if the generation script's existing release behavior is
   fixed while continuing the work.

5. Verify Stage 2 before moving on:

   ```sh
   file erizo/build/debug/erizo/erizo
   otool -L erizo/build/debug/erizo/erizo
   (cd erizo && ./runTests.sh)
   ```

   Confirm that the executable and every non-system native dependency are
   arm64, then exercise external input, external output, audio encode/decode,
   video encode/decode, and H.264 RTP aggregation.

6. Start Stage 3 only after Stage 2 passes:

   ```sh
   (cd erizoAPI && npm install && ./build.sh)
   ```

   Verify that the resulting Node addon is arm64 and loads from the intended
   native Node.js runtime.

7. Review the FFmpeg changes for behavioral issues exposed by tests, especially
   delayed packets from the send/receive API, output-buffer sizing, frame
   ownership, channel layouts, and cleanup on initialization failure.

8. Only after the native build and tests pass, decide whether the Docker,
   Ubuntu, package-lock, and macOS dependency changes should be split into
   separate commits or retained together.
