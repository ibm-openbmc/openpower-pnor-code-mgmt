#pragma once

#include "watch.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

/** @class HostFwWatch
 *
 *  @brief Adds inotify watch on host-fw patch files to load them into a volume
 *
 *  The inotify watch is hooked up with sd-event, so that on call back,
 *  a volume with the host-fw patch file is created.
 */
class HostFwWatch
{
  public:
    /** @brief ctor - hook inotify watch with sd-event
     *
     *  @param[in] loop - sd-event object
     *  @param[in] hostFwCallback - The callback function for creating a
     *                              host-fw volume.
     */
    HostFwWatch(sd_event* loop, std::function<void()> hostFwCallback);

    HostFwWatch(const HostFwWatch&) = delete;
    HostFwWatch& operator=(const HostFwWatch&) = delete;
    HostFwWatch(HostFwWatch&&) = delete;
    HostFwWatch& operator=(HostFwWatch&&) = delete;

    /** @brief dtor - remove inotify watch
     */
    ~HostFwWatch();

  private:
    /** @brief sd-event callback
     *
     *  @param[in] s - event source, floating (unused) in our case
     *  @param[in] fd - inotify fd
     *  @param[in] revents - events that matched for fd
     *  @param[in] userdata - pointer to HostFwWatch object
     *  @returns 0 on success, -1 on fail
     */
    static int callback(sd_event_source* s, int fd, uint32_t revents,
                        void* userdata);

    /**  initialize an inotify instance and returns file descriptor */
    int inotifyInit();

    /** @brief PNOR symlink file watch descriptor */
    int wd = -1;

    /** @brief event source */
    EventSourcePtr eventSource;

    /** @brief The callback function for creating the host-fw volume */
    std::function<void()> hostFwCallback;

    /** @brief inotify file descriptor */
    CustomFd fd;
};

} // namespace updater
} // namespace software
} // namespace openpower
