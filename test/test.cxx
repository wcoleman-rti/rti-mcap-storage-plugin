#include <cstring>
#include <dds/core/QosProvider.hpp>
#include "McapReader.hpp"
#include "McapWriter.hpp"
#include <mcap/internal.hpp>
#include "test.hpp"

using namespace ::rti::recording::storage;
using namespace ::rti::recording::storage::mcap;

template <typename TopicType>
void test_topic_type(int sample_count = 1, int stream_count = 1, const ::rti::recording::PropertySet& arg_properties = {})
{
    // needed to prevent logging exception
    ::dds::core::QosProvider::Default()->load_profiles();

    std::unordered_map<std::string, ::rti::recording::StreamInfo> orig_streaminfo_map;
    for (int i = 0; i < stream_count; ++i) {
        ::rti::recording::StreamInfo stream_info(
            ::dds::topic::topic_type_name<TopicType>::value() + "Topic_" + std::to_string(i),
            ::dds::topic::topic_type_name<TopicType>::value()
        );
        stream_info.type_info().dynamic_type(&(::rti::topic::dynamic_type<TopicType>::get()));
        orig_streaminfo_map.insert({stream_info.stream_name(), std::move(stream_info)});
    }

    ::dds::topic::ParticipantBuiltinTopicData participant_data;
    ::dds::sub::SampleInfo participant_info = valid_info(1);

    std::vector<std::shared_ptr<::dds::core::xtypes::DynamicData>> orig_data_seq;
    std::vector<std::shared_ptr<::dds::sub::SampleInfo>> orig_info_seq;
    for (int i = 0; i < sample_count; ++i) {
        orig_data_seq.push_back(
            std::make_shared<::dds::core::xtypes::DynamicData>(
                ::rti::core::xtypes::convert(get_data<TopicType>())));
        orig_info_seq.push_back(
            std::make_shared<::dds::sub::SampleInfo>(
                valid_info(i+1)));
    }

    // Properties
    const std::string data_dir = "test/data/";
    ::rti::recording::PropertySet shared_plugin_properties(arg_properties);
    shared_plugin_properties.try_emplace(RTI_XML_PROPERTY__DATA_FILENAME, data_dir + "data_" + ::dds::topic::topic_type_name<TopicType>::value() + ".mcap");
    shared_plugin_properties.try_emplace(RTI_XML_PROPERTY__INFO_FILENAME, data_dir + "info_" + ::dds::topic::topic_type_name<TopicType>::value() + ".mcap");

    ::rti::recording::PropertySet recorder_properties(shared_plugin_properties);
    recorder_properties.try_emplace(RTI_XML_PROPERTY__COMPRESSION_KIND, ::mcap::internal::CompressionString(::mcap::Compression::Lz4));
    recorder_properties.try_emplace(RTI_XML_PROPERTY__COMPRESSION_LEVEL, "Default");

    ::rti::recording::PropertySet replayer_properties(shared_plugin_properties);

    ::rti::recording::PropertySet shared_stream_properties(
        {
            {::rti::recording::domain_id_property_name(), "0"},
        }
    );
    ::rti::recording::PropertySet streaminfo_reader_properties(shared_stream_properties);
    ::rti::recording::PropertySet stream_reader_properties(shared_stream_properties);
    ::rti::recording::PropertySet stream_writer_properties(shared_stream_properties);

    // Test storage
    {
        McapFileWriter storage_writer(recorder_properties);

        std::unique_ptr<McapDiscoveryWriter> discovery_writer = std::unique_ptr<McapDiscoveryWriter>(
            static_cast<McapDiscoveryWriter*>(storage_writer.create_participant_writer()));
        discovery_writer->store({&participant_data}, {&participant_info});

        std::vector<McapStreamWriter*> stream_writer_seq;
        for (const auto & [_, stream_info] : orig_streaminfo_map) {
            McapStreamWriter * stream_writer = static_cast<McapStreamWriter*>(
                    storage_writer.create_stream_writer(stream_info, stream_writer_properties));
            stream_writer_seq.push_back(stream_writer);
        }

        std::vector<::dds::core::xtypes::DynamicData *> data_seq(orig_data_seq.size());
        std::vector<::dds::sub::SampleInfo *> info_seq(orig_info_seq.size());
        std::transform(orig_data_seq.begin(), orig_data_seq.end(), data_seq.begin(),
            [](const std::shared_ptr<::dds::core::xtypes::DynamicData> & data) {
                return data.get();});
        std::transform(orig_info_seq.begin(), orig_info_seq.end(), info_seq.begin(),
            [](const std::shared_ptr<::dds::sub::SampleInfo> & info) {
                return info.get();});

        for (auto stream_writer : stream_writer_seq) {
            stream_writer->store(data_seq, info_seq);
        }

        for (auto stream_writer : stream_writer_seq) {
            storage_writer.delete_stream_writer(stream_writer);
        }
    }
    
    // Test replay
    {
        McapFileReader storage_reader(replayer_properties);

        McapStreamInfoReader * streaminfo_reader = static_cast<McapStreamInfoReader*>(
            storage_reader.create_stream_info_reader(streaminfo_reader_properties));
        std::vector<::rti::routing::StreamInfo *> streaminfo_seq;
        streaminfo_reader->read(streaminfo_seq, SelectorState());

        std::vector<McapStreamReader*> stream_reader_seq;
        for (int i = 0; i < stream_count; ++i) {
            const auto & stream_info = streaminfo_seq[i];

            // StreamInfo
            assert(orig_streaminfo_map.find(stream_info->stream_name()) != orig_streaminfo_map.end());
            assert(orig_streaminfo_map.at(stream_info->stream_name()).stream_name() == stream_info->stream_name());
            assert(orig_streaminfo_map.at(stream_info->stream_name()).type_info().type_name() == stream_info->type_info().type_name());
            assert(orig_streaminfo_map.at(stream_info->stream_name()).type_info().dynamic_type() == stream_info->type_info().dynamic_type());

            McapStreamReader * stream_reader = static_cast<McapStreamReader*>(
                storage_reader.create_stream_reader(*stream_info, stream_reader_properties));
                stream_reader_seq.push_back(stream_reader);
        }
        for (const auto & stream_info : streaminfo_seq) {
            McapStreamReader * stream_reader = static_cast<McapStreamReader*>(
                storage_reader.create_stream_reader(*stream_info, stream_reader_properties));
                stream_reader_seq.push_back(stream_reader);
        }

        for (auto stream_reader : stream_reader_seq) {
            std::vector<::dds::core::xtypes::DynamicData *> data_seq;
            std::vector<::dds::sub::SampleInfo *> info_seq;
            SelectorState selector;
            stream_reader->read(data_seq, info_seq, selector);

            for (size_t i = 0; i < data_seq.size(); ++i) {                
                // SampleInfo
                assert(orig_info_seq[i]->valid() == info_seq[i]->valid());
                assert(orig_info_seq[i]->source_timestamp() == info_seq[i]->source_timestamp());
                assert(orig_info_seq[i]->extensions().reception_timestamp() == info_seq[i]->extensions().reception_timestamp());
                assert(orig_info_seq[i]->extensions().publication_sequence_number() == info_seq[i]->extensions().publication_sequence_number());

                // Sample
                assert(*(orig_data_seq[i]) == *(data_seq[i]));
            }
            stream_reader->return_loan(data_seq, info_seq);
        }

        for (auto stream_reader : stream_reader_seq) {
            storage_reader.delete_stream_reader(stream_reader);
        }
    }
    
}

// Helper function to strip surrounding quotes (single or double)
std::string strip_chars(const std::string& str, std::initializer_list<char> chars) {
    // Check if string is empty
    if (str.empty()) {
        return str;
    }

    // Find first position not in chars
    auto start = str.find_first_not_of(std::string(chars));
    if (start == std::string::npos) {
        return ""; // Entire string consists of characters to be stripped
    }

    // Find last position not in chars
    auto end = str.find_last_not_of(std::string(chars));

    // Return stripped substring
    return str.substr(start, end - start + 1);
}

::rti::recording::PropertySet parse_args(int argc, char *argv[])
{
    ::rti::recording::PropertySet properties;
    for (int i = 1; i < argc; i += 1) {
        std::string argument = strip_chars(argv[i], {'"', '\''});
        auto delimiterPos = argument.find('=');

        // Check if the argument is in the format name=value
        if (delimiterPos != std::string::npos) {
            std::string name = argument.substr(0, delimiterPos);
            std::string value = argument.substr(delimiterPos + 1);

            // Check for empty name or value
            if (!name.empty() && !value.empty()) {
                properties[name] = strip_chars(value, {'"', '\''});
            } else {
                std::cerr << "Invalid argument: " << argument << "\n";
            }
        } else {
            std::cerr << "Invalid argument format (expected name=value): " << argument << "\n";
        }
    }

    if (!properties.empty()) {
        std::cout << "Properties:" << std::endl;
        for (const auto& [n, v] : properties) {
            std::cout << std::quoted(n) << ": " << v << std::endl;
        }
    }

    return properties;
}


int main(int argc, char *argv[])
{
    auto properties = parse_args(argc, argv);
    test_topic_type<FooComplex>(10, 4, properties);
    std::cout << "pass" << std::endl;
    return 0;
}