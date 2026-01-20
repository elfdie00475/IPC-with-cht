#ifndef LLT_NNGIPC_IPCSUBSCRIBEHANDLER_H
#define LLT_NNGIPC_IPCSUBSCRIBEHANDLER_H

#include <memory>
#include <string>
#include <vector>

#include <nng/nng.h>
#include <nng/protocol/pubsub0/sub.h>

#include "NngIpcAioWorker.h"

namespace llt {
namespace nngipc {

class SubscribeHandler
{
public:
    static std::shared_ptr<SubscribeHandler> create(
        const char *ipc_name, OutputCallback cb = nullptr);

public:
    ~SubscribeHandler();

    bool init(void);

    bool start(void);

    bool stop(void);

    bool release(void);

    bool subscribe(const std::string& subscribe_str);

    bool unsubscribe(const std::string& subscribe_str);

private:
    SubscribeHandler(const char *ipc_name, OutputCallback cb);

    const std::string m_ipcName;
    nng_socket m_sock;
    bool m_init;
    uint32_t m_workerNum;
    OutputCallback m_outputCB;
    std::vector<std::shared_ptr<AioWorker>> m_workers;

}; // class SubscribeHandler

} // namespace nngipc
} // namespace llt

#endif /* LLT_NNGIPC_IPCSUBSCRIBEHANDLER_H */
