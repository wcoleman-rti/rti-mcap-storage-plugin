#include "rti/recording/storage/mcap_plugin/common/data.hpp"
#include "rti/recording/storage/mcap_plugin/common/util.hpp"

#define NANOSECS_PER_SEC 1000000000ll

namespace rti::recording::storage::mcap_plugin::data {

    dds::core::Time convert_timestamp(mcap::Timestamp timestamp) {
        int64_t secs = rti::recording::storage::mcap_plugin::util::safe_cast<int64_t>(timestamp / NANOSECS_PER_SEC);
        uint32_t nanosecs = rti::recording::storage::mcap_plugin::util::safe_cast<uint32_t>(timestamp % NANOSECS_PER_SEC);
        return dds::core::Time(secs, nanosecs);
    }

    mcap::Timestamp convert_timestamp(dds::core::Time timestamp) {
        uint64_t nanosecs;
        try {
            nanosecs = timestamp.to_nanosecs();
        } catch (std::overflow_error & e) {
            nanosecs = std::numeric_limits<uint64_t>::max();
        }
        return nanosecs;
    }

    uint64_t convert_buffer(const std::vector<char>& input_data, std::byte*& output_data, uint64_t& /*output_size*/) {
        uint64_t data_size = input_data.size();
        if (output_data != nullptr) {
            delete[] output_data;
        }
        output_data = new std::byte[data_size];
        // Check if output_data needs to be reallocated
        /* if (output_size < data_size) {
            delete[] output_data; // Free existing memory if any
            output_data = new std::byte[data_size]; // Allocate new memory
            output_size = data_size; // Update output_size to reflect the new size
        } */
        std::memcpy(output_data, reinterpret_cast<const std::byte*>(input_data.data()), data_size);
        return data_size;
    }

    std::vector<char> convert_buffer(const std::byte* input_data, uint64_t data_size) {
        std::vector<char> output_data(data_size);
        std::memcpy(output_data.data(), input_data, data_size);
        return output_data;
    }

    template <typename TopicType>
    bool dds_sample_to_mcap_message(const TopicType & dds_sample, const dds::sub::SampleInfo & dds_info, mcap::Message & mcap_message)
    {
        std::vector<char> buffer = rti::topic::to_cdr_buffer<TopicType>(buffer, dds_sample);
        mcap_message.dataSize = convert_buffer(buffer, const_cast<std::byte*&>(mcap_message.data), mcap_message.dataSize);
        mcap_message.publishTime = convert_timestamp(dds_info->source_timestamp());
        mcap_message.logTime = convert_timestamp(dds_info->reception_timestamp());

        return true;
    }

    bool dds_sample_to_mcap_message(const dds::core::xtypes::DynamicData & dds_sample, const dds::sub::SampleInfo & dds_info, mcap::Message & mcap_message)
    {
        std::vector<char> buffer;

#if (RTI_DDS_VERSION_MAJOR > 7) || (RTI_DDS_VERSION_MAJOR == 7 && RTI_DDS_VERSION_MINOR >= 3)
            
        if (dds_sample.is_cdr()) {
            // directly copy serialized dds sample buffer if possible
            auto cdr_buffer = dds_sample.get_cdr_buffer();
            std::copy(reinterpret_cast<const char*>(cdr_buffer.first),
                reinterpret_cast<const char*>(cdr_buffer.first) + cdr_buffer.second,
                buffer.begin());
        } else {
            // dds_sample_to_mcap_message dds sample if necessary
            buffer = rti::core::xtypes::to_cdr_buffer(buffer, dds_sample);
        }
#else
        // dds_sample_to_mcap_message dds sample
        buffer = rti::core::xtypes::to_cdr_buffer(buffer, dds_sample);
#endif // RTI_DDS_VERSION >= 7.3.0

        mcap_message.dataSize = convert_buffer(buffer, const_cast<std::byte*&>(mcap_message.data), mcap_message.dataSize);
        mcap_message.publishTime = convert_timestamp(dds_info->source_timestamp());
        mcap_message.logTime = convert_timestamp(dds_info->reception_timestamp());
        mcap_message.sequence = rti::recording::storage::mcap_plugin::util::safe_cast<uint32_t>(dds_info->reception_sequence_number().value());

        return true;
    }

    template <typename TopicType>
    bool mcap_message_to_dds_sample(const mcap::Message & mcap_message, TopicType & dds_sample, dds::sub::SampleInfo & dds_info)
    {
        // Populate DDS SampleInfo
        DDS_SampleInfo native_sample_info = DDS_SAMPLEINFO_DEFAULT;
        native_sample_info.valid_data = DDS_BOOLEAN_TRUE;
        auto reception_timestamp = convert_timestamp(mcap_message.logTime);
        auto source_timestamp = convert_timestamp(mcap_message.publishTime);
        native_sample_info.reception_timestamp.sec = reception_timestamp.sec();
        native_sample_info.reception_timestamp.nanosec = reception_timestamp.nanosec();
        native_sample_info.source_timestamp.sec = source_timestamp.sec();
        native_sample_info.source_timestamp.nanosec = source_timestamp.nanosec();
        dds_info->native(native_sample_info);

        std::vector<char> buffer = convert_buffer(mcap_message.data, mcap_message.dataSize);
        rti::topic::from_cdr_buffer<TopicType>(dds_sample, buffer);

        return true;
    }

    bool mcap_message_to_dds_sample(const mcap::Message & mcap_message, dds::core::xtypes::DynamicData & dds_sample, dds::sub::SampleInfo & dds_info)
    {
        // Populate DDS SampleInfo
        DDS_SampleInfo native_sample_info = DDS_SAMPLEINFO_DEFAULT;
        native_sample_info.valid_data = DDS_BOOLEAN_TRUE;
        // native_sample_info.reception_sequence_number.high = rti::recording::storage::mcap_plugin::util::safe_cast<int32_t>(mcap_message.sequence);
        auto reception_timestamp = convert_timestamp(mcap_message.logTime);
        auto source_timestamp = convert_timestamp(mcap_message.publishTime);
        native_sample_info.reception_timestamp.sec = reception_timestamp.sec();
        native_sample_info.reception_timestamp.nanosec = reception_timestamp.nanosec();
        native_sample_info.source_timestamp.sec = source_timestamp.sec();
        native_sample_info.source_timestamp.nanosec = source_timestamp.nanosec();
        dds_info->native(native_sample_info);

        std::vector<char> buffer = convert_buffer(mcap_message.data, mcap_message.dataSize);
#if (RTI_DDS_VERSION_MAJOR > 7) || (RTI_DDS_VERSION_MAJOR == 7 && RTI_DDS_VERSION_MINOR >= 3)
        if (dds_sample.is_cdr()) {
            // directly copy serialized dds sample buffer if possible
            dds_sample.set_cdr_buffer(buffer.data(), buffer.size());
        } else {
            // dds_sample_to_mcap_message from buffer into dds sample if necessary
            rti::core::xtypes::from_cdr_buffer(dds_sample, buffer);
        }
#else
        rti::core::xtypes::from_cdr_buffer(dds_sample, buffer);
#endif // RTI_DDS_VERSION >= 7.3.0

        return true;
    }

}  // rti::recording::storage::mcap_plugin::data