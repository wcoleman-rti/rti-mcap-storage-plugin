#ifndef RTI_RECORDING_STORAGE_MCAPREADER_H_
#define RTI_RECORDING_STORAGE_MCAPREADER_H_

#include <rti/recording/storage/StorageReader.hpp>
#include <rti/recording/StreamInfo.hpp>
#include <rti/recording/PropertySet.hpp>
#include <rti/mcap/file/File.hpp>
#include "Utils.hpp"
#include "Logger.hpp"
#include <exception>
#include <mutex>
#include <unordered_map>


namespace rti::mcap::storage {

/*
 * Convenience macro to forward-declare the C-style function that will be
 * called by RTI Recording Service to create your class.
 */
RTI_RECORDING_STORAGE_READER_CREATE_DECL(McapFileReader)

/**
 * A Storage Reader implementation that uses the MCAP storage format.
 */
class McapFileReader : public ::rti::recording::storage::StorageReader {
public:
    /**
     * Constructor.
     */
    McapFileReader(const ::rti::recording::PropertySet &properties);

    ~McapFileReader();

    /**
     * Recording Service will call this method to create a Stream Info Reader
     * object that can read the stored DCPSPublication samples and build
     * StreamInfo objects out of them, and pass them to the processing engine.
     * This method expects the time range information to be passed as properties
     * in the property set parameter. This will be provided by Recording Service
     * automatically.
     */
    virtual ::rti::recording::storage::StorageStreamInfoReader *create_stream_info_reader(
            const rti::recording::PropertySet &properties) override;

    virtual void delete_stream_info_reader(
            ::rti::recording::storage::StorageStreamInfoReader
                    *stream_info_reader) override;

    /**
     * Recording Service will call this method to create a Stream Reader object
     * that can read the stored user-data samples associated with a StreamInfo
     * parameter.
     * This method expects the time range information to be passed as properties
     * in the property set parameter, as well as the associated DDS Domain ID.
     * This will be provided by Recording Service automatically.
     */
    virtual ::rti::recording::storage::StorageStreamReader *create_stream_reader(
            const rti::recording::StreamInfo &stream_info,
            const rti::recording::PropertySet &properties) override;

    virtual void delete_stream_reader(
            ::rti::recording::storage::StorageStreamReader *stream_reader) override;

private:
    std::string data_filename_;

    std::string info_filename_;

    Logger logger;
};

/*
 * This Stream Reader implementation is able to read MCAP files
 * created by the 'McapStreamWriter' class.
 */
class McapStreamReader
        : public ::rti::recording::storage::DynamicDataStorageStreamReader {
public:
    McapStreamReader(
            const std::string& data_filename,
            const std::string& info_filename,
            const ::rti::recording::StreamInfo &stream_info,
            int32_t domain_id,
            ::mcap::Timestamp start_timestamp,
            ::mcap::Timestamp end_timestamp,
            Logger& logger);

    virtual ~McapStreamReader();

    /*
     * Implementation of the read operation. It should interpret the selector
     * state object that expresses the specific needs of Replay/Converter about
     * the data to be provided (data not read before vs data of any kind, lower
     * and upper time limits, etc).
     */
    virtual void read(
            std::vector<::dds::core::xtypes::DynamicData *> &data_seq,
            std::vector<::dds::sub::SampleInfo *> &info_seq,
            const ::rti::recording::storage::SelectorState &selector) override;

    /*
     * The return loan operation should free any resources allocated by the
     * read() operation.
     */
    virtual void return_loan(
            std::vector<::dds::core::xtypes::DynamicData *> &data_seq,
            std::vector<::dds::sub::SampleInfo *> &info_seq) override;

    /*
     * This method should flag Replay/Converter that all data related to the
     * data stream has been read and that we're ready for termination.
     */
    virtual bool finished() override;

    virtual void reset() override;

protected:
    virtual void read_w_data(
            std::vector<::dds::core::xtypes::DynamicData *> &data_seq,
            std::vector<::dds::sub::SampleInfo *> &info_seq,
            const ::rti::recording::storage::SelectorState &selector);
    
    virtual void read_w_info(
            std::vector<::dds::core::xtypes::DynamicData *> &data_seq,
            std::vector<::dds::sub::SampleInfo *> &info_seq,
            const ::rti::recording::storage::SelectorState &selector);

    ::dds::sub::SampleInfo convert_sample_info(
            const ::mcap::Message &data_message);
    
    ::dds::sub::SampleInfo convert_sample_info(
            const ::mcap::Message &data_message,
            const ::mcap::Message &info_message);
    
    ::dds::core::xtypes::DynamicData convert_sample_data(
            const ::mcap::Message &data_message);
    
    ::dds::core::xtypes::DynamicData convert_sample_data(
            const ::mcap::Message &data_message,
            const ::mcap::Message &info_message);

private:
    ::rti::recording::StreamInfo stream_info_;
    ::dds::core::xtypes::DynamicType type_;

    int32_t domain_id_;

    std::unique_ptr<::rti::mcap::file::Reader> data_reader_;
    std::unique_ptr<::rti::mcap::file::Reader> info_reader_;

    std::string data_channel_topic_;
    std::string info_channel_topic_;
    ::mcap::ReadMessageOptions data_read_options_;
    ::mcap::ReadMessageOptions info_read_options_;
    ::mcap::ProblemCallback on_data_problem_;
    ::mcap::ProblemCallback on_info_problem_;

    struct Loan {
        std::vector<std::unique_ptr<::dds::core::xtypes::DynamicData>> data;
        std::vector<::dds::sub::SampleInfo> info;
    };
    std::unordered_map<::dds::core::xtypes::DynamicData*, Loan> loans_;
    static constexpr size_t MAX_REUSABLE_SAMPLES = 32;
    std::vector<std::unique_ptr<::dds::core::xtypes::DynamicData>> reusable_samples_;

    std::vector<char> data_cdr_buffer_;


    struct Cursor {
        std::unique_ptr<::mcap::LinearMessageView> messages;
        std::unique_ptr<::mcap::LinearMessageView::Iterator> iterator;
    };
    Cursor data_cursor_;
    Cursor info_cursor_;
    static Cursor cursor(
            const ::rti::mcap::file::Reader& reader,
            const ::mcap::ReadMessageOptions& options,
            const ::mcap::ProblemCallback& on_problem);
    void read_batch(
            std::vector<::dds::core::xtypes::DynamicData*>& data,
            std::vector<::dds::sub::SampleInfo*>& info,
            const ::rti::recording::storage::SelectorState& selector);
    void deserialize(const ::mcap::Message& message, ::dds::core::xtypes::DynamicData& sample);
    std::mutex state_mutex_;
    std::exception_ptr read_failure_;

    ::mcap::Timestamp start_timestamp_;
    ::mcap::Timestamp end_timestamp_;

    bool finished_;

    Logger logger;
};

/*
 * This stream info reader implementation is able to read an MCAP file
 * recorded with the McapWriter implementation.
 */
class McapStreamInfoReader
        : public ::rti::recording::storage::StorageStreamInfoReader {
public:
    McapStreamInfoReader(
            const std::string& data_filename,
            const std::string& info_filename,
            Logger& logger);

    ~McapStreamInfoReader();

    /*
     * Implementation of the read operation. It should interpret the selector
     * state object that expresses the specific needs of Replay/Converter about
     * the data to be provided (data not read before vs data of any kind, lower
     * and upper time limits, etc).
     */
    virtual void read(
            std::vector<::rti::recording::StreamInfo *> &stream_info_seq,
            const ::rti::recording::storage::SelectorState &selector);

    /*
     * The return loan operation should free any resources allocated by the
     * read() operation.
     */
    virtual void return_loan(
            std::vector<::rti::recording::StreamInfo *> &stream_info_seq);

    /*
     * An int64-represented time-stamp (in nanoseconds) representing the
     * starting point in time where recorded data exists, or when the service
     * started executing.
     */
    virtual int64_t service_start_time();

    /*
     * An int64-represented time-stamp (in nanoseconds) representing the
     * final point in time where recorded data exists, or when the service
     * finished executing.
     */
    virtual int64_t service_stop_time();

    virtual bool finished();

    virtual void reset();

private:
    std::shared_ptr<::rti::mcap::file::Reader> data_reader_;
    std::shared_ptr<::rti::mcap::file::Reader> info_reader_;

    struct StreamInfoLoan {
        std::vector<std::shared_ptr<::dds::core::xtypes::DynamicType>> types;
        std::vector<std::unique_ptr<::rti::recording::StreamInfo>> streams;
    };
    std::unordered_map<::rti::routing::StreamInfo*, StreamInfoLoan> loans_;
    std::unordered_map<std::string, std::shared_ptr<::dds::core::xtypes::DynamicType>> types_;
    std::mutex state_mutex_;

    int64_t service_start_time_;
    int64_t service_end_time_;
    bool finished_;

    Logger logger;
};

}  // rti::mcap::storage

#endif // RTI_RECORDING_STORAGE_MCAPREADER_H_