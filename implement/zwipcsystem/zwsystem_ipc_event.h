#ifndef ZWSYSTEM_IPC_EVENT_H
#define ZWSYSTEM_IPC_EVENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ZSIPC_OUTPUTCALLBACK_DEFINED
#define ZSIPC_OUTPUTCALLBACK_DEFINED
typedef void (*ZSIPC_OutputCallback) (void *, const uint8_t *, size_t, uint8_t **, size_t *);
#endif // ZSIPC_OUTPUTCALLBACK_DEFINED

typedef void *ZSIPC_EventHandle;

ZSIPC_EventHandle zsipc_createEventHandle(void);
void zsipc_freeEventHandle(ZSIPC_EventHandle *pHandle);
int zsipc_startListenEvent(ZSIPC_EventHandle handle, ZSIPC_OutputCallback cb, void *cb_param, uint32_t worker_num);
int zsipc_stopListenEvent(ZSIPC_EventHandle handle);
int zsipc_subscribeEvent(ZSIPC_EventHandle handle, const char *topic, size_t topic_size);
int zsipc_unsubscribeEvent(ZSIPC_EventHandle handle, const char *topic, size_t topic_size);

int zsipc_sendEvent(ZSIPC_EventHandle handle, const char *event_topic,
        const uint8_t *data, size_t data_size);
int zsipc_checkEventWithTopic(ZSIPC_EventHandle handle, const char *event_topic,
        const uint8_t *data, size_t data_size, void **ppOutPayloadPtr, uint32_t *pOutPayloadSize);

#ifdef __cplusplus
}
#endif

#endif /* ZWSYSTEM_IPC_EVENT_H */
