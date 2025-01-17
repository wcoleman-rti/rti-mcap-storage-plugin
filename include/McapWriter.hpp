#ifndef RTI_RECORDING_STORAGE_MCAPWRITER_H_
#define RTI_RECORDING_STORAGE_MCAPWRITER_H_

#include <rti/recording/storage/StorageWriter.hpp>
#include <rti/recording/StreamInfo.hpp>
#include <rti/recording/PropertySet.hpp>
#include <mcap/writer.hpp>
#include "idl/SampleInfo.hpp"
#include "Utils.hpp"
#include "Logger.hpp"


namespace rti::recording::storage::mcap {

/*
 * Convenience macro to forward-declare the C-style function that will be
 * called by RTI Recording Service to create your class.
 */
RTI_RECORDING_STORAGE_WRITER_CREATE_DECL(McapFileWriter)

/*
 * A Storage Writer implementation that uses the LevelDB database engine.
 *
 * This implementation will create a LevelDB database called 'metadata.dat'
 * where the start (first timestamp) and stop (last timestamp) times of the
 * plugin's usage are stored. This information is requested by Replay when
 * defining the replay time span.
 *
 * Because LevelDB is a key-value store, we need a key unique for every sample.
 * The key type, which is then serialized using RTI DDS Code Generator's
 * methods, is defined in file 'LevelDb_RecorderTypes.idl'. The value type is
 * also defined in this file. The DCPSPublication topic used by the publication
 * stream writers is also defined in this file as a subset of the information
 * in the topic's type.
 */
class McapFileWriter : public ::rti::recording::storage::StorageWriter {
public:
    /**
     * Constructor. The storage writer expects the following property to be set
     * in the passed properties: "rti.recording.examples.leveldb.working_dir".
     * The property should be set in the RTI Recording Service XML configuration
     * (see file leveldb_recorder.xml) in the <plugin><property> section.
     */
    McapFileWriter(const ::rti::recording::PropertySet &properties);

    virtual ~McapFileWriter();

    /**
     * Recording Service will call this method to create a Stream Writer object
     * associated with a user-data topic that has been discovered.
     * The property set passed as a parameter contains information about the
     * stream not provided by the stream info object. For example, Recording
     * Service will add the DDS domain ID as a property to this set.
     * For reference on how we're storing the keys and samples in the key-value
     * store, see the types 'UserDataKey' and 'UserDataValue' in the IDL file
     * associated with this example.
     */
    virtual ::rti::recording::storage::StorageStreamWriter *create_stream_writer(
            const ::rti::recording::StreamInfo &stream_info,
            const ::rti::recording::PropertySet &properties) override;

    /**
     * Recording Service will call this method to delete a previously created
     * Stream Writer (no matter if it was created with the
     * create_stream_writer() or create_publication_writer() method).
     */
    virtual void delete_stream_writer(
            ::rti::recording::storage::StorageStreamWriter *writer) override;

    virtual ::rti::recording::storage::ParticipantStorageWriter *create_participant_writer() override;

private:
    std::shared_ptr<std::mutex> mutex_;
    std::shared_ptr<::mcap::McapWriter> data_writer_;
    std::shared_ptr<::mcap::McapWriter> info_writer_;
    ::mcap::SchemaId info_schema_id_;

    Logger logger;
};

/**
 * This Stream Writer implementation will create a LevelDB database with the
 * stream name and the domain ID (of the format '<stream-name>@<domain-id>' to
 * store data samples.
 */
class McapStreamWriter
        : public ::rti::recording::storage::DynamicDataStorageStreamWriter {
public:
    McapStreamWriter(
            std::shared_ptr<::mcap::McapWriter> data_writer,
            std::shared_ptr<::mcap::McapWriter> info_writer,
            std::shared_ptr<std::mutex> mutex,
            const ::rti::recording::StreamInfo &stream_info,
            uint32_t domain_id,
            ::mcap::SchemaId info_schema_id,
            Logger& logger);

    virtual ~McapStreamWriter();

    /**
     * Write user-data to the associated LevelDB database.
     * For reference on how we're storing the keys and samples in the key-value
     * store, see the types 'UserDataKey' and 'UserDataValue' in the IDL file
     * associated with this example.
     */
    virtual void store(
            const std::vector<::dds::core::xtypes::DynamicData *> &data_seq,
            const std::vector<::dds::sub::SampleInfo *> &info_seq) override;

protected:

    ::mcap::Message convert_sample_data(
            const ::dds::core::xtypes::DynamicData &data,
            const ::dds::sub::SampleInfo &info);
    
    ::mcap::Message convert_sample_info(
            const ::dds::core::xtypes::DynamicData &data,
            const ::dds::sub::SampleInfo &info);

private:
    std::shared_ptr<std::mutex> mutex_;
    std::shared_ptr<::mcap::McapWriter> data_writer_;
    std::shared_ptr<::mcap::McapWriter> info_writer_;

    ::mcap::ChannelId data_channel_id_;
    ::mcap::ChannelId info_channel_id_;

    ::mcap::ByteArray mcap_data_buffer_;
    ::mcap::ByteArray mcap_info_buffer_;
    std::vector<char> cdr_data_buffer_;
    std::vector<char> cdr_info_buffer_;

    ::rti::recording::StreamInfo stream_info_;

    uint32_t domain_id_;

    Logger logger;
};

/**
 * 
 */
class McapDiscoveryWriter
        : public ::rti::recording::storage::ParticipantStorageWriter {
public:
    McapDiscoveryWriter(
            std::shared_ptr<::mcap::McapWriter> data_writer,
            std::shared_ptr<::mcap::McapWriter> info_writer,
            std::shared_ptr<std::mutex> mutex,
            Logger& logger);

    virtual ~McapDiscoveryWriter();

    virtual void store(
            const std::vector<::dds::topic::ParticipantBuiltinTopicData *> &participant_data_seq,
            const std::vector<::dds::sub::SampleInfo *> &info_seq) override;

private:
    std::shared_ptr<std::mutex> mutex_;
    std::shared_ptr<::mcap::McapWriter> data_writer_;
    std::shared_ptr<::mcap::McapWriter> info_writer_;
    ::dds::core::Time start_time_;
    Logger logger;
};

}  // rti::recording::storage::mcap

#endif // RTI_RECORDING_STORAGE_MCAPWRITER_H_