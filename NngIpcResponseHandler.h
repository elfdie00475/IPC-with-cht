#ifndef LLT_NNGIPC_IPCRESPONSEHANDLER_H
#define LLT_NNGIPC_IPCRESPONSEHANDLER_H

#include <memory>
#include <string>
#include <vector>

#include <nng/nng.h>
#include <nng/protocol/reqrep0/rep.h>

#ifndef LLT_NNGIPC_IPCOUTPUTCALLBACK_DEFIEND
#define LLT_NNGIPC_IPCOUTPUTCALLBACK_DEFIEND
namespace llt::nngipc {

typedef void (*OutputCallback) (const uint8_t *, size_t, uint8_t **, size_t *);

struct Worker {
    enum { INIT, RECV, SEND, ERROR } state;
    nng_socket sock;
    nng_aio *aio;
    nng_ctx ctx;
    OutputCallback cb;
    bool stopping;
};

} //namespace llt::nngipc
#endif // LLT_NNGIPC_IPCOUTPUTCALLBACK_DEFIEND

namespace llt {
namespace nngipc {

class ResponseHandler
{
public:
    static std::shared_ptr<ResponseHandler> create(
        const char *ipc_name, uint32_t worker_num = 1, OutputCallback cb = nullptr);

public:
    ~ResponseHandler();

    bool init(void);

    bool start(void);

    bool stop(void);

    bool release(void);

private:
    ResponseHandler(const char *ipc_name, uint32_t worker_num, OutputCallback cb);

    const std::string m_ipcName;
    uint32_t m_workerNum;
    nng_socket m_sock;
    OutputCallback m_outputCb;
    std::vector<Worker *> m_workers;
    bool m_init;

}; // class ResponseHandler

} // namespace nngipc
} // namespace llt

#endif /* LLT_NNGIPC_IPCRESPONSEHANDLER_H */
