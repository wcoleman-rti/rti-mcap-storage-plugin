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

#ifndef DDS_MCAP_STREAM_WRITER_H
#define DDS_MCAP_STREAM_WRITER_H

#include <mcap/writer.hpp>
#include <rti/recording/storage/StorageStreamWriter.hpp>
#include "rti/recording/storage/mcap_plugin/common/common.hpp"

namespace rti::recording::storage::mcap_plugin {

/**
 * This class implements the provided DynamicData specialization of a
 * StorageStreamWriter to transform dynamic data objects into a text
 * representation that it stores into a file. It also stores some of the
 * sample info fields (reception timestamp, valid data flag) so that this info
 * is later available for StorageStreamReaders (see file McapStorageReader.hpp)
 * to convert or replay samples within a specified time range.
 */
class McapStreamWriter : public rti::recording::storage::DynamicDataStorageStreamWriter {
public:
    McapStreamWriter(
        mcap::SharedMcapWriter mcap_writer,
        std::string stream_name,
        dds::core::xtypes::DynamicType type,
        bool store_sample_info);
    virtual ~McapStreamWriter();

    /*
     * This method receives a collection of Dynamic Data objects that are
     * transformed into a textual representation and stored in a text file.
     * Some of the fields in the SampleInfo object are also transformed.
     */
    void store(
            const std::vector<dds::core::xtypes::DynamicData *> &sample_seq,
            const std::vector<dds::sub::SampleInfo *> &info_seq);

private:
    mcap::SharedMcapWriter mcap_writer;
    std::string stream_name;
    dds::core::xtypes::DynamicType type;
    mcap::ChannelId mcap_data_channel_id;
    mcap::ChannelId mcap_info_channel_id;
    std::unique_ptr<mcap::Message> current_message;
    bool store_sample_info;
};

} // namespace rti::recording::storage::mcap_plugin

#include "rti/recording/storage/mcap_plugin/McapStreamWriter.cxx"

#endif  // DDS_MCAP_STREAM_WRITER_H