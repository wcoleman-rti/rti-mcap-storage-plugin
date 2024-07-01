#ifndef DDS_MCAP_STORAGE_COMMON_DATA_H
#define DDS_MCAP_STORAGE_COMMON_DATA_H

#include <cstring>
#include <dds/core/Time.hpp>
#include <dds/sub/SampleInfo.hpp>
#include <dds/core/xtypes/DynamicData.hpp>
#include <mcap/types.hpp>

namespace rti::recording::storage::mcap_plugin::data {

    /**
     * @brief Convert from MCAP timestamp to DDS timestamp
     * @param timestamp input MCAP timestamp
     * @return DDS timestamp
    */
    dds::core::Time convert_timestamp(mcap::Timestamp timestamp);

    /**
     * @brief Convert from DDS timestamp to MCAP timestamp
     * @param timestamp input DDS timestamp
     * @return MCAP timestamp
    */
    mcap::Timestamp convert_timestamp(dds::core::Time timestamp);

    /**
     * @brief Convert raw buffer of char vector to byte pointer and length
     * @param input_data input vector of chars
     * @param output_data output pointer of bytes
     * @param output_size output length of byte array
     * @return output length of byte array
     */
    uint64_t convert_buffer(const std::vector<char>& input_data, std::byte*& output_data, uint64_t& output_size);

    /**
     * @brief Convert raw buffer of byte pointer and length to char vector
     * @param input_data input pointer of bytes
     * @param data_size input length of byte array
     * @return output vector of chars
     */
    std::vector<char> convert_buffer(const std::byte* input_data, uint64_t data_size);

    /**
     * @brief Convert DDS Sample and SampleInfo to MCAP message
     * @tparam TopicType 
     * @param dds_sample input DDS Sample object
     * @param dds_info input DDS SampleInfo object
     * @param mcap_message output MCAP message object: @ref https://mcap.dev/docs/cpp/r052958A9641F3ABE
     * @return true on success
     */
    template <typename TopicType>
    bool dds_sample_to_mcap_message(const TopicType & dds_sample, const dds::sub::SampleInfo & dds_info, mcap::Message & mcap_message);

    /**
     * @brief Convert DDS Sample and SampleInfo to MCAP message
     * @param dds_sample input DDS DynamicData Sample object
     * @param dds_info input DDS SampleInfo object
     * @param mcap_message output MCAP message object: @ref https://mcap.dev/docs/cpp/r052958A9641F3ABE
     * @return true on success
     */
    bool dds_sample_to_mcap_message(const dds::core::xtypes::DynamicData & dds_sample, const dds::sub::SampleInfo & dds_info, mcap::Message & mcap_message);

    /**
     * @brief Convert DDS Sample and SampleInfo to MCAP message
     * @tparam TopicType 
     * @param mcap_message input MCAP message object: @ref https://mcap.dev/docs/cpp/r052958A9641F3ABE
     * @param dds_sample output DDS Sample object
     * @param dds_info output DDS SampleInfo object
     * @return true on success
     */
    template <typename TopicType>
    bool mcap_message_to_dds_sample(const mcap::Message & mcap_message, TopicType & dds_sample, dds::sub::SampleInfo & dds_info);
    
    /**
     * @brief Convert DDS Sample and SampleInfo to MCAP message
     * @param mcap_message input MCAP message object: @ref https://mcap.dev/docs/cpp/r052958A9641F3ABE
     * @param dds_sample output DDS DynamicData Sample object
     * @param dds_info output DDS SampleInfo object
     * @return true on success
     */
    bool mcap_message_to_dds_sample(const mcap::Message & mcap_message, dds::core::xtypes::DynamicData & dds_sample, dds::sub::SampleInfo & dds_info);

}  // rti::recording::storage::mcap_plugin::data

// #include "rti/recording/storage/mcap_plugin/common/data.cxx"

#endif // DDS_MCAP_STORAGE_COMMON_DATA_H