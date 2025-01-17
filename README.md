# RTI Recording Service MCAP Storage Plugin

This plugin uses the [RTI Recording Service Storage API](https://community.rti.com/static/documentation/connext-dds/7.3.0/doc/manuals/connext_dds_professional/services/recording_service/recorder/record_tutorials.html#plugging-in-custom-storage) to implement the ability to record to, and replay from, [MCAP file format](https://mcap.dev/spec).

## Dependencies

* [RTI Connext Professional 7.3.0](https://community.rti.com/static/documentation/connext-dds/7.3.0/doc/manuals/connext_dds_professional/installation_guide/installation_guide/Installation_Title.htm)
* [MCAP C++ v1.4.1](https://github.com/foxglove/mcap/tree/releases/cpp/v1.4.1)
* [CMake 3.11+](https://cmake.org/cmake/help/v3.11/)

### Optional

* [LZ4 v1.9.4](https://github.com/lz4/lz4/releases/tag/v1.9.4) (for compression)
* [ZSTD v1.5.2](https://github.com/facebook/zstd/releases/tag/v1.5.2) (for compression)

## Building

```shell
source <path/to/connext/installation>/resource/scripts/rtisetenv_<architecture>.bash

mkdir build-release
cd build-release
cmake [-DCMAKE_BUILD_TYPE=Debug] ..
cmake --build .
```

## Installing

```shell
cmake --install .
```

*By default, libraries will be installed to: `$NDDSHOME/third_party/rtirecordingplugins/$CONNEXTDDS_ARCH/<release/debug>/lib`*

## Usage

1. Configure the Connext environment.

    ```sh
    source <path/to/connext/installation>/resource/scripts/rtisetenv_<architecture>.bash
    ```

2. If needed, set LD_LIBRARY_PATH to find the MCAP Storage Plugin libraries.

    ```sh
    export LD_LIBRARY_PATH=$NDDSHOME/third_party/rtirecordingplugins/$CONNEXTDDS_ARCH/release/lib:LD_LIBRARY_PATH
    ```

    Or if you have installed libraries to another directory:

    ```sh
    export LD_LIBRARY_PATH=<install_directory>/lib:LD_LIBRARY_PATH
    ```

3. Run Recording Service

    ```sh
    $NDDSHOME/bin/rtirecordingservice -cfgFile ./config/McapRecorder.xml -cfgName mcap
    ```

4. Run Replay Service

    ```sh
    $NDDSHOME/bin/rtireplayservice -cfgFile ./config/McapReplayer.xml -cfgName mcap
    ```

## Known Limitations

### Datatype inheritance is not supported in Foxglove

Inheritance is not supported in datatype definitions by the Foxglove MCAP implementation (as of [v1.4.1](https://github.com/foxglove/mcap/tree/releases/cpp/v1.4.1)). This should not affect the ability to record and replay using RTI Recording Service. However, Foxglove MCAP applications will be unable to deserialize stored data.

For example, the following type definition for `Derived` is not supported by Foxglove Studio and MCAP CLI.

```c
struct Base {
    long a;
};

struct Derived : Base {
    long b;
};
```

### Replaying by source timestamp is not supported

Foxglove's C++ MCAP API does not support indexing stored data by publish (source) time. See [https://github.com/foxglove/mcap/issues/287](https://github.com/foxglove/mcap/issues/287) for more details.

### MCAP does not support DDS keys or instances

The MCAP standard has no concept of an instance within a Channel (DDS Topic equivalent). This does not affect the ability to store and replay data. It does prevent the ability to implement instance-specific functionality efficiently (e.g. ["state of the world"](https://community.rti.com/static/documentation/connext-dds/current/doc/manuals/connext_dds_professional/services/recording_service/replay/replay_usage.html#section-using-replay-instance-history)).

### MCAP does not support Domains

MCAP does support the concept of Domains.

There have been mitigations to cover most edge cases to indicate Domain ID in MCAP Channel metadata. However, it is possible two Topics with equivalent names may have data replayed on incorrect Domain IDs.

### MCAP does not support Sample Metadata (e.g. DDS SampleInfo)

MCAP only provides a limited set of metadata for each Message recorded:

* sequence number
* publish time
* log time
* channel

This is enough metadata to replay data minimally. However, to encapsulate additional metadata and replicate such events as instance states or virtual GUIDs, more metadata must be captured.

This storage plugin allows for recording (and replaying, if found) a parallel Channel of CDR-serialized SampleInfo for each DDS Sample recorded. This should allow complete functionality expected when using `<publish_with_original_info>` set to `true`.

To record this parallel metadata channel, set `rti.recording.storage.mcap.info_file` plugin property in both the Recording and Replaying XML configuration files.
