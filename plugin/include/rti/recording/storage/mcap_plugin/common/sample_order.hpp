#ifndef DDS_MCAP_STORAGE_COMMON_SAMPLEORDER_H
#define DDS_MCAP_STORAGE_COMMON_SAMPLEORDER_H

#include <mcap/reader.hpp>

namespace rti::recording::storage::mcap_plugin::sample_order {

    /**
     * Supported sample order kinds for replay.
    */
    enum SampleOrderKind {
        RECEPTION_TIMESTAMP,
        /* SOURCE_TIMESTAMP */
    };

    /**
     * Map between DDS sample order and MCAP read order
    */
    static const std::unordered_map<SampleOrderKind, mcap::ReadMessageOptions::ReadOrder> SampleOrderMap = {
        {SampleOrderKind::RECEPTION_TIMESTAMP, mcap::ReadMessageOptions::ReadOrder::LogTimeOrder},
        /* {SampleOrderKind::SOURCE_TIMESTAMP, mcap::ReadMessageOptions::ReadOrder::PublishTimeOrder} */
    };

    SampleOrderKind sample_order_from_string(const std::string & sample_order_str) {
        if (sample_order_str == "RECEPTION_TIMESTAMP") {
            return SampleOrderKind::RECEPTION_TIMESTAMP;
        }
        /*
        else if (sample_order_str == "SOURCE_TIMESTAMP") {
            return SampleOrderKind::SOURCE_TIMESTAMP;
        }
        */
        else {
            // TODO: log error
            throw std::runtime_error("Unsupported sample order: " + sample_order_str);
        }
    }

    mcap::ReadMessageOptions::ReadOrder reverse_read_order(mcap::ReadMessageOptions::ReadOrder readOrder) {
        switch (readOrder)
        {
        case mcap::ReadMessageOptions::ReadOrder::LogTimeOrder:
            return mcap::ReadMessageOptions::ReadOrder::ReverseLogTimeOrder;
        case mcap::ReadMessageOptions::ReadOrder::ReverseLogTimeOrder:
            return mcap::ReadMessageOptions::ReadOrder::LogTimeOrder;
        
        /*
        case mcap::ReadMessageOptions::ReadOrder::PublishTimeOrder:
            return mcap::ReadMessageOptions::ReadOrder::ReversePublishTimeOrder;
        case mcap::ReadMessageOptions::ReadOrder::ReversePublishTimeOrder:
            return mcap::ReadMessageOptions::ReadOrder::PublishTimeOrder;
        */
        
        default:
            // TODO: log error
            throw std::runtime_error("Unsupported mcap::ReadMessageOptions::ReadOrder");
        }
    };

};  // rti::recording::storage::mcap_plugin::sample_order


#endif // DDS_MCAP_STORAGE_COMMON_SAMPLEORDER_H