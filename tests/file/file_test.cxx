#include <cstdio>
#include <stdexcept>
#include <string>
#include <doctest/doctest.h>
#include "TestSupport.hpp"

#include "rti/mcap/file/File.hpp"

namespace {

void require(bool condition, const char* message)
{
    REQUIRE_MESSAGE(condition, message);
}

TEST_CASE("Archive lifecycle errors and close statistics are explicit")
{
    TempDirectory directory;
    rti::mcap::file::Writer writer;
    ::mcap::McapWriterOptions options("");
    options.compression = ::mcap::Compression::None;
    CHECK_FALSE(writer.open(directory.file("missing/file.mcap").string(), options).ok());
    REQUIRE(writer.open(directory.file("file.mcap").string(), options).ok());
    CHECK_THROWS_AS((void) writer.open(directory.file("other.mcap").string(), options), std::logic_error);
    writer.close();
    CHECK_FALSE(writer.is_open());
    CHECK(writer.statistics().messageCount == 0);
    CHECK_THROWS_AS((void) writer.write(::mcap::Message{}), std::logic_error);
}

#if defined(__linux__)
TEST_CASE("Archive reports buffered output failures rather than succeeding")
{
    rti::mcap::file::Writer writer;
    ::mcap::McapWriterOptions options("");
    options.compression = ::mcap::Compression::None;
    REQUIRE(writer.open("/dev/full", options).ok());
    CHECK_THROWS_AS(writer.close(), std::ios_base::failure);
    CHECK_FALSE(writer.is_open());
    CHECK_THROWS_AS(writer.statistics(), std::logic_error);
}
#endif

}  // namespace

TEST_CASE("File wrapper preserves message payloads and registration")
{
    TempDirectory directory;
    const std::string path = directory.file("archive.mcap").string();
    const std::string payload = "project-owned file wrapper";

    {
        rti::mcap::file::Writer writer;
        ::mcap::McapWriterOptions options("");
        options.compression = ::mcap::Compression::None;
        require(writer.open(path, options).ok(), "archive should open for writing");
        ::mcap::Schema schema("test", "test", "opaque payload");
        schema.id = 0;
        const auto schema_id = writer.add_schema(std::move(schema));
        ::mcap::Channel channel("numbers", "test", schema_id);
        channel.id = 0;
        const auto channel_id = writer.add_channel(std::move(channel));
        ::mcap::Message message;
        message.channelId = channel_id;
        message.sequence = 1;
        message.publishTime = 101000000000ULL;
        message.logTime = 102000000000ULL;
        message.data = reinterpret_cast<const std::byte*>(payload.data());
        message.dataSize = payload.size();
        require(writer.write(message).ok(), "archive should write the message");
        writer.close();
        CHECK(writer.statistics().messageCount == 1);
        CHECK(writer.statistics().chunkCount == 1);
    }

    {
        rti::mcap::file::Reader reader;
        require(reader.open(path).ok(), "archive should open for reading");
        require(reader.read_summary().ok(), "archive summary should be readable");
        require(reader.channels().size() == 1, "archive should contain one channel");
        require(reader.schemas().size() == 1, "archive should contain one schema");
        auto messages = reader.messages();
        auto iterator = messages->begin();
        require(iterator != messages->end(), "archive should contain the written message");
        require((*iterator).message.sequence == 1, "message sequence should be retained");
        const auto& message = (*iterator).message;
        CHECK(message.logTime == 102000000000ULL);
        CHECK(message.publishTime == 101000000000ULL);
        CHECK(std::string(reinterpret_cast<const char*>(message.data), message.dataSize) == payload);
        ++iterator;
        require(iterator == messages->end(), "archive should contain exactly one message");
        reader.close();
    }

}
