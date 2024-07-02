#include "rti/recording/storage/mcap_plugin/McapStreamWriter.hpp"

namespace rti::recording::storage::mcap_plugin {

McapStreamWriter::McapStreamWriter(
        mcap::SharedMcapWriter mcap_writer,
        std::string stream_name,
        dds::core::xtypes::DynamicType type,
        bool store_sample_info)
        : mcap_writer(mcap_writer),
          stream_name(stream_name),
          type(type),
          current_message(std::make_unique<mcap::Message>()),
          store_sample_info(store_sample_info) {
    
    // Lock mutex
    auto writer_lock = mcap_writer.lock();
    
    // Data schema --> DDS topic type
    mcap::Schema data_schema;
    data_schema.name = type.name();
    data_schema.encoding = mcap::schema::schemaEncodingOmgIdl;
    
    // Data channel --> Sample data
    mcap::Channel data_channel;
    data_channel.topic = stream_name;
    data_channel.messageEncoding = mcap::channel::messageEncodingCdr;

    // Store DDS type in MCAP channel/schema
    type::store_channel_type(data_channel, data_schema, type);

    // Add schema and channel
    mcap_writer->addSchema(data_schema);
    data_channel.schemaId = data_schema.id;
    mcap_writer->addChannel(data_channel);
    mcap_data_channel_id = data_channel.id;
    
    // Info channel --> Sample info data
    mcap::Channel info_channel;
    if (store_sample_info) {
        info_channel.topic = info::get_info_stream_name(stream_name);
        info_channel.messageEncoding = mcap::channel::messageEncodingCdr;
        info_channel.schemaId = info::data_schema.id;
        mcap_writer->addChannel(info_channel);
        mcap_info_channel_id = info_channel.id;
    }
}

McapStreamWriter::~McapStreamWriter() {
    if (current_message->data != nullptr) {
        delete[] current_message->data;
        current_message->data = nullptr;
    }
}

void McapStreamWriter::store(
            const std::vector<dds::core::xtypes::DynamicData *> &sample_seq,
            const std::vector<dds::sub::SampleInfo *> &info_seq) {
    
    const size_t count = sample_seq.size();
    for (size_t i = 0; i < count; ++i) {
        const dds::sub::SampleInfo &info = *(info_seq[i]);
        if (info->valid()) {
            const dds::core::xtypes::DynamicData &sample = *(sample_seq[i]);
            if (!data::dds_sample_to_mcap_message(sample, info, *current_message)) {
                // TODO: log error
            }
            current_message->channelId = mcap_data_channel_id;

            // Lock mutex, write data
            auto writer_lock = mcap_writer.lock();
            if (!mcap_writer->write(*current_message).ok()) {
                // TODO: log error
            }
        }
        else if (store_sample_info) {
            if (!info::dds_info_to_mcap_message(info, *current_message)) {
                // TODO: log error
            }
            current_message->channelId = mcap_info_channel_id;

            // Lock mutex, write data
            auto writer_lock = mcap_writer.lock();
            if (!mcap_writer->write(*current_message).ok()) {
                // TODO: log error
            }
        }
    }
}


} // namespace rti::recording::storage::mcap_plugin