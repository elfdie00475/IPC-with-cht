#ifndef LLT_NNGIPC_IPCREQUESTHANDLER_C_H
#define LLT_NNGIPC_IPCREQUESTHANDLER_C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *NngIpcRequestHandle;

NngIpcRequestHandle nngipc_RequestHandler_create(const char *ipc_name);

void nngipc_RequestHandler_free(NngIpcRequestHandle *pHandle);

int nngipc_RequestHandler_append(NngIpcRequestHandle handle, const uint8_t *payload, size_t payload_len);

int nngipc_RequestHandler_send(NngIpcRequestHandle handle);

int nngipc_RequestHandler_recv(NngIpcRequestHandle handle, uint8_t **payload, size_t *payload_len);

#ifdef __cplusplus
}
#endif

#endif /* LLT_NNGIPC_IPCREQUESTHANDLER_C_H */
