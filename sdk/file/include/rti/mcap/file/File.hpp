#ifndef RTI_MCAP_FILE_HPP_
#define RTI_MCAP_FILE_HPP_

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <fstream>

#include <mcap/reader.hpp>
#include <mcap/writer.hpp>

namespace rti::mcap::file {

class Writer {
public:
    Writer();
    ~Writer();

    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    ::mcap::Status open(const std::string& path, const ::mcap::McapWriterOptions& options);
    ::mcap::SchemaId add_schema(::mcap::Schema schema);
    ::mcap::ChannelId add_channel(::mcap::Channel channel);
    ::mcap::SchemaId addSchema(::mcap::Schema& schema);
    ::mcap::ChannelId addChannel(::mcap::Channel& channel);
    ::mcap::Status write(const ::mcap::Message& message);
    ::mcap::Status write(const ::mcap::Metadata& metadata);
    void close();
    ::mcap::Statistics statistics() const;
    bool is_open() const noexcept;

private:
    std::unique_ptr<::mcap::McapWriter> writer_;
    std::unique_ptr<std::ofstream> output_;
    std::optional<::mcap::Statistics> closed_statistics_;
    bool open_{false};
};

class Reader {
public:
    Reader();
    ~Reader();

    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    ::mcap::Status open(const std::string& path);
    ::mcap::Status read_summary();
    ::mcap::Status readSummary(
            ::mcap::ReadSummaryMethod method,
            const ::mcap::ProblemCallback& on_problem);
    // Views and iterators require this reader to stay open. Message payloads
    // are borrowed and must be consumed or copied before advancing an iterator.
    std::unique_ptr<::mcap::LinearMessageView> messages(
            const ::mcap::ReadMessageOptions& options = {}) const;
    ::mcap::LinearMessageView readMessages(
            const ::mcap::ProblemCallback& on_problem,
            const ::mcap::ReadMessageOptions& options = {}) const;
    std::unordered_map<::mcap::ChannelId, ::mcap::ChannelPtr> channels() const;
    std::unordered_map<::mcap::SchemaId, ::mcap::SchemaPtr> schemas() const;
    ::mcap::SchemaPtr schema(::mcap::SchemaId id) const;
    const std::multimap<std::string, ::mcap::MetadataIndex>& metadataIndexes() const;
    ::mcap::IReadable* dataSource() const;
    static ::mcap::Status ReadRecord(::mcap::IReadable& source, uint64_t offset, ::mcap::Record* record);
    static ::mcap::Status ParseMetadata(const ::mcap::Record& record, ::mcap::Metadata* metadata);
    std::optional<::mcap::Statistics> statistics() const;
    void close();
    bool is_open() const noexcept;

private:
    std::unique_ptr<::mcap::McapReader> reader_;
};

}  // namespace rti::mcap::file

#endif
