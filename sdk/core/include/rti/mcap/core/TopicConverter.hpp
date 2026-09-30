#ifndef RTI_MCAP_TOPIC_CONVERTER_HPP_
#define RTI_MCAP_TOPIC_CONVERTER_HPP_

#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <dds/core/xtypes/DynamicData.hpp>
#include <dds/topic/TopicTraits.hpp>
#include <mcap/types.hpp>
#include <rti/core/xtypes/DynamicTypePrintFormat.hpp>
#include <rti/topic/TopicTraits.hpp>
#include <rti/topic/cdr.hpp>

#include "rti/mcap/core/SampleMetadata.hpp"

namespace rti::mcap::core {

struct TopicDescriptor {
    std::string topic_name;
    std::string type_name;
    std::string idl;
    std::string dds_xml;
    int32_t domain_id = 0;

    ::mcap::Schema schema() const
    {
        return ::mcap::Schema(type_name, "omgidl", idl);
    }

    ::mcap::Channel channel(const ::mcap::SchemaId schema_id) const
    {
        ::mcap::Channel result(topic_name, "cdr", schema_id);
        result.metadata["dds.topic_name"] = topic_name;
        result.metadata["dds.type_name"] = type_name;
        result.metadata["dds.domain_id"] = std::to_string(domain_id);
        result.metadata["dds.xml_type"] = dds_xml;
        return result;
    }
};

struct MessageView {
    ::mcap::Message message;
    SampleMetadata metadata;
};

template <typename TopicType = dds::core::xtypes::DynamicData>
class TopicConverter {
public:
    static constexpr bool dynamic_data =
            std::is_same_v<TopicType, dds::core::xtypes::DynamicData>;

    explicit TopicConverter(TopicDescriptor descriptor)
            : descriptor_(std::move(descriptor))
    {
    }

    const TopicDescriptor& descriptor() const noexcept
    {
        return descriptor_;
    }

    MessageView to_message(
            ::mcap::ChannelId channel_id,
            uint32_t sequence,
            const TopicType& sample,
            const dds::sub::SampleInfo& info,
            std::vector<char>& cdr_scratch) const
    {
        return to_message(
                channel_id, sequence, sample, to_sample_metadata(info), cdr_scratch);
    }

    MessageView to_message(
            ::mcap::ChannelId channel_id,
            uint32_t sequence,
            const TopicType& sample,
            const SampleMetadata& metadata,
            std::vector<char>& cdr_scratch) const
    {
        auto view = serialized_view(sample, cdr_scratch);
        MessageView result;
        result.message.channelId = channel_id;
        result.message.sequence = sequence;
        result.metadata = metadata;
        result.message.logTime = timestamp_to_mcap(result.metadata.reception_timestamp);
        result.message.publishTime = result.metadata.source_timestamp.valid
                ? timestamp_to_mcap(result.metadata.source_timestamp)
                : result.message.logTime;
        result.message.data = reinterpret_cast<const std::byte*>(view.first);
        result.message.dataSize = view.second;
        return result;
    }

    // DynamicData borrows message.data until it is destroyed or reassociated.
    // The caller must retain the bytes; advancing an MCAP iterator invalidates its view.
    void from_message(
            const ::mcap::Message& message,
            TopicType& sample,
            std::vector<char>& cdr_scratch) const
    {
        if (message.dataSize > std::numeric_limits<uint32_t>::max()) {
            throw std::length_error("MCAP message exceeds the Connext CDR buffer size limit");
        }
        const auto* bytes = reinterpret_cast<const char*>(message.data);
        const auto length = static_cast<uint32_t>(message.dataSize);
        if (length != 0 && bytes == nullptr) {
            throw std::invalid_argument("MCAP message has a null payload");
        }

        if constexpr (dynamic_data) {
            sample.set_cdr_buffer(bytes, length);
        } else {
            cdr_scratch.resize(length);
            if (length != 0) {
                std::memcpy(cdr_scratch.data(), bytes, length);
            }
            rti::topic::from_cdr_buffer_no_alloc(sample, cdr_scratch);
        }
    }

    static TopicDescriptor describe(
            std::string topic_name,
            const dds::core::xtypes::DynamicType& type,
            int32_t domain_id = 0)
    {
        const static rti::core::xtypes::DynamicTypePrintFormatProperty idl_format(
                0, false, rti::core::xtypes::DynamicTypePrintKind::idl, true);
        const static rti::core::xtypes::DynamicTypePrintFormatProperty xml_format(
                0, false, rti::core::xtypes::DynamicTypePrintKind::xml, true);

        TopicDescriptor result;
        result.topic_name = std::move(topic_name);
        result.type_name = type.name();
        result.idl = rti::core::xtypes::to_string(type, idl_format);
        result.dds_xml = rti::core::xtypes::to_string(type, xml_format);
        result.domain_id = domain_id;
        return result;
    }

    template <
            typename T = TopicType,
            std::enable_if_t<
                    !std::is_same_v<T, dds::core::xtypes::DynamicData>,
                    int> = 0>
    static TopicDescriptor describe(std::string topic_name, int32_t domain_id = 0)
    {
        return describe(
                std::move(topic_name),
                rti::topic::dynamic_type<T>::get(),
                domain_id);
    }

private:
    TopicDescriptor descriptor_;

    static std::pair<const char*, uint32_t> serialized_view(
            const TopicType& sample,
            std::vector<char>& cdr_scratch)
    {
        if constexpr (dynamic_data) {
            if (sample.is_cdr()) {
                return sample.get_cdr_buffer();
            }
        }

        const auto& serialized = rti::topic::to_cdr_buffer(cdr_scratch, sample);
        if (serialized.size() > std::numeric_limits<uint32_t>::max()) {
            throw std::length_error("Serialized DDS sample exceeds the MCAP message size limit");
        }
        return {serialized.data(), static_cast<uint32_t>(serialized.size())};
    }
};

}  // namespace rti::mcap::core

#endif
