#include <algorithm>
#include <limits>
#include <rti/recording/storage/StorageDefs.hpp>
#include "McapReader.hpp"
#include <rti/mcap/core/SampleMetadata.hpp>

namespace rti::mcap::storage {
using ::rti::recording::storage::SelectorState;
using ::rti::recording::storage::StorageStreamInfoReader;
using ::rti::recording::storage::StorageStreamReader;

namespace {

void open_archive(::rti::mcap::file::Reader& reader, const std::string& path, Logger& logger)
{
    if (const auto status = reader.open(path); !status.ok()) {
        logger.fatal("Unable to open MCAP archive " + path + ": " + status.message);
    }
    const auto problem = [&logger](const ::mcap::Status& status) {
        logger.fatal("Unable to read MCAP summary: " + status.message);
    };
    if (const auto status = reader.readSummary(
                ::mcap::ReadSummaryMethod::AllowFallbackScan, problem); !status.ok()) {
        problem(status);
    }
}

::mcap::Timestamp exclusive_end(const ::dds::core::Time& time)
{
    const auto timestamp = convert_time<::mcap::Timestamp>(time);
    return timestamp == ::mcap::MaxTime ? timestamp : timestamp + 1;
}

int32_t domain_of(const ::mcap::Channel& channel)
{
    const auto found = channel.metadata.find(MCAP_CHANNEL_METADATA__DDS_DOMAIN_ID);
    if (found == channel.metadata.end()) {
        throw std::runtime_error("Missing DDS domain for MCAP channel " + channel.topic);
    }
    size_t used = 0;
    const auto domain = std::stol(found->second, &used);
    if (used != found->second.size() || domain < 0 ||
            domain > std::numeric_limits<int32_t>::max()) {
        throw std::runtime_error("Invalid DDS domain for MCAP channel " + channel.topic);
    }
    return static_cast<int32_t>(domain);
}

int64_t service_time(::mcap::Timestamp timestamp)
{
    if (timestamp > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
        throw std::overflow_error("MCAP timestamp exceeds the RTI service timestamp range");
    }
    return static_cast<int64_t>(timestamp);
}

}  // namespace

RTI_RECORDING_STORAGE_READER_CREATE_DEF(McapFileReader)

McapFileReader::McapFileReader(const ::rti::recording::PropertySet& properties)
        : StorageReader(properties),
          data_filename_(get_from_properties_req<std::string>(
                  properties, RTI_XML_PROPERTY__DATA_FILENAME)),
          info_filename_(get_from_properties_opt<std::string>(
                  properties, RTI_XML_PROPERTY__INFO_FILENAME).value_or("")),
          logger(Logger::LogLevel(get_from_properties_opt<int>(
                  properties, RTI_XML_PROPERTY__LOGGING_LOG_LEVEL).value_or(Logger::WARN)))
{
}

McapFileReader::~McapFileReader() = default;

StorageStreamInfoReader* McapFileReader::create_stream_info_reader(const ::rti::routing::PropertySet&)
{
    return new McapStreamInfoReader(data_filename_, info_filename_, logger);
}

void McapFileReader::delete_stream_info_reader(StorageStreamInfoReader* reader)
{
    delete reader;
}

StorageStreamReader* McapFileReader::create_stream_reader(
        const ::rti::routing::StreamInfo& stream, const ::rti::routing::PropertySet& properties)
{
    if (stream.type_info().type_representation_kind() !=
                    ::rti::routing::TypeRepresentationKind::DYNAMIC_TYPE ||
            !stream.type_info().type_representation()) {
        logger.fatal("MCAP replay requires a non-null DynamicType for " + stream.stream_name());
    }
    const auto domain = get_from_properties_opt<int32_t>(
            properties, ::rti::recording::domain_id_property_name()).value_or(-1);
    const auto start = get_from_properties_opt<int64_t>(
            properties, ::rti::recording::start_timestamp_property_name()).value_or(0);
    const auto end = get_from_properties_opt<int64_t>(
            properties, ::rti::recording::end_timestamp_property_name())
            .value_or(std::numeric_limits<int64_t>::max());
    if (start < 0 || end < start || domain < -1) {
        logger.fatal("Invalid MCAP replay domain or timestamp range for " + stream.stream_name());
    }
    return new McapStreamReader(
            data_filename_, info_filename_, stream, domain,
            static_cast<::mcap::Timestamp>(start),
            static_cast<::mcap::Timestamp>(end) + 1, logger);
}

void McapFileReader::delete_stream_reader(StorageStreamReader* reader)
{
    delete reader;
}

McapStreamReader::McapStreamReader(
        const std::string& data_filename,
        const std::string& info_filename,
        const ::rti::recording::StreamInfo& stream_info,
        int32_t domain_id,
        ::mcap::Timestamp start_timestamp,
        ::mcap::Timestamp end_timestamp,
        Logger& parent_logger)
        : stream_info_(stream_info),
          type_(stream_info.type_info().dynamic_type()),
          domain_id_(domain_id),
          start_timestamp_(start_timestamp),
          end_timestamp_(end_timestamp),
          finished_(false),
          logger(parent_logger)
{
    if (start_timestamp > end_timestamp) {
        logger.fatal("Invalid MCAP stream timestamp range");
    }
    stream_info_.type_info().dynamic_type(&type_);
    reusable_samples_.reserve(MAX_REUSABLE_SAMPLES);
    data_reader_ = std::make_unique<::rti::mcap::file::Reader>();
    open_archive(*data_reader_, data_filename, logger);

    ::mcap::ChannelPtr selected;
    const auto channels = data_reader_->channels();
    for (const auto& entry : channels) {
        const auto& channel = entry.second;
        const auto topic = channel->metadata.find(MCAP_CHANNEL_METADATA__DDS_TOPIC_NAME);
        if (topic == channel->metadata.end() || topic->second != stream_info_.stream_name() ||
                channel->metadata.count(MCAP_CHANNEL_METADATA__INFO_CHANNEL_FLAG)) {
            continue;
        }
        const auto domain = domain_of(*channel);
        if (domain_id_ >= 0 && domain != domain_id_) {
            continue;
        }
        if (selected && selected->topic != channel->topic) {
            logger.fatal("Ambiguous DDS domain for MCAP topic " + stream_info_.stream_name());
        }
        const auto schema = data_reader_->schema(channel->schemaId);
        if (!schema || schema->name != stream_info_.type_info().type_name() ||
                schema->encoding != "omgidl" || channel->messageEncoding != "cdr") {
            logger.fatal("Unsupported MCAP data schema for " + stream_info_.stream_name());
        }
        selected = channel;
    }
    if (!selected) {
        logger.fatal("No matching MCAP data channel for " + stream_info_.stream_name());
    }
    data_channel_topic_ = selected->topic;
    data_read_options_.startTime = start_timestamp_;
    data_read_options_.endTime = end_timestamp_;
    data_read_options_.readOrder = ::mcap::ReadMessageOptions::ReadOrder::LogTimeOrder;
    data_read_options_.topicFilter = [this](std::string_view topic) {
        return topic == data_channel_topic_;
    };
    on_data_problem_ = [this](const ::mcap::Status& status) {
        logger.fatal("Unable to read MCAP data for " + stream_info_.stream_name() + ": " + status.message);
    };

    if (!info_filename.empty()) {
        const auto topic = selected->metadata.find(MCAP_CHANNEL_METADATA__INFO_CHANNEL_TOPIC);
        if (topic == selected->metadata.end()) {
            logger.fatal("No SampleInfo channel declared for " + stream_info_.stream_name());
        }
        info_channel_topic_ = topic->second;
        info_reader_ = std::make_unique<::rti::mcap::file::Reader>();
        open_archive(*info_reader_, info_filename, logger);
        bool found = false;
        for (const auto& entry : info_reader_->channels()) {
            const auto& channel = entry.second;
            if (channel->topic != info_channel_topic_) {
                continue;
            }
            const auto schema = info_reader_->schema(channel->schemaId);
            const auto version = channel->metadata.find("dds_mcap.sample_metadata.version");
            if (domain_of(*channel) != domain_of(*selected) ||
                    !schema || schema->name != "dds_mcap.SampleMetadata" ||
                    schema->encoding != ::rti::mcap::core::sample_metadata_encoding() ||
                    channel->messageEncoding != ::rti::mcap::core::sample_metadata_encoding() ||
                    version == channel->metadata.end() || version->second != "1") {
                logger.fatal("Unsupported MCAP SampleInfo schema or version for " + stream_info_.stream_name());
            }
            found = true;
        }
        if (!found) {
            logger.fatal("Missing MCAP SampleInfo channel for " + stream_info_.stream_name());
        }
        info_read_options_ = data_read_options_;
        info_read_options_.topicFilter = [this](std::string_view topic) {
            return topic == info_channel_topic_;
        };
        on_info_problem_ = [this](const ::mcap::Status& status) {
            logger.fatal("Unable to read MCAP SampleInfo for " + stream_info_.stream_name() +
                    ": " + status.message);
        };
    }
    reset();
}

McapStreamReader::~McapStreamReader() = default;

McapStreamReader::Cursor McapStreamReader::cursor(
        const ::rti::mcap::file::Reader& reader,
        const ::mcap::ReadMessageOptions& options,
        const ::mcap::ProblemCallback& problem)
{
    Cursor result;
    result.messages = std::make_unique<::mcap::LinearMessageView>(reader.readMessages(problem, options));
    result.iterator = std::make_unique<::mcap::LinearMessageView::Iterator>(result.messages->begin());
    return result;
}

void McapStreamReader::read(
        std::vector<::dds::core::xtypes::DynamicData*>& data,
        std::vector<::dds::sub::SampleInfo*>& info,
        const SelectorState& selector)
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (read_failure_) {
        std::rethrow_exception(read_failure_);
    }
    if (!data.empty() || !info.empty()) {
        logger.fatal("MCAP read requires empty output vectors");
    }
    try {
        if (info_reader_) read_w_info(data, info, selector);
        else read_w_data(data, info, selector);
    } catch (const std::exception& error) {
        read_failure_ = std::current_exception();
        logger << Logger::ERROR << "MCAP replay failed for " << stream_info_.stream_name()
               << ": " << error.what();
        throw;
    }
}

void McapStreamReader::read_w_data(
        std::vector<::dds::core::xtypes::DynamicData*>& data,
        std::vector<::dds::sub::SampleInfo*>& info, const SelectorState& selector)
{
    read_batch(data, info, selector);
}

void McapStreamReader::read_w_info(
        std::vector<::dds::core::xtypes::DynamicData*>& data,
        std::vector<::dds::sub::SampleInfo*>& info, const SelectorState& selector)
{
    read_batch(data, info, selector);
}

void McapStreamReader::read_batch(
        std::vector<::dds::core::xtypes::DynamicData*>& data,
        std::vector<::dds::sub::SampleInfo*>& info, const SelectorState& selector)
{
    if (selector.instance_history_depth() > 0) {
        logger.fatal("MCAP replay does not support instance history selection");
    }
    const bool stateless = selector.sample_state() == ::dds::sub::status::SampleState::any();
    if (!stateless && selector.sample_state() != ::dds::sub::status::SampleState::not_read()) {
        logger.fatal("MCAP replay supports only ANY and NOT_READ selectors");
    }
    const auto end = std::min(end_timestamp_, exclusive_end(selector.time_range_end()));
    Cursor local_data, local_info;
    Cursor* data_cursor = &data_cursor_;
    Cursor* info_cursor = &info_cursor_;
    if (stateless) {
        const auto start = std::max(start_timestamp_,
                convert_time<::mcap::Timestamp>(selector.time_range_start()));
        if (start > end) {
            logger.fatal("Invalid MCAP selector timestamp range");
        }
        auto options = data_read_options_;
        options.startTime = start;
        local_data = cursor(*data_reader_, options, on_data_problem_);
        data_cursor = &local_data;
        if (info_reader_) {
            options = info_read_options_;
            options.startTime = start;
            local_info = cursor(*info_reader_, options, on_info_problem_);
            info_cursor = &local_info;
        }
    }
    auto& driver = info_reader_ ? *info_cursor : *data_cursor;
    const auto exhausted = [](const Cursor& value) {
        return *value.iterator == value.messages->end();
    };
    const auto limit = selector.max_samples() < 0
            ? std::numeric_limits<size_t>::max() : static_cast<size_t>(selector.max_samples());
    ::rti::core::xtypes::DynamicDataProperty properties;
    // Zero preallocates the type's maximum bounded size, not the payload size.
    properties.buffer_initial_size(1);
    Loan batch;
    while (batch.data.size() < limit && !exhausted(driver)) {
        const auto& message = (**driver.iterator).message;
        if (message.logTime >= end) {
            break;
        }
        const auto sample_info = info_reader_
                ? convert_sample_info(::mcap::Message{}, message) : convert_sample_info(message);
        std::unique_ptr<::dds::core::xtypes::DynamicData> sample;
        if (reusable_samples_.empty()) {
            sample = std::make_unique<::dds::core::xtypes::DynamicData>(type_, properties);
        } else {
            sample = std::move(reusable_samples_.back());
            reusable_samples_.pop_back();
            sample->clear_all_members();
        }
        if (sample_info.valid()) {
            if (exhausted(*data_cursor)) {
                throw std::runtime_error("SampleInfo indicates valid data without a matching data sample");
            }
            const auto& payload = (**data_cursor->iterator).message;
            if (info_reader_ &&
                    (payload.sequence != message.sequence || payload.logTime != message.logTime)) {
                throw std::runtime_error("MCAP data and SampleInfo message identifiers or times do not match");
            }
            deserialize(payload, *sample);
            if (info_reader_) {
                ++*data_cursor->iterator;
            }
        }
        batch.data.push_back(std::move(sample));
        batch.info.push_back(sample_info);
        ++*driver.iterator;
    }
    if (exhausted(driver) && info_reader_ && !exhausted(*data_cursor)) {
        throw std::runtime_error("MCAP data contains a sample without matching SampleInfo");
    }
    finished_ = exhausted(driver);
    if (batch.data.empty()) {
        return;
    }
    data.reserve(batch.data.size());
    info.reserve(batch.info.size());
    const auto key = batch.data.front().get();
    auto result = loans_.emplace(key, std::move(batch));
    if (!result.second) {
        throw std::logic_error("Duplicate MCAP loan identifier");
    }
    auto& loan = result.first->second;
    for (size_t i = 0; i < loan.data.size(); ++i) {
        data.push_back(loan.data[i].get());
        info.push_back(&loan.info[i]);
    }
    if (logger.enabled(Logger::DEBUG)) {
        logger << Logger::DEBUG << "MCAP read " << stream_info_.stream_name()
               << ": " << data.size() << " samples";
    }
}

::dds::sub::SampleInfo McapStreamReader::convert_sample_info(const ::mcap::Message& message)
{
    DDS_SampleInfo native = DDS_SAMPLEINFO_DEFAULT;
    ::rti::core::native_conversions::to_native(native.source_timestamp, convert_time(message.publishTime));
    ::rti::core::native_conversions::to_native(native.reception_timestamp, convert_time(message.logTime));
    native.reception_sequence_number = convert_sequence_number(message.sequence).native();
    native.valid_data = DDS_BOOLEAN_TRUE;
    ::dds::sub::SampleInfo info;
    info->native(native);
    return info;
}

::dds::sub::SampleInfo McapStreamReader::convert_sample_info(
        const ::mcap::Message&, const ::mcap::Message& message)
{
    if (message.dataSize > std::numeric_limits<size_t>::max()) {
        throw std::length_error("MCAP SampleInfo exceeds the host size limit");
    }
    return ::rti::mcap::core::to_sample_info(::rti::mcap::core::decode_sample_metadata(
            reinterpret_cast<const char*>(message.data), static_cast<size_t>(message.dataSize)));
}

void McapStreamReader::deserialize(
        const ::mcap::Message& message, ::dds::core::xtypes::DynamicData& sample)
{
    if (!message.data || message.dataSize < 4 ||
            message.dataSize > std::numeric_limits<uint32_t>::max()) {
        throw std::invalid_argument("Invalid MCAP CDR payload size or pointer");
    }
    const auto* bytes = reinterpret_cast<const char*>(message.data);
    data_cdr_buffer_.assign(bytes, bytes + static_cast<size_t>(message.dataSize));
    ::rti::topic::from_cdr_buffer_no_alloc(sample, data_cdr_buffer_);
}

::dds::core::xtypes::DynamicData McapStreamReader::convert_sample_data(const ::mcap::Message& message)
{
    ::dds::core::xtypes::DynamicData sample(type_);
    deserialize(message, sample);
    return sample;
}

::dds::core::xtypes::DynamicData McapStreamReader::convert_sample_data(
        const ::mcap::Message& message, const ::mcap::Message&)
{
    return convert_sample_data(message);
}

void McapStreamReader::return_loan(
        std::vector<::dds::core::xtypes::DynamicData*>& data,
        std::vector<::dds::sub::SampleInfo*>& info)
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (data.empty() && info.empty()) return;
    if (data.empty() || data.size() != info.size()) {
        logger.fatal("Invalid MCAP loan vector sizes");
    }
    const auto found = loans_.find(data.front());
    if (found == loans_.end() || found->second.data.size() != data.size()) {
        logger.fatal("Unknown MCAP replay loan");
    }
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] != found->second.data[i].get() || info[i] != &found->second.info[i]) {
            logger.fatal("MCAP loan pointers do not match the returned batch");
        }
    }
    for (auto& sample : found->second.data) {
        if (reusable_samples_.size() == MAX_REUSABLE_SAMPLES) break;
        reusable_samples_.push_back(std::move(sample));
    }
    data.clear();
    info.clear();
    loans_.erase(found);
}

bool McapStreamReader::finished()
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return finished_;
}

void McapStreamReader::reset()
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    data_cursor_ = cursor(*data_reader_, data_read_options_, on_data_problem_);
    if (info_reader_) info_cursor_ = cursor(*info_reader_, info_read_options_, on_info_problem_);
    const auto& driver = info_reader_ ? info_cursor_ : data_cursor_;
    finished_ = *driver.iterator == driver.messages->end();
    read_failure_ = nullptr;
}

McapStreamInfoReader::McapStreamInfoReader(
        const std::string& data_filename, const std::string& info_filename, Logger& parent_logger)
        : service_start_time_(0),
          service_end_time_(std::numeric_limits<int64_t>::max()),
          finished_(false),
          logger(parent_logger)
{
    data_reader_ = std::make_shared<::rti::mcap::file::Reader>();
    open_archive(*data_reader_, data_filename, logger);
    if (!info_filename.empty()) {
        if (data_filename == info_filename) info_reader_ = data_reader_;
        else {
            info_reader_ = std::make_shared<::rti::mcap::file::Reader>();
            open_archive(*info_reader_, info_filename, logger);
        }
    }
    const auto reader = info_reader_ ? info_reader_ : data_reader_;
    const auto& indexes = reader->metadataIndexes();
    const auto index = indexes.find(MCAP_FILE_METADATA__RECORDING_TIMES);
    if (index != indexes.end()) {
        ::mcap::Record record;
        if (const auto status = reader->ReadRecord(*reader->dataSource(), index->second.offset, &record);
                !status.ok()) {
            logger.fatal("Unable to read MCAP recording times: " + status.message);
        }
        ::mcap::Metadata metadata;
        if (const auto status = reader->ParseMetadata(record, &metadata); !status.ok()) {
            logger.fatal("Unable to parse MCAP recording times: " + status.message);
        }
        const auto start = metadata.metadata.find(MCAP_FILE_METADATA__RECORDING_START_TIME);
        if (start != metadata.metadata.end()) {
            const ::rti::recording::PropertySet value{{"start", start->second}};
            service_start_time_ = get_from_properties_req<int64_t>(value, "start");
            if (service_start_time_ < 0) logger.fatal("Negative MCAP recording start time");
        }
    }
    const auto statistics = reader->statistics();
    if (statistics) {
        if (service_start_time_ == 0) service_start_time_ = service_time(statistics->messageStartTime);
        service_end_time_ = service_time(statistics->messageEndTime);
    } else {
        logger << Logger::WARN << "Unable to determine MCAP recording time range";
    }
}

McapStreamInfoReader::~McapStreamInfoReader() = default;

void McapStreamInfoReader::read(
        std::vector<::rti::routing::StreamInfo*>& output, const SelectorState&)
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!output.empty()) logger.fatal("MCAP stream discovery requires an empty output vector");
    if (finished_) return;
    StreamInfoLoan batch;
    for (const auto& entry : data_reader_->channels()) {
        const auto& channel = entry.second;
        if (channel->metadata.count(MCAP_CHANNEL_METADATA__INFO_CHANNEL_FLAG)) continue;
        const auto schema = data_reader_->schema(channel->schemaId);
        const auto xml = channel->metadata.find(MCAP_CHANNEL_METADATA__DDS_XML_TYPE);
        if (!schema || xml == channel->metadata.end() || channel->messageEncoding != "cdr" ||
                schema->encoding != "omgidl") {
            logger << Logger::WARN << "Skipping non-DDS MCAP channel " << channel->topic;
            continue;
        }
        const auto topic = channel->metadata.find(MCAP_CHANNEL_METADATA__DDS_TOPIC_NAME);
        const auto& name = topic == channel->metadata.end() ? channel->topic : topic->second;
        const auto key = schema->name + '\0' + xml->second;
        auto type = types_.find(key);
        if (type == types_.end()) {
            type = types_.emplace(key, std::make_shared<::dds::core::xtypes::DynamicType>(
                    get_type_from_xml(schema->name, xml->second))).first;
        }
        auto stream = std::make_unique<::rti::routing::StreamInfo>(name, schema->name);
        stream->type_info().dynamic_type(type->second.get());
        batch.types.push_back(type->second);
        batch.streams.push_back(std::move(stream));
    }
    if (!batch.streams.empty()) {
        output.reserve(batch.streams.size());
        const auto key = batch.streams.front().get();
        auto found = loans_.emplace(key, std::move(batch));
        if (!found.second) throw std::logic_error("Duplicate MCAP stream-info loan");
        for (const auto& stream : found.first->second.streams) output.push_back(stream.get());
    }
    finished_ = true;
}

void McapStreamInfoReader::return_loan(std::vector<::rti::routing::StreamInfo*>& output)
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (output.empty()) return;
    const auto found = loans_.find(output.front());
    if (found == loans_.end() || found->second.streams.size() != output.size()) {
        logger.fatal("Unknown MCAP stream-info loan");
    }
    for (size_t i = 0; i < output.size(); ++i) {
        if (output[i] != found->second.streams[i].get()) logger.fatal("Invalid MCAP stream-info loan pointers");
    }
    output.clear();
    loans_.erase(found);
}

int64_t McapStreamInfoReader::service_start_time() { return service_start_time_; }
int64_t McapStreamInfoReader::service_stop_time() { return service_end_time_; }
bool McapStreamInfoReader::finished()
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return finished_;
}
void McapStreamInfoReader::reset()
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    finished_ = false;
}

}  // namespace rti::mcap::storage
