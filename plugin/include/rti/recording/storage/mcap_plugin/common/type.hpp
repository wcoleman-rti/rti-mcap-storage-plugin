#ifndef DDS_MCAP_STORAGE_COMMON_TYPE_H
#define DDS_MCAP_STORAGE_COMMON_TYPE_H

#include <dds/core/xtypes/DynamicType.hpp>
#include <dds/core/QosProvider.hpp>
#include <mcap/types.hpp>

namespace rti::recording::storage::mcap_plugin::type {

    const static rti::core::xtypes::DynamicTypePrintFormatProperty IDL_PRINT_FORMAT(0, false, rti::core::xtypes::DynamicTypePrintKind::idl, true);
    const static rti::core::xtypes::DynamicTypePrintFormatProperty XML_PRINT_FORMAT(0, false, rti::core::xtypes::DynamicTypePrintKind::xml, true);

    std::optional<dds::core::xtypes::DynamicType> from_qos_provider(std::string type_name, dds::core::QosProvider qos_provider);
    std::optional<dds::core::xtypes::DynamicType> from_xml_string(std::string schema_name, std::string schema_data);

    std::unique_ptr<dds::core::xtypes::DynamicType> get_channel_type(const mcap::Channel & mcap_channel, const mcap::Schema & mcap_schema);
    void store_channel_type(mcap::Channel & mcap_channel, mcap::Schema & mcap_schema, const dds::core::xtypes::DynamicType & type);

};  // rti::recording::storage::mcap_plugin::type

#include "rti/recording/storage/mcap_plugin/common/src/type.cxx"

#endif // DDS_MCAP_STORAGE_COMMON_TYPE_H