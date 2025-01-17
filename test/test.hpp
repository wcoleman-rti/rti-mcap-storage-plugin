#ifndef RTI_STORAGEPLUGIN_TEST_HPP_
#define RTI_STORAGEPLUGIN_TEST_HPP_

#include <cstring>
#include <random>
#include <limits>
#include <type_traits>
#include "idl/Test.hpp"
#include <dds/sub/SampleInfo.hpp>
#include <dds/core/Time.hpp>

#define assert_value(statement, value) \
    assert((statement) == (value));

#define assert_exception(statement, exception_type) \
    try { \
        statement; \
        assert(false && statement && "Expected exception not thrown"); \
    } catch (exception_type &) {}

template <class T>
T random_numeric(const T& min = std::numeric_limits<T>::min(), const T& max = std::numeric_limits<T>::max()) {
    assert(min >= std::numeric_limits<T>::min());
    assert(max <= std::numeric_limits<T>::max());

    // Use a static random number generator to persist across calls
    static std::random_device rd; // Seed for the random number engine
    static std::mt19937 generator(rd()); // Mersenne Twister random number engine

    if constexpr (std::is_same_v<T, bool>) {
        // For bool, directly return a random boolean by generating a 0 or 1
        std::uniform_int_distribution<int> distribution(min, max);
        return distribution(generator) == 1;
    } else if constexpr (std::is_integral_v<T>) {
        // Use integer distribution for integral types
        std::uniform_int_distribution<T> distribution(min, max);
        return distribution(generator);
    } else if constexpr (std::is_floating_point_v<T>) {
        // Use real distribution for floating-point types
        std::uniform_real_distribution<T> distribution(min, max);
        return distribution(generator);
    } else {
        static_assert(std::is_arithmetic_v<T>, "random_numeric only supports arithmetic types");
    }
}


template <typename TopicType>
TopicType get_data()
{
    return TopicType();
}

template <>
int8_t get_data()
{
    static int8_t v = 0;
    ++v;
    return v;
}

template<>
Foo get_data<Foo>()
{
    int8_t v = get_data<int8_t>();
    Foo foo;
    foo.a(v);
    foo.b(v % 2);
    foo.o(v);
    foo.c(32 + v);
    foo.i8(v);
    foo.u8(v);
    foo.i16(v);
    foo.u16(v);
    foo.i32(v);
    foo.u32(v);
    foo.i64(v);
    foo.u64(v);
    foo.f(v);
    foo.d(v);
    return foo;
}

template<>
FooEnum get_data<FooEnum>()
{
    return static_cast<FooEnum>(get_data<int8_t>() % 3);
}

template<>
FooUnion get_data<FooUnion>()
{
    int8_t v = get_data<int8_t>();
    FooEnum d = static_cast<FooEnum>(v % 3);
    FooUnion u;
    switch (d) {
        case FooEnum::A:
            u.i8(v);
            break;
        case FooEnum::B:
            u.i16(v);
            break;
        case FooEnum::C:
            u.i32(v);
            break;
        case FooEnum::D:
            u.i64(v);
            break;
    }
    return u;
}

template<>
FooComplex get_data<FooComplex>()
{
    FooComplex fc;
    fc.nested_primative(get_data<Foo>());
    fc.union_value(get_data<FooUnion>());
    fc.enum_value(fc.union_value()._d());
    fc.bounded_string(std::string(100, fc.nested_primative().c()));
    fc.unbounded_string(std::string(200, fc.nested_primative().c()));
    fc.bounded_seq(std::vector<uint8_t>(100, fc.nested_primative().u8()));
    fc.unbounded_seq(std::vector<uint8_t>(200, fc.nested_primative().u8()));
    return fc;
}

inline ::dds::core::Time now()
{
    return ::dds::core::Time::from_microsecs(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count());
}


::dds::sub::SampleInfo valid_info(int64_t seq_no = 1)
{
    ::dds::sub::SampleInfo info;
    auto native_info = DDS_SAMPLEINFO_DEFAULT;
    native_info.valid_data = DDS_BOOLEAN_TRUE;
    ::rti::core::native_conversions::to_native(native_info.source_timestamp, now());
    ::rti::core::native_conversions::to_native(native_info.reception_timestamp, now());
    native_info.reception_sequence_number = ::rti::core::SequenceNumber(seq_no).native();
    info->native(native_info);
    return info;

}


#endif  // RTI_STORAGEPLUGIN_TEST_HPP_