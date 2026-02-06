#ifndef ZWSYSTEM_IPC_EVENT_H
#define ZWSYSTEM_IPC_EVENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ZS_IPC_OUTPUTCALLBACK_DEFINED
#define ZS_IPC_OUTPUTCALLBACK_DEFINED
typedef void (*ZS_IPC_OutputCallback) (void *, const uint8_t *, size_t, uint8_t **, size_t *);
#endif // ZS_IPC_OUTPUTCALLBACK_DEFINED

typedef void *ZSIPC_EventHandle;

ZSIPC_EventHandle zw_ipc_createEventHandle(void);
void zw_ipc_freeEventHandle(ZSIPC_EventHandle *pHandle);
int zs_ipc_startListenEvent(ZSIPC_EventHandle handle, ZS_IPC_OutputCallback cb, void *cb_param, uint32_t worker_num);
int zs_ipc_stopListenEvent(ZSIPC_EventHandle handle);
int zs_ipc_subscribeEvent(ZSIPC_EventHandle handle, const char *topic, size_t topic_size);
int zs_ipc_unsubscribeEvent(ZSIPC_EventHandle handle, const char *topic, size_t topic_size);

int zs_ipc_sendEvent(ZSIPC_EventHandle handle, const char *event_topic,
        const uint8_t *data, size_t data_size);
int zs_ipc_checkEventWithTopic(ZSIPC_EventHandle handle, const char *event_topic,
        const uint8_t *data, size_t data_size, void **ppOutPayloadPtr, uint32_t *pOutPayloadSize);

#ifdef __cplusplus
}
#endif

#endif /* ZWSYSTEM_IPC_EVENT_H */
