#ifndef DDS_MCAP_STORAGE_COMMON_UTIL_H
#define DDS_MCAP_STORAGE_COMMON_UTIL_H

namespace rti::recording::storage::mcap_plugin::util {

    // Templated function to safely cast between integer types
    template<typename TO_INT_TYPE, typename FROM_INT_TYPE>
    TO_INT_TYPE safe_cast(FROM_INT_TYPE value) {
        static_assert(std::is_integral<TO_INT_TYPE>::value, "Target type must be an integral type");
        static_assert(std::is_integral<FROM_INT_TYPE>::value, "Source type must be an integral type");

        auto new_value = value;
        // Check for signed to unsigned conversion
        if (std::is_signed<FROM_INT_TYPE>::value && std::is_unsigned<TO_INT_TYPE>::value) {
            if (value < 0) {
                // throw std::overflow_error("Negative value cannot be safely cast to unsigned type");
                new_value = 0; 
            }
        }

        // Check for unsigned to signed conversion
        if (std::is_unsigned<FROM_INT_TYPE>::value && std::is_signed<TO_INT_TYPE>::value) {
            if (value > static_cast<typename std::make_unsigned<TO_INT_TYPE>::type>(std::numeric_limits<TO_INT_TYPE>::max())) {
                // throw std::overflow_error("Value cannot be safely cast to signed type");
                new_value = static_cast<typename std::make_unsigned<TO_INT_TYPE>::type>(std::numeric_limits<TO_INT_TYPE>::max());
            }
        }

        // Check for signed to signed or unsigned to unsigned conversion
        if (std::is_signed<FROM_INT_TYPE>::value == std::is_signed<TO_INT_TYPE>::value) {
            if (value > std::numeric_limits<TO_INT_TYPE>::max()) {
                // throw std::overflow_error("Value cannot be safely cast to target type");
                new_value = std::numeric_limits<TO_INT_TYPE>::max();
            }
            else if (value < std::numeric_limits<TO_INT_TYPE>::min()) {
                // throw std::overflow_error("Value cannot be safely cast to target type");
                new_value = std::numeric_limits<TO_INT_TYPE>::min();
            }
        }

        return static_cast<TO_INT_TYPE>(new_value);
    };

};   // rti::recording::storage::mcap_plugin::util

#endif // DDS_MCAP_STORAGE_COMMON_UTIL_H