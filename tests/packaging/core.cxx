#include <rti/mcap/core/TopicConverter.hpp>

int main()
{
    rti::mcap::core::TopicDescriptor descriptor;
    descriptor.topic_name = "consumer";
    descriptor.type_name = "ConsumerType";
    const auto schema = descriptor.schema();
    const auto channel = descriptor.channel(1);
    if (schema.name != descriptor.type_name || channel.topic != descriptor.topic_name) {
        return 1;
    }
    rti::mcap::core::SampleMetadata metadata;
    std::vector<char> bytes;
    rti::mcap::core::encode_sample_metadata(metadata, bytes);
    const auto restored = rti::mcap::core::decode_sample_metadata(bytes.data(), bytes.size());
    return restored.valid_data ? 1 : 0;
}
