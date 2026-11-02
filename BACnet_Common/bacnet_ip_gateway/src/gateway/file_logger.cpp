#include "file_logger.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <mutex>
#include <streambuf>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace gateway {

namespace {

// ---------------------------------------------------------------------------
// RotatingFileSink — 크기 제한 로그 파일 (현재 파일 + ".1" 백업 1개)
// ---------------------------------------------------------------------------
class RotatingFileSink {
public:
    RotatingFileSink(const std::string& path, std::size_t max_file_bytes)
        : path_(path), backup_path_(path + ".1"), max_file_bytes_(max_file_bytes) {}

    bool open()
    {
        fd_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
        if (fd_ < 0) {
            return false;
        }
        struct stat st;
        current_bytes_ = (::fstat(fd_, &st) == 0 && st.st_size > 0)
                             ? static_cast<std::size_t>(st.st_size) : 0U;
        if (current_bytes_ >= max_file_bytes_) {
            rotate();
        }
        return fd_ >= 0;
    }

    void write(const char* data, std::size_t len)
    {
        if (fd_ < 0 || len == 0U) {
            return;
        }
        if (current_bytes_ + len > max_file_bytes_) {
            rotate();
            if (fd_ < 0) {
                return;
            }
        }
        std::size_t done = 0U;
        while (done < len) {
            const ssize_t n = ::write(fd_, data + done, len - done);
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return; // 디스크 full 등: 로그 손실은 허용하고 본 기능은 계속 동작
            }
            done += static_cast<std::size_t>(n);
        }
        current_bytes_ += done;
    }

    std::mutex& mutex() { return mutex_; }

private:
    void rotate()
    {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
        (void)std::rename(path_.c_str(), backup_path_.c_str()); // 이전 .1 은 원자적으로 대체됨
        fd_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_CLOEXEC, 0644);
        current_bytes_ = 0U;
    }

    std::string  path_;
    std::string  backup_path_;
    std::size_t  max_file_bytes_;
    std::size_t  current_bytes_ = 0U;
    int          fd_ = -1;
    std::mutex   mutex_;
};

// ---------------------------------------------------------------------------
// TeeLogBuf — 콘솔 streambuf 와 로그 파일에 동시에 기록하는 streambuf
// ---------------------------------------------------------------------------
// 여러 스레드가 "<<" 조각 단위로 출력하면 줄 중간이 서로 섞이므로, 스레드별 버퍼에
// 모았다가 완성된 줄 단위로 콘솔과 파일에 원자적으로 기록한다.
// - '\n' 이 들어오면 완성된 줄까지 즉시 기록한다.
// - flush(sync) 시 미완성 줄도 기록한다 (예: 줄바꿈 없는 진행 메시지).
//   단, std::cerr 는 unitbuf(매 출력마다 flush)라서 조각마다 기록되지 않도록
//   flush_partial_on_sync=false 로 두고 줄바꿈 또는 버퍼 한도에서만 기록한다.
constexpr std::size_t kStreamCount      = 3U;     // cout, cerr, clog
constexpr std::size_t kMaxPendingBytes  = 4096U;  // 줄바꿈 없는 출력의 강제 기록 한도

thread_local std::string t_pending[kStreamCount];
thread_local bool        t_mid_line[kStreamCount] = {false, false, false};

class TeeLogBuf : public std::streambuf {
public:
    TeeLogBuf(std::streambuf* console, RotatingFileSink& sink, char level,
              std::size_t index, bool flush_partial_on_sync)
        : console_(console), sink_(sink), level_(level), index_(index),
          flush_partial_on_sync_(flush_partial_on_sync) {}

    std::streambuf* console() const { return console_; }

    /// 호출 스레드의 미완성 출력까지 모두 기록한다.
    void flushPending()
    {
        std::string& pending = t_pending[index_];
        if (!pending.empty()) {
            emit(pending.data(), pending.size());
            pending.clear();
        }
    }

protected:
    int_type overflow(int_type ch) override
    {
        if (traits_type::eq_int_type(ch, traits_type::eof())) {
            return traits_type::not_eof(ch);
        }
        const char c = traits_type::to_char_type(ch);
        return (xsputn(&c, 1) == 1) ? ch : traits_type::eof();
    }

    std::streamsize xsputn(const char* s, std::streamsize n) override
    {
        if (n <= 0) {
            return 0;
        }
        std::string& pending = t_pending[index_];
        pending.append(s, static_cast<std::size_t>(n));
        const std::size_t last_nl = pending.rfind('\n');
        if (last_nl != std::string::npos) {
            emit(pending.data(), last_nl + 1U);
            pending.erase(0, last_nl + 1U);
        }
        if (pending.size() >= kMaxPendingBytes) {
            emit(pending.data(), pending.size());
            pending.clear();
        }
        return n;
    }

    int sync() override
    {
        if (flush_partial_on_sync_) {
            flushPending();
        }
        std::lock_guard<std::mutex> lock(sink_.mutex());
        return console_->pubsync();
    }

private:
    void emit(const char* s, std::size_t n)
    {
        std::lock_guard<std::mutex> lock(sink_.mutex());
        (void)console_->sputn(s, static_cast<std::streamsize>(n));
        // 접두사와 줄 내용을 한 번에 기록하여 파일 교체(rotate)가 줄 중간에서 일어나지 않게 한다.
        bool& mid_line = t_mid_line[index_];
        std::string out;
        out.reserve(n + 64U);
        for (std::size_t i = 0U; i < n; ++i) {
            if (!mid_line) {
                appendPrefix(out);
                mid_line = true;
            }
            out.push_back(s[i]);
            if (s[i] == '\n') {
                mid_line = false;
            }
        }
        sink_.write(out.data(), out.size());
    }

    void appendPrefix(std::string& out) const
    {
        struct timespec ts;
        ::clock_gettime(CLOCK_REALTIME, &ts);
        struct tm tm_buf;
        ::localtime_r(&ts.tv_sec, &tm_buf);
        char prefix[40];
        const int len = std::snprintf(prefix, sizeof(prefix),
                                      "%04d-%02d-%02d %02d:%02d:%02d.%03ld %c ",
                                      tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                                      tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                                      static_cast<long>(ts.tv_nsec / 1000000L), level_);
        if (len > 0) {
            out.append(prefix, static_cast<std::size_t>(len));
        }
    }

    std::streambuf*    console_;
    RotatingFileSink&  sink_;
    char               level_;
    std::size_t        index_;
    bool               flush_partial_on_sync_;
};

// 프로세스 종료 시점까지 cout/cerr 가 참조할 수 있도록 의도적으로 해제하지 않는다.
RotatingFileSink* g_sink    = nullptr;
TeeLogBuf*        g_out_buf = nullptr;
TeeLogBuf*        g_err_buf = nullptr;
TeeLogBuf*        g_log_buf = nullptr;

} // namespace

constexpr const char* FileLogger::kDefaultPath;
constexpr std::size_t FileLogger::kDefaultMaxKb;
constexpr std::size_t FileLogger::kMinMaxKb;

bool FileLogger::installFromEnvironment()
{
    std::string path = kDefaultPath;
    const char* env_path = std::getenv("BACNET_LOG_FILE");
    if (env_path != nullptr) {
        path = env_path;
    }
    if (path.empty() || path == "none") {
        return false;
    }

    std::size_t max_kb = kDefaultMaxKb;
    const char* env_kb = std::getenv("BACNET_LOG_MAX_KB");
    if (env_kb != nullptr && env_kb[0] != '\0') {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(env_kb, &end, 10);
        if (end != nullptr && *end == '\0' && parsed > 0UL) {
            max_kb = static_cast<std::size_t>(parsed);
        }
    }
    if (max_kb < kMinMaxKb) {
        max_kb = kMinMaxKb;
    }
    return install(path, max_kb * 1024U);
}

bool FileLogger::install(const std::string& path, std::size_t max_total_bytes)
{
    if (g_sink != nullptr) {
        return true;
    }
    RotatingFileSink* sink = new RotatingFileSink(path, max_total_bytes / 2U);
    if (!sink->open()) {
        std::cerr << "[Log] File logging disabled: cannot open '" << path << "' ("
                  << std::strerror(errno) << "). Console output only." << std::endl;
        delete sink;
        return false;
    }
    g_sink    = sink;
    g_out_buf = new TeeLogBuf(std::cout.rdbuf(), *g_sink, 'I', 0U, true);
    g_err_buf = new TeeLogBuf(std::cerr.rdbuf(), *g_sink, 'E', 1U, false);
    g_log_buf = new TeeLogBuf(std::clog.rdbuf(), *g_sink, 'I', 2U, true);
    std::cout.rdbuf(g_out_buf);
    std::cerr.rdbuf(g_err_buf);
    std::clog.rdbuf(g_log_buf);
    std::cout << "[Log] File logging enabled: " << path << " (max "
              << (max_total_bytes / 1024U) << " KB, rotate to " << path << ".1)" << std::endl;
    return true;
}

void FileLogger::uninstall()
{
    if (g_sink == nullptr) {
        return;
    }
    std::cout.flush();
    std::cerr.flush();
    std::clog.flush();
    g_err_buf->flushPending(); // 호출 스레드의 줄바꿈 없는 stderr 출력까지 기록
    std::cout.rdbuf(g_out_buf->console());
    std::cerr.rdbuf(g_err_buf->console());
    std::clog.rdbuf(g_log_buf->console());
}

} // namespace gateway
