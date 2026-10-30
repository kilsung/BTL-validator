#pragma once

#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <poll.h>
#include <cstring>
#include <iostream>

namespace modbus {

/**
 * @brief Inotify-based virtual/physical serial port creation/reconnection watcher.
 *
 * Monitors parent directory of target device node (e.g., /dev for /dev/ttyMASTER2)
 * for creation, symlink updates, attribute changes, and deletions.
 */
class InotifyPortWatcher {
public:
    InotifyPortWatcher() : inotify_fd_(-1), watch_wd_(-1) {}

    ~InotifyPortWatcher() {
        closeWatcher();
    }

    // Disable copy
    InotifyPortWatcher(const InotifyPortWatcher&) = delete;
    InotifyPortWatcher& operator=(const InotifyPortWatcher&) = delete;

    bool init(const std::string& device_path) {
        closeWatcher();
        device_path_ = device_path;

        if (device_path.empty() || device_path == "none" || device_path == "auto") {
            return false;
        }

        // Split device_path into parent dir and filename
        size_t last_slash = device_path.find_last_of('/');
        if (last_slash == std::string::npos) {
            dir_path_ = ".";
            file_name_ = device_path;
        } else if (last_slash == 0) {
            dir_path_ = "/";
            file_name_ = device_path.substr(1);
        } else {
            dir_path_ = device_path.substr(0, last_slash);
            file_name_ = device_path.substr(last_slash + 1);
        }

        inotify_fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (inotify_fd_ < 0) {
            std::cerr << "[InotifyPortWatcher] inotify_init1 failed: " << strerror(errno)
                      << " (will use fallback timer polling)" << std::endl;
            return false;
        }

        watch_wd_ = inotify_add_watch(inotify_fd_, dir_path_.c_str(),
                                      IN_CREATE | IN_MOVED_TO | IN_ATTRIB | IN_DELETE | IN_MODIFY);
        if (watch_wd_ < 0) {
            std::cerr << "[InotifyPortWatcher] Warning: inotify_add_watch failed for '"
                      << dir_path_ << "': " << strerror(errno)
                      << " (will use fallback timer polling)" << std::endl;
            ::close(inotify_fd_);
            inotify_fd_ = -1;
            return false;
        }

        return true;
    }

    /**
     * @brief Waits for device creation/modification events or timeout.
     * @param timeout_ms Fallback timer in milliseconds (e.g. 1000ms).
     * @return true if an inotify event matching the device was detected, false on timeout/error.
     */
    bool waitForDevice(int timeout_ms = 1000) {
        if (inotify_fd_ < 0) {
            if (timeout_ms > 0) {
                usleep(static_cast<useconds_t>(timeout_ms * 1000));
            }
            return false;
        }

        struct pollfd pfd;
        pfd.fd = inotify_fd_;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, timeout_ms);
        if (ret <= 0) {
            return false; // Timeout or signal
        }

        if (pfd.revents & POLLIN) {
            char buffer[4096];
            bool matched = false;
            ssize_t len = ::read(inotify_fd_, buffer, sizeof(buffer));
            if (len > 0) {
                ssize_t i = 0;
                while (i < len) {
                    struct inotify_event* event = reinterpret_cast<struct inotify_event*>(&buffer[i]);
                    if (event->len > 0) {
                        if (file_name_ == event->name) {
                            matched = true;
                        }
                    }
                    i += sizeof(struct inotify_event) + event->len;
                }
            }
            return matched;
        }

        return false;
    }

    void closeWatcher() {
        if (inotify_fd_ >= 0) {
            if (watch_wd_ >= 0) {
                inotify_rm_watch(inotify_fd_, watch_wd_);
                watch_wd_ = -1;
            }
            ::close(inotify_fd_);
            inotify_fd_ = -1;
        }
    }

    bool isWatching() const {
        return inotify_fd_ >= 0;
    }

    const std::string& getDevicePath() const { return device_path_; }
    const std::string& getDirPath() const { return dir_path_; }
    const std::string& getFileName() const { return file_name_; }

private:
    int inotify_fd_;
    int watch_wd_;
    std::string device_path_;
    std::string dir_path_;
    std::string file_name_;
};

} // namespace modbus
