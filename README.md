# RTI Recording Service MCAP Storage Plugin

This plugin uses the [RTI Recording Service Storage API](https://community.rti.com/static/documentation/connext-dds/7.3.0/doc/manuals/connext_dds_professional/services/recording_service/recorder/record_tutorials.html#plugging-in-custom-storage) to implement the ability to record to, and replay from, [MCAP file format](https://mcap.dev/spec).


## Dependencies

* [RTI Connext Professional 7.3.0](https://community.rti.com/static/documentation/connext-dds/7.3.0/doc/manuals/connext_dds_professional/installation_guide/installation_guide/Installation_Title.htm)
* [MCAP C++ v1.4.0](https://github.com/foxglove/mcap/tree/releases/cpp/v1.4.0)
* [CMake 3.11+](https://cmake.org/cmake/help/v3.11/)


## Building

```shell
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=ON [-DCMAKE_BUILD_TYPE=RELEASE] [-DCMAKE_BUILD_TYPE=DEBUG] ..
cmake --build .
```

## Installing

```shell
cmake [--install-prefix ./lib] --install . 
```

## Usage


1. Configure the Connext environment.

    ```sh
    source <path/to/connext/installation>/resource/scripts/rtisetenv_<architecture>.bash
    ```

2. If needed, set LD_LIBRARY_PATH to find the MCAP Storage Plugin libraries.

    ```sh
    export LD_LIBRARY_PATH=<install_prefix/plugin>:LD_LIBRARY_PATH
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

### Replay Does Not Maintain Timestamp Integrity
Replaying recorded MCAP data is not published with the same frequency at which it was recorded. This is currently under investigation as it renders practical Replay functionality useless.

### Instance History Replay not supported
[Replaying Instance History](https://community.rti.com/static/documentation/connext-dds/7.3.0/doc/manuals/connext_dds_professional/services/recording_service/replay/replay_usage.html#section-using-replay-instance-history) is not supported by this version of the MCAP Storage Plugin. The MCAP format specification does not account for instances and does not support tracking of instance data natively. To support this, a separate, plugin-defined MCAP channel for each user-data MCAP channel will likely need to be utilized to concurrently track instance metadata.

### Datatype inheritance is not supported
Inheritance is not supported in datatype definitions by the Foxglove MCAP implementation (as of [v1.4.0](https://github.com/foxglove/mcap/tree/releases/cpp/v1.4.0)). This should not affect the ability to record and replay using RTI Recording Service. However, Foxglove MCAP applications will be unable to deserialize stored data.

For example, the following type definition for `Derived` is not supported by Foxglove.
```c
struct Base {
    long a;
};

struct Derived : Base {
    long b;
};
```
