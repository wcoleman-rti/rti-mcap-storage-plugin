#include "rti/recording/storage/mcap_plugin/common/type.hpp"
#include "mcap/mcap_impl.hpp"

namespace rti::recording::storage::mcap_plugin::type {

    inline const std::string fq_xml(std::string content) {
        return "<dds><types>" + content + "</types></dds>";
    }

    inline const std::string uri_string_profile(std::string content) {
        return "str://\"" + content + "\"";
    }

    std::optional<dds::core::xtypes::DynamicType> from_qos_provider(std::string type_name, dds::core::QosProvider qos_provider) {
        try {
            auto type = qos_provider->type(type_name);
            return type;
        } catch (dds::core::Exception &e) {
            return std::nullopt;
        }
    }

    std::optional<dds::core::xtypes::DynamicType> from_xml_string(std::string schema_name, std::string schema_data) {
        std::string stringProfile = uri_string_profile(fq_xml(schema_data));
        dds::core::QosProvider qos_provider(stringProfile);
        return from_qos_provider(schema_name, qos_provider);
    }

    std::unique_ptr<dds::core::xtypes::DynamicType> get_channel_type(const mcap::Channel & mcap_channel, const mcap::Schema & mcap_schema) {
        auto found_xml = mcap_channel.metadata.find(mcap::channel::METADATA_KEY_DDSXML_TYPE);
        if (found_xml == mcap_channel.metadata.end()) {
            // TODO: log error
            return nullptr;
        }
        auto xml = found_xml->second;
        auto found_type = from_xml_string(mcap_schema.name, xml);

        if (!found_type) {
            // TODO: log error
            return nullptr;
        }

        return std::make_unique<dds::core::xtypes::DynamicType>(*found_type);
    }

    void store_channel_type(mcap::Channel & mcap_channel, mcap::Schema & mcap_schema, const dds::core::xtypes::DynamicType & type) {
        const auto & idl_type = rti::core::xtypes::to_string(type, IDL_PRINT_FORMAT);
        const auto & xml_type = rti::core::xtypes::to_string(type, XML_PRINT_FORMAT);
        mcap_schema.data = std::vector<std::byte>(
            reinterpret_cast<const std::byte*>(idl_type.data()),
            reinterpret_cast<const std::byte*>(idl_type.data() + idl_type.size()));
        mcap_channel.metadata[mcap::channel::METADATA_KEY_DDSXML_TYPE] = xml_type;
    }

}  // rti::recording::storage::mcap_plugin::type