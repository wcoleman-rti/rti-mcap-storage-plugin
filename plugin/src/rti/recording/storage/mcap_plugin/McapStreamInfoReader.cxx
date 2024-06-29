#include "rti/recording/storage/mcap_plugin/McapStreamInfoReader.hpp"

namespace rti::recording::storage::mcap_plugin {

McapStreamInfoReader::McapStreamInfoReader(
        mcap::SharedMcapReader mcap_reader,
        sample_order::SampleOrderKind sample_order)
        : mcap_reader(mcap_reader),
          sample_order(sample_order) {
    mcap_read_options.readOrder = sample_order::SampleOrderMap.at(sample_order);
    reset();
}

void McapStreamInfoReader::read(
        std::vector<rti::routing::StreamInfo *> &sample_seq, 
        const rti::recording::storage::SelectorState &selector) {

    if (finished()) {
        return;
    }

    if (selector.instance_history_depth() > 0) {
        // TODO: log warning (unsupported feature)
        return;
    }

    // Lock mutex, find topics
    auto lock = mcap_reader.lock();
    for (const auto & channel_info : mcap_reader->channels()) {

        const auto & channel_id = channel_info.first;

        // Filter on sample states
        if (selector.sample_state() != dds::sub::status::SampleState::any()) {
            if (selector.sample_state() != message_states[channel_id])
                continue;
        }

        const auto & channel_ptr = channel_info.second;
        const auto & schema_ptr = mcap_reader->schema(channel_ptr->schemaId);
        rti::routing::StreamInfo * stream_info = new rti::routing::StreamInfo(channel_ptr->topic, schema_ptr->name);
        auto type = type::get_channel_type(*channel_ptr, *schema_ptr);
        if (type) {
            stream_info->type_info().dynamic_type(type.release());
        }
        sample_seq.push_back(stream_info);
        message_states.insert_or_assign(channel_id, dds::sub::status::SampleState::read());
    }

    is_finished = true;
}

void McapStreamInfoReader::return_loan(std::vector<rti::routing::StreamInfo *> &sample_seq) {
    for (size_t i = 0; i < sample_seq.size(); i++) {
        auto type = &(sample_seq[i]->type_info().dynamic_type());
        if (type != nullptr && sample_seq[i]->type_info().type_representation_kind() == rti::routing::TypeRepresentationKind::DYNAMIC_TYPE) {
            delete type;
        }
        delete sample_seq[i];
    }
    sample_seq.clear();
}

bool McapStreamInfoReader::finished() {
    return is_finished;
}

void McapStreamInfoReader::reset() {
    is_finished = false;
    mcap::ChannelId channel_id;
    
    // Lock mutex, quickly read messages to discover channels
    auto lock = mcap_reader.lock();
    message_states.clear();
    for (const auto & channel_info : mcap_reader->channels()) {
        const auto & channel_id = channel_info.first;
        message_states.insert_or_assign(channel_id, dds::sub::status::SampleState::not_read());
    }
}

int64_t McapStreamInfoReader::service_start_time() {
    mcap::Timestamp timestamp = 0;

    // Lock mutex, create indexed mcap reader
    auto lock = mcap_reader.lock();
    auto callback = [&timestamp](const mcap::Message& message, mcap::RecordOffset offset) {
        timestamp = message.logTime;};
    mcap::IndexedMessageReader indexed_reader(*mcap_reader, mcap_read_options, callback);

    indexed_reader.next();
    if (!indexed_reader.status().ok()) {
        // TODO: log status
        throw std::runtime_error("MCAP Reader failed: " + indexed_reader.status().message);
    }

    // safely cast uint64_t --> int64_t
    int64_t value;
    try {
        value = util::safe_cast<int64_t>(timestamp);
    } catch (std::overflow_error & e) {
         value = 0;
    }
    return value;
}

int64_t McapStreamInfoReader::service_stop_time() {
    mcap::Timestamp timestamp = mcap::MaxTime;

    // Lock mutex, create indexed mcap reader
    auto lock = mcap_reader.lock();
    auto callback = [&timestamp](const mcap::Message& message, mcap::RecordOffset offset) {
        timestamp = message.logTime;};
    mcap::ReadMessageOptions current_mcap_read_options(mcap_read_options);
    current_mcap_read_options.readOrder = sample_order::reverse_read_order(mcap_read_options.readOrder);
    mcap::IndexedMessageReader indexed_reader(*mcap_reader, current_mcap_read_options, callback);

    indexed_reader.next();
    if (!indexed_reader.status().ok()) {
        // TODO: log status
        throw std::runtime_error("MCAP Reader failed: " + indexed_reader.status().message);
    }

    // safely cast uint64_t --> int64_t
    int64_t value;
    try {
        value = util::safe_cast<int64_t>(timestamp);
    } catch (std::overflow_error & e) {
         value = std::numeric_limits<int64_t>::max();
    }
    return value;
}


} // namespace rti::recording::storage::mcap_plugin