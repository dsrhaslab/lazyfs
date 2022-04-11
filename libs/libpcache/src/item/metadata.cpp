
#include <cache/item/metadata.hpp>
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <time.h>

using namespace std;

namespace cache::item {

int timespec2str (char* buf, int len, struct timespec* ts) {
    int ret;
    struct tm t;

    tzset ();
    if (localtime_r (&(ts->tv_sec), &t) == NULL)
        return 1;

    ret = strftime (buf, len, "%F %T", &t);
    if (ret == 0)
        return 2;
    len -= ret - 1;

    ret = snprintf (&buf[strlen (buf)], len, ".%09ld", ts->tv_nsec);
    if (ret >= len)
        return 3;

    return 0;
}

Metadata::Metadata () { size = 0; }

void Metadata::print_metadata () {

    char atime_buf[30];
    char mtime_buf[30];
    char ctime_buf[30];

    timespec2str (atime_buf, sizeof (atime_buf), &this->atim);
    timespec2str (mtime_buf, sizeof (mtime_buf), &this->mtim);
    timespec2str (ctime_buf, sizeof (ctime_buf), &this->ctim);

    std::printf (
        "\t\t> size = %d\n\t\t> access_time = %s\n\t\t> modify_time = %s\n\t\t> change_time = %s\n",
        (int)this->size,
        atime_buf,
        mtime_buf,
        ctime_buf);
}

} // namespace cache::item
