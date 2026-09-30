#ifndef RTI_STORAGEPLUGIN_MCAPLOGGERHELPER_HPP_
#define RTI_STORAGEPLUGIN_MCAPLOGGERHELPER_HPP_

#include <iostream>
#include <mcap/reader.hpp>

namespace rti::mcap::storage {

/// `<<` operator for `std::optional`
template <typename T>
inline std::ostream& operator<<(std::ostream& os, const std::optional<T>& opt) {
    if (opt.has_value()) {
        os << *opt;
    } else {
        os << "[not set]";
    }
    return os;
}

/// `<<` operator for `::mcap::ReadMessageOptions::ReadOrder`
inline std::ostream& operator<<(std::ostream& os, const ::mcap::ReadMessageOptions::ReadOrder& readOrder) {
    switch (readOrder)
    {
    case ::mcap::ReadMessageOptions::ReadOrder::FileOrder:
        os << "FileOrder";
        break;
    case ::mcap::ReadMessageOptions::ReadOrder::LogTimeOrder:
        os << "LogTimeOrder";
        break;
    case ::mcap::ReadMessageOptions::ReadOrder::ReverseLogTimeOrder:
        os << "ReverseLogTimeOrder";
        break;
    default:
        std::runtime_error("Unknown ::mcap::ReadMessageOptions::ReadOrder value: " + std::to_string((int)readOrder));
        break;
    }
    return os;
}


}  // rti::mcap::storage


#endif // RTI_STORAGEPLUGIN_MCAPLOGGERHELPER_HPP_