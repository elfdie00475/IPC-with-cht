
#include <memory>
#include <string>

#include "NngIpcSubscribeHandler.h"
#include "NngIpcSubscribeHandler_C.h"

using namespace llt::nngipc;

struct SubHandlerWrapper {
    std::shared_ptr<SubscribeHandler> sp;
};

extern "C" {

typedef void *NngIpcSubscribeHandle;

NngIpcSubscribeHandle nngipc_SubscribeHandler_create(
    const char *ipc_name, uint32_t worker_num, OutputCallback_C cb, void *cb_param)
{
    auto wrapper = new (std::nothrow) SubHandlerWrapper();
    if (!wrapper) return NULL;

    wrapper->sp = SubscribeHandler::create(ipc_name, worker_num, cb, cb_param);
    if (!wrapper->sp) {
        delete wrapper;
        return NULL;
    }

    if (!wrapper->sp->start()) {
        wrapper->sp.reset();
        delete wrapper;
        return NULL;
    }

    return (NngIpcSubscribeHandle)wrapper;
}

void nngipc_SubscribeHandler_free(NngIpcSubscribeHandle *pHandle)
{
    if (!pHandle || !(*pHandle)) return;

    auto wrapper = (SubHandlerWrapper *)(*pHandle);
    if (wrapper->sp) {
        wrapper->sp->stop();
        wrapper->sp.reset();
    }
    delete wrapper;
    *pHandle = NULL;
}

int nngipc_SubscribeHandler_subscribe(NngIpcSubscribeHandle handle, const char *topic, size_t topic_size)
{
    if (!handle || !topic) return -1;
    // allow topic_size == 0 ( allow "" means all)

    auto wrapper = (SubHandlerWrapper *)(handle);
    if (wrapper->sp) {
        std::string top;
        if (topic_size == 0) {
            top = "";
        } else {
            top.assign(topic, topic_size);
        }

        int rc = wrapper->sp->subscribe(top);
        if (!rc) return -2;
    }

    return 0;
}

int nngipc_SubscribeHandler_unsubscribe(NngIpcSubscribeHandle handle, const char *topic, size_t topic_size)
{
    if (!handle || !topic) return -1;
    // allow topic_size == 0

    auto wrapper = (SubHandlerWrapper *)(handle);
    if (wrapper->sp) {
        std::string top;
        if (topic_size == 0) {
            top = "";
        } else {
            top.assign(topic, topic_size);
        }

        int rc = wrapper->sp->unsubscribe(top);
        if (!rc) return -2;
    }

    return 0;
}

} // extern "C"
