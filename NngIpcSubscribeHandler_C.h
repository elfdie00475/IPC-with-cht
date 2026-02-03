#ifndef LLT_NNGIPC_IPCSUBSCRIBEHANDLER_C_H
#define LLT_NNGIPC_IPCSUBSCRIBEHANDLER_C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LLT_NNGIPC_C_OUTPUTCALLBACK_DEFINED
#define LLT_NNGIPC_C_OUTPUTCALLBACK_DEFINED
typedef void (*OutputCallback_C) (void *, const uint8_t *, size_t, uint8_t **, size_t *);
#endif // LLT_NNGIPC_C_OUTPUTCALLBACK_DEFINED

typedef void *NngIpcSubscribeHandle;

NngIpcSubscribeHandle nngipc_SubscribeHandler_create(
    const char *ipc_name, uint32_t worker_num, OutputCallback_C cb, void *cb_param);

void nngipc_SubscribeHandler_free(NngIpcSubscribeHandle *pHandle);

int nngipc_SubscribeHandler_subscribe(NngIpcSubscribeHandle handle, const char *topic, size_t topic_size);

int nngipc_SubscribeHandler_unsubscribe(NngIpcSubscribeHandle handle, const char *topic, size_t topic_size);

#ifdef __cplusplus
}
#endif

#endif /* LLT_NNGIPC_IPCSUBSCRIBEHANDLER_C_H */
