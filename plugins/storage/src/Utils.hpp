#ifndef RTI_STORAGEPLUGIN_MCAPUTILS_HPP_
#define RTI_STORAGEPLUGIN_MCAPUTILS_HPP_

#include <type_traits>
#include <dds/core/xtypes/DynamicData.hpp>
#include <dds/sub/SampleInfo.hpp>
#include <rti/recording/PropertySet.hpp>
#include <mcap/types.hpp>
#include <mcap/reader.hpp>
#include "McapDefs.hpp"

namespace rti::mcap::storage {

template <typename T>
std::optional<T> get_from_properties_opt(
        const rti::recording::PropertySet& properties,
        const std::string& property_name);

template <typename T>
T get_from_properties_req(
        const rti::recording::PropertySet& properties,
        const std::string& property_name);

template <typename TimeType>
TimeType convert_time(const ::dds::core::Time& dds_time);

template <typename TimeType>
::dds::core::Time convert_time(const TimeType& time);

uint32_t convert_sequence_number(const ::rti::core::SequenceNumber& sequence_number);

::rti::core::SequenceNumber convert_sequence_number(const uint32_t& sequence_number);

/**
 * @brief Copies the contents of a source vector to a target vector of a different type.
 * 
 * @tparam TargetT The type of elements in the target vector.
 * @tparam SourceT The type of elements in the source vector.
 * @pre TargetT and SourceT must be:
 *      1. Trivially copyable.
 *      2. Same size.
 *      3. Same alignment.
 * @param source The source vector to copy from.
 * @return std::vector<TargetT> The target vector with copied elements.
 */
template <typename TargetT, typename SourceT>
std::vector<TargetT> copy_buffer(const std::vector<SourceT>& source);

/**
 * @brief Copies the contents of the source vector to the target vector.
 *
 * @tparam TargetT The type of elements in the target vector.
 * @tparam SourceT The type of elements in the source vector.
 * @pre TargetT and SourceT must be:
 *      1. Trivially copyable.
 *      2. Same size.
 *      3. Same alignment.
 * @param source The source vector containing elements to be copied.
 * @param target The target vector where elements will be copied to.
 * @return A vector of TargetT containing the copied elements.
*/
template <typename TargetT, typename SourceT>
std::vector<TargetT> copy_buffer(const std::vector<SourceT>& source, std::vector<TargetT>& target);

/**
 * @brief Reinterprets the contents of the source vector to a target vector (no copy).
 *
 * @tparam TargetT The type of elements in the target vector.
 * @tparam SourceT The type of elements in the source vector.
 * @pre TargetT and SourceT must be:
 *      1. Same size.
 *      2. Same alignment.
 * @param source The source vector containing elements to be reinterpreted.
 * @return A vector of TargetT containing the reinterpreted elements.
*/
template <typename TargetT, typename SourceT>
std::vector<TargetT> reinterpret_buffer(const std::vector<SourceT>& source);

/**
 * @brief Reinterprets the contents of the source vector to the target vector (no copy).
 *
 * @tparam TargetT The type of elements in the target vector.
 * @tparam SourceT The type of elements in the source vector.
 * @pre TargetT and SourceT must be:
 *      1. Same size.
 *      2. Same alignment.
 * @param source The source vector containing elements to be reinterpreted.
 * @param target The target vector where elements will be reinterpreted.
 * @return A vector of TargetT containing the reinterpreted elements.
*/
template <typename TargetT, typename SourceT>
std::vector<TargetT> reinterpret_buffer(const std::vector<SourceT>& source, std::vector<TargetT>& target);

// /**
//  * @brief Reinterprets the contents of the source container to a target span.
//  *
//  * @tparam TargetT The type of elements in the target span.
//  * @tparam SourceT The type of elements in the source container.
//  * @pre TargetT and SourceT must be:
//  *      1. Same size.
//  *      2. Same alignment.
//  * @param source The source container with elements to be reinterpreted.
//  * @return A `std::span` of TargetT containing the reinterpreted elements.
// */
// template <typename TargetT, typename SourceT>
// std::span<TargetT> reinterpret_buffer(std::span<SourceT> source);

// template <typename T>
// inline std::vector<T> vector_cast(const T* data, size_t size) {
//     return std::vector<T>(data, data + size);
// }

std::string to_dds_xml(const ::dds::core::xtypes::DynamicType& type);

std::string to_dds_idl(const ::dds::core::xtypes::DynamicType& type);

::dds::core::xtypes::DynamicType get_type_from_xml(const std::string& type_name, const std::string& xml);

std::string to_str(const ::dds::core::InstanceHandle& instance_handle);

} // rti::mcap::storage

namespace rti::mcap::storage {

inline std::string topic_data_channel_name(
        const std::string& topic_name,
        int32_t domain_id) {
    return "dds/domain/" + std::to_string(domain_id) + "/" + topic_name;
}

inline std::string topic_info_channel_name(
        const std::string& topic_name,
        int32_t domain_id) {
    return topic_data_channel_name(topic_name, domain_id) + MCAP_CHANNEL_TOPIC_INFO_PREFIX;
}

inline std::string instance_data_channel_name(const std::string& topic_name, const ::dds::core::InstanceHandle& instance_handle) {
    return topic_name + MCAP_CHANNEL_INSTANCE_POSTFIX + to_str(instance_handle);
}

inline std::string instance_info_channel_name(const std::string& topic_name, const ::dds::core::InstanceHandle& instance_handle) {
    return topic_name + MCAP_CHANNEL_INSTANCE_POSTFIX + to_str(instance_handle) + MCAP_CHANNEL_TOPIC_INFO_PREFIX;
}

::mcap::Timestamp get_message_timestamp(const ::mcap::Message& message, const ::mcap::ReadMessageOptions::ReadOrder& read_order);



template <typename T>
inline typename rti::core::native_type_traits<T>::native_type to_native(const T& source)
{
    return source.native();
}

template <typename NativeT, typename T>
inline NativeT to_native(const T& source)
{
    return ::rti::core::native_conversions::to_native(source);
}

} // rti::mcap::storage

#include <rti/core/NativeValueType.hpp>




#include "Utils.tpp"

#endif // RTI_STORAGEPLUGIN_MCAPUTILS_HPP_