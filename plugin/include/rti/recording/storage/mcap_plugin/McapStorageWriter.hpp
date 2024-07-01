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

#ifndef DDS_MCAP_STORAGE_WRITER_H
#define DDS_MCAP_STORAGE_WRITER_H

#include <rti/recording/storage/StorageWriter.hpp>
#include "rti/recording/storage/mcap_plugin/McapStreamWriter.hpp"

namespace rti::recording::storage::mcap_plugin {

/*
 * Convenience macro to forward-declare the C-style function that will be
 * called by RTI Recording Service to create your class.
 */
RTI_RECORDING_STORAGE_WRITER_CREATE_DECL(McapStorageWriter)

/*
 * This class acts as a factory for Stream Writer objects, that store data
 * samples in a text file, transforming them from dynamic data representation
 * into text.
 * This storage writer creates three files: 1) a data file with the text
 * samples; 2) a publication file, containing a subset of the information in the
 * DCPSPublication built-in discovery topic samples; and 3) an info file, that
 * only contains the starting and ending points in time where there are data
 * samples.
 */
class McapStorageWriter : public rti::recording::storage::StorageWriter {
public:
    McapStorageWriter(const rti::routing::PropertySet &properties);
    virtual ~McapStorageWriter();

    /*
     * Recording Service will call this method to create a Stream Writer object
     * associated with a user-data topic that has been discovered.
     * The property set passed as a parameter contains information about the
     * stream not provided by the stream info object. For example, Recording
     * Service will add the DDS domain ID as a property to this set.
     */
    rti::recording::storage::StorageStreamWriter *create_stream_writer(
            const rti::routing::StreamInfo &stream_info,
            const rti::routing::PropertySet &properties);

    /*
     * Recording Service will call this method to delete a previously created
     * Stream Writer (no matter if it was created with the
     * create_stream_writer() or create_discovery_stream_writer() method).
     */
    void delete_stream_writer(
            rti::recording::storage::StorageStreamWriter *stream_writer);
private:
    mcap::SharedMcapWriter mcap_writer;
    bool store_sample_info;
};


} // namespace rti::recording::storage::mcap_plugin

#endif  // DDS_MCAP_STORAGE_WRITER_H