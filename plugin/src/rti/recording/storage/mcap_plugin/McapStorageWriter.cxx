#include "rti/recording/storage/mcap_plugin/McapStorageWriter.hpp"

namespace rti::recording::storage::mcap_plugin {

RTI_RECORDING_STORAGE_WRITER_CREATE_DEF(McapStorageWriter)

McapStorageWriter::McapStorageWriter(const rti::routing::PropertySet &properties) 
        : rti::recording::storage::StorageWriter(properties),
          mcap_writer(),
          store_sample_info(false) {
    
    rti::routing::PropertySet::const_iterator property_found;

    property_found = properties.find(property::MCAP_FILEPATH);
    if (property_found != properties.end()) {
        auto mcap_filepath = property_found->second;

        // Allow the use of indexed reading
        mcap::McapWriterOptions writer_options("");
        writer_options.noChunking = false;
        writer_options.noMessageIndex = false;
        writer_options.noSummary = false;

        // Lock mutex, open MCAP file
        auto writer_lock = mcap_writer.lock();
        if (!mcap_writer->open(mcap_filepath, writer_options).ok()) {
            // TODO: log error
        }
    } else {
        // TODO: log error
        throw std::runtime_error("Failed to get MCAP file name from properties");
    }

    property_found = properties.find(property::STORE_SAMPLE_INFO);
    if (property_found != properties.end()) {
        auto store_sample_info_value = property_found->second;
        if (property::is_true(store_sample_info_value)) {
            store_sample_info = true;
        } else if (property::is_false(store_sample_info_value)) {
            store_sample_info = false;
        } else {
            // TODO: log er+ror
        }
    } else {
        // TODO: log info
    }

    if (store_sample_info) {
        // Lock mutex, add info schema
        auto writer_lock = mcap_writer.lock();
        mcap_writer->addSchema(info::data_schema);
    }
}

McapStorageWriter::~McapStorageWriter() {
    // Lock mutex, close MCAP file
    auto writer_lock = mcap_writer.lock();
    mcap_writer->close();
}

rti::recording::storage::StorageStreamWriter * McapStorageWriter::create_stream_writer(
        const rti::routing::StreamInfo &stream_info,
        const rti::routing::PropertySet &) {
    auto type = stream_info.type_info().dynamic_type();
    return new McapStreamWriter(mcap_writer, stream_info.stream_name(), type, store_sample_info);
}

void McapStorageWriter::delete_stream_writer(
            rti::recording::storage::StorageStreamWriter *stream_writer) {
    delete stream_writer;
}

} // namespace rti::recording::storage::mcap_plugin