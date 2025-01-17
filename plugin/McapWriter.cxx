#include <cassert>
#include <cstring>
#include <rti/recording/storage/StorageDefs.hpp>
#include "McapWriter.hpp"
#include "mcap/internal.hpp"


namespace rti::recording::storage::mcap {

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


    data_writer_ = std::make_shared<::mcap::McapWriter>();
    if (info_filename) {
        if (info_filename == data_filename) {
            // if data & info are same file, reuse the MCAP writer
            info_writer_ = data_writer_;
        }
        else {
            info_writer_ = std::make_shared<::mcap::McapWriter>();
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
            logger  << Logger::FATAL << "!get_from_properties_opt() [" << RTI_XML_PROPERTY__COMPRESSION_KIND << "]"
                    << ", invalid value: " << compression_kind;
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
            logger  << Logger::FATAL << "!get_from_properties_opt() [" << RTI_XML_PROPERTY__COMPRESSION_LEVEL << "]"
                    << ", invalid value: " << compression_level;
        }
        logger  << Logger::INFO << "options.compressionLevel"
                << ", value: " << static_cast<int>(options.compressionLevel);
        

    }

    if (data_writer_) {
        if (auto status = data_writer_->open(data_filename, options); !status.ok()) {
            logger  << Logger::FATAL << "!open() [data]"
                    << ", unable to open MCAP file " << data_filename
                    << ", " << status.message;
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
                logger  << Logger::FATAL << "!open() [info]"
                        << ", unable to open MCAP file " << *info_filename
                        << ", " << status.message;
            }
            else {
                logger  << Logger::INFO << "open() [info]"
                        << ", file: " << *info_filename;
            }
        }

        // add SampleInfo schema
        const static rti::core::xtypes::DynamicTypePrintFormatProperty IDL_PRINT_FORMAT(
            0, false, rti::core::xtypes::DynamicTypePrintKind::idl, true);
        auto idl_type = ::rti::core::xtypes::to_string(::rti::topic::dynamic_type<SampleInfo>::get(), IDL_PRINT_FORMAT); // data
        ::mcap::Schema info_schema(
            ::rti::topic::dynamic_type<SampleInfo>::get().name(), // name
            "omgidl", // encoding
            std::vector<std::byte>(  // data
                reinterpret_cast<const std::byte*>(idl_type.data()),
                reinterpret_cast<const std::byte*>(idl_type.data() + idl_type.size()))
            // ::rti::core::xtypes::to_string(::rti::topic::dynamic_type<SampleInfo>::get(), IDL_PRINT_FORMAT) // data
        );
        info_writer_->addSchema(info_schema);
        info_schema_id_ = info_schema.id;
        logger  << Logger::INFO << "addSchema() [info]"
                << ", name: " << std::quoted(info_schema.name)
                << ", schema: " << std::to_string(info_schema.id);
        logger  << Logger::DEBUG << "addSchema() [info]"
                << ", schema DDS type: \n" << reinterpret_cast<char*>(info_schema.data.data());
    }
}

McapFileWriter::~McapFileWriter()
{
    auto lock = std::lock_guard(*mutex_);
    if (data_writer_) {
        auto statistics = data_writer_->statistics();
        logger  << Logger::INFO << "statistics() [data]"
                << ", message count: " << statistics.messageCount
                << ", schema count: " << statistics.schemaCount
                << ", channel count: " << statistics.channelCount
                << ", attachment count: " << statistics.attachmentCount
                << ", metadata count: " << statistics.metadataCount
                << ", chunk count: " << statistics.chunkCount
                << ", message start time: " << statistics.messageStartTime
                << ", message end time: " << statistics.messageEndTime;
        for (const auto& [channel_id, message_count] : statistics.channelMessageCounts) {
            logger  << Logger::DEBUG << "statistics() [data]"
                    << ", channel: " << channel_id
                    << ", message count: " << message_count;
        }
        data_writer_->close();
        logger  << Logger::INFO << "close() [data]"
                << ", file ...";
    }
    if (info_writer_ && (info_writer_ != data_writer_)) {
        auto statistics = data_writer_->statistics();
        logger  << Logger::INFO << "statistics() [info]"
                << ", message count: " << statistics.messageCount
                << ", schema count: " << statistics.schemaCount
                << ", channel count: " << statistics.channelCount
                << ", attachment count: " << statistics.attachmentCount
                << ", metadata count: " << statistics.metadataCount
                << ", chunk count: " << statistics.chunkCount
                << ", message start time: " << statistics.messageStartTime
                << ", message end time: " << statistics.messageEndTime;
        for (const auto& [channel_id, message_count] : statistics.channelMessageCounts) {
            logger  << Logger::DEBUG << "statistics() [info]"
                    << ", channel: " << channel_id
                    << ", message count: " << message_count;
        }
        info_writer_->close();
        logger  << Logger::INFO << "close() [info]"
                << ", file ...";
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
    auto lock = std::lock_guard(*mutex_);
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
        std::shared_ptr<::mcap::McapWriter> data_writer,
        std::shared_ptr<::mcap::McapWriter> info_writer,
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

    const static rti::core::xtypes::DynamicTypePrintFormatProperty IDL_PRINT_FORMAT(
        0, false, rti::core::xtypes::DynamicTypePrintKind::idl, true);
    ::mcap::Schema data_schema(
        stream_info.type_info().dynamic_type().name(), // name
        "omgidl", // encoding
        ::rti::core::xtypes::to_string(stream_info.type_info().dynamic_type(), IDL_PRINT_FORMAT) // data
    );
    data_writer_->addSchema(data_schema);
    logger  << Logger::INFO << "addSchema() [data]"
            << ", stream: " << std::quoted(stream_info.stream_name())
            << ", name: " << std::quoted(data_schema.name)
            << ", schema: " << std::to_string(data_schema.id);
    logger  << Logger::DEBUG << "addSchema() [data]"
            << ", schema DDS type: \n" << reinterpret_cast<char*>(data_schema.data.data());

    const static rti::core::xtypes::DynamicTypePrintFormatProperty XML_PRINT_FORMAT(
        0, false, rti::core::xtypes::DynamicTypePrintKind::xml, true);
    ::mcap::Channel data_channel(
        topic_data_channel_name(stream_info.stream_name()), // topic
        "cdr", // message encoding
        data_schema.id // schema id
    );
    data_channel.metadata[MCAP_CHANNEL_METADATA__DDS_XML_TYPE] =
        ::rti::core::xtypes::to_string(stream_info.type_info().dynamic_type(), XML_PRINT_FORMAT);
    data_channel.metadata[MCAP_CHANNEL_METADATA__DDS_DOMAIN_ID] = std::to_string(domain_id);
    auto info_topic_name = topic_info_channel_name(stream_info.stream_name());
    data_channel.metadata[MCAP_CHANNEL_METADATA__INFO_CHANNEL_TOPIC] = info_topic_name;
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
            "cdr", // message encoding
            info_schema_id // schema id
        );
        info_channel.metadata[MCAP_CHANNEL_METADATA__DDS_XML_TYPE] =
            ::rti::core::xtypes::to_string(::rti::topic::dynamic_type<SampleInfo>::get(), XML_PRINT_FORMAT);
        info_channel.metadata[MCAP_CHANNEL_METADATA__DDS_DOMAIN_ID] = std::to_string(domain_id);
        info_channel.metadata[MCAP_CHANNEL_METADATA__INFO_CHANNEL_FLAG] = "1";
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

    for (size_t i = 0; i < data_seq.size(); ++i) {
        const auto& data = *(data_seq[i]);
        const auto& info = *(info_seq[i]);

        // Build MCAP info message, only if writing info
        if (info_writer_) {
            ::mcap::Message mcap_info = convert_sample_info(data, info);
            if (auto status = info_writer_->write(mcap_info); !status.ok()) {
                logger  << Logger::ERROR << "!write() [info]"
                        << "error writing MCAP SampleInfo from topic " << std::quoted(stream_info_.stream_name())
                        << ", " << status.message;
            }
            else {
                logger  << Logger::DEBUG << "write() [info]"
                        << ", stream: " << stream_info_.stream_name()
                        << ", channel: " << mcap_info.channelId
                        << ", sequence: " << mcap_info.sequence;
            }
        }

        // Build MCAP data message, only if valid
        if (info.valid()) {
            assert(data_writer_);

            auto mcap_data = convert_sample_data(data, info);
            if (auto status = data_writer_->write(mcap_data); !status.ok()) {
                logger  << Logger::ERROR << "!write() [data]"
                        << "Error writing MCAP data from topic " << std::quoted(stream_info_.stream_name())
                        << ", " << status.message;
            }
            else {
                logger  << Logger::DEBUG << "write() [data]"
                        << ", stream: " << std::quoted(stream_info_.stream_name())
                        << ", channel: " << mcap_data.channelId
                        << ", sequence: " << mcap_data.sequence;
            }
        }
    }
}

::mcap::Message McapStreamWriter::convert_sample_data(
        const ::dds::core::xtypes::DynamicData &data,
        const ::dds::sub::SampleInfo &info)
{
    
    // Serialize the CDR buffer, only if valid
    if (info.valid()) {
        ::rti::topic::to_cdr_buffer(cdr_data_buffer_, data);
    }
    else {
        cdr_data_buffer_.clear();
    }

    ::mcap::Message mcap_data = ::mcap::Message();
    mcap_data.channelId = data_channel_id_;
    mcap_data.publishTime = convert_time<::mcap::Timestamp>(info.source_timestamp());
    mcap_data.logTime = convert_time<::mcap::Timestamp>(info->reception_timestamp());
    mcap_data.sequence = convert_sequence_number(info->reception_sequence_number());
    mcap_data.data = reinterpret_cast<const std::byte*>(cdr_data_buffer_.data());
    mcap_data.dataSize = cdr_data_buffer_.size();

    return mcap_data;
}

::mcap::Message McapStreamWriter::convert_sample_info(
        const ::dds::core::xtypes::DynamicData &,
        const ::dds::sub::SampleInfo &info)
{
    // extract native dds info for modification
    const auto & native_dds_info = info->native();

    SampleInfo cdrinfo_data;
    cdrinfo_data.sample_state(native_dds_info.sample_state);
    cdrinfo_data.view_state(native_dds_info.view_state);
    cdrinfo_data.instance_state(native_dds_info.instance_state);

    cdrinfo_data.source_timestamp().sec(native_dds_info.source_timestamp.sec);
    cdrinfo_data.source_timestamp().nanosec(native_dds_info.source_timestamp.nanosec);

    std::copy(std::begin(native_dds_info.instance_handle.keyHash.value),
                std::end(native_dds_info.instance_handle.keyHash.value),
                cdrinfo_data.instance_handle().value().begin());
    cdrinfo_data.instance_handle().length(native_dds_info.instance_handle.keyHash.length);
    cdrinfo_data.instance_handle().isValid(native_dds_info.instance_handle.isValid);

    std::copy(std::begin(native_dds_info.publication_handle.keyHash.value),
                std::end(native_dds_info.publication_handle.keyHash.value),
                cdrinfo_data.publication_handle().value().begin());
    cdrinfo_data.publication_handle().length(native_dds_info.publication_handle.keyHash.length);
    cdrinfo_data.publication_handle().isValid(native_dds_info.publication_handle.isValid);
    
    cdrinfo_data.disposed_generation_count(native_dds_info.disposed_generation_count);
    cdrinfo_data.no_writers_generation_count(native_dds_info.no_writers_generation_count);
    cdrinfo_data.sample_rank(native_dds_info.sample_rank);
    cdrinfo_data.generation_rank(native_dds_info.generation_rank);
    cdrinfo_data.absolute_generation_rank(native_dds_info.absolute_generation_rank);
    cdrinfo_data.valid_data(native_dds_info.valid_data);

    
    cdrinfo_data.reception_timestamp().sec(native_dds_info.reception_timestamp.sec);
    cdrinfo_data.reception_timestamp().nanosec(native_dds_info.reception_timestamp.nanosec);

    cdrinfo_data.publication_sequence_number().high(native_dds_info.publication_sequence_number.high);
    cdrinfo_data.publication_sequence_number().low(native_dds_info.publication_sequence_number.low);
    cdrinfo_data.reception_sequence_number().high(native_dds_info.reception_sequence_number.high);
    cdrinfo_data.reception_sequence_number().low(native_dds_info.reception_sequence_number.low);

    std::copy(std::begin(native_dds_info.publication_virtual_guid.value),
                std::end(native_dds_info.publication_virtual_guid.value),
                cdrinfo_data.publication_virtual_guid().value().begin());
    cdrinfo_data.publication_virtual_sequence_number().high(native_dds_info.publication_virtual_sequence_number.high);
    cdrinfo_data.publication_virtual_sequence_number().low(native_dds_info.publication_virtual_sequence_number.low);

    std::copy(std::begin(native_dds_info.original_publication_virtual_guid.value),
                std::end(native_dds_info.original_publication_virtual_guid.value),
                cdrinfo_data.original_publication_virtual_guid().value().begin());
    cdrinfo_data.original_publication_virtual_sequence_number().high(native_dds_info.original_publication_virtual_sequence_number.high);
    cdrinfo_data.original_publication_virtual_sequence_number().low(native_dds_info.original_publication_virtual_sequence_number.low);

    // serialize cdr buffer
    ::rti::topic::to_cdr_buffer<SampleInfo>(cdr_info_buffer_, cdrinfo_data);
    
    // build mcap info message
    ::mcap::Message mcap_info = ::mcap::Message();
    mcap_info.channelId = info_channel_id_;
    mcap_info.publishTime = convert_time<::mcap::Timestamp>(info.source_timestamp());
    mcap_info.logTime = convert_time<::mcap::Timestamp>(info->reception_timestamp());
    mcap_info.sequence = convert_sequence_number(info->reception_sequence_number());
    mcap_info.data = reinterpret_cast<const std::byte*>(cdr_info_buffer_.data());
    mcap_info.dataSize = cdr_info_buffer_.size();

    return mcap_info;
}

McapDiscoveryWriter::McapDiscoveryWriter(
        std::shared_ptr<::mcap::McapWriter> data_writer,
        std::shared_ptr<::mcap::McapWriter> info_writer,
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
    logger  << Logger::DEBUG << "~McapDiscoveryWriter() [discovery]";

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

    // auto lock = std::lock_guard(*mutex_);
    if (data_writer_) {
        if (auto status = data_writer_->write(file_metadata); !status.ok()) {
            logger  << Logger::ERROR << "!write() [data]"
                    << "error writing MCAP file metadata " << std::quoted(MCAP_FILE_METADATA__RECORDING_TIMES)
                    << ", " << status.message;
        }
        else {
            logger  << Logger::DEBUG << "write() [data]: metadata[" << std::quoted(MCAP_FILE_METADATA__RECORDING_TIMES) << "]"
                    << ", " << MCAP_FILE_METADATA__RECORDING_START_TIME << " " << file_metadata.metadata[MCAP_FILE_METADATA__RECORDING_START_TIME]
                    << ", " << MCAP_FILE_METADATA__RECORDING_END_TIME << " " << file_metadata.metadata[MCAP_FILE_METADATA__RECORDING_END_TIME];
        }
    }
    if (info_writer_ && info_writer_ != data_writer_) {
        if (auto status = info_writer_->write(file_metadata); !status.ok()) {
            logger  << Logger::ERROR << "!write() [info]"
                    << "error writing MCAP file metadata " << std::quoted(MCAP_FILE_METADATA__RECORDING_START_TIME)
                    << ", " << status.message;
        }
        else {
            logger  << Logger::DEBUG << "write() [info]: metadata[" << std::quoted(MCAP_FILE_METADATA__RECORDING_TIMES) << "]"
                    << ", " << MCAP_FILE_METADATA__RECORDING_START_TIME << " " << file_metadata.metadata[MCAP_FILE_METADATA__RECORDING_START_TIME]
                    << ", " << MCAP_FILE_METADATA__RECORDING_END_TIME << " " << file_metadata.metadata[MCAP_FILE_METADATA__RECORDING_END_TIME];
        }
    }
}

void McapDiscoveryWriter::store(
        const std::vector<dds::topic::ParticipantBuiltinTopicData *> &,
        const std::vector<dds::sub::SampleInfo *> &info_seq)
{
    for (size_t i = 0; i < info_seq.size(); ++i) {
        const auto& info = *(info_seq[i]);
        if (start_time_ == ::dds::core::Time::zero() || info->reception_timestamp() < start_time_) {
            start_time_ = info->reception_timestamp();
            logger  << Logger::DEBUG << "store() [discovery]"
                << ", start time: " << convert_time<::mcap::Timestamp>(start_time_);
        }
    }
}


}  // rti::recording::storage::mcap