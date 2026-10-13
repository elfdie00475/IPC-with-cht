#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "NngIpcAioWorker.h"
#include <nng/protocol/reqrep0/req.h>
#include <nng/protocol/reqrep0/rep.h>

static std::atomic<unsigned> s_errors{0};

static void s_echo(void *, const uint8_t *input, size_t size,
                   uint8_t **output, size_t *outputSize)
{
    *output = static_cast<uint8_t *>(std::malloc(size));
    if (!*output) { ++s_errors; return; }
    std::memcpy(*output, input, size);
    *outputSize = size;
}

static bool s_exchange(const char *url, const uint8_t *input, size_t size,
                       uint8_t **output, size_t *outputSize)
{
    nng_socket request = NNG_SOCKET_INITIALIZER;
    if (nng_req0_open(&request)) return false;
    int rv = nng_socket_set_ms(request, NNG_OPT_RECVTIMEO, 1000);
    if (!rv) rv = nng_socket_set_ms(request, NNG_OPT_SENDTIMEO, 1000);
    if (!rv) rv = nng_dial(request, url, nullptr, 0);
    if (!rv) rv = nng_send(request, const_cast<uint8_t *>(input), size, 0);
    void *received = nullptr;
    size_t length = 0;
    if (!rv) rv = nng_recv(request, &received, &length, NNG_FLAG_ALLOC);
    if (!rv) {
        *output = static_cast<uint8_t *>(std::malloc(length));
        if (*output) { std::memcpy(*output, received, length); *outputSize = length; }
        else rv = NNG_ENOMEM;
    }
    if (received) nng_free(received, length);
    nng_close(request);
    return rv == 0;
}

static void s_forward(void *, const uint8_t *input, size_t size,
                      uint8_t **output, size_t *outputSize)
{
    if (!s_exchange("inproc://aio-downstream", input, size, output, outputSize))
        ++s_errors;
}

int main()
{
    nng_socket downstream = NNG_SOCKET_INITIALIZER;
    nng_socket frontend = NNG_SOCKET_INITIALIZER;
    if (nng_rep0_open(&downstream) || nng_rep0_open(&frontend)) return 1;
    if (nng_listen(downstream, "inproc://aio-downstream", nullptr, 0) ||
        nng_listen(frontend, "inproc://aio-frontend", nullptr, 0)) return 2;
    std::vector<std::shared_ptr<llt::nngipc::AioWorker>> workers;
    for (unsigned i = 0; i < 5; ++i) {
        auto worker = llt::nngipc::AioWorker::create(
            i == 0 ? downstream : frontend, llt::nngipc::AioWorker::Response,
            i == 0 ? s_echo : s_forward, nullptr);
        if (!worker) return 3;
        worker->start();
        workers.push_back(worker);
    }
    for (unsigned i = 0; i < 1000; ++i) {
        uint8_t *reply = nullptr;
        size_t length = 0;
        bool ok = s_exchange("inproc://aio-frontend",
            reinterpret_cast<const uint8_t *>(&i), sizeof(i), &reply, &length);
        ok = ok && length == sizeof(i) && !std::memcmp(reply, &i, sizeof(i));
        std::free(reply);
        if (!ok) return 4;
    }
    for (auto& worker : workers) worker->stop();
    workers.clear();
    nng_close(frontend);
    nng_close(downstream);
    nng_fini();
    if (s_errors) return 5;
    std::puts("PASS: 1000 forwarded requests with per-call client close; clean shutdown");
    return 0;
}
