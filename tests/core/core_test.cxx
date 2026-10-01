#include "idl/Test.hpp"

#include <stdexcept>
#include <doctest/doctest.h>

#include <dds/core/xtypes/DynamicData.hpp>
#include <rti/core/xtypes/DynamicDataImpl.hpp>
#include <rti/topic/cdr.hpp>

#include "rti/mcap/core/TopicConverter.hpp"

namespace {

void require(bool condition, const char* message)
{
    REQUIRE_MESSAGE(condition, message);
}

}  // namespace

TEST_CASE("Topic conversion and versioned SampleInfo mapping")
{
    Foo sample;
    sample.a(17);
    sample.b(true);
    sample.u32(42);

    std::vector<char> encode_scratch;
    rti::mcap::core::TopicDescriptor descriptor =
            rti::mcap::core::TopicConverter<Foo>::describe("numbers", 7);
    rti::mcap::core::TopicConverter<Foo> converter(descriptor);

    dds::sub::SampleInfo info;
    auto native = DDS_SAMPLEINFO_DEFAULT;
    native.valid_data = DDS_BOOLEAN_TRUE;
    native.source_timestamp = {123, 456};
    native.reception_timestamp = {124, 789};
    native.reception_sequence_number = {1, 2};
    native.instance_handle.keyHash.value[0] = 0x12;
    native.instance_handle.keyHash.length = 16;
    native.instance_handle.isValid = DDS_BOOLEAN_TRUE;
    native.publication_handle.keyHash.value[0] = 0x34;
    native.publication_handle.keyHash.length = 16;
    native.publication_handle.isValid = DDS_BOOLEAN_TRUE;
    native.publication_sequence_number = {3, 4};
    native.publication_virtual_guid.value[0] = 0x56;
    native.publication_virtual_sequence_number = {5, 6};
    native.original_publication_virtual_guid.value[0] = 0x78;
    native.original_publication_virtual_sequence_number = {7, 8};
    native.sample_rank = 9;
    info->native(native);

    auto converted = converter.to_message(1, 1, sample, info, encode_scratch);
    require(converted.message.dataSize != 0, "generated type should serialize");
    require(converted.message.data == reinterpret_cast<const std::byte*>(encode_scratch.data()),
            "generated type serialization must use the reusable scratch buffer");
    require(converted.message.logTime == 124000000789ULL, "reception time should map to log time");
    require(converted.message.publishTime == 123000000456ULL, "source time should map to publish time");

    Foo decoded;
    std::vector<char> decode_scratch;
    converter.from_message(converted.message, decoded, decode_scratch);
    require(decoded.a() == sample.a(), "generated sample key did not round-trip");
    require(decoded.u32() == sample.u32(), "generated sample field did not round-trip");

    const auto metadata = rti::mcap::core::to_sample_metadata(info);
    const auto reconstructed = rti::mcap::core::to_sample_info(metadata);
    require(reconstructed.valid(), "valid_data should round-trip");
    require(reconstructed.source_timestamp() == info.source_timestamp(),
            "source timestamp should round-trip through clean metadata");
    require(reconstructed->native().instance_handle.keyHash.value[0] == 0x12 &&
                    reconstructed->native().instance_handle.keyHash.length == 16 &&
                    reconstructed->native().instance_handle.isValid == DDS_BOOLEAN_TRUE,
            "instance handle should round-trip through clean metadata");
    require(reconstructed->native().publication_handle.keyHash.value[0] == 0x34 &&
                    reconstructed->native().publication_handle.keyHash.length == 16 &&
                    reconstructed->native().publication_handle.isValid == DDS_BOOLEAN_TRUE,
            "publication handle should round-trip through clean metadata");

    std::vector<char> encoded_metadata;
    rti::mcap::core::encode_sample_metadata(metadata, encoded_metadata);
    CHECK(encoded_metadata.size() == 177);
    CHECK(std::string(encoded_metadata.data(), 4) == "DDSM");
    CHECK(encoded_metadata[4] == 1);
    CHECK(encoded_metadata[5] == 0);
    const auto decoded_metadata = rti::mcap::core::decode_sample_metadata(
            encoded_metadata.data(), encoded_metadata.size());
    const auto decoded_info = rti::mcap::core::to_sample_info(decoded_metadata);
    require(decoded_info.source_timestamp() == info.source_timestamp(),
            "source timestamp should round-trip through metadata encoding");
    require(
            decoded_info->native().reception_sequence_number.high ==
                            info->native().reception_sequence_number.high &&
                    decoded_info->native().reception_sequence_number.low ==
                            info->native().reception_sequence_number.low,
            "full reception sequence number should round-trip through metadata encoding");
    require(decoded_info->native().publication_virtual_guid.value[0] == 0x56 &&
                    decoded_info->native().original_publication_virtual_guid.value[0] == 0x78,
            "virtual identities should round-trip through metadata encoding");
    require(decoded_info->native().sample_rank == 9,
            "sample rank should round-trip through metadata encoding");
    bool rejected_truncated_metadata = false;
    try {
        (void) rti::mcap::core::decode_sample_metadata(
                encoded_metadata.data(), encoded_metadata.size() - 1);
    } catch (const std::invalid_argument&) {
        rejected_truncated_metadata = true;
    }

    require(rejected_truncated_metadata, "truncated metadata should be rejected");
    auto unsupported_metadata = encoded_metadata;
    unsupported_metadata[4] = 2;
    bool rejected_unsupported_version = false;
    try {
        (void) rti::mcap::core::decode_sample_metadata(
                unsupported_metadata.data(), unsupported_metadata.size());
    } catch (const std::invalid_argument&) {
        rejected_unsupported_version = true;
    }
    require(rejected_unsupported_version, "unsupported metadata versions should be rejected");

    auto invalid_time_info = info;
    native.source_timestamp = DDS_TIME_INVALID;
    invalid_time_info->native(native);
    require(
            !rti::mcap::core::to_sample_metadata(invalid_time_info).source_timestamp.valid,
            "the DDS invalid timestamp sentinel should remain invalid");

    rti::mcap::core::TopicConverter<> dynamic_converter(descriptor);
    dds::core::xtypes::DynamicData dynamic_sample(
            rti::topic::dynamic_type<Foo>::get());
    dynamic_sample.set_cdr_buffer(
            encode_scratch.data(),
            static_cast<uint32_t>(encode_scratch.size()));
    auto dynamic_message = dynamic_converter.to_message(
            1, 2, dynamic_sample, info, encode_scratch);
    require(dynamic_sample.is_cdr(), "DynamicData from serialization should be CDR-backed");
    const auto cdr_view = dynamic_sample.get_cdr_buffer();
    require(dynamic_message.message.data == reinterpret_cast<const std::byte*>(cdr_view.first),
            "CDR-backed DynamicData should be passed through without a payload copy");

    dds::core::xtypes::DynamicData restored(
            rti::topic::dynamic_type<Foo>::get());
    dynamic_converter.from_message(dynamic_message.message, restored, decode_scratch);
    require(restored.is_cdr(), "DynamicData replay should associate the borrowed CDR buffer");
    require(restored.get_cdr_buffer().first ==
                    reinterpret_cast<const char*>(dynamic_message.message.data),
            "replayed DynamicData should retain the MCAP message view");
}

TEST_CASE("Project timestamp and borrowed-payload boundaries reject invalid values")
{
    rti::mcap::core::SampleMetadata metadata;
    metadata.source_timestamp = {static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1, 0, true};
    CHECK_THROWS_AS(rti::mcap::core::to_sample_info(metadata), std::invalid_argument);
    CHECK_THROWS_AS(rti::mcap::core::timestamp_to_mcap({std::numeric_limits<int64_t>::max(), 0, true}),
            std::overflow_error);
    rti::mcap::core::TopicConverter<Foo> converter(rti::mcap::core::TopicConverter<Foo>::describe("bounds"));
    Foo sample;
    std::vector<char> scratch;
    ::mcap::Message message;
    message.dataSize = 1;
    CHECK_THROWS_AS(converter.from_message(message, sample, scratch), std::invalid_argument);
}
