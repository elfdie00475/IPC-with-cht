#ifndef LLT_NNGIPC_IPCAIOWORKER_H
#define LLT_NNGIPC_IPCAIOWORKER_H

#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>

#include <nng/nng.h>
#include <nng/protocol/pubsub0/sub.h>

namespace llt {
namespace nngipc {

typedef void (*OutputCallback) (void *, const uint8_t *, size_t, uint8_t **, size_t *);

class AioWorker {
public:
    enum STATE { INIT, RECV, SEND, ERROR };
    enum TYPE { Response, Subscribe };

public:
    static std::shared_ptr<AioWorker> create(nng_socket sock, TYPE type, 
        OutputCallback cb, void *cb_param);

public:
    ~AioWorker();

    bool init(void);

    void start(void);

    void stop(void);

    bool release(void);

    bool subscribe(const std::string& subscribe_str);

    bool unsubscribe(const std::string& subscribe_str);

private:
    AioWorker(nng_socket sock, TYPE type, OutputCallback cb, void *cb_param);

    void workerLoop(void);
    void process(void);

private:
    std::mutex m_stateMutex;
    std::thread m_thread;

    nng_socket m_sock;
    nng_aio *m_aio = nullptr;
    nng_ctx m_ctx  = NNG_CTX_INITIALIZER;
    OutputCallback m_cb;
    void *m_cbParam;
    STATE m_state;
    TYPE m_type;
    // Callbacks run on our worker, so synchronous forwarded requests and socket
    // close cannot block NNG's completion threads. A callback must not stop or
    // release its own worker; the owning process performs lifecycle operations.
    std::atomic<bool> m_stopping;
};

} // namespace nngipc
} // namespace llt

#endif /* LLT_NNGIPC_IPCAIOWORKER_H */
