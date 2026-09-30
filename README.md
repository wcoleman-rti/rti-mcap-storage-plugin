# RTI Recording Service MCAP Storage Plugin

## Quick Start

Requirements: RTI Connext DDS 7.7.0 (with Recording Service), CMake 3.20+, and a C++17 compiler. Set `NDDSHOME` and source the environment script for your Connext architecture.

```sh
source "$NDDSHOME/resource/scripts/rtisetenv_x64Linux4gcc8.5.0.bash"
cmake -B build
cmake --build build --parallel
LD_LIBRARY_PATH="$PWD/build/plugins/storage${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    "$NDDSHOME/bin/rtirecordingservice" \
    -cfgFile resources/config/McapRecorder.xml -cfgName mcap
```

The provided recorder configuration writes to `build/data.mcap` and stores DDS samples only. Start a publisher using the same domain and topic definitions to populate the archive. To replay the archive, use:

```sh
LD_LIBRARY_PATH="$PWD/build/plugins/storage${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    "$NDDSHOME/bin/rtireplayservice" \
    -cfgFile resources/config/McapReplayer.xml -cfgName mcap
```

## Components

- **Core** (`rti::mcap::core`): DDS topic-type to MCAP schema/channel and sample conversion, including the CDR-backed `DynamicData` fast path and SampleInfo mapping. Depends on Connext DDS core and MCAP headers, not file I/O or service libraries.
- **File** (`rti::mcap::file`): checked MCAP file reading/writing, summaries, and indexed message views. Depends on MCAP C++ v2.1.3, not Connext or Core.
- **Storage plugins** (`rti::mcap::storage::record` and `rti::mcap::storage::replay`): Recording/Replay Service adapters using both SDK components. Both use C++ namespace `rti::mcap::storage`. The services own DDS endpoints.

Core and File have matching C++ namespaces and CMake target names. These are
project-provided RTI extensions, not APIs shipped with Connext. Upstream MCAP
types remain in the global `::mcap` namespace.

```text
sdk/core/                   Public headers: include/rti/mcap/core/
sdk/file/                   Public headers: include/rti/mcap/file/
plugins/storage/            Both adapters and their private headers
tests/                      Component tests, support, test IDL, package consumers
benchmarks/                 Independently enabled benchmark
resources/cmake/            Build helpers and package templates
resources/config/           Recording/Replay Service XML configurations
resources/dependencies/     Pinned dependency integration; downloads stay in build/
```

The core defaults to `dds::core::xtypes::DynamicData` while allowing generated DDS topic types as template arguments. MCAP compression is disabled by default; enable LZ4 or Zstd with `RTI_MCAP_ENABLE_LZ4` or `RTI_MCAP_ENABLE_ZSTD` when their development libraries are installed. Compression dependencies are discovered only when File is built or consumed.

## Building and Consuming Components

Standalone builds enable Core, File, Storage, and project tests by default.
The independent build switches are `RTI_MCAP_BUILD_CORE`,
`RTI_MCAP_BUILD_FILE`, and `RTI_MCAP_BUILD_STORAGE`; Storage requires both SDK
components. Tests, benchmarks, and storage plugins default off when this
project is included through `add_subdirectory` or FetchContent. Parent
build-type, `BUILD_TESTING`, and shared/static preferences are not overridden.

Core only (does not build the MCAP implementation):

```sh
cmake -S . -B build/core-only -DRTI_MCAP_BUILD_STORAGE=OFF \
    -DRTI_MCAP_BUILD_FILE=OFF -DRTI_MCAP_BUILD_TESTS=OFF
cmake --build build/core-only --parallel 2
```

File only (does not require `NDDSHOME`, Connext, or DDS code generation):

```sh
cmake -S . -B build/file-only -DRTI_MCAP_BUILD_CORE=OFF \
    -DRTI_MCAP_BUILD_STORAGE=OFF -DRTI_MCAP_BUILD_TESTS=OFF
cmake --build build/file-only --parallel 2
```

Install the configured components with `cmake --install build --prefix /path/to/prefix`.
An external project can then use:

```cmake
find_package(RTIMcap 1 CONFIG REQUIRED COMPONENTS core)
add_executable(application main.cpp)
target_link_libraries(application PRIVATE rti::mcap::core)
```

Set `CMAKE_PREFIX_PATH` to that installation. Components are `core`, `file`,
and `storage`; requesting `storage` loads both plugin targets and their
dependencies. Omitting components loads all installed components. Required
unknown or uninstalled components fail explicitly. Core consumers need Connext
7.7.0 discovery (normally `NDDSHOME`); File consumers do not. Installs are
relocatable and do not expose plugin-private headers.

For source consumption:

```cmake
set(RTI_MCAP_BUILD_FILE OFF CACHE BOOL "")
add_subdirectory(path/to/rti-mcap)
target_link_libraries(application PRIVATE rti::mcap::core)
```

Public includes are `<rti/mcap/core/TopicConverter.hpp>`,
`<rti/mcap/core/SampleMetadata.hpp>`, and `<rti/mcap/file/File.hpp>`.
Service-loaded plugin binaries retain the `debug/lib` and `release/lib`
installation locations and their existing filenames.

### Clean API Migration

This is a breaking source/build API rename; no legacy aliases or forwarding
headers are provided. Reconfigure existing builds with the new options:

| Previous | Replacement |
|---|---|
| C++ `dds_mcap` | `rti::mcap::core` |
| C++ `dds_mcap::archive` | `rti::mcap::file` |
| C++ `rti::recording::storage::mcap` | `rti::mcap::storage` |
| CMake `DdsMcap::Core` / `DdsMcap::Archive` | `rti::mcap::core` / `rti::mcap::file` |
| `DDS_MCAP_BUILD_ARCHIVE` | `RTI_MCAP_BUILD_FILE` |
| `DDS_MCAP_BUILD_PLUGINS` | `RTI_MCAP_BUILD_STORAGE` |
| `DDS_MCAP_BUILD_BENCHMARKS` | `RTI_MCAP_BUILD_BENCHMARKS` |
| `DDS_MCAP_ENABLE_{LZ4,ZSTD,SANITIZERS}` | `RTI_MCAP_ENABLE_{LZ4,ZSTD,SANITIZERS}` |
| Project test selection through `BUILD_TESTING` | `RTI_MCAP_BUILD_TESTS` |

Plugin C entry points, shared-library filenames, XML property keys, channel
names, and the v1 metadata wire format remain unchanged. XML configuration
files now live under `resources/config/`; update command-line paths accordingly.

## SampleInfo Metadata

Recording and replay store sample data only by default. To include DDS SampleInfo, add the optional `rti.recording.storage.mcap.info_file` property to both plugin property sets in the XML configuration and set it to the same path as `rti.recording.storage.mcap.data_file`. The metadata channel is then stored in the same MCAP archive as the data channels. The project-owned metadata encoding is versioned; replay rejects unknown versions and mismatched data/metadata message identifiers.

## Versions and Tests

- RTI Connext DDS 7.7.0
- MCAP C++ v2.1.3
- C++17 and CMake 3.20+

Run the focused tests with:

```sh
cmake --preset debug
cmake --build --preset debug --parallel 2
ctest --preset debug
```

Equivalent `release` and `sanitizers` presets are provided. Sanitizers use
ASan and UBSan with GCC or Clang; the proprietary Connext libraries are not
instrumented. Presets require a Unix Makefiles environment; the regular
`cmake -S . -B build` workflow remains available with other generators.
Tests use pinned doctest v2.4.12 and CTest, with assertions active in Release.
The first test-enabled configure downloads doctest; production builds with
`RTI_MCAP_BUILD_TESTS=OFF` do not fetch it.

For an existing configured build:

```sh
ctest --test-dir build --output-on-failure
```

Tests exercise this project's converters, metadata codec, checked archive
adapter, configuration parsing, and recording/replay ownership and selectors.
They do not independently validate DDS transport/QoS or MCAP index/compression
algorithms. Generated test types and the framework main are compiled once;
plugin tests link the actual recording and replay libraries.

Installed-package smoke consumers live under `tests/packaging`. After installing
to a staging prefix, exercise an independently configured consumer with:

```sh
cmake -S tests/packaging -B build/consumer -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/path/to/prefix -DRTI_MCAP_COMPONENT=core
cmake --build build/consumer --parallel 2
build/consumer/consumer
```

Use `file` for DDS-free consumption or `storage` to resolve plugin file
locations. These checks validate this project's packaging, not dependency
functionality.

## Performance Benchmark

The benchmark is opt-in, excluded from the default build, and is not a
timing-based test gate:

```sh
cmake --preset release -DRTI_MCAP_BUILD_BENCHMARKS=ON
cmake --build --preset release --target rti_mcap_benchmark --parallel 2
build/release/benchmarks/rti_mcap_benchmark --samples 4000 --streams 4 \
    --batch 32 --metadata
# Indexed range replay: returns (4000 - 3000) * 4 samples.
build/release/benchmarks/rti_mcap_benchmark --samples 4000 --streams 4 \
    --batch 32 --metadata --start 3000
```

Omit `--metadata` for data-only recording/replay. Run `--help` for sample,
payload, batch, stream, chunk-size, and compression options. Replay counts are
checked before reporting results. Peak RSS includes dependencies and all
benchmark phases, not just the plugin. Compare repeated runs in the same
Release configuration rather than treating one measurement as a general
speedup guarantee.

Example before/after measurements on Linux with GCC 15.2 and Connext 7.7,
using the command above (16,000 plugin samples, metadata enabled, three
Release runs per version):

| Metric | Before | After |
|---|---:|---:|
| Plugin recording | 41.2-44.6 ms | 20.5-32.1 ms |
| Plugin replay | 707.9-714.4 ms | 27.6-34.5 ms |
| Aggregate replay open/seek | 6.3-6.8 ms | 2.3-3.6 ms |
| Peak process RSS | approximately 38,900 KiB | 38,996-39,300 KiB |

The plugin archive remained 12,251,321 bytes. Replay's main improvement comes
from bounded reuse of returned normal DynamicData, rather than changing MCAP
index semantics or exposing borrowed CDR buffers.

## Indexing, Ownership, and Error Contracts

- DDS reception time maps to MCAP `logTime`; source time maps to `publishTime`,
  falling back to reception time when the source timestamp is invalid.
  Standard MCAP indexes use log time and channel. The pinned MCAP version has
  no released source-time or general secondary-index API. Source-time
  sidecars/custom indexes remain future opt-in work.
- Chunking, message/chunk indexes, summaries, and existing CRC defaults remain
  enabled. Compression remains off by default. Benchmark chunk/compression
  options tune its native archive-writing phase, not the plugin's defaults.
- `NOT_READ` continues the stream cursor and ignores the selector's lower time
  bound. `ANY` uses a fresh range view without consuming that cursor. DDS time
  bounds are inclusive; the adapter safely translates the upper bound to MCAP's
  exclusive end. Negative `max_samples` means unlimited. Unsupported READ and
  instance-history selectors fail explicitly.
- `finished()` describes whether the last read exhausted the configured stream
  range, not just its requested batch or narrower selector range. `reset()`
  restarts the sequential cursor and clears a latched read failure; it does
  not invalidate outstanding loans.
- Each returned batch owns normal, field-accessible DynamicData and SampleInfo
  until its matching `return_loan()`. Returning another batch does not release
  it. At most 32 returned normal DynamicData objects per stream are reused;
  this is an idle-cache limit, not a read or outstanding-loan limit. Buffers
  grow with actual data instead of eagerly reserving the type's maximum bound.
  CDR-associated replay is not enabled.
- Converter message views and MCAP iterator payloads are borrowed. Consume or
  copy them before reusing converter scratch/input storage or advancing the
  iterator; keep the archive reader open while views and iterators are in use.
  Recording consumes callback-owned CDR storage synchronously.
- Numeric properties reject trailing text and negative unsigned values; path
  properties preserve spaces. Fatal errors throw explicitly, not from a log
  destructor. Write/read failures are reported and latched per stream.
  Archive writes and close use checked output; destruction reports cleanup
  failures without throwing. Metadata encoding remains version 1 (177 bytes),
  with full DDS sequence numbers and invalid-data lifecycle events preserved.
