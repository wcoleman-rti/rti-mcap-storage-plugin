#ifndef DDS_MCAP_STORAGE_COMMON_MCAPVENDOR_H
#define DDS_MCAP_STORAGE_COMMON_MCAPVENDOR_H

#include <mutex>
#include <mcap/mcap.hpp>

namespace mcap {

    template <typename T>
    class SharedObj {
    public:

        SharedObj() : SharedObj(std::make_shared<T>()) {}

        SharedObj(std::shared_ptr<T> object) : SharedObj(object, std::make_shared<std::mutex>()) {}
        
        SharedObj(std::shared_ptr<T> object, std::shared_ptr<std::mutex> mutex) : object_(object), mutex_(mutex) {}

        SharedObj(const SharedObj& other) : object_(other.object_), 
                                            mutex_(other.mutex_) {}
        
        std::shared_ptr<T> ptr() const {
            return object_;
        }

        void ptr(const std::shared_ptr<T> & other) {
            object_ = other;
        }

        // Overload dereference operator
        T& operator*() const {
            return *object_;
        }

        // Overload arrow operator
        T* operator->() const {
            return object_.get();
        }

        std::mutex& mutex() const {
            return *mutex_;
        }

        // Lock method to return a lock_guard for the mutex
        std::lock_guard<std::mutex> lock() const {
            return std::lock_guard<std::mutex>(*mutex_);
        }

    private:
        std::shared_ptr<T> object_;
        std::shared_ptr<std::mutex> mutex_;
    };

    using SharedMcapWriter = SharedObj<McapWriter>;
    using SharedMcapReader = SharedObj<McapReader>;

    namespace channel {
        const static std::string METADATA_KEY_DDSXML_TYPE = "DDSXML_TYPE";
        const static std::string messageEncodingCdr = "cdr";
    }  // mcap::channel

    namespace schema {
        const static std::string schemaEncodingOmgIdl = "omgidl";
    }  // mcap::schema

} // mcap

#endif // DDS_MCAP_STORAGE_COMMON_MCAPVENDOR_H