/*
 * (c) 2019 Copyright, Real-Time Innovations, Inc.  All rights reserved.
 *
 * RTI grants Licensee a license to use, modify, compile, and create derivative
 * works of the Software.  Licensee has the right to distribute object form
 * only for use with RTI products.  The Software is provided "as is", with no
 * warranty of any type, including any warranty for fitness for any purpose.
 * RTI is under no obligation to maintain or support the Software.  RTI shall
 * not be liable for any incidental or consequential damages arising out of the
 * use or inability to use the software.
 */

#ifndef DDS_MCAP_STORAGE_READER_H
#define DDS_MCAP_STORAGE_READER_H

#include <rti/recording/storage/StorageReader.hpp>

#include "rti/recording/storage/mcap_plugin/McapStreamReader.hpp"
#include "rti/recording/storage/mcap_plugin/McapStreamInfoReader.hpp"
// #include "mcap/reader_impl.hpp"

namespace rti::recording::storage::mcap_plugin {


/*
 * Convenience macro to forward-declare the C-style function that will be
 * called by RTI Recording Service to create your class.
 */
RTI_RECORDING_STORAGE_READER_CREATE_DECL(McapStorageReader)

/**
 * This class acts as a factory for objects of classes PluginStreamReader
 * and PluginStreamInfoReader. These objects are used by Replay and/or
 * Converter to retrieve data from the storage.
 */
class McapStorageReader : public rti::recording::storage::StorageReader {
public:
    McapStorageReader(const rti::routing::PropertySet &properties);
    ~McapStorageReader();

    rti::recording::storage::StorageStreamInfoReader *create_stream_info_reader(
            const rti::routing::PropertySet &properties);

    void delete_stream_info_reader(
            rti::recording::storage::StorageStreamInfoReader *stream_info_reader);

    rti::recording::storage::StorageStreamReader *create_stream_reader(
            const rti::routing::StreamInfo &stream_info,
            const rti::routing::PropertySet &properties);

    void delete_stream_reader(
            rti::recording::storage::StorageStreamReader *stream_reader);

private:
    mcap::SharedMcapReader mcap_reader;

};

} // namespace rti::recording::storage::mcap_plugin

#endif  // DDS_MCAP_STORAGE_READER_H
