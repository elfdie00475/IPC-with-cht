#ifndef LLT_NNGIPC_IPCPUBLISHHANDLER_C_H
#define LLT_NNGIPC_IPCPUBLISHHANDLER_C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *NngIpcPublishHandle;

NngIpcPublishHandle nngipc_PublishHandler_create(const char *ipc_name, bool proxyMode);

void nngipc_PublishHandler_free(NngIpcPublishHandle *pHandle);

int nngipc_PublishHandler_append(NngIpcPublishHandle handle, const uint8_t *payload, size_t payload_len);

int nngipc_PublishHandler_send(NngIpcPublishHandle handle);

#ifdef __cplusplus
}
#endif

#endif /* LLT_NNGIPC_IPCPUBLISHHANDLER_C_H */
