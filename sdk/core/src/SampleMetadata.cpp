#include "rti/mcap/core/SampleMetadata.hpp"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace rti::mcap::core {
namespace {

constexpr char METADATA_MAGIC[] = {'D', 'D', 'S', 'M'};
constexpr uint16_t METADATA_VERSION = 1;

template <typename T>
void append_integer(std::vector<char>& output, T value)
{
    using Unsigned = std::make_unsigned_t<T>;
    Unsigned bits;
    if constexpr (std::is_signed_v<T>) {
        std::memcpy(&bits, &value, sizeof(bits));
    } else {
        bits = value;
    }
    for (size_t i = 0; i < sizeof(bits); ++i) {
        output.push_back(static_cast<char>((bits >> (i * 8)) & 0xFF));
    }
}

void append_boolean(std::vector<char>& output, bool value)
{
    output.push_back(value ? 1 : 0);
}

template <typename T>
T read_integer(const char* data, size_t size, size_t& offset)
{
    using Unsigned = std::make_unsigned_t<T>;
    if (size - offset < sizeof(Unsigned)) {
        throw std::invalid_argument("Sample metadata payload is truncated");
    }
    Unsigned bits = 0;
    for (size_t i = 0; i < sizeof(bits); ++i) {
        bits |= static_cast<Unsigned>(
                        static_cast<unsigned char>(data[offset++]))
                << (i * 8);
    }
    if constexpr (std::is_signed_v<T>) {
        T value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    } else {
        return bits;
    }
}

bool read_boolean(const char* data, size_t size, size_t& offset)
{
    if (offset >= size) {
        throw std::invalid_argument("Sample metadata payload is truncated");
    }
    const auto value = static_cast<unsigned char>(data[offset++]);
    if (value > 1) {
        throw std::invalid_argument("Sample metadata boolean value is invalid");
    }
    return value != 0;
}

void append_timestamp(std::vector<char>& output, const Timestamp& timestamp)
{
    if (timestamp.valid &&
            (timestamp.seconds < 0 || timestamp.nanoseconds >= 1'000'000'000)) {
        throw std::invalid_argument("Sample metadata timestamp is invalid");
    }
    append_integer(output, timestamp.seconds);
    append_integer(output, timestamp.nanoseconds);
    append_boolean(output, timestamp.valid);
}

Timestamp read_timestamp(const char* data, size_t size, size_t& offset)
{
    Timestamp result{
            read_integer<int64_t>(data, size, offset),
            read_integer<uint32_t>(data, size, offset),
            read_boolean(data, size, offset)};
    if (result.valid &&
            (result.seconds < 0 || result.nanoseconds >= 1'000'000'000)) {
        throw std::invalid_argument("Sample metadata timestamp is invalid");
    }
    return result;
}

void append_sequence(std::vector<char>& output, const SequenceNumber& sequence)
{
    append_integer(output, sequence.high);
    append_integer(output, sequence.low);
    append_boolean(output, sequence.valid);
}

SequenceNumber read_sequence(const char* data, size_t size, size_t& offset)
{
    return {
            read_integer<int32_t>(data, size, offset),
            read_integer<uint32_t>(data, size, offset),
            read_boolean(data, size, offset)};
}

void append_identity(std::vector<char>& output, const VirtualIdentity& identity)
{
    output.insert(output.end(), identity.guid.begin(), identity.guid.end());
    append_sequence(output, identity.sequence);
    append_boolean(output, identity.valid);
}

VirtualIdentity read_identity(const char* data, size_t size, size_t& offset)
{
    if (size - offset < 16) {
        throw std::invalid_argument("Sample metadata payload is truncated");
    }
    VirtualIdentity result;
    std::copy_n(reinterpret_cast<const uint8_t*>(data + offset), 16, result.guid.begin());
    offset += 16;
    result.sequence = read_sequence(data, size, offset);
    result.valid = read_boolean(data, size, offset);
    return result;
}

void append_handle(std::vector<char>& output, const InstanceHandle& handle)
{
    if (handle.length > handle.key_hash.size()) {
        throw std::invalid_argument("Sample metadata handle length is invalid");
    }
    output.insert(output.end(), handle.key_hash.begin(), handle.key_hash.end());
    append_integer(output, handle.length);
    append_boolean(output, handle.valid);
}

InstanceHandle read_handle(const char* data, size_t size, size_t& offset)
{
    if (size - offset < 16) {
        throw std::invalid_argument("Sample metadata payload is truncated");
    }
    InstanceHandle result;
    std::copy_n(reinterpret_cast<const uint8_t*>(data + offset), 16, result.key_hash.begin());
    offset += 16;
    result.length = read_integer<uint32_t>(data, size, offset);
    result.valid = read_boolean(data, size, offset);
    if (result.length > result.key_hash.size()) {
        throw std::invalid_argument("Sample metadata handle length is invalid");
    }
    return result;
}

Timestamp from_native(const DDS_Time_t& value)
{
    return {
            value.sec,
            value.nanosec,
            value.sec >= 0 && value.nanosec < 1'000'000'000};
}

void to_native(DDS_Time_t& destination, const Timestamp& source)
{
    if (source.valid) {
        if (source.seconds < 0 ||
                source.seconds > std::numeric_limits<DDS_Long>::max() ||
                source.nanoseconds >= 1'000'000'000) {
            throw std::invalid_argument("Sample metadata timestamp exceeds the DDS range");
        }
        destination.sec = static_cast<DDS_Long>(source.seconds);
        destination.nanosec = source.nanoseconds;
    } else {
        destination = DDS_TIME_INVALID;
    }
}

SequenceNumber from_native(const DDS_SequenceNumber_t& value)
{
    const bool known = value.high != DDS_SEQUENCENUMBER_HIGH_DEFAULT ||
            value.low != 0xFFFFFFFFU;
    return {value.high, value.low, known};
}

void to_native(DDS_SequenceNumber_t& destination, const SequenceNumber& source)
{
    destination = source.valid
            ? DDS_SequenceNumber_t{source.high, source.low}
            : DDS_SEQUENCE_NUMBER_UNKNOWN;
}

VirtualIdentity from_native(
        const DDS_GUID_t& guid,
        const DDS_SequenceNumber_t& sequence)
{
    VirtualIdentity result;
    static_assert(sizeof(guid.value) == 16, "DDS virtual GUID must be 16 bytes");
    std::copy(std::begin(guid.value), std::end(guid.value), result.guid.begin());
    result.sequence = from_native(sequence);
    result.valid = result.sequence.valid &&
            !DDS_GUID_equals(&guid, &DDS_GUID_UNKNOWN);
    return result;
}

void to_native(DDS_GUID_t& guid, const VirtualIdentity& identity)
{
    std::copy(identity.guid.begin(), identity.guid.end(), std::begin(guid.value));
}

InstanceHandle from_native(const DDS_InstanceHandle_t& value)
{
    InstanceHandle result;
    std::copy(
            std::begin(value.keyHash.value),
            std::end(value.keyHash.value),
            result.key_hash.begin());
    result.length = value.keyHash.length;
    result.valid = value.isValid == DDS_BOOLEAN_TRUE;
    return result;
}

void to_native(DDS_InstanceHandle_t& destination, const InstanceHandle& source)
{
    std::copy(
            source.key_hash.begin(),
            source.key_hash.end(),
            std::begin(destination.keyHash.value));
    destination.keyHash.length = source.length;
    destination.isValid = source.valid ? DDS_BOOLEAN_TRUE : DDS_BOOLEAN_FALSE;
}

}  // namespace

uint64_t timestamp_to_mcap(const Timestamp& timestamp)
{
    if (!timestamp.valid || timestamp.seconds < 0 ||
            timestamp.nanoseconds >= 1'000'000'000) {
        throw std::runtime_error("DDS timestamp cannot be represented as an MCAP timestamp");
    }
    const auto seconds = static_cast<uint64_t>(timestamp.seconds);
    if (seconds > (std::numeric_limits<uint64_t>::max() - timestamp.nanoseconds) /
                    1'000'000'000ULL) {
        throw std::overflow_error("DDS timestamp exceeds the MCAP range");
    }
    return seconds * 1'000'000'000ULL + timestamp.nanoseconds;
}

SampleMetadata to_sample_metadata(const dds::sub::SampleInfo& info)
{
    const auto& native = info->native();
    SampleMetadata result;
    result.source_timestamp = from_native(native.source_timestamp);
    result.reception_timestamp = from_native(native.reception_timestamp);
    result.publication_sequence = from_native(native.publication_sequence_number);
    result.reception_sequence = from_native(native.reception_sequence_number);
    result.publication_virtual_identity = from_native(
            native.publication_virtual_guid,
            native.publication_virtual_sequence_number);
    result.original_publication_virtual_identity = from_native(
            native.original_publication_virtual_guid,
            native.original_publication_virtual_sequence_number);
    result.instance_handle = from_native(native.instance_handle);
    result.publication_handle = from_native(native.publication_handle);
    result.sample_state = static_cast<uint32_t>(native.sample_state);
    result.view_state = static_cast<uint32_t>(native.view_state);
    result.instance_state = static_cast<uint32_t>(native.instance_state);
    result.disposed_generation_count = native.disposed_generation_count;
    result.no_writers_generation_count = native.no_writers_generation_count;
    result.sample_rank = native.sample_rank;
    result.generation_rank = native.generation_rank;
    result.absolute_generation_rank = native.absolute_generation_rank;
    result.valid_data = native.valid_data == DDS_BOOLEAN_TRUE;
    return result;
}

dds::sub::SampleInfo to_sample_info(const SampleMetadata& metadata)
{
    DDS_SampleInfo native = DDS_SAMPLEINFO_DEFAULT;
    to_native(native.source_timestamp, metadata.source_timestamp);
    to_native(native.reception_timestamp, metadata.reception_timestamp);
    to_native(native.publication_sequence_number, metadata.publication_sequence);
    to_native(native.reception_sequence_number, metadata.reception_sequence);
    to_native(native.instance_handle, metadata.instance_handle);
    to_native(native.publication_handle, metadata.publication_handle);

    if (metadata.publication_virtual_identity.valid) {
        to_native(native.publication_virtual_guid, metadata.publication_virtual_identity);
        to_native(native.publication_virtual_sequence_number,
                metadata.publication_virtual_identity.sequence);
    }
    if (metadata.original_publication_virtual_identity.valid) {
        to_native(native.original_publication_virtual_guid,
                metadata.original_publication_virtual_identity);
        to_native(native.original_publication_virtual_sequence_number,
                metadata.original_publication_virtual_identity.sequence);
    }

    native.sample_state = static_cast<DDS_SampleStateKind>(metadata.sample_state);
    native.view_state = static_cast<DDS_ViewStateKind>(metadata.view_state);
    native.instance_state = static_cast<DDS_InstanceStateKind>(metadata.instance_state);
    native.disposed_generation_count = metadata.disposed_generation_count;
    native.no_writers_generation_count = metadata.no_writers_generation_count;
    native.sample_rank = metadata.sample_rank;
    native.generation_rank = metadata.generation_rank;
    native.absolute_generation_rank = metadata.absolute_generation_rank;
    native.valid_data = metadata.valid_data ? DDS_BOOLEAN_TRUE : DDS_BOOLEAN_FALSE;

    dds::sub::SampleInfo result;
    result->native(native);
    return result;
}

std::string_view sample_metadata_encoding() noexcept
{
    return "rti.dds-mcap.sample-metadata";
}

std::string_view sample_metadata_schema() noexcept
{
    return "DDSM v1; little-endian; source/reception timestamps; publication/"
           "reception sequences; publication/original virtual identities; "
           "instance/publication handles; sample/view/instance states; "
           "generation counts and ranks; valid_data.";
}

void encode_sample_metadata(
        const SampleMetadata& metadata,
        std::vector<char>& output)
{
    output.clear();
    output.reserve(177);
    output.insert(output.end(), std::begin(METADATA_MAGIC), std::end(METADATA_MAGIC));
    append_integer(output, METADATA_VERSION);
    append_timestamp(output, metadata.source_timestamp);
    append_timestamp(output, metadata.reception_timestamp);
    append_sequence(output, metadata.publication_sequence);
    append_sequence(output, metadata.reception_sequence);
    append_identity(output, metadata.publication_virtual_identity);
    append_identity(output, metadata.original_publication_virtual_identity);
    append_handle(output, metadata.instance_handle);
    append_handle(output, metadata.publication_handle);
    append_integer(output, metadata.sample_state);
    append_integer(output, metadata.view_state);
    append_integer(output, metadata.instance_state);
    append_integer(output, metadata.disposed_generation_count);
    append_integer(output, metadata.no_writers_generation_count);
    append_integer(output, metadata.sample_rank);
    append_integer(output, metadata.generation_rank);
    append_integer(output, metadata.absolute_generation_rank);
    append_boolean(output, metadata.valid_data);
}

SampleMetadata decode_sample_metadata(const char* data, size_t size)
{
    constexpr size_t header_size = sizeof(METADATA_MAGIC) + sizeof(METADATA_VERSION);
    if (data == nullptr || size < header_size ||
            !std::equal(std::begin(METADATA_MAGIC), std::end(METADATA_MAGIC), data)) {
        throw std::invalid_argument("Sample metadata header is invalid");
    }

    size_t offset = sizeof(METADATA_MAGIC);
    if (read_integer<uint16_t>(data, size, offset) != METADATA_VERSION) {
        throw std::invalid_argument("Unsupported sample metadata version");
    }

    SampleMetadata result;
    result.source_timestamp = read_timestamp(data, size, offset);
    result.reception_timestamp = read_timestamp(data, size, offset);
    result.publication_sequence = read_sequence(data, size, offset);
    result.reception_sequence = read_sequence(data, size, offset);
    result.publication_virtual_identity = read_identity(data, size, offset);
    result.original_publication_virtual_identity = read_identity(data, size, offset);
    result.instance_handle = read_handle(data, size, offset);
    result.publication_handle = read_handle(data, size, offset);
    result.sample_state = read_integer<uint32_t>(data, size, offset);
    result.view_state = read_integer<uint32_t>(data, size, offset);
    result.instance_state = read_integer<uint32_t>(data, size, offset);
    result.disposed_generation_count = read_integer<int32_t>(data, size, offset);
    result.no_writers_generation_count = read_integer<int32_t>(data, size, offset);
    result.sample_rank = read_integer<int32_t>(data, size, offset);
    result.generation_rank = read_integer<int32_t>(data, size, offset);
    result.absolute_generation_rank = read_integer<int32_t>(data, size, offset);
    result.valid_data = read_boolean(data, size, offset);
    if (offset != size) {
        throw std::invalid_argument("Sample metadata payload contains trailing bytes");
    }
    return result;
}

}  // namespace rti::mcap::core
