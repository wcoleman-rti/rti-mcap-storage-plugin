#include "rti/recording/storage/mcap_plugin/McapStreamReader.hpp"

namespace rti::recording::storage::mcap_plugin {

McapStreamReader::McapStreamReader(
        mcap::SharedMcapReader mcap_reader,
        std::string stream_name,
        dds::core::xtypes::DynamicType type,
        sample_order::SampleOrderKind sample_order)
            : mcap_reader(mcap_reader),
              indexed_reader(nullptr),
              current_message(nullptr),
              type(type),
              sample_order(sample_order) {

    data_stream_name = stream_name;
    info_stream_name = info::get_info_stream_name(stream_name);

    mcap_read_options.readOrder = sample_order::SampleOrderMap.at(sample_order);
    mcap_read_options.topicFilter = [this](std::string_view topic) {
            if ((topic == data_stream_name) || (topic == info_stream_name))
                return true;
            else
                return false;
        };
    
    {
        // Lock mutex, find channels
        auto reader_lock = mcap_reader.lock();
        auto indexed_reader_lock = indexed_reader.lock();
        auto current_message_lock = current_message.lock();
        for (const auto & channel_info : mcap_reader->channels()) {
            const auto & channel_id = channel_info.first;
            const auto & channel_ptr = channel_info.second;
            if (channel_ptr->topic == data_stream_name) {
                mcap_data_channel_id = channel_id;
            }
            else if (channel_ptr->topic == info_stream_name) {
                mcap_info_channel_id = channel_id;
            }
        }
    }
    
    reset();
}

void McapStreamReader::read(
        std::vector<dds::core::xtypes::DynamicData *> &sample_seq,
        std::vector<dds::sub::SampleInfo *> &info_seq,
        const rti::recording::storage::SelectorState &selector) {

    // If the stream is finished, stop reading immediately
    if (finished()) {
        return;
    }

    int32_t messages_read = 0;

    while (!finished()) {
        {
            // Grab locks
            auto reader_lock = mcap_reader.lock();
            auto indexed_reader_lock = indexed_reader.lock();
            auto current_message_lock = current_message.lock();

            // Skip messages if not read from file yet
            if (!current_message.ptr()) {
                // TODO: log status
                goto nextmessage;
            }

            auto message_timestamp = data::convert_timestamp(
                    sample_order::get_mcap_message_timestamp(*current_message, sample_order));

            // Stop reading if max timestamp is passed
            if (message_timestamp > selector.time_range_end()) {
                // TODO: log status
                return;
            }

            // Stop reading if max samples is reached or passed
            if (selector.max_samples() != dds::core::LENGTH_UNLIMITED &&
                    messages_read >= selector.max_samples()) {
                // TODO: log status
                return;
            }

            // Skip messages if min timestamp is not reached
            if (message_timestamp < selector.time_range_start()) {
                // TODO: log status
                goto nextmessage;
            }
            
            // Skip messages with mismatched sample states
            if (selector.sample_state() != dds::sub::status::SampleState::any() &&
                    selector.sample_state() != message_states[current_message->sequence]) {
                // TODO: log status
                goto nextmessage;
            }

            // Convert MCAP -> DDS
            dds::core::xtypes::DynamicData * sample = new dds::core::xtypes::DynamicData(type);
            dds::sub::SampleInfo * info = new dds::sub::SampleInfo();
            if (current_message->channelId == mcap_data_channel_id) {
                if (!data::mcap_message_to_dds_sample(*current_message, *sample, *info)) {
                    // TODO: log status
                    goto nextmessage;
                }
            }
            else if (current_message->channelId == mcap_info_channel_id) {
                if (!info::mcap_message_to_dds_info(*current_message, *info)) {
                    // TODO: log status
                    goto nextmessage;
                }
            }

            // Skip subsequent instance-specific messages
            const auto & instance_handle = info->instance_handle();
            if (selector.instance_history_depth() > 0 && selector.instance_history_depth() > read_instance_map[instance_handle]) {
                // TODO: log status
                goto nextmessage;
            }


            // TODO: log status

            // Push DDS sample & info
            sample_seq.push_back(sample);
            info_seq.push_back(info);

            // Mark MCAP message as read
            message_states.insert_or_assign(current_message->sequence, dds::sub::status::SampleState::read());

            // Increment instance handle counter
            ++read_instance_map[info->instance_handle()];

            // Increment message read counter
            ++messages_read;

        }

    nextmessage:
        queue_next_message();
    }
}

void McapStreamReader::return_loan(
            std::vector<dds::core::xtypes::DynamicData *> &sample_seq,
            std::vector<dds::sub::SampleInfo *> &info_seq) {

    // Delete DDS Sample and SampleInfo pointers
    for (size_t i = 0; i < sample_seq.size(); i++) {
        delete sample_seq[i];
        delete info_seq[i];
    }

    // Clear vectors
    sample_seq.clear();
    info_seq.clear();
}

bool McapStreamReader::finished() {
    return is_finished;
}

void McapStreamReader::reset() {
    // Reset 'finished' flag
    is_finished = false;

    // For each message sequence, mark as "not read"
    reset_message_states();

    // Reset indexed reader to return to first message
    reset_indexed_reader();
}

void McapStreamReader::reset_indexed_reader(mcap::Timestamp min_timestamp, mcap::Timestamp max_timestamp) {

    // Lock mutex, reset indexed reader to return to first message
    auto reader_lock = mcap_reader.lock();
    auto indexed_reader_lock = indexed_reader.lock();
    auto current_message_lock = current_message.lock();
    
    // replace 'current_message' upon new message reads
    auto callback = [this](const mcap::Message& message, mcap::RecordOffset) {
            // auto reader_lock = mcap_reader.lock();
            // auto indexed_reader_lock = indexed_reader.lock();
            // auto current_message_lock = current_message.lock();
            current_message.ptr(std::make_shared<mcap::Message>(message));
        };
    
    // configure read options (topics, read order, min/max timestamp)
    auto current_mcap_read_options(mcap_read_options);
    mcap_read_options.startTime = min_timestamp;
    mcap_read_options.endTime = max_timestamp;

    // create/replace the indexed reader
    indexed_reader.ptr(std::make_shared<mcap::IndexedMessageReader>(*mcap_reader, current_mcap_read_options, callback));

    // delete the old current message
    current_message.ptr(nullptr);
}

void McapStreamReader::reset_message_states() {

    // Lock mutex, scan messages, reset message states map
    auto reader_lock = mcap_reader.lock();
    auto indexed_reader_lock = indexed_reader.lock();
    auto current_message_lock = current_message.lock();

    uint32_t sequence = 0;
    auto callback = [&sequence](const mcap::Message& message, mcap::RecordOffset) {
        sequence = message.sequence;};
    mcap::IndexedMessageReader states_reader(*mcap_reader, mcap_read_options, callback);

    // For each message sequence, mark as "not read"
    message_states.clear();
    while (states_reader.next()) {
        message_states.insert_or_assign(sequence, dds::sub::status::SampleState::not_read());
        ++sequence;
    }

    if (!states_reader.status().ok()) {
        // TODO: log status
        throw std::runtime_error("MCAP Indexed Message Reader failed: " + states_reader.status().message);
    }
}

void McapStreamReader::queue_next_message() {

    // Grab locks
    auto reader_lock = mcap_reader.lock();
    auto indexed_reader_lock = indexed_reader.lock();
    // auto current_message_lock = current_message.lock();

    auto end = !indexed_reader->next();

    if (!indexed_reader->status().ok()) {
        // TODO: log status
        throw std::runtime_error("MCAP Indexed Message Reader failed: " + indexed_reader->status().message);
    }

    if (!end && !current_message.ptr()) {
        // TODO: log status
        throw std::runtime_error("MCAP Indexed Message Reader failed (unknown): " + indexed_reader->status().message);
    }

    // If we reached the last message, mark Stream Reader finished
    if (end) {
        // Read all messages, done
        is_finished = true;
    }
}

} // namespace rti::recording::storage::mcap_plugin