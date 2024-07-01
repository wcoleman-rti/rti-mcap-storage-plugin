#include "rti/recording/storage/mcap_plugin/McapStreamReader.hpp"

namespace rti::recording::storage::mcap_plugin {

McapStreamReader::McapStreamReader(
        mcap::SharedMcapReader mcap_reader,
        std::string stream_name,
        dds::core::xtypes::DynamicType type,
        sample_order::SampleOrderKind sample_order)
            : mcap_reader(mcap_reader),
              type(type),
              sample_order(sample_order) {

    std::string data_stream_name = stream_name;
    std::string info_stream_name = info::get_info_stream_name(stream_name);

    mcap_read_options.readOrder = sample_order::SampleOrderMap.at(sample_order);
    mcap_read_options.topicFilter = [data_stream_name, info_stream_name](std::string_view topic) {
            if ((topic == data_stream_name) || (topic == info_stream_name))
                return true;
            else
                return false;
        };
    
    {
        // Lock mutex, find channels
        auto lock = mcap_reader.lock();
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

    if (finished()) {
        return;
    }

    if (selector.instance_history_depth() > 0) {
        // TODO: log warning (unsupported feature)
        return;
    }

    // Configure mcap read options (topic, start/stop time)
    mcap::ReadMessageOptions current_mcap_read_options(mcap_read_options);
    try {
        current_mcap_read_options.startTime = data::convert_timestamp(selector.time_range_start());
    } catch (std::overflow_error & e) {
        current_mcap_read_options.startTime = 0;
    }
    try {
        current_mcap_read_options.endTime = data::convert_timestamp(selector.time_range_end());
    } catch (std::overflow_error & e) {
        current_mcap_read_options.endTime = mcap::MaxTime;
    }

    std::unique_ptr<mcap::Message> current_message;

    // Lock mutex, create indexed mcap reader
    auto lock = mcap_reader.lock();
    auto callback = [&current_message](const mcap::Message& message, mcap::RecordOffset) {
        current_message = std::make_unique<mcap::Message>(message);};
    mcap::IndexedMessageReader indexed_reader(*mcap_reader, current_mcap_read_options, callback);

    int32_t messages_read = 0;
    while (indexed_reader.next()) {
        
        // Filter on sample states
        if (selector.sample_state() != dds::sub::status::SampleState::any()) {
            if (selector.sample_state() != message_states[current_message->sequence])
                continue;
        }

        dds::core::xtypes::DynamicData * sample = new dds::core::xtypes::DynamicData(type);
        dds::sub::SampleInfo * info = new dds::sub::SampleInfo();
        if (current_message->channelId == mcap_data_channel_id) {
            if (!data::mcap_message_to_dds_sample(*current_message, *sample, *info)) {
                // TODO: log status
            }
        }
        else if (current_message->channelId == mcap_info_channel_id) {
            if (!info::mcap_message_to_dds_info(*current_message, *info)) {
                // TODO: log status
            }
        }        

        // Push DDS sample & info
        sample_seq.push_back(sample);
        info_seq.push_back(info);

        // Mark MCAP message as read
        message_states.insert_or_assign(current_message->sequence, dds::sub::status::SampleState::read());
        ++messages_read;

        // Stop if we reached max_samples
        if (selector.max_samples() != dds::core::LENGTH_UNLIMITED && messages_read >= selector.max_samples()) {
            return;
        }
    }

    if (!indexed_reader.status().ok()) {
        // TODO: log status
        throw std::runtime_error("MCAP Reader failed: " + indexed_reader.status().message);
    }

    // Read all messages, done
    is_finished = true;
}

void McapStreamReader::return_loan(
            std::vector<dds::core::xtypes::DynamicData *> &sample_seq,
            std::vector<dds::sub::SampleInfo *> &info_seq) {
    for (size_t i = 0; i < sample_seq.size(); i++) {
        delete sample_seq[i];
        delete info_seq[i];
    }
    sample_seq.clear();
    info_seq.clear();
}

bool McapStreamReader::finished() {
    return is_finished;
}

void McapStreamReader::reset() {
    is_finished = false;

    uint32_t sequence = 0;
    
    // Lock mutex, create indexed mcap reader
    auto lock = mcap_reader.lock();
    message_states.clear();
    auto callback = [&sequence](const mcap::Message& message, mcap::RecordOffset) {
        sequence = message.sequence;};
    mcap::IndexedMessageReader indexed_reader(*mcap_reader, mcap_read_options, callback);

    while (indexed_reader.next()) {
        message_states.insert_or_assign(sequence, dds::sub::status::SampleState::not_read());
        ++sequence;
    }

    if (!indexed_reader.status().ok()) {
        // TODO: log status
        throw std::runtime_error("MCAP Reader failed: " + indexed_reader.status().message);
    }
}


} // namespace rti::recording::storage::mcap_plugin