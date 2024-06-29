#include "rti/recording/storage/mcap_plugin/common/info.hpp"
#include "rti/recording/storage/mcap_plugin/common/data.hpp"
#include "rti/recording/storage/mcap_plugin/common/idl/dds_rtf2_dcpsPlugin.hpp"

namespace rti::recording::storage::mcap_plugin::info {

    InfoType to_info_sample(const dds::sub::SampleInfo &info) {
        const auto & native_info = info->native();
        InfoType info_sample;

        info_sample.sample_state(native_info.sample_state);
        info_sample.view_state(native_info.view_state);
        info_sample.instance_state(native_info.instance_state);

        info_sample.source_timestamp().sec(native_info.source_timestamp.sec);
        info_sample.source_timestamp().nanosec(native_info.source_timestamp.nanosec);
        info_sample.reception_timestamp().sec(native_info.reception_timestamp.sec);
        info_sample.reception_timestamp().nanosec(native_info.reception_timestamp.nanosec);

        info_sample.publication_sequence_number().high(native_info.publication_sequence_number.high);
        info_sample.publication_sequence_number().low(native_info.publication_sequence_number.low);
        info_sample.reception_sequence_number().high(native_info.reception_sequence_number.high);
        info_sample.reception_sequence_number().low(native_info.reception_sequence_number.low);

        std::copy(std::begin(native_info.instance_handle.keyHash.value),
                  std::end(native_info.instance_handle.keyHash.value),
                  info_sample.instance_handle().value().begin());
        info_sample.instance_handle().length(native_info.instance_handle.keyHash.length);
        info_sample.instance_handle().isValid(native_info.instance_handle.isValid);

        std::copy(std::begin(native_info.publication_handle.keyHash.value),
                  std::end(native_info.publication_handle.keyHash.value),
                  info_sample.publication_handle().value().begin());
        info_sample.publication_handle().length(native_info.publication_handle.keyHash.length);
        info_sample.publication_handle().isValid(native_info.publication_handle.isValid);
        
        info_sample.disposed_generation_count(native_info.disposed_generation_count);
        info_sample.no_writers_generation_count(native_info.no_writers_generation_count);
        info_sample.sample_rank(native_info.sample_rank);
        info_sample.generation_rank(native_info.generation_rank);
        info_sample.absolute_generation_rank(native_info.absolute_generation_rank);
        info_sample.valid_data(native_info.valid_data);

        std::copy(std::begin(native_info.publication_virtual_guid.value),
                  std::end(native_info.publication_virtual_guid.value),
                  info_sample.publication_virtual_guid().value().begin());
        info_sample.publication_virtual_sequence_number().high(native_info.publication_virtual_sequence_number.high);
        info_sample.publication_virtual_sequence_number().low(native_info.publication_virtual_sequence_number.low);

        std::copy(std::begin(native_info.original_publication_virtual_guid.value),
                  std::end(native_info.original_publication_virtual_guid.value),
                  info_sample.original_publication_virtual_guid().value().begin());
        info_sample.original_publication_virtual_sequence_number().high(native_info.original_publication_virtual_sequence_number.high);
        info_sample.original_publication_virtual_sequence_number().low(native_info.original_publication_virtual_sequence_number.low);

        return info_sample;
    };

    dds::sub::SampleInfo from_info_sample(const InfoType & info_sample) {
        DDS_SampleInfo native_info = DDS_SAMPLEINFO_DEFAULT;
        
        native_info.sample_state = DDS_SampleStateKind(info_sample.sample_state());
        native_info.view_state = DDS_ViewStateKind(info_sample.view_state());
        native_info.instance_state = DDS_InstanceStateKind(info_sample.instance_state());

        native_info.source_timestamp.sec = info_sample.source_timestamp().sec();
        native_info.source_timestamp.nanosec = info_sample.source_timestamp().nanosec();
        native_info.reception_timestamp.sec = info_sample.reception_timestamp().sec();
        native_info.reception_timestamp.nanosec = info_sample.reception_timestamp().nanosec();

        native_info.publication_sequence_number.high = info_sample.publication_sequence_number().high();
        native_info.publication_sequence_number.low = info_sample.publication_sequence_number().low();
        native_info.reception_sequence_number.high = info_sample.reception_sequence_number().high();
        native_info.reception_sequence_number.low = info_sample.reception_sequence_number().low();

        std::copy(info_sample.instance_handle().value().begin(), 
                  info_sample.instance_handle().value().end(), 
                  native_info.instance_handle.keyHash.value);
        // native_info.instance_handle.keyHash.value = *(info_sample.instance_handle().value().data());
        native_info.instance_handle.keyHash.length = info_sample.instance_handle().length();
        native_info.instance_handle.isValid = info_sample.instance_handle().isValid();

        std::copy(info_sample.publication_handle().value().begin(), 
                  info_sample.publication_handle().value().end(), 
                  native_info.publication_handle.keyHash.value);
        // native_info.publication_handle.keyHash.value = *(info_sample.publication_handle().value().data());
        native_info.publication_handle.keyHash.length = info_sample.publication_handle().length();
        native_info.publication_handle.isValid = info_sample.publication_handle().isValid();
        
        native_info.disposed_generation_count = info_sample.disposed_generation_count();
        native_info.no_writers_generation_count = info_sample.no_writers_generation_count();
        native_info.sample_rank = info_sample.sample_rank();
        native_info.generation_rank = info_sample.generation_rank();
        native_info.absolute_generation_rank = info_sample.absolute_generation_rank();
        native_info.valid_data = info_sample.valid_data();

        
        std::copy(info_sample.publication_virtual_guid().value().begin(), 
                  info_sample.publication_virtual_guid().value().end(), 
                  native_info.publication_virtual_guid.value);
        // *native_info.publication_virtual_guid.value = info_sample.publication_virtual_guid().value().data();
        native_info.publication_virtual_sequence_number.high = info_sample.publication_virtual_sequence_number().high();
        native_info.publication_virtual_sequence_number.low = info_sample.publication_virtual_sequence_number().low();

        std::copy(info_sample.original_publication_virtual_guid().value().begin(), 
                  info_sample.original_publication_virtual_guid().value().end(), 
                  native_info.original_publication_virtual_guid.value);
        // *native_info.original_publication_virtual_guid.value = info_sample.original_publication_virtual_guid().value().data();
        native_info.original_publication_virtual_sequence_number.high = info_sample.original_publication_virtual_sequence_number().high();
        native_info.original_publication_virtual_sequence_number.low = info_sample.original_publication_virtual_sequence_number().low();

        dds::sub::SampleInfo info;
        info->native(native_info);
        return info;
    };

    bool info_to_mcap(const dds::sub::SampleInfo & info, mcap::Message & mcap_message) {
        const auto & dds_sample = to_info_sample(info);
        std::vector<char> buffer;

        // First get the length of the buffer
        {
            auto representation = dds::core::policy::DataRepresentation::auto_id();

            // First get the length of the buffer
            unsigned int length = 0;
            RTIBool ok = DDSMonitoring::SampleInfoPlugin_serialize_to_cdr_buffer(
                NULL, 
                &length,
                &dds_sample,
                representation);
            ::rti::core::check_return_code(
                ok ? DDS_RETCODE_OK : DDS_RETCODE_ERROR,
                "Failed to calculate cdr buffer size");

            // Create a vector with that size and copy the cdr buffer into it
            buffer.resize(length);
            ok = DDSMonitoring::SampleInfoPlugin_serialize_to_cdr_buffer(
                &buffer[0], 
                &length, 
                &dds_sample,
                representation);
            rti::core::check_return_code(
                ok ? DDS_RETCODE_OK : DDS_RETCODE_ERROR,
                "Failed to copy cdr buffer");
        }

        mcap_message.dataSize = rti::recording::storage::mcap_plugin::data::convert(buffer, const_cast<std::byte*&>(mcap_message.data), mcap_message.dataSize);
        mcap_message.publishTime = rti::recording::storage::mcap_plugin::data::convert_timestamp(info->source_timestamp());
        mcap_message.logTime = rti::recording::storage::mcap_plugin::data::convert_timestamp(info->reception_timestamp());

        return true;
    };

    bool mcap_to_info(const mcap::Message & mcap_message, dds::sub::SampleInfo & info) {

        std::vector<char> buffer = rti::recording::storage::mcap_plugin::data::convert(mcap_message.data, mcap_message.dataSize);
        InfoType info_sample;

        // First get the length of the buffer
        {
            RTIBool ok  = DDSMonitoring::SampleInfoPlugin_deserialize_from_cdr_buffer(
                &info_sample, 
                &buffer[0], 
                static_cast<unsigned int>(buffer.size()));
            rti::core::check_return_code(ok ? DDS_RETCODE_OK : DDS_RETCODE_ERROR,
            "Failed to create ::DDSMonitoring::SampleInfo from cdr buffer");
        }

        info = from_info_sample(info_sample);
        return true;
    };

};  // rti::recording::storage::mcap_plugin::info