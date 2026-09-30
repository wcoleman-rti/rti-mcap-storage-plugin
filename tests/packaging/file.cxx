#include <rti/mcap/file/File.hpp>

int main()
{
    rti::mcap::file::Reader reader;
    rti::mcap::file::Writer writer;
    return reader.is_open() || writer.is_open() ? 1 : 0;
}
