
#include "test.hpp"
#include "Utils.hpp"
#include <rti/core/xtypes/DynamicDataTuples.hpp>


using namespace rti::recording::storage;
using namespace rti::recording::storage::mcap;

void buffer_test()
{
    std::vector<char> cdr_buffer = {'a', 'b', 'c', 'd', 'e', 'f'};

    {
        ::mcap::ByteArray mcap_buffer;
        mcap_buffer.assign(
            reinterpret_cast<const std::byte*>(cdr_buffer.data()), 
            reinterpret_cast<const std::byte*>(cdr_buffer.data()) + cdr_buffer.size());
        assert_value(cdr_buffer.size(), mcap_buffer.size())

        std::vector<char> new_cdr_buffer;
        new_cdr_buffer.assign(
            reinterpret_cast<const char*>(mcap_buffer.data()), 
            reinterpret_cast<const char*>(mcap_buffer.data()) + mcap_buffer.size());
        assert_value(new_cdr_buffer, cdr_buffer)
    }

    {
        ::mcap::ByteArray mcap_buffer(cdr_buffer.size());
        std::memcpy(mcap_buffer.data(), 
            reinterpret_cast<const std::byte*>(cdr_buffer.data()), cdr_buffer.size());
        assert_value(cdr_buffer.size(), mcap_buffer.size())
        
        std::vector<char> new_cdr_buffer(mcap_buffer.size());
        std::memcpy(new_cdr_buffer.data(), 
            reinterpret_cast<const char*>(mcap_buffer.data()), mcap_buffer.size());

        assert_value(new_cdr_buffer, cdr_buffer)
    }
}

void time_test()
{
    auto nanosecs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();

    {
        ::dds::core::Time dds_time = ::dds::core::Time::from_microsecs(nanosecs / 1000);
        ::mcap::Timestamp mcap_time = convert_time<::mcap::Timestamp>(dds_time);
        ::dds::core::Time __attribute__((unused)) new_dds_time = convert_time(mcap_time);
        assert_value(new_dds_time, dds_time)
    }

    {
        ::mcap::Timestamp mcap_time(nanosecs);
        ::dds::core::Time dds_time = convert_time(mcap_time);
        ::mcap::Timestamp __attribute__((unused)) new_mcap_time = convert_time<::mcap::Timestamp>(dds_time);
        assert_value(new_mcap_time, mcap_time)
    }

    {
        ::dds::core::Time dds_time = ::dds::core::Time::invalid();
        assert_exception(convert_time<::mcap::Timestamp>(dds_time), std::runtime_error)
    }

    {
        ::dds::core::Time dds_time(-1, 0);
        assert_exception(convert_time<::mcap::Timestamp>(dds_time), std::runtime_error)
    }
}

void sequence_number_test()
{
    {
        ::rti::core::SequenceNumber dds_sn(5);
        uint32_t mcap_sn = convert_sequence_number(dds_sn);
        assert_value(dds_sn.value(), mcap_sn)

        ::rti::core::SequenceNumber new_dds_sn = convert_sequence_number(mcap_sn);
        assert_value(dds_sn, new_dds_sn)
    }

    {
        uint32_t mcap_sn = 22;
        ::rti::core::SequenceNumber dds_sn = convert_sequence_number(mcap_sn);
        assert_value(dds_sn.value(), mcap_sn)

        uint32_t __attribute__((unused)) new_mcap_sn = convert_sequence_number(dds_sn);
        assert_value(mcap_sn, new_mcap_sn)
    }

    {
        ::rti::core::SequenceNumber unknown = ::rti::core::SequenceNumber::unknown();
        assert_exception(convert_sequence_number(unknown), std::runtime_error)
    }

    {
        uint32_t mcap_sn = 0;
        ::rti::core::SequenceNumber dds_sn = convert_sequence_number(mcap_sn);
        assert_value(dds_sn, ::rti::core::SequenceNumber::zero())
        assert_value(dds_sn.value(), 0);
    }

    {
        ::rti::core::SequenceNumber dds_sn = ::rti::core::SequenceNumber::maximum();
        uint32_t __attribute__((unused)) mcap_sn = convert_sequence_number(dds_sn);
        assert_value(mcap_sn, std::numeric_limits<uint32_t>::max())
    }

    {
        ::rti::core::SequenceNumber too_high((int64_t)std::numeric_limits<uint32_t>::max() + 1);
        assert_exception(convert_sequence_number(too_high), std::runtime_error)
    }

    {
        ::rti::core::SequenceNumber too_low(std::numeric_limits<int64_t>::min());
        assert_exception(convert_sequence_number(too_low), std::runtime_error)
    }
}

void properties_test()
{
    ::rti::recording::PropertySet properties(
        {
            {"required", "1"},
            {"optional", "2"},
        }
    );

    assert_value(get_from_properties_req<bool>(properties, "required"), true)
    assert_value(get_from_properties_opt<int>(properties, "optional").has_value(), true);
    assert_value(get_from_properties_opt<int>(properties, "does_not_exist").has_value(), false);
    assert_exception(get_from_properties_req<bool>(properties, "does_not_exist"), std::runtime_error)
}

void dds_types()
{
    using type_tuple = std::tuple<bool, char, int, float, std::string>;
    auto type = ::rti::core::xtypes::create_type_from_tuple<type_tuple>("MyType");

    auto xml = to_dds_xml(type);
    auto new_type = get_type_from_xml(type.name(), xml);
    assert_value(type, new_type)
}

void message_timestamp()
{
    ::mcap::Message message;
    message.logTime = 1;
    message.publishTime = 2;

    assert_value(get_message_timestamp(message, ::mcap::ReadMessageOptions::ReadOrder::LogTimeOrder), message.logTime)
    assert_value(get_message_timestamp(message, ::mcap::ReadMessageOptions::ReadOrder::ReverseLogTimeOrder), message.logTime)
    assert_exception(get_message_timestamp(message, ::mcap::ReadMessageOptions::ReadOrder::FileOrder), std::runtime_error)
}



int main(int , char *[])
{
    properties_test();
    buffer_test();
    time_test();
    sequence_number_test();
    dds_types();
    message_timestamp();

    std::cout << "pass" << std::endl;
    return 0;
}