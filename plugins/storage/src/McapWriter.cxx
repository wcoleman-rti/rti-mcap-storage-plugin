#include <cassert>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <rti/recording/storage/StorageDefs.hpp>
#include "McapWriter.hpp"
#include "mcap/internal.hpp"


namespace rti::mcap::storage {
namespace {

::mcap::Message metadata_message(
        ::mcap::ChannelId channel,
        uint32_t sequence,
        const ::rti::mcap::core::SampleMetadata& metadata,
        std::vector<char>& buffer)
{
    ::rti::mcap::core::encode_sample_metadata(metadata, buffer);
    ::mcap::Message message;
    message.channelId = channel;
    message.sequence = sequence;
    message.logTime = ::rti::mcap::core::timestamp_to_mcap(metadata.reception_timestamp);
    message.publishTime = metadata.source_timestamp.valid
            ? ::rti::mcap::core::timestamp_to_mcap(metadata.source_timestamp) : message.logTime;
    message.data = reinterpret_cast<const std::byte*>(buffer.data());
    message.dataSize = buffer.size();
    return message;
}

}  // namespace

/*
 * Convenience macro to define the C-style function that will be called by RTI
 * Recording Service to create the main class.
 */
RTI_RECORDING_STORAGE_WRITER_CREATE_DEF(McapFileWriter)

/**
 * In the XML configuration, under the property tag for the storage plugin, a
 * collection of name/value pairs can be passed. This example expects the
 * working directory to be passed as a property here (see WORKING_DIR_PROPERTY
 * above).
 */
McapFileWriter::McapFileWriter(const rti::routing::PropertySet &properties)
        : StorageWriter(properties),
          mutex_(std::make_shared<std::mutex>()),
          info_schema_id_(0),
          logger(Logger::LogLevel(
                    get_from_properties_opt<int>(properties, 
                        RTI_XML_PROPERTY__LOGGING_LOG_LEVEL)
                        .value_or(Logger::LogLevel::WARN)))
{
    auto data_filename = get_from_properties_req<std::string>(
        properties,
        RTI_XML_PROPERTY__DATA_FILENAME);
    logger  << Logger::INFO << "get_from_properties_req() [" << RTI_XML_PROPERTY__DATA_FILENAME << "]"
            << ", value: " << std::quoted(data_filename);

    auto info_filename = get_from_properties_opt<std::string>(
        properties,
        RTI_XML_PROPERTY__INFO_FILENAME);
    logger  << Logger::INFO << "get_from_properties_opt() [" << RTI_XML_PROPERTY__INFO_FILENAME << "]"
            << ", value: " << info_filename;


    data_writer_ = std::make_shared<::rti::mcap::file::Writer>();
    if (info_filename) {
        if (info_filename == data_filename) {
            // if data & info are same file, reuse the MCAP writer
            info_writer_ = data_writer_;
        }
        else {
            info_writer_ = std::make_shared<::rti::mcap::file::Writer>();
        }
    }

    auto compression_kind = get_from_properties_opt<std::string>(
        properties,
        RTI_XML_PROPERTY__COMPRESSION_KIND);
    logger  << Logger::INFO << "get_from_properties_opt() [" << RTI_XML_PROPERTY__COMPRESSION_KIND << "]"
            << ", value: " << compression_kind;
    
    auto compression_level = get_from_properties_opt<std::string>(
        properties,
        RTI_XML_PROPERTY__COMPRESSION_LEVEL);
    logger  << Logger::INFO << "get_from_properties_opt() [" << RTI_XML_PROPERTY__COMPRESSION_LEVEL << "]"
            << ", value: " << compression_level;

    ::mcap::McapWriterOptions options("");
    options.compression = ::mcap::Compression::None;

    if (compression_kind) {
        std::transform(compression_kind->begin(), compression_kind->end(), compression_kind->begin(),
            [](unsigned char c){ return std::tolower(c); });
        if (*compression_kind == ::mcap::internal::CompressionString(::mcap::Compression::None) || 
                *compression_kind == "none") {
            options.compression = ::mcap::Compression::None;
        } 
    #ifndef MCAP_COMPRESSION_NO_LZ4
        else if (*compression_kind == ::mcap::internal::CompressionString(::mcap::Compression::Lz4)) {
            options.compression = ::mcap::Compression::Lz4;
        } 
    #endif

    #ifndef MCAP_COMPRESSION_NO_ZSTD
        else if (*compression_kind == ::mcap::internal::CompressionString(::mcap::Compression::Zstd)) {
            options.compression = ::mcap::Compression::Zstd;
        } 
    #endif
        else {
            logger.fail([&](std::ostream& message) {
                message << "Invalid MCAP compression kind: " << compression_kind;
            });
        }
        logger  << Logger::INFO << "options.Compression"
            << ", value: " << ::mcap::internal::CompressionString(options.compression);
    }

    if (compression_level) {
        std::transform(compression_level->begin(), compression_level->end(), compression_level->begin(),
            [](unsigned char c){ return std::tolower(c); });
        if (*compression_level == "fastest" || 
                *compression_level == std::to_string(static_cast<int>(::mcap::CompressionLevel::Fastest))) {
            options.compressionLevel = ::mcap::CompressionLevel::Fastest;
        } else if (*compression_level == "fast" || 
                *compression_level == std::to_string(static_cast<int>(::mcap::CompressionLevel::Fast))) {
            options.compressionLevel = ::mcap::CompressionLevel::Fast;
        } else if (*compression_level == "default" || 
                *compression_level == std::to_string(static_cast<int>(::mcap::CompressionLevel::Default))) {
            options.compressionLevel = ::mcap::CompressionLevel::Default;
        } else if (*compression_level == "slow" || 
                *compression_level == std::to_string(static_cast<int>(::mcap::CompressionLevel::Slow))) {
            options.compressionLevel = ::mcap::CompressionLevel::Slow;
        } else if (*compression_level == "slowest" || 
                *compression_level == std::to_string(static_cast<int>(::mcap::CompressionLevel::Slowest))) {
            options.compressionLevel = ::mcap::CompressionLevel::Slowest;
        } else {
            logger.fail([&](std::ostream& message) {
                message << "Invalid MCAP compression level: " << compression_level;
            });
        }
        logger  << Logger::INFO << "options.compressionLevel"
                << ", value: " << static_cast<int>(options.compressionLevel);
        

    }

    if (data_writer_) {
        if (auto status = data_writer_->open(data_filename, options); !status.ok()) {
            logger.fatal("Unable to open MCAP data file " + data_filename + ": " + status.message);
        }
        else {
            logger  << Logger::INFO << "open() [data]"
                    << ", file: " << data_filename;
        }
    }

    if (info_writer_) {
        if (info_writer_ == data_writer_) {
            logger  << Logger::INFO << "open() [info]"
                    << ", file: " << data_filename
                    << ", reusing data MCAP file for info";
        }
        else {
            assert(info_filename);
            if (auto status = info_writer_->open(*info_filename, options); !status.ok()) {
                logger.fatal("Unable to open MCAP info file " + *info_filename + ": " + status.message);
            }
            else {
                logger  << Logger::INFO << "open() [info]"
                        << ", file: " << *info_filename;
            }
        }

        const auto schema_text = ::rti::mcap::core::sample_metadata_schema();
        ::mcap::Schema info_schema(
            "dds_mcap.SampleMetadata",
            std::string(::rti::mcap::core::sample_metadata_encoding()),
            std::vector<std::byte>(
                    reinterpret_cast<const std::byte*>(schema_text.data()),
                    reinterpret_cast<const std::byte*>(
                            schema_text.data() + schema_text.size())));
        info_writer_->addSchema(info_schema);
        info_schema_id_ = info_schema.id;
        logger  << Logger::INFO << "addSchema() [info]"
                << ", name: " << std::quoted(info_schema.name)
                << ", schema: " << std::to_string(info_schema.id);
        logger  << Logger::DEBUG << "addSchema() [info]"
                << ", schema: \n"
                << std::string(
                        reinterpret_cast<const char*>(info_schema.data.data()),
                        info_schema.data.size());
    }
}

McapFileWriter::~McapFileWriter()
{
    auto lock = std::lock_guard(*mutex_);
    const auto close = [this](
            const std::shared_ptr<::rti::mcap::file::Writer>& writer,
            const char* kind) {
        try {
            writer->close();
            const auto statistics = writer->statistics();
            logger << Logger::INFO << "statistics() [" << kind << "]"
                   << ", message count: " << statistics.messageCount
                   << ", schema count: " << statistics.schemaCount
                   << ", channel count: " << statistics.channelCount
                   << ", attachment count: " << statistics.attachmentCount
                   << ", metadata count: " << statistics.metadataCount
                   << ", chunk count: " << statistics.chunkCount
                   << ", message start time: " << statistics.messageStartTime
                   << ", message end time: " << statistics.messageEndTime;
            for (const auto& [channel_id, message_count] : statistics.channelMessageCounts) {
                logger << Logger::DEBUG << "statistics() [" << kind << "]"
                       << ", channel: " << channel_id
                       << ", message count: " << message_count;
            }
        } catch (const std::exception& error) {
            logger << Logger::ERROR << "Unable to finalize MCAP " << kind
                   << " archive: " << error.what();
        }
    };
    if (data_writer_) {
        close(data_writer_, "data");
    }
    if (info_writer_ && info_writer_ != data_writer_) {
        close(info_writer_, "info");
    }
}

rti::recording::storage::StorageStreamWriter *McapFileWriter::
        create_stream_writer(
                const rti::routing::StreamInfo &stream_info,
                const rti::routing::PropertySet &properties)
{
    /*
     * Recorder will pass the Domain ID information as a property. Get its value
     * and pass it to the stream writer constructor.
     * Note: we provide the utility below to get a value of a certain type from
     * a set of properties */
    auto domain_id = get_from_properties_req<uint32_t>(
            properties,
            rti::recording::domain_id_property_name());
    logger  << Logger::DEBUG << "get_from_properties_req() [" << rti::recording::domain_id_property_name() << "]"
            << ", value: " << domain_id;

    return new McapStreamWriter(
        data_writer_,
        info_writer_,
        mutex_,
        stream_info,
        domain_id,
        info_schema_id_,
        logger);
}

void McapFileWriter::delete_stream_writer(
        rti::recording::storage::StorageStreamWriter *writer)
{
    delete writer;
    logger  << Logger::DEBUG << "delete_stream_writer()";
}

rti::recording::storage::ParticipantStorageWriter *McapFileWriter::create_participant_writer()
{
    return new McapDiscoveryWriter(
        data_writer_,
        info_writer_,
        mutex_,
        logger);
}

McapStreamWriter::McapStreamWriter(
        std::shared_ptr<::rti::mcap::file::Writer> data_writer,
        std::shared_ptr<::rti::mcap::file::Writer> info_writer,
        std::shared_ptr<std::mutex> mutex,
        const rti::routing::StreamInfo &stream_info,
        uint32_t domain_id,
        ::mcap::SchemaId info_schema_id,
        Logger& logger)
      : mutex_(mutex),
        data_writer_(data_writer),
        info_writer_(info_writer),
        data_channel_id_(0),
        info_channel_id_(0),
        stream_info_(stream_info),
        domain_id_(domain_id),
        logger(logger)
{
    auto lock = std::lock_guard(*mutex_);
    topic_converter_ = std::make_unique<
            ::rti::mcap::core::TopicConverter<::dds::core::xtypes::DynamicData>>(
                    ::rti::mcap::core::TopicConverter<
                            ::dds::core::xtypes::DynamicData>::describe(
                            stream_info.stream_name(),
                            stream_info.type_info().dynamic_type(),
                            static_cast<int32_t>(domain_id)));

    const auto& descriptor = topic_converter_->descriptor();
    auto data_schema = descriptor.schema();
    data_writer_->addSchema(data_schema);
    logger  << Logger::INFO << "addSchema() [data]"
            << ", stream: " << std::quoted(stream_info.stream_name())
            << ", name: " << std::quoted(data_schema.name)
            << ", schema: " << std::to_string(data_schema.id);
    logger  << Logger::DEBUG << "addSchema() [data]"
            << ", schema DDS type: \n"
            << std::string(
                    reinterpret_cast<const char*>(data_schema.data.data()),
                    data_schema.data.size());

    ::mcap::Channel data_channel(
        topic_data_channel_name(stream_info.stream_name(), domain_id), // topic
        "cdr", // message encoding
        data_schema.id // schema id
    );
    data_channel.metadata[MCAP_CHANNEL_METADATA__DDS_XML_TYPE] =
        descriptor.dds_xml;
    data_channel.metadata[MCAP_CHANNEL_METADATA__DDS_TOPIC_NAME] =
        stream_info.stream_name();
    data_channel.metadata[MCAP_CHANNEL_METADATA__DDS_DOMAIN_ID] = std::to_string(domain_id);
    auto info_topic_name = topic_info_channel_name(stream_info.stream_name(), domain_id);
    if (info_writer_) {
        data_channel.metadata[MCAP_CHANNEL_METADATA__INFO_CHANNEL_TOPIC] = info_topic_name;
    }
    data_writer_->addChannel(data_channel);
    data_channel_id_ = data_channel.id;
    logger  << Logger::INFO << std::string("addChannel() [data]")
            << ", stream: " << std::quoted(stream_info.stream_name())
            << ", domain: " << domain_id
            << ", channel: " << data_channel.id
            << ", topic: " << std::quoted(data_channel.topic);

    if (info_writer_) {
        ::mcap::Channel info_channel(
            info_topic_name, // topic
            std::string(::rti::mcap::core::sample_metadata_encoding()),
            info_schema_id // schema id
        );
        info_channel.metadata[MCAP_CHANNEL_METADATA__DDS_DOMAIN_ID] = std::to_string(domain_id);
        info_channel.metadata[MCAP_CHANNEL_METADATA__INFO_CHANNEL_FLAG] = "1";
        info_channel.metadata["dds_mcap.sample_metadata.version"] = "1";
        info_writer_->addChannel(info_channel);
        info_channel_id_ = info_channel.id;
        logger  << Logger::INFO << std::string("addChannel() [info]")
                << ", stream: " << std::quoted(stream_info_.stream_name())
                << ", domain: " << domain_id
                << ", channel: " << info_channel.id
                << ", topic: " << std::quoted(info_channel.topic);
    }
}

McapStreamWriter::~McapStreamWriter()
{
}

void McapStreamWriter::store(
        const std::vector<dds::core::xtypes::DynamicData *> &data_seq,
        const std::vector<dds::sub::SampleInfo *> &info_seq)
{
    auto lock = std::lock_guard(*mutex_);
    if (write_failure_) {
        std::rethrow_exception(write_failure_);
    }
    if (data_seq.size() != info_seq.size()) {
        throw std::invalid_argument("MCAP store requires matching data and SampleInfo counts");
    }
    try {
        for (size_t i = 0; i < data_seq.size(); ++i) {
            if (!info_seq[i] || (info_seq[i]->valid() && !data_seq[i])) {
                throw std::invalid_argument("MCAP store received a null valid sample or SampleInfo");
            }
            const auto& info = *info_seq[i];
            const auto metadata = ::rti::mcap::core::to_sample_metadata(info);
            uint32_t sequence;
            if (info_writer_) {
                if (archive_sequence_ > std::numeric_limits<uint32_t>::max()) {
                    throw std::overflow_error("MCAP per-stream metadata sequence space is exhausted");
                }
                sequence = static_cast<uint32_t>(archive_sequence_);
            } else {
                sequence = convert_sequence_number(info->reception_sequence_number());
            }

            std::optional<::mcap::Message> data_message;
            if (info.valid()) {
                data_message = topic_converter_->to_message(
                        data_channel_id_, sequence, *data_seq[i], metadata, cdr_data_buffer_).message;
            }
            if (info_writer_) {
                const auto mcap_info = metadata_message(
                        info_channel_id_, sequence, metadata, metadata_buffer_);
                if (auto status = info_writer_->write(mcap_info); !status.ok()) {
                    throw std::runtime_error("Unable to write MCAP SampleInfo: " + status.message);
                }
                if (logger.enabled(Logger::DEBUG)) {
                    logger << Logger::DEBUG << "write() [info]"
                           << ", stream: " << stream_info_.stream_name()
                           << ", channel: " << mcap_info.channelId
                           << ", sequence: " << mcap_info.sequence;
                }
            }

            if (data_message) {
                assert(data_writer_);
                const auto& mcap_data = *data_message;
                if (auto status = data_writer_->write(mcap_data); !status.ok()) {
                    throw std::runtime_error("Unable to write MCAP data: " + status.message);
                }
                if (logger.enabled(Logger::DEBUG)) {
                    logger << Logger::DEBUG << "write() [data]"
                           << ", stream: " << std::quoted(stream_info_.stream_name())
                           << ", channel: " << mcap_data.channelId
                           << ", sequence: " << mcap_data.sequence;
                }
            }
            if (info_writer_) {
                ++archive_sequence_;
            }
        }
    } catch (const std::exception& error) {
        logger << Logger::ERROR << "MCAP store failed for " << stream_info_.stream_name()
               << ": " << error.what();
        write_failure_ = std::current_exception();
        throw;
    }
}

::mcap::Message McapStreamWriter::convert_sample_data(
        const ::dds::core::xtypes::DynamicData &data,
        const ::dds::sub::SampleInfo &info,
        uint32_t sequence)
{
    return topic_converter_->to_message(
            data_channel_id_,
            sequence,
            data,
            info,
            cdr_data_buffer_)
            .message;
}

::mcap::Message McapStreamWriter::convert_sample_info(
        const ::dds::core::xtypes::DynamicData &,
        const ::dds::sub::SampleInfo &info,
        uint32_t sequence)
{
    return metadata_message(
            info_channel_id_, sequence, ::rti::mcap::core::to_sample_metadata(info), metadata_buffer_);
}

McapDiscoveryWriter::McapDiscoveryWriter(
        std::shared_ptr<::rti::mcap::file::Writer> data_writer,
        std::shared_ptr<::rti::mcap::file::Writer> info_writer,
        std::shared_ptr<std::mutex> mutex,
        Logger& logger)
      : mutex_(mutex),
        data_writer_(data_writer),
        info_writer_(info_writer),
        start_time_(::dds::core::Time::zero()),
        logger(logger)
{
}

McapDiscoveryWriter::~McapDiscoveryWriter()
{
    auto lock = std::lock_guard(*mutex_);
    try {
        logger << Logger::DEBUG << "~McapDiscoveryWriter() [discovery]";

        ::mcap::Timestamp end_timestamp = ::mcap::MaxTime;
        if (data_writer_) {
            end_timestamp = data_writer_->statistics().messageEndTime;
        }
        if (info_writer_ && info_writer_ != data_writer_) {
            end_timestamp = std::min(end_timestamp, info_writer_->statistics().messageEndTime);
        }

        ::mcap::Metadata file_metadata;
        file_metadata.name = MCAP_FILE_METADATA__RECORDING_TIMES;
        file_metadata.metadata[MCAP_FILE_METADATA__RECORDING_START_TIME] =
                std::to_string(convert_time<::mcap::Timestamp>(start_time_));
        file_metadata.metadata[MCAP_FILE_METADATA__RECORDING_END_TIME] =
                std::to_string(end_timestamp);

        if (data_writer_) {
            if (auto status = data_writer_->write(file_metadata); !status.ok()) {
                logger << Logger::ERROR << "Unable to write MCAP data recording times: " << status.message;
            } else {
                logger << Logger::DEBUG << "write() [data]: metadata["
                       << std::quoted(MCAP_FILE_METADATA__RECORDING_TIMES) << "]"
                       << ", " << MCAP_FILE_METADATA__RECORDING_START_TIME << " "
                       << file_metadata.metadata[MCAP_FILE_METADATA__RECORDING_START_TIME]
                       << ", " << MCAP_FILE_METADATA__RECORDING_END_TIME << " "
                       << file_metadata.metadata[MCAP_FILE_METADATA__RECORDING_END_TIME];
            }
        }
        if (info_writer_ && info_writer_ != data_writer_) {
            if (auto status = info_writer_->write(file_metadata); !status.ok()) {
                logger << Logger::ERROR << "Unable to write MCAP SampleInfo recording times: " << status.message;
            } else {
                logger << Logger::DEBUG << "write() [info]: metadata["
                       << std::quoted(MCAP_FILE_METADATA__RECORDING_TIMES) << "]"
                       << ", " << MCAP_FILE_METADATA__RECORDING_START_TIME << " "
                       << file_metadata.metadata[MCAP_FILE_METADATA__RECORDING_START_TIME]
                       << ", " << MCAP_FILE_METADATA__RECORDING_END_TIME << " "
                       << file_metadata.metadata[MCAP_FILE_METADATA__RECORDING_END_TIME];
            }
        }
    } catch (const std::exception& error) {
        logger << Logger::ERROR << "Unable to write MCAP recording times: " << error.what();
    }
}

void McapDiscoveryWriter::store(
        const std::vector<dds::topic::ParticipantBuiltinTopicData *> &,
        const std::vector<dds::sub::SampleInfo *> &info_seq)
{
    auto lock = std::lock_guard(*mutex_);
    for (size_t i = 0; i < info_seq.size(); ++i) {
        const auto& info = *(info_seq[i]);
        if (start_time_ == ::dds::core::Time::zero() || info->reception_timestamp() < start_time_) {
            start_time_ = info->reception_timestamp();
            logger  << Logger::DEBUG << "store() [discovery]"
                << ", start time: " << convert_time<::mcap::Timestamp>(start_time_);
        }
    }
}


}  // rti::mcap::storage