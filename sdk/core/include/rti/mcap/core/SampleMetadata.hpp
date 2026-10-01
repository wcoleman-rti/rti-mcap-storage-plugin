#ifndef RTI_MCAP_SAMPLE_METADATA_HPP_
#define RTI_MCAP_SAMPLE_METADATA_HPP_

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include <dds/sub/SampleInfo.hpp>

namespace rti::mcap::core {

struct Timestamp {
    int64_t seconds = 0;
    uint32_t nanoseconds = 0;
    bool valid = false;
};

struct SequenceNumber {
    int32_t high = 0;
    uint32_t low = 0;
    bool valid = false;
};

struct VirtualIdentity {
    std::array<uint8_t, 16> guid{};
    SequenceNumber sequence;
    bool valid = false;
};

struct InstanceHandle {
    std::array<uint8_t, 16> key_hash{};
    uint32_t length = 0;
    bool valid = false;
};

struct SampleMetadata {
    Timestamp source_timestamp;
    Timestamp reception_timestamp;
    SequenceNumber publication_sequence;
    SequenceNumber reception_sequence;
    VirtualIdentity publication_virtual_identity;
    VirtualIdentity original_publication_virtual_identity;
    InstanceHandle instance_handle;
    InstanceHandle publication_handle;
    uint32_t sample_state = 0;
    uint32_t view_state = 0;
    uint32_t instance_state = 0;
    int32_t disposed_generation_count = 0;
    int32_t no_writers_generation_count = 0;
    int32_t sample_rank = 0;
    int32_t generation_rank = 0;
    int32_t absolute_generation_rank = 0;
    bool valid_data = false;
};

SampleMetadata to_sample_metadata(const dds::sub::SampleInfo& info);
uint64_t timestamp_to_mcap(const Timestamp& timestamp);
dds::sub::SampleInfo to_sample_info(const SampleMetadata& metadata);
std::string_view sample_metadata_encoding() noexcept;
std::string_view sample_metadata_schema() noexcept;
void encode_sample_metadata(const SampleMetadata& metadata, std::vector<char>& output);
SampleMetadata decode_sample_metadata(const char* data, size_t size);

}  // namespace rti::mcap::core

#endif
