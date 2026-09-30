#include <charconv>
#include <chrono>
#include <iostream>
#include <limits>
#include <dds/core/QosProvider.hpp>
#include "idl/Test.hpp"
#include "McapReader.hpp"
#include "McapWriter.hpp"
#include "TestSupport.hpp"
#if defined(__linux__)
#include <sys/resource.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;
using namespace rti::recording::storage;
using namespace rti::mcap::storage;

struct Options {
    size_t samples = 10'000;
    size_t batch = 32;
    size_t streams = 4;
    size_t payload = 200;
    size_t start = 0;
    uint64_t chunk = ::mcap::DefaultChunkSize;
    std::string compression = "none";
    bool metadata = false;
};

size_t number(const std::string& value)
{
    size_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        throw std::invalid_argument("Invalid benchmark integer: " + value);
    }
    return result;
}

Options parse(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string name = argv[i];
        if (name == "--metadata") {
            options.metadata = true;
            continue;
        }
        if (i + 1 == argc) {
            throw std::invalid_argument("Missing value for " + name);
        }
        const std::string value = argv[++i];
        if (name == "--samples") options.samples = number(value);
        else if (name == "--batch") options.batch = number(value);
        else if (name == "--streams") options.streams = number(value);
        else if (name == "--payload") options.payload = number(value);
        else if (name == "--start") options.start = number(value);
        else if (name == "--chunk-size") options.chunk = number(value);
        else if (name == "--compression") options.compression = value;
        else throw std::invalid_argument("Unknown benchmark option: " + name);
    }
    if (!options.samples || !options.batch || !options.streams || !options.chunk ||
            options.batch > static_cast<size_t>(std::numeric_limits<int32_t>::max()) ||
            options.samples > static_cast<size_t>(std::numeric_limits<int32_t>::max()) ||
            options.start >= options.samples ||
            options.streams > std::numeric_limits<size_t>::max() / options.samples) {
        throw std::invalid_argument("Invalid benchmark sample, batch, stream, or range size");
    }
    return options;
}

void check(const ::mcap::Status& status)
{
    if (!status.ok()) {
        throw std::runtime_error(status.message);
    }
}

void report(const char* phase, Clock::time_point start, size_t samples, uint64_t bytes)
{
    const auto seconds = std::chrono::duration<double>(Clock::now() - start).count();
    std::cout << phase << ": samples=" << samples << " ms=" << seconds * 1000
              << " samples/s=" << samples / seconds
              << " payload_MiB/s=" << bytes / seconds / (1024 * 1024) << '\n';
}

void run(const Options& options)
{
    dds::core::QosProvider::Default()->load_profiles();
    TempDirectory directory;
    FooComplex sample;
    sample.unbounded_string(std::string(options.payload, 'x'));
    sample.unbounded_seq(std::vector<uint8_t>(options.payload, 42));
    auto dynamic_sample = rti::core::xtypes::convert(sample);
    std::vector<dds::sub::SampleInfo> infos(options.samples);
    for (size_t i = 0; i < infos.size(); ++i) {
        auto native = DDS_SAMPLEINFO_DEFAULT;
        native.valid_data = DDS_BOOLEAN_TRUE;
        native.source_timestamp = native.reception_timestamp = {
                100 + static_cast<DDS_Long>(i / 1000),
                static_cast<DDS_UnsignedLong>(i % 1000) * 1'000'000};
        native.reception_sequence_number =
                rti::core::SequenceNumber(static_cast<int64_t>(i + 1)).native();
        infos[i]->native(native);
    }
    const auto descriptor = rti::mcap::core::TopicConverter<FooComplex>::describe("benchmark", 0);
    rti::mcap::core::TopicConverter<FooComplex> converter(descriptor);
    std::vector<char> scratch, decoded_scratch;
    FooComplex decoded;
    const auto payload_bytes = converter.to_message(1, 1, sample, infos[0], scratch).message.dataSize;
    auto start = Clock::now();
    for (size_t i = 0; i < options.samples; ++i) {
        const auto view = converter.to_message(1, static_cast<uint32_t>(i), sample, infos[i], scratch);
        converter.from_message(view.message, decoded, decoded_scratch);
    }
    if (decoded.unbounded_string() != sample.unbounded_string()) {
        throw std::runtime_error("Conversion benchmark payload mismatch");
    }
    report("conversion", start, options.samples, payload_bytes * options.samples);

    ::mcap::McapWriterOptions archive_options("");
    archive_options.chunkSize = options.chunk;
    archive_options.compression = ::mcap::Compression::None;
    if (options.compression == "lz4") {
#ifndef MCAP_COMPRESSION_NO_LZ4
        archive_options.compression = ::mcap::Compression::Lz4;
#else
        throw std::invalid_argument("LZ4 support is not enabled in this build");
#endif
    } else if (options.compression == "zstd") {
#ifndef MCAP_COMPRESSION_NO_ZSTD
        archive_options.compression = ::mcap::Compression::Zstd;
#else
        throw std::invalid_argument("Zstd support is not enabled in this build");
#endif
    } else if (options.compression != "none") {
        throw std::invalid_argument("Unknown compression: " + options.compression);
    }
    const auto archive_path = directory.file("archive.mcap").string();
    start = Clock::now();
    {
        rti::mcap::file::Writer writer;
        check(writer.open(archive_path, archive_options));
        const auto schema = writer.add_schema(descriptor.schema());
        const auto channel = writer.add_channel(descriptor.channel(schema));
        for (size_t i = 0; i < options.samples; ++i) {
            check(writer.write(converter.to_message(
                    channel, static_cast<uint32_t>(i), sample, infos[i], scratch).message));
        }
        writer.close();
    }
    report("archive_write", start, options.samples, payload_bytes * options.samples);
    std::cout << "archive_bytes=" << std::filesystem::file_size(archive_path)
              << " chunk_size=" << options.chunk << " compression=" << options.compression << '\n';

    rti::recording::PropertySet properties{
            {RTI_XML_PROPERTY__DATA_FILENAME, directory.file("plugin.mcap").string()},
            {RTI_XML_PROPERTY__COMPRESSION_KIND, options.compression}};
    if (options.metadata) {
        properties[RTI_XML_PROPERTY__INFO_FILENAME] = properties[RTI_XML_PROPERTY__DATA_FILENAME];
    }
    std::vector<rti::recording::StreamInfo> streams;
    streams.reserve(options.streams);
    for (size_t i = 0; i < options.streams; ++i) {
        streams.emplace_back("benchmark_" + std::to_string(i), descriptor.type_name);
        streams.back().type_info().dynamic_type(&rti::topic::dynamic_type<FooComplex>::get());
    }
    const rti::recording::PropertySet domain{{rti::recording::domain_id_property_name(), "0"}};
    start = Clock::now();
    {
        McapFileWriter writer(properties);
        for (const auto& stream : streams) {
            std::unique_ptr<McapStreamWriter> output(static_cast<McapStreamWriter*>(
                    writer.create_stream_writer(stream, domain)));
            for (size_t offset = 0; offset < options.samples; offset += options.batch) {
                const auto count = std::min(options.batch, options.samples - offset);
                std::vector<dds::core::xtypes::DynamicData*> data(count, &dynamic_sample);
                std::vector<dds::sub::SampleInfo*> info;
                info.reserve(count);
                for (size_t i = 0; i < count; ++i) info.push_back(&infos[offset + i]);
                output->store(data, info);
            }
        }
    }
    const auto total = options.samples * options.streams;
    report("plugin_write", start, total, payload_bytes * total);
    std::cout << "plugin_archive_bytes="
              << std::filesystem::file_size(properties[RTI_XML_PROPERTY__DATA_FILENAME])
              << " metadata=" << options.metadata << '\n';

    McapFileReader reader(properties);
    size_t returned = 0, max_loan = 0;
    double open_ms = 0;
    start = Clock::now();
    for (const auto& stream : streams) {
        auto range = domain;
        range[rti::recording::start_timestamp_property_name()] =
                std::to_string(100'000'000'000ULL + options.start * 1'000'000ULL);
        const auto open_start = Clock::now();
        std::unique_ptr<McapStreamReader> input(static_cast<McapStreamReader*>(
                reader.create_stream_reader(stream, range)));
        open_ms += std::chrono::duration<double, std::milli>(Clock::now() - open_start).count();
        SelectorState selector;
        selector.sample_state(dds::sub::status::SampleState::not_read());
        selector.max_samples(static_cast<int32_t>(options.batch));
        while (!input->finished()) {
            std::vector<dds::core::xtypes::DynamicData*> data;
            std::vector<dds::sub::SampleInfo*> info;
            input->read(data, info, selector);
            if (data.size() != info.size() || (data.empty() && !input->finished())) {
                throw std::runtime_error("Replay benchmark produced an invalid batch");
            }
            returned += data.size();
            max_loan = std::max(max_loan, data.size());
            input->return_loan(data, info);
        }
    }
    const auto expected = (options.samples - options.start) * options.streams;
    if (returned != expected) throw std::runtime_error("Replay benchmark sample count mismatch");
    report("plugin_replay", start, returned, payload_bytes * returned);
    std::cout << "replay_open_seek_ms=" << open_ms << " max_loan_samples=" << max_loan << '\n';
#if defined(__linux__)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) throw std::runtime_error("getrusage failed");
    std::cout << "process_peak_RSS_KiB_including_dependencies=" << usage.ru_maxrss << '\n';
#else
    std::cout << "process_peak_RSS=unavailable_on_this_platform\n";
#endif
}

}  // namespace

int main(int argc, char** argv)
{
    try {
        if (argc == 2 && std::string(argv[1]) == "--help") {
            std::cout << "Options: --samples N --streams N --batch N --payload N\n"
                      << "         --start N --metadata --chunk-size N\n"
                      << "         --compression none|lz4|zstd\n"
                      << "Defaults: samples=10000 streams=4 batch=32 payload=200 start=0\n"
                      << "Chunk size and compression apply to the native archive phase.\n";
            return 0;
        }
        run(parse(argc, argv));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
