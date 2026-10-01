#ifndef RTI_MCAP_TEST_SUPPORT_HPP_
#define RTI_MCAP_TEST_SUPPORT_HPP_

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

class TempDirectory {
public:
    TempDirectory()
    {
        static std::atomic<unsigned> counter{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0; attempt < 100; ++attempt) {
            path_ = std::filesystem::temp_directory_path() /
                    ("dds-mcap-" + std::to_string(stamp) + "-" +
                     std::to_string(counter.fetch_add(1)));
            if (std::filesystem::create_directory(path_)) {
                return;
            }
        }
        throw std::runtime_error("Unable to create a unique test directory");
    }

    ~TempDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        if (error) {
            std::fprintf(stderr, "Unable to remove test directory: %s\n", error.message().c_str());
        }
    }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    std::filesystem::path file(const char* name) const
    {
        return path_ / name;
    }

private:
    std::filesystem::path path_;
};

#endif
