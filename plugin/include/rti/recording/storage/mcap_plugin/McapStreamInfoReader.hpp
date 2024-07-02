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

#ifndef DDS_MCAP_STREAMINFO_READER_H
#define DDS_MCAP_STREAMINFO_READER_H

#include <mcap/reader.hpp>
#include <rti/recording/storage/StorageStreamInfoReader.hpp>

#include "rti/recording/storage/mcap_plugin/common/common.hpp"


namespace rti::recording::storage::mcap_plugin {

/*
 * The discovery stream readers have to provide Replay/Converter with all the
 * different streams contained in the storage. In the case of our example, we
 * only provide one stream, for the only topic that's recorded (see the type
 * definition in file HelloMsg.idl).
 * This class is also in charge of providing information about the total range
 * of time where valid recorded data can be found, or for which the Recorder app
 * executed.
 */
class McapStreamInfoReader : public rti::recording::storage::StorageStreamInfoReader {
public:
    McapStreamInfoReader(
            mcap::SharedMcapReader mcap_reader,
            sample_order::SampleOrderKind sample_order);
    virtual ~McapStreamInfoReader() = default;

    /*
     * Implementation of the read operation. It should interpret the selector
     * state object that expresses the specific needs of Replay/Converter about
     * the data to be provided (data not read before vs data of any kind, lower
     * and upper time limits, etc).
     */
    virtual void read(
            std::vector<rti::routing::StreamInfo *> &sample_seq,
            const rti::recording::storage::SelectorState &selector);
    
    /*
     * The return loan operation should free any resources allocated by the
     * read() operation.
     */
    virtual void return_loan(std::vector<rti::routing::StreamInfo *> &sample_seq);
    
    /*
     * This method should flag Replay/Converter that all data related to the
     * data stream has been read and that we're ready for termination.
     */
    bool finished();

    void reset();

    /*
     * An int64-represented time-stamp (in nanoseconds) representing the
     * starting point in time where recorded data exists, or when the service
     * started executing.
     */
    virtual int64_t service_start_time();

    /*
     * An int64-represented time-stamp (in nanoseconds) representing the
     * final point in time where recorded data exists, or when the service
     * finished executing.
     */
    virtual int64_t service_stop_time();

private:
    mcap::SharedMcapReader mcap_reader;
    std::unordered_map<mcap::ChannelId, dds::sub::status::SampleState> message_states;
    bool is_finished;
    sample_order::SampleOrderKind sample_order;
    mcap::ReadMessageOptions mcap_read_options;

    void reset_message_states();
};

} // namespace rti::recording::storage::mcap_plugin

#include "rti/recording/storage/mcap_plugin/McapStreamInfoReader.cxx"

#endif  // DDS_MCAP_STREAMINFO_READER_H