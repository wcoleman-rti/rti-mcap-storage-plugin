#ifndef DDS_MCAP_STORAGE_COMMON_PROPERTY_H
#define DDS_MCAP_STORAGE_COMMON_PROPERTY_H

#include <string>

namespace rti::recording::storage::mcap_plugin::property {

    typedef std::string PropertyName;

    const static PropertyName MCAP_FILEPATH("mcap_filepath");
    /* const static PropertyName TYPES_FILEPATH("types_filepath"); */
    const static PropertyName STORE_SAMPLE_INFO("store_sample_info");

    std::vector<std::string> PROPERTY_VALUES_TRUE = {"true", "TRUE", "1"};
    std::vector<std::string> PROPERTY_VALUES_FALSE = {"false", "FALSE", "0"};

    static inline bool is_true(std::string property_value) {
        return std::any_of(PROPERTY_VALUES_TRUE.begin(), PROPERTY_VALUES_TRUE.end(), [&property_value](const std::string& s) {
            return property_value == s;});
    }

    static inline bool is_false(std::string property_value) {
        return std::any_of(PROPERTY_VALUES_FALSE.begin(), PROPERTY_VALUES_FALSE.end(), [&property_value](const std::string& s) {
            return property_value == s;});
    }

}   // rti::recording::storage::mcap_plugin::property

#endif // DDS_MCAP_STORAGE_COMMON_PROPERTY_H