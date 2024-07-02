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

    // If the streaminfo is finished, stop reading immediately
    if (finished()) {
        return;
    }

    int32_t streams_read = 0;

    // Lock mutex, find topics
    auto reader_lock = mcap_reader.lock();
    for (const auto & channel_info : mcap_reader->channels()) {

        bool skip_message = false;
        const auto & channel_id = channel_info.first;
        const auto & channel_ptr = channel_info.second;
        const auto & schema_ptr = mcap_reader->schema(channel_ptr->schemaId);

        // Stop reading if max samples is reached or passed
        if (selector.max_samples() != dds::core::LENGTH_UNLIMITED &&
                streams_read >= selector.max_samples()) {
            // TODO: log status
            return;
        }

        // Skip messages with mismatched sample states
        if (selector.sample_state() != dds::sub::status::SampleState::any() &&
                selector.sample_state() != message_states[channel_id]) {
            // TODO: log status
            skip_message = true;
        }


        if (!skip_message) {
            // TODO: log status

            // Convert MCAP Channel info --> DDS Stream/Topic info
            rti::routing::StreamInfo * stream_info = new rti::routing::StreamInfo(channel_ptr->topic, schema_ptr->name);
            auto type = type::get_channel_type(*channel_ptr, *schema_ptr);
            if (type) {
                stream_info->type_info().dynamic_type(type.release());
            }

            // Push DDS sample
            sample_seq.push_back(stream_info);

            // Mark MCAP channel as read
            message_states.insert_or_assign(channel_id, dds::sub::status::SampleState::read());
            
            // Increment message read counter
            ++streams_read;
        }
    }

    is_finished = true;
}

void McapStreamInfoReader::return_loan(std::vector<rti::routing::StreamInfo *> &sample_seq) {
    
    // Delete DDS Sample and DynamicType pointers
    for (size_t i = 0; i < sample_seq.size(); i++) {
        auto type = &(sample_seq[i]->type_info().dynamic_type());
        if (type != nullptr && sample_seq[i]->type_info().type_representation_kind() == rti::routing::TypeRepresentationKind::DYNAMIC_TYPE) {
            delete type;
        }
        delete sample_seq[i];
    }

    // Clear vectors
    sample_seq.clear();
}

bool McapStreamInfoReader::finished() {
    return is_finished;
}

void McapStreamInfoReader::reset() {
    // Reset 'finished' flag
    is_finished = false;

    // For each message sequence, mark as "not read"
    reset_message_states();
}

int64_t McapStreamInfoReader::service_start_time() {
    mcap::Timestamp timestamp = 0;

    // Lock mutex, create indexed mcap reader
    auto reader_lock = mcap_reader.lock();
    auto callback = [&timestamp](const mcap::Message& message, mcap::RecordOffset) {
        timestamp = message.logTime;};
    
    // Use same order to read first message
    mcap::ReadMessageOptions current_mcap_read_options(mcap_read_options);
    mcap::IndexedMessageReader indexed_reader(*mcap_reader, current_mcap_read_options, callback);

    // Read first message
    indexed_reader.next();
    if (!indexed_reader.status().ok()) {
        // TODO: log status
        throw std::runtime_error("MCAP Reader failed: " + indexed_reader.status().message);
    }

    // safely cast uint64_t --> int64_t
    return util::safe_cast<int64_t>(timestamp);
}

int64_t McapStreamInfoReader::service_stop_time() {
    mcap::Timestamp timestamp = mcap::MaxTime;

    // Lock mutex, create indexed mcap reader
    auto reader_lock = mcap_reader.lock();
    auto callback = [&timestamp](const mcap::Message& message, mcap::RecordOffset) {
        timestamp = message.logTime;};

    // Reverse order to read last message
    mcap::ReadMessageOptions current_mcap_read_options(mcap_read_options);
    current_mcap_read_options.readOrder = sample_order::reverse_read_order(mcap_read_options.readOrder);
    mcap::IndexedMessageReader indexed_reader(*mcap_reader, current_mcap_read_options, callback);

    // Read last message
    indexed_reader.next();
    if (!indexed_reader.status().ok()) {
        // TODO: log status
        throw std::runtime_error("MCAP Reader failed: " + indexed_reader.status().message);
    }

    // safely cast uint64_t --> int64_t
    return util::safe_cast<int64_t>(timestamp);
}

void McapStreamInfoReader::reset_message_states() {

    // Lock mutex, quickly read messages to discover channels
    auto reader_lock = mcap_reader.lock();

    // For each channel id, mark as "not read"
    message_states.clear();
    for (const auto & channel_info : mcap_reader->channels()) {
        const auto & channel_id = channel_info.first;
        message_states.insert_or_assign(channel_id, dds::sub::status::SampleState::not_read());
    }
}


} // namespace rti::recording::storage::mcap_plugin