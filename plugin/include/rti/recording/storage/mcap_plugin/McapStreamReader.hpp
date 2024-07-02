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

#ifndef DDS_MCAP_STREAM_READER_H
#define DDS_MCAP_STREAM_READER_H

#include <mcap/reader.hpp>
#include <rti/recording/storage/StorageStreamReader.hpp>

#include "rti/recording/storage/mcap_plugin/common/common.hpp"

namespace rti::recording::storage::mcap_plugin {

/*
 * This class uses the provided DynamicData specialization of a
 * StorageStreamReader to read data stored by the classes in file
 * McapStorageWriter.hpp, for a given data stream (DDS topic).
 * All data is stored in a text file. Data is serialized to a text format.
 * Note: this example is only fit to work with the provided type definition
 * (type HelloMsg - see HelloMsg.idl for more information).
 */

class McapStreamReader : public rti::recording::storage::DynamicDataStorageStreamReader {
public:

    McapStreamReader(
            mcap::SharedMcapReader mcap_reader,
            std::string stream_name,
            dds::core::xtypes::DynamicType type,
            sample_order::SampleOrderKind sample_order);

    virtual ~McapStreamReader() = default;
    
    /*
     * Implementation of the read operation. It should interpret the selector
     * state object that expresses the specific needs of Replay/Converter about
     * the data to be provided (data not read before vs data of any kind, lower
     * and upper time limits, etc).
     */
    virtual void read(
            std::vector<dds::core::xtypes::DynamicData *> &sample_seq,
            std::vector<dds::sub::SampleInfo *> &info_seq,
            const rti::recording::storage::SelectorState &selector);

    /*
     * The return loan operation should free any resources allocated by the
     * read() operation.
     */
    void return_loan(
            std::vector<dds::core::xtypes::DynamicData *> &sample_seq,
            std::vector<dds::sub::SampleInfo *> &info_seq);

    /*
     * This method should flag Replay/Converter that all data related to the
     * data stream has been read and that we're ready for termination.
     */
    bool finished();

    void reset();

private:
    mcap::SharedMcapReader mcap_reader;
    mcap::SharedObj<mcap::IndexedMessageReader> indexed_reader;
//     std::unique_ptr<mcap::IndexedMessageReader> indexed_reader;
    mcap::SharedObj<mcap::Message> current_message;
    // std::unique_ptr<mcap::Message> current_message;
    dds::core::xtypes::DynamicType type;
    std::unordered_map<uint32_t, dds::sub::status::SampleState> message_states;
    std::unordered_map<dds::core::InstanceHandle, int32_t> read_instance_map;
    bool is_finished;
    sample_order::SampleOrderKind sample_order;
    mcap::ReadMessageOptions mcap_read_options;
    std::string data_stream_name;
    std::string info_stream_name;
    mcap::ChannelId mcap_data_channel_id;
    mcap::ChannelId mcap_info_channel_id;

    void reset_message_states();
    void reset_indexed_reader(mcap::Timestamp min_timestamp = 0, mcap::Timestamp max_timestamp = mcap::MaxTime);
    void queue_next_message();
};

} // namespace rti::recording::storage::mcap_plugin

#include "rti/recording/storage/mcap_plugin/McapStreamReader.cxx"

#endif  // DDS_MCAP_STREAM_READER_H