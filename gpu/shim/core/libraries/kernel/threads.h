// bbport: host threads that may call guest code (AvPlayer allocator callbacks).
// Each thread gets a guest TCB (GS base, TLS) from the C runtime before running.
#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>
#include "common/types.h"

extern "C" void runtime_thread_attach_host(const char* name);

namespace Libraries::Kernel {
/// Stop and Join may come from several threads at once: AvPlayer's demuxer joins the decoders at
/// the end of a movie while they finish (and "join" themselves), and the game stops the player
/// meanwhile. One caller joins; the others wait for it, so every Stop returns with the thread
/// ended. A second pthread_join of the same thread failed with EINVAL on macOS (libc++ threw).
class Thread {
public:
    Thread() = default;
    ~Thread() { Stop(); }
    void Run(std::function<void(std::stop_token)>&& func) {
        std::scoped_lock lk{mutex};
        thread = std::jthread([func = std::move(func)](std::stop_token stop) {
            runtime_thread_attach_host("bb:hle");
            func(stop);
        });
        id = thread.get_id();
        stop_source = thread.get_stop_source();
    }
    // A thread may stop its own Thread object (AvPlayer does); it detaches instead of joining,
    // unless another thread is joining it already.
    void Join() {
        std::unique_lock lk{mutex};
        if (id == std::this_thread::get_id()) {
            if (thread.joinable() && !joining) thread.detach();
            return;
        }
        if (joining) {
            joined.wait(lk, [this] { return !joining; });
            return;
        }
        if (!thread.joinable()) return;
        joining = true;
        std::jthread ending = std::move(thread);
        lk.unlock();
        ending.join();
        lk.lock();
        joining = false;
        joined.notify_all();
    }
    bool Joinable() const {
        std::scoped_lock lk{mutex};
        return thread.joinable() || joining;
    }
    void Stop() {
        {
            std::scoped_lock lk{mutex};
            if (!thread.joinable() && !joining) return;
            stop_source.request_stop();
        }
        Join();
    }

private:
    mutable std::mutex mutex;
    std::condition_variable joined;
    std::jthread thread;
    std::thread::id id;
    std::stop_source stop_source{std::nostopstate};
    bool joining = false;
};
} // namespace Libraries::Kernel
