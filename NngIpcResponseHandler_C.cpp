
#include <memory>

#include "NngIpcResponseHandler.h"
#include "NngIpcResponseHandler_C.h"

using namespace llt::nngipc;

struct RespHandlerWrapper {
    std::shared_ptr<ResponseHandler> sp;
};

extern "C" {

NngIpcResponseHandle nngipc_ResponseHandler_create(
    const char *ipc_name, uint32_t worker_num, OutputCallback_C cb, void *cb_param)
{
    auto wrapper = new (std::nothrow) RespHandlerWrapper();
    if (!wrapper) return NULL;

    wrapper->sp = ResponseHandler::create(ipc_name, worker_num, cb, cb_param);
    if (!wrapper->sp) {
        delete wrapper;
        return NULL;
    }

    if (!wrapper->sp->start()) {
        wrapper->sp.reset();
        delete wrapper;
        return NULL;
    }

    return (NngIpcResponseHandle)wrapper;
}

void nngipc_ResponseHandler_free(NngIpcResponseHandle *pHandle)
{
    if (!pHandle || !(*pHandle)) return;

    auto wrapper = (RespHandlerWrapper *)(*pHandle);
    if (wrapper->sp) {
        wrapper->sp->stop();
        wrapper->sp.reset();
    }
    delete wrapper;
    *pHandle = NULL;
}

} // extern "C"
