# RTI Recording Service MCAP Storage Plugin

This plugin uses the [RTI Recording Service Storage API](https://community.rti.com/static/documentation/connext-dds/7.3.0/doc/manuals/connext_dds_professional/services/recording_service/recorder/record_tutorials.html#plugging-in-custom-storage) to implement the ability to record to, and replay from, [MCAP file format](https://mcap.dev/spec).

## Dependencies

* [RTI Connext Professional 7.3.0](https://community.rti.com/static/documentation/connext-dds/7.3.0/doc/manuals/connext_dds_professional/installation_guide/installation_guide/Installation_Title.htm)
* [MCAP C++ v1.4.1](https://github.com/foxglove/mcap/tree/releases/cpp/v1.4.1)
* [CMake 3.11+](https://cmake.org/cmake/help/v3.11/)

*If an existing installation of MCAP C++ is not found with CMake's FindPackage, it will be retrieved and built locally within the build step/directory.*

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

### MCAP only supports DDS-IDL datatype definitions

MCAP does not support DDS-XML datatype definitions, which allows for more dynamic (de)serialization of discovered types at runtime. Connext does not support interpretting IDL datatype definitions as a string at runtime, which is how it is stored in MCAP Schemas.

As a workaround, the plugin stores the DDS-XML type as a key-value string (with key: `"dds.xml_type"`) in the Channel metadata. When replaying, if RTI Replay Service does not find the corresponding metadata in the recorded Channel, it can still use types defined and loaded via XML (using the `-cfgFile` option, `NDDS_QOS_PROFILES`, or via the default locations).

### Datatype inheritance is not supported in Foxglove

Inheritance is not supported in datatype definitions by the Foxglove MCAP implementation (as of [v1.4.1](https://github.com/foxglove/mcap/tree/releases/cpp/v1.4.1)).

For example, the following type definition for `Derived` is not supported by Foxglove Studio and MCAP CLI.

```c
struct Base {
    long a;
};

struct Derived : Base {
    long b;
};
```

This does not affect the ability to record and replay using RTI Recording Service. However, Foxglove MCAP applications will be unable to deserialize stored data.

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

### RTI Recording Service Storage API does not support SRO/Pass Through

RTI Recording Service does not support "Simple-Route Optimization", "Fast-Forwarding", or "Pass-Through" mode. This is a mode in which Recording Service passes a serialized sample to the storage plugin, and Replay Service accepts a serialized sample from the storage plugin. This would be well suited for MCAP since MCAP is storing the data in CDR-serialized format.

For now, data received by RTI Recording Service is deserialized, before being re-serialized into MCAP storage. Data replayed by RTI Replay Service is re-serialized as it is read from MCAP storage and before it is sent over DDS.

## Future Improvements

| Improvement   | Description
| -----------   | -----------
| Expose [`McapWriterOptions`](https://mcap.dev/docs/cpp/r832FE362A16BB6E8) as configuraable properties. | `chunkSize`, `enableDataCRC`
| Store slimmer `SampleInfo` data. | Store only: `valid`, `source_timestamp`, `reception_timestamp`, `original_publication_virtual_guid`, `original_publication_virtual_sequence_number` to still capture adequete metadata for `<publish_with_original_info>` on replay.
| Better Domain ID enforcement on replay. | Enable a boolean property to enable a per-message check on replay to validate domain ID. Otherwise requires update to the Storage API.
| More efficient CDR (de)serialization. | Use `get_cdr_buffer()` / `set_cdr_buffer()` APIs once supported in RTI Recording Service Storage API.
| Include example for [CompressedVideo.idl](https://github.com/foxglove/schemas/blob/main/schemas/omgidl/foxglove/CompressedVideo.idl). | Demonstrate compressed video recording. Can potentially use modified [RTI GStreamer Plugin](https://github.com/rticommunity/rticonnextdds-usecases/tree/master/VideoData).
| Expand tests. | Test usage of actual RTI Recording/Replay Service binaries. Test record/replay of specific `SampleInfo` fields.
