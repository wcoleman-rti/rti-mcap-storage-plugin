#include "rti/mcap/file/File.hpp"

#include <stdexcept>
#include <cstdio>

namespace rti::mcap::file {
Writer::Writer() = default;

Writer::~Writer()
{
    try {
        close();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Unable to close MCAP archive: %s\n", error.what());
    }
}

::mcap::Status Writer::open(
        const std::string& path,
        const ::mcap::McapWriterOptions& options)
{
    if (is_open()) {
        throw std::logic_error("MCAP archive writer is already open");
    }
    closed_statistics_.reset();
    auto output = std::make_unique<std::ofstream>();
    output->exceptions(std::ios::failbit | std::ios::badbit);
    try {
        output->open(path, std::ios::binary | std::ios::trunc);
    } catch (const std::ios_base::failure& error) {
        return {::mcap::StatusCode::OpenFailed, error.what()};
    }
    writer_ = std::make_unique<::mcap::McapWriter>();
    try {
        writer_->open(*output, options);
    } catch (const std::exception&) {
        writer_->terminate();
        writer_.reset();
        throw;
    }
    output_ = std::move(output);
    open_ = true;
    return ::mcap::StatusCode::Success;
}

::mcap::SchemaId Writer::add_schema(::mcap::Schema schema)
{
    return addSchema(schema);
}

::mcap::ChannelId Writer::add_channel(::mcap::Channel channel)
{
    return addChannel(channel);
}

::mcap::SchemaId Writer::addSchema(::mcap::Schema& schema)
{
    if (!is_open()) {
        throw std::logic_error("MCAP archive writer is not open");
    }
    writer_->addSchema(schema);
    return schema.id;
}

::mcap::ChannelId Writer::addChannel(::mcap::Channel& channel)
{
    if (!is_open()) {
        throw std::logic_error("MCAP archive writer is not open");
    }
    writer_->addChannel(channel);
    return channel.id;
}

::mcap::Status Writer::write(const ::mcap::Message& message)
{
    if (!is_open()) {
        throw std::logic_error("MCAP archive writer is not open");
    }
    return writer_->write(message);
}

::mcap::Status Writer::write(const ::mcap::Metadata& metadata)
{
    if (!is_open()) {
        throw std::logic_error("MCAP archive writer is not open");
    }
    return writer_->write(metadata);
}

void Writer::close()
{
    if (is_open()) {
        try {
            writer_->closeLastChunk();
            closed_statistics_ = writer_->statistics();
            writer_->close();
            output_->close();
        } catch (const std::exception&) {
            writer_->terminate();
            writer_.reset();
            output_.reset();
            closed_statistics_.reset();
            open_ = false;
            throw;
        }
        writer_.reset();
        output_.reset();
        open_ = false;
    }
}

bool Writer::is_open() const noexcept
{
    return open_;
}

::mcap::Statistics Writer::statistics() const
{
    if (!writer_) {
        if (closed_statistics_) {
            return *closed_statistics_;
        }
        throw std::logic_error("MCAP archive writer is not open");
    }
    return writer_->statistics();
}

Reader::Reader() = default;
Reader::~Reader() = default;

::mcap::Status Reader::open(const std::string& path)
{
    if (reader_) {
        throw std::logic_error("MCAP archive reader is already open");
    }
    reader_ = std::make_unique<::mcap::McapReader>();
    const auto status = reader_->open(path);
    if (!status.ok()) {
        reader_.reset();
    }
    return status;
}

::mcap::Status Reader::read_summary()
{
    return readSummary(::mcap::ReadSummaryMethod::AllowFallbackScan, {});
}

::mcap::Status Reader::readSummary(
        ::mcap::ReadSummaryMethod method,
        const ::mcap::ProblemCallback& on_problem)
{
    if (!reader_) {
        throw std::logic_error("MCAP archive reader is not open");
    }
    return reader_->readSummary(method, on_problem);
}

std::unique_ptr<::mcap::LinearMessageView> Reader::messages(
        const ::mcap::ReadMessageOptions& options) const
{
    if (!reader_) {
        throw std::logic_error("MCAP archive reader is not open");
    }
    return std::make_unique<::mcap::LinearMessageView>(
            readMessages([](const ::mcap::Status& status) {
                if (!status.ok()) {
                    throw std::runtime_error(
                            "Unable to read MCAP message: " + status.message);
                }
            }, options));
}

::mcap::LinearMessageView Reader::readMessages(
        const ::mcap::ProblemCallback& on_problem,
        const ::mcap::ReadMessageOptions& options) const
{
    if (!reader_) {
        throw std::logic_error("MCAP archive reader is not open");
    }
    return reader_->readMessages(on_problem, options);
}

std::unordered_map<::mcap::ChannelId, ::mcap::ChannelPtr> Reader::channels() const
{
    if (!reader_) {
        throw std::logic_error("MCAP archive reader is not open");
    }
    return reader_->channels();
}

std::unordered_map<::mcap::SchemaId, ::mcap::SchemaPtr> Reader::schemas() const
{
    if (!reader_) {
        throw std::logic_error("MCAP archive reader is not open");
    }
    return reader_->schemas();
}

std::optional<::mcap::Statistics> Reader::statistics() const
{
    if (!reader_) {
        return {};
    }
    return reader_->statistics();
}

::mcap::SchemaPtr Reader::schema(::mcap::SchemaId id) const
{
    if (!reader_) {
        throw std::logic_error("MCAP archive reader is not open");
    }
    return reader_->schema(id);
}

const std::multimap<std::string, ::mcap::MetadataIndex>& Reader::metadataIndexes() const
{
    if (!reader_) {
        throw std::logic_error("MCAP archive reader is not open");
    }
    return reader_->metadataIndexes();
}

::mcap::IReadable* Reader::dataSource() const
{
    if (!reader_) {
        throw std::logic_error("MCAP archive reader is not open");
    }
    return reader_->dataSource();
}

::mcap::Status Reader::ReadRecord(
        ::mcap::IReadable& source,
        uint64_t offset,
        ::mcap::Record* record)
{
    return ::mcap::McapReader::ReadRecord(source, offset, record);
}

::mcap::Status Reader::ParseMetadata(const ::mcap::Record& record, ::mcap::Metadata* metadata)
{
    return ::mcap::McapReader::ParseMetadata(record, metadata);
}

void Reader::close()
{
    if (reader_) {
        reader_->close();
        reader_.reset();
    }
}

bool Reader::is_open() const noexcept
{
    return static_cast<bool>(reader_);
}

}  // namespace rti::mcap::file
