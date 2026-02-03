#ifndef LLT_NNGIPC_IPCRESPONSEHANDLER_C_H
#define LLT_NNGIPC_IPCRESPONSEHANDLER_C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LLT_NNGIPC_C_OUTPUTCALLBACK_DEFINED
#define LLT_NNGIPC_C_OUTPUTCALLBACK_DEFINED
typedef void (*OutputCallback_C) (void *, const uint8_t *, size_t, uint8_t **, size_t *);
#endif // LLT_NNGIPC_C_OUTPUTCALLBACK_DEFINED

typedef void *NngIpcResponseHandle;

NngIpcResponseHandle nngipc_ResponseHandler_create(
    const char *ipc_name, uint32_t worker_num, OutputCallback_C cb, void *cb_param);

void nngipc_ResponseHandler_free(NngIpcResponseHandle *pHandle);

#ifdef __cplusplus
}
#endif

#endif /* LLT_NNGIPC_IPCRESPONSEHANDLER_C_H */
