
#include <memory>

#include "NngIpcRequestHandler.h"
#include "NngIpcRequestHandler_C.h"

using namespace llt::nngipc;

struct ReqHandlerWrapper {
    std::shared_ptr<RequestHandler> sp;
};

extern "C" {

NngIpcRequestHandle nngipc_RequestHandler_create(const char *ipc_name)
{
    auto wrapper = new (std::nothrow) ReqHandlerWrapper();
    if (!wrapper) return NULL;

    wrapper->sp = RequestHandler::create(ipc_name);
    if (!wrapper->sp) {
        delete wrapper;
        return NULL;
    }

    return (NngIpcRequestHandle)wrapper;
}

void nngipc_RequestHandler_free(NngIpcRequestHandle *pHandle)
{
    if (!pHandle || !(*pHandle)) return;

    auto wrapper = (ReqHandlerWrapper *)(*pHandle);
    if (wrapper->sp) {
        wrapper->sp.reset();
    }
    delete wrapper;
    *pHandle = NULL;
}

int nngipc_RequestHandler_append(NngIpcRequestHandle handle, const uint8_t *payload, size_t payload_len)
{
    if (!handle) return -1;

    auto wrapper = (ReqHandlerWrapper *)(handle);
    if (wrapper->sp) {
        int rc = wrapper->sp->append(payload, payload_len);
        if (!rc) return -2;
    }

    return 0;
}

int nngipc_RequestHandler_send(NngIpcRequestHandle handle)
{
    if (!handle) return -1;

    auto wrapper = (ReqHandlerWrapper *)(handle);
    if (wrapper->sp) {
        int rc = wrapper->sp->send();
        if (!rc) return -2;
    }

    return 0;
}

int nngipc_RequestHandler_recv(NngIpcRequestHandle handle, uint8_t **payload, size_t *payload_len)
{
    if (!handle) return -1;

    auto wrapper = (ReqHandlerWrapper *)(handle);
    if (wrapper->sp) {
        int rc = wrapper->sp->recv(payload, payload_len);
        if (!rc) return -2;
    }

    return 0;
}

} // extern "C"
