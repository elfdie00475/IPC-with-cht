#ifndef LLT_NNGIPC_IPCAIOWORKER_H
#define LLT_NNGIPC_IPCAIOWORKER_H

#include <memory>
#include <string>
#include <vector>
#include <mutex>

#include <nng/nng.h>
#include <nng/protocol/pubsub0/sub.h>

namespace llt {
namespace nngipc {

typedef void (*OutputCallback) (const uint8_t *, size_t, uint8_t **, size_t *);

class AioWorker {
public:
    enum STATE { INIT, RECV, SEND, ERROR };
    enum TYPE { Response, Subscribe };

public:
    static std::shared_ptr<AioWorker> create(nng_socket sock, TYPE type, OutputCallback cb);

public:
    ~AioWorker();

    bool init(void);

    void start(void);

    void stop(void);

    bool release(void);

    bool subscribe(const std::string& subscribe_str);

    bool unsubscribe(const std::string& subscribe_str);

private:
    AioWorker(nng_socket sock, TYPE type, OutputCallback cb);

    static void process_wrapper(void *arg);

    void process(void);

private:
    std::mutex m_stateMutex;

    nng_socket m_sock;
    nng_aio *m_aio;
    nng_ctx m_ctx;
    OutputCallback m_cb;
    STATE m_state;
    TYPE m_type;
    bool m_stopping;
};

} // namespace nngipc
} // namespace llt

#endif /* LLT_NNGIPC_IPCAIOWORKER_H */
