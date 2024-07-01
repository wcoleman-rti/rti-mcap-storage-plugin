#ifndef DDS_MCAP_STORAGE_COMMON_DATA_H
#define DDS_MCAP_STORAGE_COMMON_DATA_H

#include <cstring>
#include <dds/core/Time.hpp>
#include <dds/sub/SampleInfo.hpp>
#include <dds/core/xtypes/DynamicData.hpp>
#include <mcap/types.hpp>

namespace rti::recording::storage::mcap_plugin::data {

    dds::core::Time convert_timestamp(mcap::Timestamp timestamp);
    mcap::Timestamp convert_timestamp(dds::core::Time timestamp);

    uint64_t convert(const std::vector<char>& input_data, std::byte*& output_data, uint64_t& output_size);
    std::vector<char> convert(const std::byte* input_data, uint64_t data_size);

    // template <typename TopicType>
    // bool dds_to_mcap(const TopicType & dds_sample, const dds::sub::SampleInfo & dds_info, mcap::Message & mcap_message);
    bool dds_to_mcap(const dds::core::xtypes::DynamicData & dds_sample, const dds::sub::SampleInfo & dds_info, mcap::Message & mcap_message);

    // template <typename TopicType>
    // bool mcap_to_dds(const mcap::Message & mcap_message, TopicType & dds_sample, dds::sub::SampleInfo & dds_info);
    bool mcap_to_dds(const mcap::Message & mcap_message, dds::core::xtypes::DynamicData & dds_sample, dds::sub::SampleInfo & dds_info);

}  // rti::recording::storage::mcap_plugin::data

#include "rti/recording/storage/mcap_plugin/common/data.cxx"

#endif // DDS_MCAP_STORAGE_COMMON_DATA_H