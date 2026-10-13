#ifndef LLT_NNGIPC_IPCRESPONSEHANDLER_H
#define LLT_NNGIPC_IPCRESPONSEHANDLER_H

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <nng/nng.h>
#include <nng/protocol/reqrep0/rep.h>

#include "NngIpcAioWorker.h"

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

private:
    std::mutex m_mutex;

    const std::string m_ipcName;
    nng_socket m_sock;
    bool m_init;
    uint32_t m_workerNum;
    OutputCallback m_outputCB;
    std::vector<std::shared_ptr<AioWorker>> m_workers;

}; // class ResponseHandler

} // namespace nngipc
} // namespace llt

#endif /* LLT_NNGIPC_IPCRESPONSEHANDLER_H */
