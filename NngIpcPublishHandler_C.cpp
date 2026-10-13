
#include <memory>

#include "NngIpcPublishHandler.h"
#include "NngIpcPublishHandler_C.h"

using namespace llt::nngipc;

struct PubHandlerWrapper {
    std::shared_ptr<PublishHandler> sp;
};

extern "C" {

NngIpcPublishHandle nngipc_PublishHandler_create(const char *ipc_name, bool proxyMode)
{
    auto wrapper = new (std::nothrow) PubHandlerWrapper();
    if (!wrapper) return NULL;

    wrapper->sp = PublishHandler::create(ipc_name, proxyMode);
    if (!wrapper->sp) {
        delete wrapper;
        return NULL;
    }

    return (NngIpcPublishHandle)wrapper;
}

void nngipc_PublishHandler_free(NngIpcPublishHandle *pHandle)
{
    if (!pHandle || !(*pHandle)) return;

    auto wrapper = (PubHandlerWrapper *)(*pHandle);
    if (wrapper->sp) {
        wrapper->sp.reset();
    }
    delete wrapper;
    *pHandle = NULL;
}

int nngipc_PublishHandler_append(NngIpcPublishHandle handle, const uint8_t *payload, size_t payload_len)
{
    if (!handle || !payload || payload_len == 0) return -1;

    auto wrapper = (PubHandlerWrapper *)(handle);
    if (wrapper->sp) {
        bool rc = wrapper->sp->append(payload, payload_len);
        if (!rc) return -2;
    }

    return 0;
}

int nngipc_PublishHandler_send(NngIpcPublishHandle handle)
{
    if (!handle) return -1;

    auto wrapper = (PubHandlerWrapper *)(handle);
    if (wrapper->sp) {
        bool rc = wrapper->sp->send();
        if (!rc) return -2;
    }

    return 0;
}

} // extern "C"
