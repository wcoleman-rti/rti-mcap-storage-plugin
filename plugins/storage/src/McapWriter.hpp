#ifndef RTI_RECORDING_STORAGE_MCAPWRITER_H_
#define RTI_RECORDING_STORAGE_MCAPWRITER_H_

#include <rti/recording/storage/StorageWriter.hpp>
#include <rti/recording/StreamInfo.hpp>
#include <rti/recording/PropertySet.hpp>
#include <rti/mcap/file/File.hpp>
#include <rti/mcap/core/TopicConverter.hpp>
#include <exception>
#include "Utils.hpp"
#include "Logger.hpp"


namespace rti::mcap::storage {

/*
 * Convenience macro to forward-declare the C-style function that will be
 * called by RTI Recording Service to create your class.
 */
RTI_RECORDING_STORAGE_WRITER_CREATE_DECL(McapFileWriter)

/** Recording Service storage writer for MCAP archives. */
class McapFileWriter : public ::rti::recording::storage::StorageWriter {
public:
    /** Creates the archive writer from the properties in the service configuration. */
    McapFileWriter(const ::rti::recording::PropertySet &properties);

    virtual ~McapFileWriter();

    /** Creates the per-topic stream writer for a discovered DDS stream. */
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
    std::shared_ptr<::rti::mcap::file::Writer> data_writer_;
    std::shared_ptr<::rti::mcap::file::Writer> info_writer_;
    ::mcap::SchemaId info_schema_id_;

    Logger logger;
};

/** Writes DDS samples and optional SampleInfo metadata to MCAP channels. */
class McapStreamWriter
        : public ::rti::recording::storage::DynamicDataStorageStreamWriter {
public:
    McapStreamWriter(
            std::shared_ptr<::rti::mcap::file::Writer> data_writer,
            std::shared_ptr<::rti::mcap::file::Writer> info_writer,
            std::shared_ptr<std::mutex> mutex,
            const ::rti::recording::StreamInfo &stream_info,
            uint32_t domain_id,
            ::mcap::SchemaId info_schema_id,
            Logger& logger);

    virtual ~McapStreamWriter();

    /** Writes the provided sample and SampleInfo sequences. */
    virtual void store(
            const std::vector<::dds::core::xtypes::DynamicData *> &data_seq,
            const std::vector<::dds::sub::SampleInfo *> &info_seq) override;

protected:

    ::mcap::Message convert_sample_data(
            const ::dds::core::xtypes::DynamicData &data,
            const ::dds::sub::SampleInfo &info,
            uint32_t sequence);
    
    ::mcap::Message convert_sample_info(
            const ::dds::core::xtypes::DynamicData &data,
            const ::dds::sub::SampleInfo &info,
            uint32_t sequence);

private:
    std::shared_ptr<std::mutex> mutex_;
    std::shared_ptr<::rti::mcap::file::Writer> data_writer_;
    std::shared_ptr<::rti::mcap::file::Writer> info_writer_;

    ::mcap::ChannelId data_channel_id_;
    ::mcap::ChannelId info_channel_id_;

    std::vector<char> cdr_data_buffer_;
    std::vector<char> metadata_buffer_;
    std::unique_ptr<
            ::rti::mcap::core::TopicConverter<::dds::core::xtypes::DynamicData>>
            topic_converter_;

    ::rti::recording::StreamInfo stream_info_;

    uint32_t domain_id_;
    uint64_t archive_sequence_{0};
    std::exception_ptr write_failure_;

    Logger logger;
};

/** Records service-level participant discovery times. */
class McapDiscoveryWriter
        : public ::rti::recording::storage::ParticipantStorageWriter {
public:
    McapDiscoveryWriter(
            std::shared_ptr<::rti::mcap::file::Writer> data_writer,
            std::shared_ptr<::rti::mcap::file::Writer> info_writer,
            std::shared_ptr<std::mutex> mutex,
            Logger& logger);

    virtual ~McapDiscoveryWriter();

    virtual void store(
            const std::vector<::dds::topic::ParticipantBuiltinTopicData *> &participant_data_seq,
            const std::vector<::dds::sub::SampleInfo *> &info_seq) override;

private:
    std::shared_ptr<std::mutex> mutex_;
    std::shared_ptr<::rti::mcap::file::Writer> data_writer_;
    std::shared_ptr<::rti::mcap::file::Writer> info_writer_;
    ::dds::core::Time start_time_;
    Logger logger;
};

}  // rti::mcap::storage

#endif // RTI_RECORDING_STORAGE_MCAPWRITER_H_