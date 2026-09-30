#include "Utils.hpp"
#include <dds/core/QosProvider.hpp>
#include <rti/core/constants.hpp>

namespace rti::mcap::storage {

template <>
::mcap::Timestamp convert_time(const ::dds::core::Time& dds_time) {
    if (dds_time == ::dds::core::Time::invalid()) {
        throw std::runtime_error("Cannot convert DDS timestamp to MCAP timestamp (invalid).");
    }
    else if (dds_time == ::dds::core::Time::invalid() || dds_time.sec() < 0) {
        throw std::runtime_error("Cannot convert DDS timestamp to MCAP timestamp (too low).");
    }
    else if (dds_time == ::dds::core::Time::zero()) {
        return 0;
    }
    else if (dds_time == ::dds::core::Time::maximum()) {
        return ::mcap::MaxTime;
    }
    ::mcap::Timestamp mcap_time = dds_time.sec();
    mcap_time *= (::mcap::Timestamp) rti::core::nanosec_per_sec;
    mcap_time += (::mcap::Timestamp) dds_time.nanosec();
    return mcap_time;
}

template <>
::dds::core::Time convert_time(const ::mcap::Timestamp& mcap_time) {
    ::mcap::Timestamp sec = mcap_time / (::mcap::Timestamp) rti::core::nanosec_per_sec;
    if (sec > (::mcap::Timestamp) std::numeric_limits<int32_t>::max()) {
        // return ::dds::core::Time::maximum();
        throw std::runtime_error("Cannot convert MCAP timestamp to DDS timestamp (too high).");
    }

    dds::core::Time dds_time;
    dds_time.sec(sec);
    dds_time.nanosec(mcap_time % (::mcap::Timestamp) rti::core::nanosec_per_sec);
    return dds_time;
}

uint32_t convert_sequence_number(const ::rti::core::SequenceNumber& sequence_number) {
    if (sequence_number == ::rti::core::SequenceNumber::unknown()) {
        // return 0;
        throw std::runtime_error("Cannot convert DDS sequence number to MCAP message sequence (unknown).");
    }
    else if (sequence_number == ::rti::core::SequenceNumber::zero()) {
        return 0;
    }
    else if (sequence_number == ::rti::core::SequenceNumber::maximum()) {
        return std::numeric_limits<uint32_t>::max();
    }
    else {
        auto value = sequence_number.value();
        if (value < 0) {
            // return 0;
            throw std::runtime_error("Cannot convert DDS sequence number to MCAP message sequence (too low).");
        }
        else if (value > std::numeric_limits<uint32_t>::max()) {
            // return std::numeric_limits<uint32_t>::max();
            throw std::runtime_error("Cannot convert DDS sequence number to MCAP message sequence (too high).");
        }
        else {
            return static_cast<uint32_t>(value);
        }
    }
}

::rti::core::SequenceNumber convert_sequence_number(const uint32_t& sequence_number) {
    if (sequence_number == 0) {
        return ::rti::core::SequenceNumber::zero();
    }
    // else if (sequence_number == std::numeric_limits<uint32_t>::max()) {
    //     return ::rti::core::SequenceNumber::maximum();
    // }
    else {
        return ::rti::core::SequenceNumber(sequence_number);
    }
}

std::string to_dds_xml(const ::dds::core::xtypes::DynamicType& type) {
    const static rti::core::xtypes::DynamicTypePrintFormatProperty XML_PRINT_FORMAT(0, false, rti::core::xtypes::DynamicTypePrintKind::xml, true);
    return rti::core::xtypes::to_string(type, XML_PRINT_FORMAT);
}

std::string to_dds_idl(const ::dds::core::xtypes::DynamicType& type) {
    const static rti::core::xtypes::DynamicTypePrintFormatProperty IDL_PRINT_FORMAT(0, false, rti::core::xtypes::DynamicTypePrintKind::idl, true);
    return rti::core::xtypes::to_string(type, IDL_PRINT_FORMAT);
}

::rti::core::QosProviderParams get_isolated_provider_params() {
    ::rti::core::QosProviderParams qos_provider_params;
    qos_provider_params.ignore_environment_profile(true);
    qos_provider_params.ignore_resource_profile(true);
    qos_provider_params.ignore_user_profile(true);
    return qos_provider_params;
}

inline const std::string fq_xml(const std::string& content) {
    return "<dds><types>" + content + "</types></dds>";
}

inline const std::string uri_string_profile(const std::string& content) {
    return "str://\"" + content + "\"";
}

::dds::core::xtypes::DynamicType get_type_from_xml(const std::string& type_name, const std::string& xml) {
    // ::dds::core::QosProvider qos_provider("str://\"<dds><types>" + xml + "</types></dds>");

    // auto params = get_isolated_provider_params();
    // auto params = rti::core::default_qos_provider_params();
    // std::string x = fq_xml(xml);
    // params.string_profile({x});
    // auto qos_provider = rti::core::create_qos_provider_ex(params);

    // dds::core::QosProvider qos_provider = dds::core::null;
    // dds::core::QosProvider::Default()->load_profiles();
    // qos_provider = dds::core::QosProvider::Default();
    
    // qos_provider = dds::core::QosProvider("");
    dds::core::QosProvider qos_provider(uri_string_profile(fq_xml(xml)));

    // ::dds::core::QosProvider qos_provider(uri_string_profile(fq_xml(xml)));
    // qos_provider->provider_params(qos_provider_params);
    // std::string stringProfile = uri_string_profile(fq_xml(xml));
    // dds::core::QosProvider qos_provider(stringProfile);
    try {
        auto type = qos_provider->type(type_name);
        return type;
    } catch (::dds::core::Error &) {
        throw std::runtime_error("Failed to retrieve DDS DynamicType [rti::storage_plugin::adapter::convert_type()]: " + type_name);
    }
}

std::string to_str(const ::dds::core::InstanceHandle& instance_handle) {
    std::ostringstream oss;
    oss << instance_handle; // Uses the overloaded operator<<
    return oss.str();
}


} // rti::mcap::storage


namespace rti::mcap::storage {

::mcap::Timestamp get_message_timestamp(const ::mcap::Message& message, const ::mcap::ReadMessageOptions::ReadOrder& read_order) {
    switch (read_order)
    {
    case ::mcap::ReadMessageOptions::ReadOrder::LogTimeOrder:
        return message.logTime;
    
    case ::mcap::ReadMessageOptions::ReadOrder::ReverseLogTimeOrder:
        return message.logTime;

    /*
    case ::mcap::ReadMessageOptions::ReadOrder::PublishOrder:
        return message.publishTime;
    */

    default:
        // TODO: log error
        throw std::runtime_error("Unsupported ReadOrder.");
    };
}


} // rti::mcap::storage