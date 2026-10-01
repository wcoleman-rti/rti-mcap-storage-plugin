#include <dds/core/QosProvider.hpp>
#include "McapReader.hpp"
#include "McapWriter.hpp"
#include "TestSupport.hpp"
#include "test.hpp"

using namespace ::rti::recording::storage;
using namespace ::rti::mcap::storage;
using ::rti::recording::PropertySet;

namespace {

struct RecordedTopics {
    TempDirectory directory;
    std::vector<dds::core::xtypes::DynamicData> samples;
    std::vector<dds::sub::SampleInfo> infos;
    std::vector<rti::recording::StreamInfo> streams;
    PropertySet properties;
    std::unique_ptr<McapFileReader> reader;
    std::unique_ptr<McapStreamInfoReader> stream_reader;
    std::vector<rti::routing::StreamInfo*> discovered;
    std::vector<std::unique_ptr<McapStreamReader>> readers;

    explicit RecordedTopics(bool metadata = true, size_t count = 10, size_t stream_count = 4)
    {
        dds::core::QosProvider::Default()->load_profiles();
        properties[RTI_XML_PROPERTY__DATA_FILENAME] = directory.file("archive with spaces.mcap").string();
        properties[RTI_XML_PROPERTY__COMPRESSION_KIND] = "none";
        if (metadata) {
            properties[RTI_XML_PROPERTY__INFO_FILENAME] =
                    properties[RTI_XML_PROPERTY__DATA_FILENAME];
        }
        for (size_t i = 0; i < count; ++i) {
            samples.push_back(rti::core::xtypes::convert(get_data<FooComplex>()));
            infos.push_back(valid_info(static_cast<int64_t>(i + 1)));
        }
        for (size_t i = 0; i < stream_count; ++i) {
            streams.emplace_back("Topic_" + std::to_string(i), "FooComplex");
            streams.back().type_info().dynamic_type(&rti::topic::dynamic_type<FooComplex>::get());
        }
    }

    void record()
    {
        McapFileWriter writer(properties);
        std::vector<dds::core::xtypes::DynamicData*> data;
        std::vector<dds::sub::SampleInfo*> info;
        for (size_t i = 0; i < samples.size(); ++i) {
            data.push_back(&samples[i]);
            info.push_back(&infos[i]);
        }
        for (const auto& stream : streams) {
            std::unique_ptr<McapStreamWriter> output(static_cast<McapStreamWriter*>(
                    writer.create_stream_writer(stream, {{rti::recording::domain_id_property_name(), "0"}})));
            output->store(data, info);
        }
    }

    void open(PropertySet range = {})
    {
        range[rti::recording::domain_id_property_name()] = "0";
        reader = std::make_unique<McapFileReader>(properties);
        stream_reader.reset(static_cast<McapStreamInfoReader*>(
                reader->create_stream_info_reader({})));
        stream_reader->read(discovered, SelectorState());
        REQUIRE(discovered.size() == streams.size());
        for (const auto* stream : discovered) {
            readers.emplace_back(static_cast<McapStreamReader*>(
                    reader->create_stream_reader(*stream, range)));
        }
    }
};

void check_roundtrip(bool metadata)
{
    RecordedTopics archive(metadata);
    archive.record();
    REQUIRE(std::filesystem::exists(archive.properties[RTI_XML_PROPERTY__DATA_FILENAME]));
    archive.open();
    for (auto& reader : archive.readers) {
        std::vector<dds::core::xtypes::DynamicData*> data;
        std::vector<dds::sub::SampleInfo*> info;
        reader->read(data, info, SelectorState());
        REQUIRE(data.size() == archive.samples.size());
        REQUIRE(info.size() == data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            CHECK(*data[i] == archive.samples[i]);
            CHECK(info[i]->valid() == archive.infos[i].valid());
            CHECK(info[i]->source_timestamp() == archive.infos[i].source_timestamp());
            CHECK(info[i]->extensions().reception_timestamp() ==
                    archive.infos[i].extensions().reception_timestamp());
            if (metadata) {
                CHECK(info[i]->extensions().publication_sequence_number() ==
                        archive.infos[i].extensions().publication_sequence_number());
                CHECK((*info[i])->native().sample_rank == archive.infos[i]->native().sample_rank);
                CHECK((*info[i])->native().publication_virtual_guid.value[0] ==
                        archive.infos[i]->native().publication_virtual_guid.value[0]);
                CHECK((*info[i])->native().original_publication_virtual_guid.value[0] ==
                        archive.infos[i]->native().original_publication_virtual_guid.value[0]);
                CHECK((*info[i])->native().instance_handle.keyHash.value[0] ==
                        archive.infos[i]->native().instance_handle.keyHash.value[0]);
                CHECK((*info[i])->native().publication_handle.keyHash.value[0] ==
                        archive.infos[i]->native().publication_handle.keyHash.value[0]);
            }
        }
        reader->return_loan(data, info);
        CHECK(data.empty());
        CHECK(info.empty());
    }
    archive.stream_reader->return_loan(archive.discovered);
}

}  // namespace

TEST_CASE("Plugin round trip with paired metadata and multiple streams")
{
    check_roundtrip(true);
}

TEST_CASE("Plugin round trip without metadata")
{
    check_roundtrip(false);
}

TEST_CASE("Paired replay uses the same inclusive archive range")
{
    for (bool metadata : {false, true}) {
        INFO(metadata);
        RecordedTopics archive(metadata, 6, 1);
        archive.record();
        archive.open({
                {rti::recording::start_timestamp_property_name(), "100003000000"},
                {rti::recording::end_timestamp_property_name(), "100005000000"}});
        std::vector<dds::core::xtypes::DynamicData*> data;
        std::vector<dds::sub::SampleInfo*> info;
        archive.readers[0]->read(data, info, SelectorState());
        REQUIRE(data.size() == 3);
        REQUIRE(info.size() == 3);
        for (size_t i = 0; i < data.size(); ++i) CHECK(*data[i] == archive.samples[i + 2]);
        archive.readers[0]->return_loan(data, info);
    }
}

TEST_CASE("Outstanding replay loans survive other releases and reset")
{
    RecordedTopics archive(true, 6, 1);
    archive.record();
    archive.open();
    auto& reader = *archive.readers[0];
    SelectorState selector;
    selector.sample_state(dds::sub::status::SampleState::not_read()).max_samples(2);
    std::vector<dds::core::xtypes::DynamicData*> first, second, third;
    std::vector<dds::sub::SampleInfo*> first_info, second_info, third_info;
    reader.read(first, first_info, selector);
    reader.read(second, second_info, selector);
    REQUIRE(first.size() == 2);
    REQUIRE(second.size() == 2);
    CHECK(*first[0] == archive.samples[0]);
    reader.return_loan(first, first_info);
    CHECK(*second[0] == archive.samples[2]);
    reader.reset();
    reader.read(third, third_info, selector);
    REQUIRE(third.size() == 2);
    CHECK(*third[0] == archive.samples[0]);
    CHECK(*second[0] == archive.samples[2]);
    reader.return_loan(second, second_info);
    CHECK(*third[0] == archive.samples[0]);
    reader.return_loan(third, third_info);
}

TEST_CASE("Stateless queries do not consume the stateful replay cursor")
{
    RecordedTopics archive(true, 6, 1);
    archive.record();
    archive.open();
    auto& reader = *archive.readers[0];
    SelectorState sequential;
    sequential.sample_state(dds::sub::status::SampleState::not_read()).max_samples(2);
    std::vector<dds::core::xtypes::DynamicData*> data;
    std::vector<dds::sub::SampleInfo*> info;
    reader.read(data, info, sequential);
    REQUIRE(data.size() == 2);
    reader.return_loan(data, info);
    SelectorState window;
    window.sample_state(dds::sub::status::SampleState::any())
            .time_range_start(dds::core::Time(100, 2'000'000))
            .time_range_end(dds::core::Time(100, 3'000'000));
    for (int repeat = 0; repeat < 2; ++repeat) {
        reader.read(data, info, window);
        REQUIRE(data.size() == 2);
        CHECK(*data[0] == archive.samples[1]);
        CHECK(*data[1] == archive.samples[2]);
        reader.return_loan(data, info);
    }
    reader.read(data, info, sequential);
    REQUIRE(data.size() == 2);
    CHECK(*data[0] == archive.samples[2]);
    reader.return_loan(data, info);
}

TEST_CASE("Repeated identifiers and equal timestamps retain every sample")
{
    for (bool metadata : {false, true}) {
        INFO(metadata);
        RecordedTopics archive(metadata, 6, 1);
        for (auto& info : archive.infos) {
            auto native = info->native();
            native.reception_sequence_number = rti::core::SequenceNumber(42).native();
            native.reception_timestamp = {100, 1'000'000};
            info->native(native);
        }
        archive.record();
        archive.open();
        SelectorState selector;
        selector.sample_state(dds::sub::status::SampleState::not_read()).max_samples(2);
        size_t count = 0;
        while (!archive.readers[0]->finished()) {
            std::vector<dds::core::xtypes::DynamicData*> data;
            std::vector<dds::sub::SampleInfo*> info;
            archive.readers[0]->read(data, info, selector);
            REQUIRE(data.size() == 2);
            REQUIRE(count + data.size() <= archive.samples.size());
            for (const auto* sample : data) CHECK(*sample == archive.samples[count++]);
            archive.readers[0]->return_loan(data, info);
        }
        CHECK(count == archive.samples.size());
    }
}

TEST_CASE("Metadata preserves full DDS sequences and invalid-source lifecycle events")
{
    for (int32_t batch_size : {1, 3}) {
        INFO(batch_size);
        RecordedTopics archive(true, 3, 1);
        auto native = archive.infos[0]->native();
        native.reception_sequence_number = {1, 7};
        native.source_timestamp = DDS_TIME_INVALID;
        archive.infos[0]->native(native);
        native = archive.infos[1]->native();
        native.valid_data = DDS_BOOLEAN_FALSE;
        native.source_timestamp = DDS_TIME_INVALID;
        native.instance_state = DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE;
        archive.infos[1]->native(native);
        archive.record();
        archive.open();
        std::vector<dds::core::xtypes::DynamicData*> data;
        std::vector<dds::sub::SampleInfo*> info;
        SelectorState selector;
        selector.sample_state(dds::sub::status::SampleState::not_read()).max_samples(batch_size);
        size_t count = 0;
        while (!archive.readers[0]->finished()) {
            archive.readers[0]->read(data, info, selector);
            REQUIRE(data.size() == static_cast<size_t>(batch_size));
            REQUIRE(info.size() == data.size());
            REQUIRE(count + data.size() <= archive.samples.size());
            for (size_t i = 0; i < data.size(); ++i, ++count) {
                if (count == 0) {
                    CHECK((*info[i])->native().reception_sequence_number.high == 1);
                    CHECK((*info[i])->native().reception_sequence_number.low == 7);
                    CHECK(info[i]->source_timestamp() == dds::core::Time::invalid());
                }
                if (count == 1) {
                    CHECK_FALSE(info[i]->valid());
                    CHECK(*data[i] == dds::core::xtypes::DynamicData(archive.samples[0].type()));
                } else {
                    CHECK(info[i]->valid());
                    CHECK(*data[i] == archive.samples[count]);
                }
            }
            archive.readers[0]->return_loan(data, info);
        }
        CHECK(count == archive.samples.size());
    }
}

TEST_CASE("Replay input errors are recoverable exceptions")
{
    RecordedTopics archive;
    archive.record();
    McapFileReader reader(archive.properties);
    CHECK_THROWS_AS(reader.create_stream_reader(archive.streams[0], {
            {rti::recording::start_timestamp_property_name(), "-1"}}), std::runtime_error);
    CHECK_THROWS_AS(reader.create_stream_reader(archive.streams[0], {
            {rti::recording::start_timestamp_property_name(), "5"},
            {rti::recording::end_timestamp_property_name(), "4"}}), std::runtime_error);
    auto properties = archive.properties;
    properties[RTI_XML_PROPERTY__COMPRESSION_KIND] = "unsupported";
    CHECK_THROWS_AS(McapFileWriter{properties}, std::runtime_error);
    properties[RTI_XML_PROPERTY__DATA_FILENAME] = archive.directory.file("missing/input.mcap").string();
    McapFileReader missing(properties);
    CHECK_THROWS_AS(missing.create_stream_reader(archive.streams[0], {}), std::runtime_error);
    CHECK_THROWS_AS(reader.create_stream_reader(archive.streams[0], {
            {rti::recording::domain_id_property_name(), "1"}}), std::runtime_error);
}

TEST_CASE("Replay failures latch until reset without releasing outstanding loans")
{
    RecordedTopics archive(true, 2, 1);
    archive.record();
    archive.open();
    SelectorState selector;
    selector.sample_state(dds::sub::status::SampleState::not_read()).max_samples(1);
    std::vector<dds::core::xtypes::DynamicData*> held, data;
    std::vector<dds::sub::SampleInfo*> held_info, info;
    auto& reader = *archive.readers[0];
    reader.read(held, held_info, selector);
    REQUIRE(held.size() == 1);
    selector.sample_state(dds::sub::status::SampleState::read());
    CHECK_THROWS_AS(reader.read(data, info, selector), std::runtime_error);
    selector.sample_state(dds::sub::status::SampleState::not_read());
    CHECK_THROWS_AS(reader.read(data, info, selector), std::runtime_error);
    reader.reset();
    reader.read(data, info, selector);
    REQUIRE(data.size() == 1);
    CHECK(*held[0] == archive.samples[0]);
    CHECK(*data[0] == archive.samples[0]);
    reader.return_loan(held, held_info);
    reader.return_loan(data, info);
}

TEST_CASE("Recording failures latch only the affected stream")
{
    RecordedTopics archive(true, 1, 2);
    McapFileWriter writer(archive.properties);
    std::unique_ptr<McapStreamWriter> failed(static_cast<McapStreamWriter*>(
            writer.create_stream_writer(archive.streams[0],
                    {{rti::recording::domain_id_property_name(), "0"}})));
    CHECK_THROWS_AS(failed->store({&archive.samples[0]}, {nullptr}), std::invalid_argument);
    CHECK_THROWS_AS(failed->store({&archive.samples[0]}, {&archive.infos[0]}), std::invalid_argument);
    std::unique_ptr<McapStreamWriter> other(static_cast<McapStreamWriter*>(
            writer.create_stream_writer(archive.streams[1],
                    {{rti::recording::domain_id_property_name(), "0"}})));
    CHECK_NOTHROW(other->store({&archive.samples[0]}, {&archive.infos[0]}));
}

TEST_CASE("Stream-info loans retain their types across reset and other releases")
{
    RecordedTopics archive(true, 1, 1);
    archive.record();
    archive.open();
    archive.stream_reader->reset();
    std::vector<rti::routing::StreamInfo*> other;
    archive.stream_reader->read(other, SelectorState());
    REQUIRE(other.size() == 1);
    archive.stream_reader->return_loan(archive.discovered);
    CHECK(other[0]->type_info().dynamic_type().name() == "FooComplex");
    archive.stream_reader->return_loan(other);
}
