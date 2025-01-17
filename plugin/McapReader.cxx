#include <cassert>
#include <cstring>
#include <rti/recording/storage/StorageDefs.hpp>
#include "McapReader.hpp"

namespace rti::recording::storage::mcap {

/*
 * Convenience macro to define the C-style function that will be called by RTI
 * Recording Service to create your class.
 */
RTI_RECORDING_STORAGE_READER_CREATE_DEF(McapFileReader)

McapFileReader::McapFileReader(const ::rti::recording::PropertySet &properties)
        : StorageReader(properties),
          logger(Logger::LogLevel(
            get_from_properties_opt<int>(properties, 
                RTI_XML_PROPERTY__LOGGING_LOG_LEVEL)
                .value_or(Logger::LogLevel::WARN)))
{
    data_filename_ = get_from_properties_req<std::string>(
        properties,
        RTI_XML_PROPERTY__DATA_FILENAME);
    logger  << Logger::INFO << "get_from_properties_req() [" << RTI_XML_PROPERTY__DATA_FILENAME << "]"
            << ", value: " << data_filename_;
    
    data_filename_ = get_from_properties_req<std::string>(
        properties,
        RTI_XML_PROPERTY__DATA_FILENAME);
    logger  << Logger::INFO << "get_from_properties_req() [" << RTI_XML_PROPERTY__DATA_FILENAME << "]"
            << ", value: " << data_filename_;

    info_filename_ = get_from_properties_opt<std::string>(
        properties,
        RTI_XML_PROPERTY__INFO_FILENAME).value_or("");
    logger  << Logger::INFO << "get_from_properties_opt() [" << RTI_XML_PROPERTY__INFO_FILENAME << "]"
            << ", value: " << info_filename_;
}

McapFileReader::~McapFileReader()
{
    logger  << Logger::INFO << "Finished.";
}

rti::recording::storage::StorageStreamInfoReader *McapFileReader::
        create_stream_info_reader(const rti::routing::PropertySet &)
{
    return new McapStreamInfoReader(
            data_filename_,
            info_filename_,
            logger);
}

void McapFileReader::delete_stream_info_reader(
        rti::recording::storage::StorageStreamInfoReader *stream_info_reader)
{
    delete stream_info_reader;
}

rti::recording::storage::StorageStreamReader *McapFileReader::
        create_stream_reader(
                const rti::routing::StreamInfo &stream_info,
                const rti::routing::PropertySet &properties)
{
    /*
     * Make sure that there is a valid type representation the stream reader can
     * work with in the StreamInfo object
     */
    if (stream_info.type_info().type_representation_kind()
        != rti::routing::TypeRepresentationKind::DYNAMIC_TYPE) {
        logger  << Logger::ERROR << "!type_representation_kind() [stream_info]"
                << ", invalid type representation kind in StreamInfo object"
                << ", stream: " << stream_info.stream_name()
                << ", type: " << stream_info.type_info().type_name();
    }
    if (stream_info.type_info().type_representation() == nullptr) {
        logger  << Logger::ERROR << "!type_representation_kind() [stream_info]"
                << ", null pointer invalid for type representation"
                << ", stream: " << stream_info.stream_name()
                << ", type: " << stream_info.type_info().type_name();
    }
    /* Get time range and domain ID information from the properties */
    auto domain_id = get_from_properties_opt<int32_t>(
        properties,
        rti::recording::domain_id_property_name())
        .value_or(-1);
    logger  << Logger::DEBUG << "get_from_properties_opt() [" << rti::recording::domain_id_property_name() << "]"
            << ", value: " << domain_id;

    auto start_timestamp = get_from_properties_opt<int64_t>(
        properties,
        rti::recording::start_timestamp_property_name())
        .value_or(0);
    logger  << Logger::DEBUG << "get_from_properties_opt() [" << rti::recording::start_timestamp_property_name() << "]"
            << ", value: " << start_timestamp;

    auto end_timestamp = get_from_properties_opt<int64_t>(
        properties,
        rti::recording::end_timestamp_property_name())
        .value_or(std::numeric_limits<int64_t>::max());
    logger  << Logger::DEBUG << "get_from_properties_opt() [" << rti::recording::end_timestamp_property_name() << "]"
            << ", value: " << end_timestamp;

    return new McapStreamReader(
            data_filename_,
            info_filename_,
            stream_info,
            domain_id,
            static_cast<::mcap::Timestamp>(start_timestamp),
            static_cast<::mcap::Timestamp>(end_timestamp) + 1, // exclusive
            logger);
}

void McapFileReader::delete_stream_reader(
        rti::recording::storage::StorageStreamReader *stream_reader)
{
    delete stream_reader;
    logger  << Logger::DEBUG << "delete_stream_reader()";
}


/*
 * Create a data stream reader. For each discovered stream that matches the set
 * of interest defined in the configuration, Replay or Converter will ask us to
 * create a reader for that stream (a stream reader).
 * The start and stop timestamp parameters define the time range for which the
 * application is asking for data.
 *
 * @pre stream_info.type_info().type_representation_kind() == DYNAMIC_TYPE
 * @pre stream_info.type_info().type_representation() != null
 */
McapStreamReader::McapStreamReader(
        const std::string& data_filename,
        const std::string& info_filename,
        const ::rti::recording::StreamInfo &stream_info,
        int32_t domain_id,
        ::mcap::Timestamp start_timestamp,
        ::mcap::Timestamp end_timestamp,
        Logger& logger)
        : stream_info_(stream_info),
          type_(stream_info.type_info().dynamic_type()),
          domain_id_(domain_id),
          start_timestamp_(start_timestamp),
          end_timestamp_(end_timestamp),
          finished_(false),
          logger(logger)
{
    logger << Logger::INFO << std::string("McapStreamReader()") + \
                ", stream: " + stream_info_.stream_name() + \
                ", type: " + stream_info_.type_info().type_name() + \
                ", domain: " + std::to_string(domain_id) + \
                ", start: " + std::to_string(start_timestamp) + \
                ", end: " + std::to_string(end_timestamp);

    // read summaries
    ::mcap::ProblemCallback on_problem = [this](const ::mcap::Status& status) {
        this->logger  << Logger::ERROR << "!readSummary()"
                << ", stream: " << this->stream_info_.stream_name()
                << ", error reading MCAP summary: " << status.message;
    };

    // create mcap reader for user data
    auto topic_name = stream_info.stream_name();
    data_channel_topic_ = topic_data_channel_name(topic_name);
    data_reader_ = std::make_unique<::mcap::McapReader>();
    data_read_options_.startTime = start_timestamp;
    data_read_options_.endTime = end_timestamp;
    data_read_options_.readOrder = ::mcap::ReadMessageOptions::ReadOrder::LogTimeOrder;
    data_read_options_.topicFilter = [this](std::string_view topic) {
        return topic == data_channel_topic_;
    };
    on_data_problem_ = [this](const ::mcap::Status& status) {
        this->logger  << Logger::ERROR << "!read() [data]"
                << ", stream: " << this->stream_info_.stream_name()
                << ", error reading data MCAP message"
                << ", " << status.message;
    };

    // open the data MCAP file
    if (auto status = data_reader_->open(data_filename); !status.ok()) {
        logger  << Logger::FATAL << "!open() [data]"
                << ", stream: " << this->stream_info_.stream_name()
                << ", unable to open MCAP file " << data_filename
                << ", " << status.message;
    }
    else {
        logger  << Logger::INFO << "open() [data]"
                << ", stream: " << this->stream_info_.stream_name()
                << ", file: " << data_filename;
    }

    // read data MCAP file summary
    if (data_reader_) {
        if (auto status = data_reader_->readSummary(::mcap::ReadSummaryMethod::AllowFallbackScan, on_problem); !status.ok()) {
            on_problem(status);
        }
    }

    // Make sure there is a channel in the correct domain
    if (domain_id_ >= 0) {
        bool data_channel_found = false;
        for (const auto& [channel_id, channel_ptr] : data_reader_->channels()) {
            
            // match the data channel with our topic filter
            if (data_read_options_.topicFilter(channel_ptr->topic)) {

                // Skip data channels on the wrong domain
                auto channel_domain_id = channel_ptr->metadata.find(MCAP_CHANNEL_METADATA__DDS_DOMAIN_ID);
                if (channel_domain_id != channel_ptr->metadata.end()) {
                    if (std::stol(channel_domain_id->second) == domain_id_) {
                        if (!data_channel_found) {
                            logger  << Logger::INFO << "metadata[" << MCAP_CHANNEL_METADATA__DDS_DOMAIN_ID << "] [data]"
                                    << ", found matching stream on matching domain"
                                    << ", stream: " << this->stream_info_.stream_name()
                                    << ", domain: " << channel_domain_id->second;
                            data_channel_found = true;
                        }
                        else {
                            logger  << Logger::WARN << "metadata[" << MCAP_CHANNEL_METADATA__DDS_DOMAIN_ID << "] [data]"
                                    << ", found multiple matching streams on matching domain"
                                    << ", stream: " << this->stream_info_.stream_name()
                                    << ", domain: " << channel_domain_id->second;
                        }
                    }
                    else {
                        logger  << Logger::INFO << "metadata[" << MCAP_CHANNEL_METADATA__DDS_DOMAIN_ID << "] [data]"
                                << ", found matching stream on mismatched domain"
                                << ", stream: " << this->stream_info_.stream_name()
                                << ", expected domain: " << domain_id_
                                << ", actual domain: " << channel_domain_id->second
                                << ", skipping.";
                                
                    }
                }
            }
        }
        if (!data_channel_found) {
            logger  << Logger::ERROR << "!metadata[" << MCAP_CHANNEL_METADATA__DDS_DOMAIN_ID << "] [data]"
                    << ", did not find matching stream on expected domain"
                    << ", expected stream: " << this->stream_info_.stream_name()
                    << ", expected domain: " << domain_id_
                    << ", skipping.";
        }
    }

    // if info filename is passed, try to find & read info channel
    if (!info_filename.empty()) {

        // check if the data channel indicates it has a parallel info channel
        for (const auto& [channel_id, channel_ptr] : data_reader_->channels()) {
            
            // match the data channel with our topic filter
            if (data_read_options_.topicFilter(channel_ptr->topic)) {

                // Skip data channels on the wrong domain
                if (domain_id_ >= 0) {
                    auto channel_domain_id = channel_ptr->metadata.find(MCAP_CHANNEL_METADATA__DDS_DOMAIN_ID);
                    if (channel_domain_id != channel_ptr->metadata.end()) {
                        if (std::stol(channel_domain_id->second) != domain_id_) {
                            logger  << Logger::INFO << "metadata[" << MCAP_CHANNEL_METADATA__DDS_DOMAIN_ID << "] [data]"
                                    << ", found matching stream on mismatched domain"
                                    << ", stream: " << this->stream_info_.stream_name()
                                    << ", expected domain: " << domain_id_
                                    << ", actual domain: " << channel_domain_id->second
                                    << ", skipping.";
                            continue;
                        }
                    }
                }

                // check if the data channel has info channel metadata
                auto info_topic_it = channel_ptr->metadata.find(MCAP_CHANNEL_METADATA__INFO_CHANNEL_TOPIC);
                if (info_topic_it != channel_ptr->metadata.end()) {

                    // use info topic name from data channel's metadata
                    info_channel_topic_ = info_topic_it->second;
                    info_reader_ = std::make_unique<::mcap::McapReader>();
                    info_read_options_.readOrder = ::mcap::ReadMessageOptions::ReadOrder::LogTimeOrder;
                    info_read_options_.topicFilter = [this](std::string_view topic) {
                        return topic == info_channel_topic_;
                    };
                    on_info_problem_ = [this](const ::mcap::Status& status) {
                        this->logger  << Logger::ERROR << "!read() [info]"
                                << ", stream: " << this->stream_info_.stream_name()
                                << ", error reading data MCAP message"
                                << ", " << status.message;
                    };

                    // open the info MCAP file
                    if (auto status = info_reader_->open(info_filename); !status.ok()) {
                        logger  << Logger::FATAL << "!open() [info]"
                                << ", stream: " << this->stream_info_.stream_name()
                                << ", unable to open MCAP file " << info_filename
                                << ", " << status.message;
                    }
                    else {
                        logger  << Logger::INFO << "open() [info]"
                                << ", stream: " << this->stream_info_.stream_name()
                                << ", file: " << info_filename;
                    }

                    // read info MCAP file summary
                    if (info_reader_) {
                        if (auto status = info_reader_->readSummary(::mcap::ReadSummaryMethod::AllowFallbackScan, on_problem); !status.ok()) {
                            on_problem(status);
                        }

                        bool info_channel_found = false;
                        for (const auto& [info_channel_id, info_channel_ptr] : info_reader_->channels()) {

                            // match the info channel with our topic filter
                            if (info_channel_ptr->topic == info_channel_topic_) {
                                logger  << Logger::INFO << "channels() [info]"
                                        << ", stream: " << this->stream_info_.stream_name()
                                        << ", channel: " << info_channel_topic_
                                        << ", channel id: " << info_channel_ptr->id
                                        << ", file: " << info_filename;
                            }
                            info_channel_found = true;
                            break;
                        }
                        if (!info_channel_found) {
                            logger  << Logger::ERROR << "!channels() [info]"
                                    << "did not find info channel"
                                    << ", stream: " << this->stream_info_.stream_name()
                                    << ", channel: " << info_channel_topic_
                                    << ", file: " << info_filename;
                        }
                    }

                    break;
                }
            }
        }

        if (!info_reader_) {
            logger  << Logger::WARN << "metadata[" << MCAP_CHANNEL_METADATA__INFO_CHANNEL_TOPIC << "]"
                    << ", info channel name not found in data channel" //, unable to read SampleInfo channel from MCAP file: " << info_filename;
                    << ", stream: " << this->stream_info_.stream_name()
                    << ", topic: " << topic_name
                    << ", channel: " << topic_data_channel_name(topic_name)
                    << ", file: " << data_filename;
        }
    }

    reset();
}

McapStreamReader::~McapStreamReader()
{
    if (data_reader_) {
        auto statistics = data_reader_->statistics();
        if (statistics.has_value()) {
            logger  << Logger::INFO << "statistics() [data]"
                    << ", stream: " << this->stream_info_.stream_name()
                    << ", message count: " << statistics->messageCount
                    << ", schema count: " << statistics->schemaCount
                    << ", channel count: " << statistics->channelCount
                    << ", attachment count: " << statistics->attachmentCount
                    << ", metadata count: " << statistics->metadataCount
                    << ", chunk count: " << statistics->chunkCount
                    << ", message start time: " << statistics->messageStartTime
                    << ", message end time: " << statistics->messageEndTime;
            for (const auto& [channel_id, message_count] : statistics->channelMessageCounts) {
                logger  << Logger::DEBUG << "statistics() [data]"
                        << ", channel: " << channel_id
                        << ", message count: " << message_count;
            }
        }
        data_reader_->close();
        logger  << Logger::INFO << "close() [data]"
                << ", stream: " << this->stream_info_.stream_name()
                << ", file ...";
    }
    if (info_reader_) {
        auto statistics = info_reader_->statistics();
        if (statistics.has_value()) {
            logger  << Logger::INFO << "statistics() [info]"
                    << ", stream: " << this->stream_info_.stream_name()
                    << ", message count: " << statistics->messageCount
                    << ", schema count: " << statistics->schemaCount
                    << ", channel count: " << statistics->channelCount
                    << ", attachment count: " << statistics->attachmentCount
                    << ", metadata count: " << statistics->metadataCount
                    << ", chunk count: " << statistics->chunkCount
                    << ", message start time: " << statistics->messageStartTime
                    << ", message end time: " << statistics->messageEndTime;
            for (const auto& [channel_id, message_count] : statistics->channelMessageCounts) {
                logger  << Logger::DEBUG << "statistics() [info]"
                        << ", stream: " << this->stream_info_.stream_name()
                        << ", channel: " << channel_id
                        << ", message count: " << message_count;
            }
        }
        info_reader_->close();
        logger  << Logger::INFO << "close() [info]"
                << ", stream: " << this->stream_info_.stream_name()
                << ", file ...";
    }
}

void McapStreamReader::read(
        std::vector<dds::core::xtypes::DynamicData *> &data_seq,
        std::vector<dds::sub::SampleInfo *> &info_seq,
        const rti::recording::storage::SelectorState &selector)
{
    assert(data_reader_);
    if (!info_reader_) {
        read_w_data(data_seq, info_seq, selector);
    }
    else {
        read_w_info(data_seq, info_seq, selector);
    }
}

void McapStreamReader::read_w_data(
        std::vector<dds::core::xtypes::DynamicData *> &data_seq,
        std::vector<dds::sub::SampleInfo *> &info_seq,
        const rti::recording::storage::SelectorState &selector)
{
    if (finished()) {
        logger  << Logger::DEBUG << "read() [data]"
                << ", already finished reading MCAP stream"
                << ", stream: " << this->stream_info_.stream_name()
                << ", done.";
        return;
    }

    if (selector.instance_history_depth() > 0) {
        logger  << Logger::WARN << "read() [data]"
                << ", instance history depth not supported"
                << ", stream: " << this->stream_info_.stream_name()
                << ", skipping.";
        return;
    }

    // Iteration constraints
    auto timestamp_limit = convert_time<::mcap::Timestamp>(selector.time_range_end());
    int32_t samples_read = 0;

    logger  << Logger::DEBUG << "time_range_end() [data]"
            << ", stream: " << this->stream_info_.stream_name()
            << ", value: " << ((timestamp_limit == ::mcap::MaxTime) ? "max" : std::to_string(timestamp_limit));

    /* Read MCAP messages
        - Use while (true) to log explicit reasons for stopping & skipping
        - Use break to stop reading
        - Use goto to skip messages
            - goto next_data: to skip data messages
    */
    while (true) {

        // Check if we have reached the end of messages
        if (finished()) {
            logger  << Logger::DEBUG << "finished() [data]"
                << ", end of messages reached"
                << ", stream: " << this->stream_info_.stream_name()
                << ", done.";
            break;
        }

        // Check if we have read max samples
        if (selector.max_samples() >= 0 && samples_read >= selector.max_samples()) {
            logger  << Logger::DEBUG << "max_samples() [data]"
                    << ", max samples reached: " << selector.max_samples()
                    << ", stream: " << this->stream_info_.stream_name()
                    << ", stopping.";
            break;
        }

        // Read data messages
            
        assert(data_iterator_);
        assert(data_messages_);
        if (*data_iterator_ == data_messages_->end()) {
            logger  << Logger::DEBUG << "read() [data]"
                    << ", no more messages"
                    << ", stream: " << this->stream_info_.stream_name()
                    << ", done.";
            finished_ = true;
            break;
        }
        auto message_view = **data_iterator_;

        const ::mcap::Message & mcap_message = message_view.message;
        // const ::mcap::Channel & mcap_channel = *(message_view.channel.get());

    
        /*
        This check should be handled in the StreamReader creation, instead of per-message.

        // Skip messages on the wrong domain
        if (domain_id_ >= 0) {
            auto channel_domain_id = mcap_channel.metadata.find(MCAP_CHANNEL_METADATA__DDS_DOMAIN_ID);
            if (channel_domain_id != mcap_channel.metadata.end()) {
                if (std::stol(channel_domain_id->second) != domain_id_) {
                    logger  << Logger::WARN << "metadata[" << MCAP_CHANNEL_METADATA__DDS_DOMAIN_ID << "] [data]"
                            << ", message on unexpected domain"
                            << ", stream: " << this->stream_info_.stream_name()
                            << ", expected: " << domain_id_
                            << ", actual: " << channel_domain_id->second
                            << ", skipping.";
                    // continue;
                    goto next_data;
                }
            }
        }
        */

        // Check timestamp is within limit
        if (mcap_message.logTime > timestamp_limit) {
            logger  << Logger::DEBUG << "time_range_end() [data]"
                << ", message timestamp exceeds range limit"
                << ", stream: " << this->stream_info_.stream_name()
                << ", limit: " << timestamp_limit
                << ", message: " << mcap_message.logTime
                << ", stopping.";
            break;
        }

        // Skip messages with mismatched sample states
        if (selector.sample_state() == dds::sub::status::SampleState::not_read() &&
                read_messages_map_.find(mcap_message.sequence) != read_messages_map_.end()) {
            logger  << Logger::DEBUG << "sample_state() [data]"
                    << ", stream: " << this->stream_info_.stream_name()
                    << ", message already read"
                    << ", sequence: " << mcap_message.sequence
                    << ", skipping.";
            // continue;
            goto next_data;
        }

        // deserialize data message into info and data
        loaned_info_.push_back(std::make_shared<::dds::sub::SampleInfo>(convert_sample_info(mcap_message)));
        loaned_data_.push_back(std::make_shared<::dds::core::xtypes::DynamicData>(convert_sample_data(mcap_message)));

        data_seq.push_back(loaned_data_.back().get());
        info_seq.push_back(loaned_info_.back().get());

        read_messages_map_.insert(mcap_message.sequence);
        samples_read++;
        logger  << Logger::DEBUG << "read() [data]"
                << ", read message"
                << ", stream: " << this->stream_info_.stream_name()
                << ", sequence: " << mcap_message.sequence;

    next_data:
        ++(*data_iterator_);

        
    };

    logger  << Logger::DEBUG << "read() [data]"
            << ", stream: " << this->stream_info_.stream_name()
            << ", total messages: " << samples_read;
}

void McapStreamReader::read_w_info(
        std::vector<dds::core::xtypes::DynamicData *> &data_seq,
        std::vector<dds::sub::SampleInfo *> &info_seq,
        const rti::recording::storage::SelectorState &selector)
{
    if (finished()) {
        logger  << Logger::DEBUG << "read() [info]"
                << ", stream: " << this->stream_info_.stream_name()
                << ", already finished reading MCAP stream"
                << ", done.";
        return;
    }

    if (selector.instance_history_depth() > 0) {
        logger  << Logger::WARN << "read() [info]"
                << ", stream: " << this->stream_info_.stream_name()
                << ", instance history depth not supported"
                << ", skipping.";
        return;
    }

    // Iteration constraints
    auto timestamp_limit = convert_time<::mcap::Timestamp>(selector.time_range_end());
    int32_t samples_read = 0;

    logger  << Logger::DEBUG << "time_range_end() [info]"
            << ", stream: " << this->stream_info_.stream_name()
            << ", value: " << ((timestamp_limit == ::mcap::MaxTime) ? "max" : std::to_string(timestamp_limit));

    /* Read MCAP messages
        - Use while (true) to log explicit reasons for stopping & skipping
        - Use break to stop reading
        - Use goto to skip messages
            - goto next_data: to skip data messages
            - goto next_info: to skip info messages    
    */
    while (true) {

        // Check if we have reached the end of messages
        if (finished()) {
            logger  << Logger::DEBUG << "finished() [info]"
                << ", stream: " << this->stream_info_.stream_name()
                << ", end of messages reached"
                << ", done.";
            break;
        }

        // Check if we have read max samples
        if (selector.max_samples() >= 0 && samples_read >= selector.max_samples()) {
            logger  << Logger::DEBUG << "max_samples() [info]"
                    << ", stream: " << this->stream_info_.stream_name()
                    << ", max samples reached: " << selector.max_samples()
                    << ", stopping.";
            break;
        }

        // Read info messages

        assert(info_iterator_);
        assert(info_messages_);
        if (*info_iterator_ == info_messages_->end()) {
            logger  << Logger::DEBUG << "read() [info]"
                    << ", stream: " << this->stream_info_.stream_name()
                    << ", no more messages"
                    << ", done.";
            finished_ = true;
            break;
        }
        const auto & info_message_view = **info_iterator_;
        const ::mcap::Message & info_message = info_message_view.message;
        // const ::mcap::Channel & info_channel = *(info_message_view.channel.get());

        /*
        This check should be handled in the StreamReader creation, instead of per-message.

        // Skip messages on the wrong domain
        if (domain_id_ >= 0) {
            auto channel_domain_id = info_channel.metadata.find(MCAP_CHANNEL_METADATA__DDS_DOMAIN_ID);
            if (channel_domain_id != info_channel.metadata.end()) {
                if (std::stol(channel_domain_id->second) != domain_id_) {
                    logger  << Logger::WARN << "metadata[" << MCAP_CHANNEL_METADATA__DDS_DOMAIN_ID << "] [info]"
                            << ", stream: " << this->stream_info_.stream_name()
                            << ", message on unexpected domain"
                            << ", expected: " << domain_id_
                            << ", actual: " << channel_domain_id->second
                            << ", skipping.";
                    // continue;
                    goto next_info;
                }
            }
        }
        */

        // Check timestamp is within limit
        if (info_message.logTime > timestamp_limit) {
            logger  << Logger::DEBUG << "time_range_end() [info]"
                << ", stream: " << this->stream_info_.stream_name()
                << ", message timestamp exceeds range limit"
                << ", limit: " << timestamp_limit
                << ", message: " << info_message.logTime
                << ", stopping.";
            break;
        }

        // Skip messages with mismatched sample states
        if (selector.sample_state() == dds::sub::status::SampleState::not_read() &&
                read_messages_map_.find(info_message.sequence) != read_messages_map_.end()) {
            logger  << Logger::DEBUG << "sample_state() [info]"
                    << ", stream: " << this->stream_info_.stream_name()
                    << ", message already read"
                    << ", sequence: " << info_message.sequence
                    << ", skipping.";
            // continue;
            goto next_info;
        }

        // deserialize info message
        static const ::mcap::Message empty_data_message = ::mcap::Message();            
        loaned_info_.push_back(std::make_shared<::dds::sub::SampleInfo>(convert_sample_info(empty_data_message, info_message_view.message)));

        if (loaned_info_.back()->valid()) {

            // read next data message
            assert(data_iterator_);
            assert(data_messages_);
            if (*data_iterator_ == data_messages_->end()) {
                logger  << Logger::DEBUG << "read() [data]"
                        << ", stream: " << this->stream_info_.stream_name()
                        << ", no more messages"
                        << ", done.";
                finished_ = true;
                break;
            }
            const auto & data_message_view = **data_iterator_;
            const ::mcap::Message & data_message = data_message_view.message;
            // const ::mcap::Channel & data_channel = *(data_message_view.channel.get());

            // Skip messages with mismatched sequence numbers
            if (data_message.sequence != info_message.sequence) {
                logger  << Logger::DEBUG << "message.sequence [data, info]"
                        << ", stream: " << this->stream_info_.stream_name()
                        << ", mismatched sequence numbers between data and info messages"
                        << ", skipping.";
                // continue;
                goto next_data;
            }

            // deserialize data message
            loaned_data_.push_back(
                std::make_shared<::dds::core::xtypes::DynamicData>(
                    convert_sample_data(data_message)));

        next_data:
            ++(*data_iterator_);
        }
        else {
            logger  << Logger::DEBUG << "valid() [info]"
                    << ", stream: " << this->stream_info_.stream_name()
                    << ", invalid SampleInfo message"
                    << ", sequence: " << info_message.sequence;
            // use empty data message
            loaned_data_.push_back(
                std::make_shared<::dds::core::xtypes::DynamicData>(type_));
        }

        data_seq.push_back(loaned_data_.back().get());
        info_seq.push_back(loaned_info_.back().get());

        read_messages_map_.insert(info_message.sequence);
        samples_read++;
        logger  << Logger::DEBUG << "read() [data, info]"
                << ", stream: " << this->stream_info_.stream_name()
                << ", read message"
                << ", sequence: " << info_message.sequence;

    next_info:
        ++(*info_iterator_);
        
    }

    logger  << Logger::DEBUG << "read() [data, info]"
            << ", stream: " << this->stream_info_.stream_name()
            << ", total messages: " << samples_read;
}

::dds::sub::SampleInfo McapStreamReader::convert_sample_info(
            const ::mcap::Message &data_message)
{
    // populate native dds info
    DDS_SampleInfo native_dds_info = DDS_SAMPLEINFO_DEFAULT;
    ::rti::core::native_conversions::to_native(
        native_dds_info.source_timestamp, 
        convert_time(data_message.publishTime));
    native_dds_info.reception_sequence_number = 
        convert_sequence_number(data_message.sequence).native();
    native_dds_info.valid_data = DDS_BOOLEAN_TRUE;
    ::rti::core::native_conversions::to_native(
        native_dds_info.reception_timestamp, 
        convert_time(data_message.logTime));

    // construct dds info from native dds info
    ::dds::sub::SampleInfo info;
    info->native(native_dds_info);
    return info;
}

::dds::sub::SampleInfo McapStreamReader::convert_sample_info(
        const ::mcap::Message &/*data_message*/,
        const ::mcap::Message &info_message)
{
    // reinterpret mcap buffer as cdr buffer
    info_cdr_buffer_.assign(
        reinterpret_cast<const char*>(info_message.data), 
        reinterpret_cast<const char*>(info_message.data + info_message.dataSize));
    
    // SampleInfo cdrinfo_data = ::rti::topic::from_cdr_buffer<SampleInfo>(info_cdr_buffer_);
    if (!cdr_sample_info_) {
        cdr_sample_info_ = std::make_unique<SampleInfo>(::rti::topic::from_cdr_buffer<SampleInfo>(info_cdr_buffer_));
    }
    else {
        ::rti::topic::from_cdr_buffer_no_alloc(*cdr_sample_info_, info_cdr_buffer_);
    }
    SampleInfo& cdrinfo_data = *cdr_sample_info_;


    DDS_SampleInfo native_dds_info = DDS_SAMPLEINFO_DEFAULT;

    native_dds_info.sample_state = DDS_SampleStateKind(cdrinfo_data.sample_state());
    native_dds_info.view_state = DDS_ViewStateKind(cdrinfo_data.view_state());
    native_dds_info.instance_state = DDS_InstanceStateKind(cdrinfo_data.instance_state());

    native_dds_info.source_timestamp.sec = cdrinfo_data.source_timestamp().sec();
    native_dds_info.source_timestamp.nanosec = cdrinfo_data.source_timestamp().nanosec();

    native_dds_info.publication_sequence_number.high = cdrinfo_data.publication_sequence_number().high();
    native_dds_info.publication_sequence_number.low = cdrinfo_data.publication_sequence_number().low();
    native_dds_info.reception_sequence_number.high = cdrinfo_data.reception_sequence_number().high();
    native_dds_info.reception_sequence_number.low = cdrinfo_data.reception_sequence_number().low();

    std::copy(cdrinfo_data.instance_handle().value().begin(), 
                cdrinfo_data.instance_handle().value().end(), 
                native_dds_info.instance_handle.keyHash.value);
    native_dds_info.instance_handle.keyHash.length = cdrinfo_data.instance_handle().length();
    native_dds_info.instance_handle.isValid = cdrinfo_data.instance_handle().isValid();

    std::copy(cdrinfo_data.publication_handle().value().begin(), 
                cdrinfo_data.publication_handle().value().end(), 
                native_dds_info.publication_handle.keyHash.value);
    native_dds_info.publication_handle.keyHash.length = cdrinfo_data.publication_handle().length();
    native_dds_info.publication_handle.isValid = cdrinfo_data.publication_handle().isValid();
    
    native_dds_info.disposed_generation_count = cdrinfo_data.disposed_generation_count();
    native_dds_info.no_writers_generation_count = cdrinfo_data.no_writers_generation_count();
    native_dds_info.sample_rank = cdrinfo_data.sample_rank();
    native_dds_info.generation_rank = cdrinfo_data.generation_rank();
    native_dds_info.absolute_generation_rank = cdrinfo_data.absolute_generation_rank();
    native_dds_info.valid_data = cdrinfo_data.valid_data();

    native_dds_info.reception_timestamp.sec = cdrinfo_data.reception_timestamp().sec();
    native_dds_info.reception_timestamp.nanosec = cdrinfo_data.reception_timestamp().nanosec();
    
    std::copy(cdrinfo_data.publication_virtual_guid().value().begin(), 
                cdrinfo_data.publication_virtual_guid().value().end(), 
                native_dds_info.publication_virtual_guid.value);
    native_dds_info.publication_virtual_sequence_number.high = cdrinfo_data.publication_virtual_sequence_number().high();
    native_dds_info.publication_virtual_sequence_number.low = cdrinfo_data.publication_virtual_sequence_number().low();

    std::copy(cdrinfo_data.original_publication_virtual_guid().value().begin(), 
                cdrinfo_data.original_publication_virtual_guid().value().end(), 
                native_dds_info.original_publication_virtual_guid.value);
    native_dds_info.original_publication_virtual_sequence_number.high = cdrinfo_data.original_publication_virtual_sequence_number().high();
    native_dds_info.original_publication_virtual_sequence_number.low = cdrinfo_data.original_publication_virtual_sequence_number().low();

    // construct dds info from native dds info
    ::dds::sub::SampleInfo info;
    info->native(native_dds_info);
    return info;
}

::dds::core::xtypes::DynamicData McapStreamReader::convert_sample_data(
        const ::mcap::Message &data_message)
{
    // reinterpret mcap buffer as cdr buffer
    data_cdr_buffer_.assign(
        reinterpret_cast<const char*>(data_message.data), 
        reinterpret_cast<const char*>(data_message.data + data_message.dataSize));

    // serialize cdr buffer into DDS sample data
    ::dds::core::xtypes::DynamicData data(type_);
    ::rti::topic::from_cdr_buffer_no_alloc(data, data_cdr_buffer_);
    
    return data;
}

::dds::core::xtypes::DynamicData McapStreamReader::convert_sample_data(
        const ::mcap::Message &data_message,
        const ::mcap::Message &/*info_message*/)
{
    return convert_sample_data(data_message);
}

void McapStreamReader::return_loan(
        std::vector<dds::core::xtypes::DynamicData *> &data_seq,
        std::vector<dds::sub::SampleInfo *> &info_seq)
{
    logger  << Logger::DEBUG << "return_loan()"
            << ", stream: " << this->stream_info_.stream_name()
            << ", num samples: " << loaned_data_.size();

    data_seq.clear();
    info_seq.clear();

    loaned_data_.clear();
    loaned_info_.clear();
}

bool McapStreamReader::finished()
{
    return finished_;
}

void McapStreamReader::reset()
{
    assert(data_reader_);
    data_messages_ = std::make_unique<::mcap::LinearMessageView>(data_reader_->readMessages(on_data_problem_, data_read_options_));
    data_iterator_ = std::make_unique<::mcap::LinearMessageView::Iterator>(data_messages_->begin());
    logger  << Logger::DEBUG << "reset() [data]"
            << ", stream: " << this->stream_info_.stream_name();

    if (info_reader_) {
        info_messages_ = std::make_unique<::mcap::LinearMessageView>(info_reader_->readMessages(on_info_problem_, info_read_options_));
        info_iterator_ = std::make_unique<::mcap::LinearMessageView::Iterator>(info_messages_->begin());
        logger  << Logger::DEBUG << "reset() [info]"
                << ", stream: " << this->stream_info_.stream_name();
    }

    read_messages_map_.clear();
    finished_ = false;
}

McapStreamInfoReader::McapStreamInfoReader(
        const std::string& data_filename,
        const std::string& info_filename,
        Logger& logger)
        : service_start_time_(0),
          service_end_time_(std::numeric_limits<int64_t>::max()),
          finished_(false),
          logger(logger)
{
    // open MCAP data file
    data_reader_ = std::make_shared<::mcap::McapReader>();
    if (auto status = data_reader_->open(data_filename); !status.ok()) {
        logger  << Logger::FATAL << "!open() [data] for StreamInfo"
                << ", unable to open MCAP file: " << data_filename
                << ", " << status.message;
    }
    else {
        logger  << Logger::INFO << "open() [data] for StreamInfo"
                << ", file: " << data_filename;
    }

    // if info filename is passed, open it
    if (!info_filename.empty()) {
        if (data_filename == info_filename) {
            // if data & info are same file, reuse the MCAP reader
            info_reader_ = data_reader_;
        }
        else {
            info_reader_ = std::make_shared<::mcap::McapReader>();
            if (auto status = info_reader_->open(info_filename); !status.ok()) {
                logger  << Logger::FATAL << "!open() [info] for StreamInfo"
                        << ", unable to open MCAP file: " << info_filename
                        << ", " << status.message;
            }
            else {
                logger  << Logger::INFO << "open() [info] for StreamInfo"
                        << ", file: " << info_filename;
            }
        }
    }

    reset();
}

McapStreamInfoReader::~McapStreamInfoReader()
{
    if (data_reader_) {
        data_reader_->close();
        logger  << Logger::INFO << "close() [data] for StreamInfo"
                << ", file ...";
    }

    if (info_reader_ && (info_reader_ != data_reader_)) {
        info_reader_->close();
        logger  << Logger::INFO << "close() [info] for StreamInfo"
                << ", file ...";
    }
}

void McapStreamInfoReader::read(
        std::vector<rti::routing::StreamInfo *> &data_seq,
        const rti::recording::storage::SelectorState &)
{
    if (finished()) {
        return;
    }

    int32_t stream_count = 0;
    for (auto [channel_id, channel_ptr] : data_reader_->channels()) {
        if (channel_ptr->schemaId > 0) {
            auto schema_ptr = data_reader_->schema(channel_ptr->schemaId);
            if (!schema_ptr) {
                logger  << Logger::WARN << "schema() [data]"
                        << ", unable to retrieve MCAP schema for topic: " << channel_ptr->topic
                        << ", skipping.";
                continue;
            }

            if (channel_ptr->metadata.find(MCAP_CHANNEL_METADATA__INFO_CHANNEL_FLAG) != channel_ptr->metadata.end()) {
                logger  << Logger::DEBUG << "metadata["  << std::quoted(MCAP_CHANNEL_METADATA__INFO_CHANNEL_FLAG) << "] [data]"
                        << ", found info topic: " << channel_ptr->topic
                        << ", channel id: " << channel_ptr->id;
                continue;
            }
            
            auto dds_xml_it = channel_ptr->metadata.find(MCAP_CHANNEL_METADATA__DDS_XML_TYPE);
            if (dds_xml_it == channel_ptr->metadata.end()) {
                logger  << Logger::WARN << "metadata["  << std::quoted(MCAP_CHANNEL_METADATA__DDS_XML_TYPE) << "] [data]"
                        << ", unable to retrieve DDS-XML type definition for topic: " << channel_ptr->topic
                        << ", skipping.";
                continue;
            }

            loaned_streaminfos_.push_back(std::make_shared<::rti::recording::StreamInfo>(channel_ptr->topic, schema_ptr->name));
            loaned_types_.push_back(std::make_shared<::dds::core::xtypes::DynamicType>(get_type_from_xml(schema_ptr->name, dds_xml_it->second)));
            loaned_streaminfos_.back()->type_info().dynamic_type(loaned_types_.back().get());

            data_seq.push_back(loaned_streaminfos_.back().get());
            stream_count++;

            logger  << Logger::DEBUG << "read()"
                    << ", read StreamInfo"
                    << ", stream: " << channel_ptr->topic
                    << ", type: " << schema_ptr->name;
        }
    }

    finished_ = true;
    
    logger  << Logger::DEBUG << "read()"
            << ", total StreamInfos: " << stream_count;
}

void McapStreamInfoReader::return_loan(
        std::vector<rti::routing::StreamInfo *> &stream_info_seq)
{
    logger  << Logger::DEBUG << "return_loan()"
            << ", num StreamInfos: " << stream_info_seq.size();
    
    stream_info_seq.clear();

    loaned_types_.clear();
    loaned_streaminfos_.clear();
}

/*
 * Replay and Converter need to know the initial and final timestamps of the
 * recording being read.
 */
inline int64_t McapStreamInfoReader::service_start_time()
{
    return service_start_time_;
}

/*
 * Replay and Converter need to know the initial and final timestamps of the
 * recording being read.
 */
inline int64_t McapStreamInfoReader::service_stop_time()
{
    return service_end_time_;
}

inline bool McapStreamInfoReader::finished()
{
    return finished_;
}

void McapStreamInfoReader::reset()
{
    // read summaries
    ::mcap::ProblemCallback on_problem = [this](const ::mcap::Status& status) {
        this->logger  << Logger::ERROR << "!readSummary()"
                << ", error reading MCAP summary: " << status.message;
    };

    if (info_reader_) {
        if (auto status = info_reader_->readSummary(::mcap::ReadSummaryMethod::AllowFallbackScan, on_problem); !status.ok()) {
            on_problem(status);
        }
    }
    if (data_reader_) {
        if (auto status = data_reader_->readSummary(::mcap::ReadSummaryMethod::AllowFallbackScan, on_problem); !status.ok()) {
            on_problem(status);
        }
    }

    // parse start/end times from MCAP file(s)
    auto selected_reader = info_reader_ ? info_reader_ : data_reader_;
    if (selected_reader) {
        auto metadata_indexes = selected_reader->metadataIndexes();
        auto map_it = metadata_indexes.find(MCAP_FILE_METADATA__RECORDING_TIMES);
        if (map_it != metadata_indexes.end()) {
            auto & metadata_index = map_it->second;

            ::mcap::Record record;
            if (auto status = selected_reader->ReadRecord(*(selected_reader->dataSource()), metadata_index.offset, &record); !status.ok()) {
                logger  << Logger::ERROR << "!ReadMetadataIndex()"
                        << ", unable to read metadata index record: " << status.message;
            }
            ::mcap::Metadata metadata;
            if (auto status = selected_reader->ParseMetadata(record, &metadata); !status.ok()) {
                logger  << Logger::ERROR << "!ParseMetadata()"
                        << ", unable to parse metadata: " << status.message;
            }
            
            auto metadata_it = metadata.metadata.find(MCAP_FILE_METADATA__RECORDING_START_TIME);
            if (metadata_it != metadata.metadata.end()) {
                service_start_time_ = std::stoll(metadata_it->second);
                logger  << Logger::DEBUG << "metadata[" << std::quoted(MCAP_FILE_METADATA__RECORDING_START_TIME) << "]"
                        << ", service start time: " << service_start_time_;
            }
            else {
                logger  << Logger::WARN << "metadata[" << std::quoted(MCAP_FILE_METADATA__RECORDING_START_TIME) << "]"
                        << ", unable to determine service start time from MCAP file metadata (2).";
            }
        }
        else {
            logger  << Logger::WARN << "metadata[" << std::quoted(MCAP_FILE_METADATA__RECORDING_TIMES) << "]"
                    << ", unable to determine service start time from MCAP file metadata.";
        }
    }

    std::optional<::mcap::Statistics> statistics;
    if (info_reader_) {
        statistics = info_reader_->statistics();
    }
    if (!statistics && data_reader_) {
        statistics = data_reader_->statistics();
    }
    if (statistics) {
        if (service_start_time_ == 0) {
            service_start_time_ = static_cast<int64_t>(statistics->messageStartTime);
        }
        service_end_time_ = static_cast<int64_t>(statistics->messageEndTime);
        logger  << Logger::DEBUG << "statistics()"
                << ", start: " << statistics->messageStartTime
                << ", end: " << statistics->messageEndTime;
    }
    else {
        logger  << Logger::WARN << "statistics()"
                << ", unable to determine service start/end times from MCAP file(s).";
    }

    finished_ = false;
    logger  << Logger::DEBUG << "reset()"
            << ", reset MCAP StreamInfo reader.";
}

}  // rti::recording::storage::mcap