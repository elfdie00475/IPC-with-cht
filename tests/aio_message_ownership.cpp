#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

#include "NngIpcAioWorker.h"
#include <nng/protocol/pubsub0/pub.h>

static std::atomic<unsigned> s_freed{0};

extern "C" void __real_nng_msg_free(nng_msg *);
extern "C" void __wrap_nng_msg_free(nng_msg *msg)
{
    if (msg) ++s_freed;
    __real_nng_msg_free(msg);
}

struct CallbackState {
    std::mutex mutex;
    std::condition_variable cv;
    unsigned received = 0;
    bool block = false;
    bool release = false;
};

static void s_receive(void *context, const uint8_t *, size_t,
                      uint8_t **, size_t *)
{
    auto *state = static_cast<CallbackState *>(context);
    std::unique_lock<std::mutex> lock(state->mutex);
    ++state->received;
    state->cv.notify_all();
    state->cv.wait(lock, [&] { return !state->block || state->release; });
}

static bool s_wait(CallbackState& state, unsigned count)
{
    std::unique_lock<std::mutex> lock(state.mutex);
    return state.cv.wait_for(lock, std::chrono::seconds(3),
                             [&] { return state.received >= count; });
}

int main()
{
    nng_socket pub = NNG_SOCKET_INITIALIZER;
    nng_socket sub = NNG_SOCKET_INITIALIZER;
    if (nng_pub0_open(&pub) || nng_sub0_open(&sub)) return 1;
    const char *url = "inproc://aio-message-ownership";
    if (nng_listen(pub, url, nullptr, 0)) return 2;

    CallbackState state;
    auto worker = llt::nngipc::AioWorker::create(
        sub, llt::nngipc::AioWorker::TYPE::Subscribe, s_receive, &state);
    if (!worker || !worker->subscribe("")) return 3;
    worker->start();
    if (nng_dial(sub, url, nullptr, 0)) return 4;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    unsigned char payload[4096] = {};
    for (unsigned i = 1; i <= 1000; ++i) {
        if (nng_send(pub, payload, sizeof(payload), 0) || !s_wait(state, i)) return 5;
    }
    worker->stop();
    worker->stop();
    worker->release();
    worker->release();
    if (s_freed != 1000) {
        std::fprintf(stderr, "received=%u freed=%u\n", state.received, s_freed.load());
        return 6;
    }
    worker.reset();

    // A shutdown must wait for an active callback, then free its input once.
    state.block = true;
    state.release = false;
    worker = llt::nngipc::AioWorker::create(
        sub, llt::nngipc::AioWorker::TYPE::Subscribe, s_receive, &state);
    if (!worker || !worker->subscribe("")) return 7;
    worker->start();
    if (nng_send(pub, payload, sizeof(payload), 0) || !s_wait(state, 1001)) return 8;
    std::atomic<bool> stopped{false};
    std::thread stopper([&] { worker->stop(); stopped = true; });
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    bool waited = !stopped.load();
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.release = true;
    }
    state.cv.notify_all();
    stopper.join();
    worker->release();
    worker.reset();
    nng_close(sub);
    nng_close(pub);
    nng_fini();
    if (!waited || s_freed != 1001) return 9;
    std::puts("PASS: 1000 no-reply messages freed; active callback shutdown; repeated cleanup");
    return 0;
}
