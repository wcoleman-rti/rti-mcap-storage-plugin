#ifndef RTI_STORAGEPLUGIN_MCAPDEFS_HPP_
#define RTI_STORAGEPLUGIN_MCAPDEFS_HPP_

// RTI XML plugin property names
#define RTI_XML_PROPERTY__DATA_FILENAME               "rti.recording.storage.mcap.data_file"
#define RTI_XML_PROPERTY__INFO_FILENAME               "rti.recording.storage.mcap.info_file"
#define RTI_XML_PROPERTY__LOGGING_LOG_LEVEL           "rti.recording.storage.mcap.logging.log_level"
#define RTI_XML_PROPERTY__LOGGING_OUTPUT_FILE         "rti.recording.storage.mcap.logging.output_file"
#define RTI_XML_PROPERTY__COMPRESSION_KIND            "rti.recording.storage.mcap.compression.kind"  // None, Lz4, Zstd
#define RTI_XML_PROPERTY__COMPRESSION_LEVEL           "rti.recording.storage.mcap.compression.level" // Fastest(0), Fast(1), Default(2), Slow(3), Slowest(4)

// MCAP Channel Metadata keys
#define MCAP_CHANNEL_METADATA__DATA_CHANNEL_TOPIC     "mcap.data_channel.topic"
#define MCAP_CHANNEL_METADATA__INFO_CHANNEL_TOPIC     "mcap.info_channel.topic"
#define MCAP_CHANNEL_METADATA__INFO_CHANNEL_FLAG      "mcap.info_channel.flag"
#define MCAP_CHANNEL_METADATA__DDS_TOPIC_NAME         "dds.topic_name"
#define MCAP_CHANNEL_METADATA__DDS_XML_TYPE           "dds.xml_type"
#define MCAP_CHANNEL_METADATA__DDS_DOMAIN_ID          "dds.domain_id"
#define MCAP_FILE_METADATA__RECORDING_TIMES           "dds.recording_times"
#define MCAP_FILE_METADATA__RECORDING_START_TIME      "dds.recording_start_time"
#define MCAP_FILE_METADATA__RECORDING_END_TIME        "dds.recording_end_time"

// MCAP Channel Topic Naming Convention
#define MCAP_CHANNEL_TOPIC_INFO_PREFIX      "##SampleInfo"
#define MCAP_CHANNEL_INSTANCE_POSTFIX       "@"


#endif // RTI_STORAGEPLUGIN_MCAPDEFS_HPP_