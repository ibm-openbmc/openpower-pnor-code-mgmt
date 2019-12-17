#include "config.h"

#include "host_fw_watch.hpp"

#include "item_updater_ubi.hpp"

#include <sys/inotify.h>
#include <unistd.h>

#include <experimental/filesystem>
#include <functional>

namespace openpower
{
namespace software
{
namespace updater
{

namespace fs = std::experimental::filesystem;

HostFwWatch::HostFwWatch(sd_event* loop, std::function<void()> hostFwCallback) :
    hostFwCallback(hostFwCallback), fd(inotifyInit())

{
    if (!fs::is_directory(HOST_FW_SQUASHFS_PATH))
    {
        fs::create_directories(HOST_FW_SQUASHFS_PATH);
    }

    wd = inotify_add_watch(fd(), HOST_FW_SQUASHFS_PATH, IN_CLOSE_WRITE);
    if (-1 == wd)
    {
        auto error = errno;
        throw std::system_error(error, std::generic_category(),
                                "Error occurred during the inotify_init1");
    }

    decltype(eventSource.get()) sourcePtr = nullptr;
    auto rc = sd_event_add_io(loop, &sourcePtr, fd(), EPOLLIN, callback, this);

    eventSource.reset(sourcePtr);

    if (0 > rc)
    {
        throw std::system_error(-rc, std::generic_category(),
                                "Error occurred during the inotify_init1");
    }
}

HostFwWatch::~HostFwWatch()
{
    if ((-1 != fd()) && (-1 != wd))
    {
        inotify_rm_watch(fd(), wd);
    }
}

int HostFwWatch::callback(sd_event_source* s, int fd, uint32_t revents,
                          void* userdata)
{
    if (!(revents & EPOLLIN))
    {
        return 0;
    }

    constexpr auto maxBytes = 1024;
    uint8_t buffer[maxBytes];
    auto bytes = read(fd, buffer, maxBytes);
    if (0 > bytes)
    {
        auto error = errno;
        throw std::system_error(error, std::generic_category(),
                                "failed to read inotify event");
    }

    auto offset = 0;
    while (offset < bytes)
    {
        auto event = reinterpret_cast<inotify_event*>(&buffer[offset]);
        static_cast<HostFwWatch*>(userdata)->hostFwCallback();

        offset += offsetof(inotify_event, name) + event->len;
    }

    return 0;
}

int HostFwWatch::inotifyInit()
{
    auto fd = inotify_init1(IN_NONBLOCK);

    if (-1 == fd)
    {
        auto error = errno;
        throw std::system_error(error, std::generic_category(),
                                "Error occurred during the inotify_init1");
    }

    return fd;
}

} // namespace updater
} // namespace software
} // namespace openpower
