#include "rti/recording/storage/mcap_plugin/McapStorageReader.hpp"

namespace rti::recording::storage::mcap_plugin {

RTI_RECORDING_STORAGE_READER_CREATE_DEF(McapStorageReader)

McapStorageReader::McapStorageReader(const rti::routing::PropertySet &properties)
        : rti::recording::storage::StorageReader(properties),
          mcap_reader() {
    
    rti::routing::PropertySet::const_iterator property_found;

    property_found = properties.find(property::MCAP_FILEPATH);
    if (property_found != properties.end()) {
        auto mcap_filepath = property_found->second;

        // Lock mutex, open MCAP file
        auto reader_lock = mcap_reader.lock();
        if (!mcap_reader->open(mcap_filepath).ok()) {
            // TODO: log error
            throw std::runtime_error("Failed to open MCAP file");
        }
    } else {
        // TODO: log error
        throw std::runtime_error("Failed to get MCAP file name from properties");
    }

    // Lock mutex, quickly read messages to discover channels
    {
        auto reader_lock = mcap_reader.lock();
        if (!mcap_reader->readSummary(mcap::ReadSummaryMethod::AllowFallbackScan).ok()) {
            // TODO: log error
            throw std::runtime_error("Failed to read MCAP file summary");
        }
    }
}

McapStorageReader::~McapStorageReader() {
    // Lock mutex, close MCAP file
    auto reader_lock = mcap_reader.lock();
    mcap_reader->close();
}

rti::recording::storage::StorageStreamReader * McapStorageReader::create_stream_reader(
            const rti::routing::StreamInfo &stream_info,
            const rti::routing::PropertySet &properties) {
    rti::routing::PropertySet::const_iterator property_found;

    auto sample_order = sample_order::SampleOrderKind::RECEPTION_TIMESTAMP;
    property_found = properties.find("rti.recording_service.playback_settings.sample_order");
    if (property_found != properties.end()) {
        auto sample_order_str = property_found->second;
        sample_order = sample_order::sample_order_from_string(sample_order_str);
    } else {
        // TODO: log info
    }

    auto type = stream_info.type_info().dynamic_type();
    return new McapStreamReader(mcap_reader, stream_info.stream_name(), type, sample_order);
}

void McapStorageReader::delete_stream_reader(
            rti::recording::storage::StorageStreamReader *stream_reader) {
    delete stream_reader;
}

rti::recording::storage::StorageStreamInfoReader * McapStorageReader::create_stream_info_reader(
        const rti::routing::PropertySet &properties) {
    rti::routing::PropertySet::const_iterator property_found;
    
    auto sample_order = sample_order::SampleOrderKind::RECEPTION_TIMESTAMP;
    property_found = properties.find("rti.recording_service.playback_settings.sample_order");
    if (property_found != properties.end()) {
        auto sample_order_str = property_found->second;
        sample_order = sample_order::sample_order_from_string(sample_order_str);
    } else {
        // TODO: log info
    }
    return new McapStreamInfoReader(mcap_reader, sample_order);
}

void McapStorageReader::delete_stream_info_reader(
        rti::recording::storage::StorageStreamInfoReader *stream_info_reader) {
    delete stream_info_reader;
}

} // namespace rti::recording::storage::mcap_plugin