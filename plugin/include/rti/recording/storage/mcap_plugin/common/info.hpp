#ifndef DDS_MCAP_STORAGE_COMMON_INFO_H
#define DDS_MCAP_STORAGE_COMMON_INFO_H

#include <dds/core/xtypes/DynamicData.hpp>
#include <dds/core/xtypes/DynamicType.hpp>
#include <dds/sub/SampleInfo.hpp>
#include <mcap/types.hpp>
#include "rti/recording/storage/mcap_plugin/common/mcap_vendor.hpp"
#include "rti/recording/storage/mcap_plugin/common/type.hpp"
#include "rti/recording/storage/mcap_plugin/common/idl/dds_rtf2_dcps.hpp"

namespace rti::recording::storage::mcap_plugin::info {
    
    using InfoType = DDSMonitoring::SampleInfo;
    inline static const dds::core::xtypes::DynamicType type = rti::topic::dynamic_type<InfoType>::get();
    inline static const std::string type_name = dds::topic::topic_type_name<InfoType>::value();
    inline static const std::string stream_name_prefix = "rti/recording/storage/mcap_plugin/info/";

    inline std::string get_info_stream_name(std::string stream_name) { return stream_name_prefix + stream_name; }

    inline static const auto & idl_type = rti::core::xtypes::to_string(type, rti::recording::storage::mcap_plugin::type::IDL_PRINT_FORMAT);
    inline static const auto & xml_type = rti::core::xtypes::to_string(type, rti::recording::storage::mcap_plugin::type::XML_PRINT_FORMAT);

    inline static mcap::Schema data_schema(
        type.name(),  // schema.name
        mcap::schema::schemaEncodingOmgIdl,  // schema.encoding
        std::vector<std::byte>(  // schema.data
            reinterpret_cast<const std::byte*>(idl_type.data()),
            reinterpret_cast<const std::byte*>(idl_type.data() + idl_type.size())));

    InfoType to_info_sample(const dds::sub::SampleInfo &info);
    dds::sub::SampleInfo from_info_sample(const InfoType & info_sample);

    bool info_to_mcap(const dds::sub::SampleInfo & info, mcap::Message & mcap_message);
    bool mcap_to_info(const mcap::Message & mcap_message, dds::sub::SampleInfo & info);

}  // rti::recording::storage::mcap_plugin::info

#include "rti/recording/storage/mcap_plugin/common/info.cxx"

#endif // DDS_MCAP_STORAGE_COMMON_INFO_H