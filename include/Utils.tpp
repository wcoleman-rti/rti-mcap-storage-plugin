#include "Utils.hpp"
#include <cstring>
#include <vector>

namespace rti::recording::storage {

template <typename T>
std::optional<T> get_from_properties_opt(
        const rti::recording::PropertySet& properties,
        const std::string& property_name)
{
    rti::recording::PropertySet::const_iterator found =
            properties.find(property_name);
    if (found == properties.end()) {
        return std::nullopt;
    }
    T value;
    std::stringstream str_stream(found->second);
    str_stream >> value;
    if (str_stream.fail()) {
       throw std::runtime_error(
                "!parse value from property with name=" +  property_name);
    }
    return std::make_optional<T>(value);
}

template <typename T>
T get_from_properties_req(
        const rti::recording::PropertySet& properties,
        const std::string& property_name)
{
    auto found = get_from_properties_opt<T>(properties, property_name);
    if (!found) {
        throw std::runtime_error(
                "!get value from property with name=" +  property_name);
    }
    return *found;
}

template <typename TargetT, typename SourceT>
std::vector<TargetT> copy_buffer(const std::vector<SourceT>& source, std::vector<TargetT>& target) {
    static_assert(std::is_trivially_copyable<SourceT>::value, "SourceT must be trivially copyable");
    static_assert(std::is_trivially_copyable<TargetT>::value, "TargetT must be trivially copyable");
    static_assert(sizeof(SourceT) == sizeof(TargetT), "SourceT and TargetT must have the same size");
    static_assert(alignof(SourceT) == alignof(TargetT), "SourceT and TargetT must have the same alignment");
    
    target.resize(source.size());
    std::memcpy(target.data(), reinterpret_cast<const TargetT*>(source.data()), source.size());
    return target;
}

template <typename TargetT, typename SourceT>
std::vector<TargetT> copy_buffer(const std::vector<SourceT>& source) {
    std::vector<TargetT> target(source.size());
    return copy_buffer(source, target);
}

template <typename TargetT, typename SourceT>
std::vector<TargetT> reinterpret_buffer(const std::vector<SourceT>& source, std::vector<TargetT>& target) {
    static_assert(sizeof(SourceT) == sizeof(TargetT), "SourceT and TargetT must have the same size");
    static_assert(alignof(SourceT) == alignof(TargetT), "SourceT and TargetT must have the same alignment");

    target.assign(
        reinterpret_cast<const TargetT*>(source.data()), 
        reinterpret_cast<const TargetT*>(source.data()) + source.size());
    return target;
}

template <typename TargetT, typename SourceT>
std::vector<TargetT> reinterpret_buffer(const std::vector<SourceT>& source) {
    std::vector<TargetT> target(source.size());
    return reinterpret_buffer(source, target);
}


} // rti::recording::storage