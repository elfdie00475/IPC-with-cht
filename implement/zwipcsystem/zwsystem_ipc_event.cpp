#include <sys/time.h>

#include <string>

#include <nngipc.h>

#include "zwsystem_ipc_event_defined.h"
#include "zwsystem_ipc_event.h"

#ifdef PUBSUB_USE_FORWARDING
#define ZWSYSTEM_PUBLISH_NAME "pubsub_proxy_front.sock"
#define ZWSYSTEM_SUBSCRIBE_NAME "pubsub_proxy_back.sock"
static const bool g_proxyMode = true;
#else
#define ZWSYSTEM_PUBLISH_NAME "zwsystem_pubsub.ipc"
#define ZWSYSTEM_SUBSCRIBE_NAME "zwsystem_pubsub.ipc"
static const bool g_proxyMode = false;
#endif

using namespace llt::nngipc;

struct EventHandlerWrapper {
    uint32_t seq_id;
    std::shared_ptr<PublishHandler> pub_sp;
    std::shared_ptr<SubscribeHandler> sub_sp;

    EventHandlerWrapper() : seq_id{0}, pub_sp{nullptr}, sub_sp{nullptr} {}
};

ZSIPC_EventHandle zw_ipc_createEventHandle(void)
{
    auto wrapper = new (std::nothrow) EventHandlerWrapper();
    if (!wrapper) return NULL;

    return (ZSIPC_EventHandle)wrapper;
}

void zw_ipc_freeEventHandle(ZSIPC_EventHandle *pHandle)
{
    if (!pHandle || !(*pHandle)) return;

    auto wrapper = (EventHandlerWrapper *)(*pHandle);
    if (wrapper->pub_sp) {
        wrapper->pub_sp.reset();
    }

    if (wrapper->sub_sp) {
        wrapper->sub_sp->stop();
        wrapper->sub_sp.reset();
    }

    delete wrapper;
    *pHandle = NULL;
}

int zs_ipc_startListenEvent(ZSIPC_EventHandle handle, ZS_IPC_OutputCallback cb, void *cb_param, uint32_t worker_num)
{
    if (!handle || !cb) return -1;

    auto wrapper = (EventHandlerWrapper *)handle;
    if (wrapper->sub_sp) return -2;

    wrapper->sub_sp = SubscribeHandler::create(ZWSYSTEM_SUBSCRIBE_NAME, worker_num, cb, cb_param);
    if (!wrapper->sub_sp) return -3;

    if (!wrapper->sub_sp->start()) {
        wrapper->sub_sp.reset();
        wrapper->sub_sp = nullptr;
        return -4;
    }

    return 0;
}

int zs_ipc_stopListenEvent(ZSIPC_EventHandle handle)
{
    if (!handle) return -1;

    auto wrapper = (EventHandlerWrapper *)handle;
    if (wrapper->sub_sp) return -2;

    if (wrapper->sub_sp) {
        wrapper->sub_sp->stop();
        wrapper->sub_sp.reset();
        wrapper->sub_sp = nullptr;
    }

    return 0;
}

int zs_ipc_subscribeEvent(ZSIPC_EventHandle handle, const char *topic, size_t topic_size)
{
    if (!handle || !topic) return -1;
    // allow topic_size == 0 ( allow "" means all)

    auto wrapper = (EventHandlerWrapper *)(handle);
    if (wrapper->sub_sp) {
        std::string top;
        if (topic_size == 0) {
            top = "";
        } else {
            top.assign(topic, topic_size);
        }

        int rc = wrapper->sub_sp->subscribe(top);
        if (!rc) return -2;
    }

    return 0;
}

int zs_ipc_unsubscribeEvent(ZSIPC_EventHandle handle, const char *topic, size_t topic_size)
{
    if (!handle || !topic) return -1;
    // allow topic_size == 0 ( allow "" means all)

    auto wrapper = (EventHandlerWrapper *)(handle);
    if (wrapper->sub_sp) {
        std::string top;
        if (topic_size == 0) {
            top = "";
        } else {
            top.assign(topic, topic_size);
        }

        int rc = wrapper->sub_sp->unsubscribe(top);
        if (!rc) return -2;
    }

    return 0;
}

int zs_ipc_sendEvent(ZSIPC_EventHandle handle, const char *event_topic,
        const uint8_t *data, size_t data_size)
{
    if (!handle || !data || data_size == 0) return -1;

    auto wrapper = (EventHandlerWrapper *)(handle);
    if (!wrapper->pub_sp) {
        // create  publish handler
        wrapper->pub_sp = PublishHandler::create(ZWSYSTEM_PUBLISH_NAME, g_proxyMode);
    }

    if (!wrapper->pub_sp) {
        return -2;
    }

    // create event struct
    stZsIpcEventHdr eventHdr;
    snprintf(eventHdr.szTopic, sizeof(eventHdr.szTopic), "%s", event_topic?event_topic:"");
    eventHdr.u32SeqId = wrapper->seq_id++;

    struct timespec rt, mt;
    clock_gettime(CLOCK_REALTIME, &rt);
    clock_gettime(CLOCK_MONOTONIC, &mt);

    eventHdr.u64LocalTimestampNs = rt.tv_sec * 1e9 + rt.tv_nsec;
    eventHdr.u64MonoTimestampNs = mt.tv_sec * 1e9 + mt.tv_nsec;

    struct tm tm_loc;
    localtime_r(&rt.tv_sec, &tm_loc);

    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_loc);

    char msbuf[8];
    snprintf(msbuf, sizeof(msbuf), "%03d", (int)(rt.tv_nsec/1e6));

    // %:z not support, use %z to get +0800，then transfrom to +08:00
    char zbuf[8];
    strftime(zbuf, sizeof(zbuf), "%z", &tm_loc);
    std::string tz = zbuf;                  // e.g. +0800
    if (tz.size() == 5) tz.insert(3, ":");  // -> +08:00

    snprintf(eventHdr.szUtcString, sizeof(eventHdr.szUtcString), "%s.%sZ %s", buf, msbuf, tz.c_str());

    eventHdr.u32MsgSize = sizeof(stZsIpcMsgHdr) + data_size;

    // create msg struct
    stZsIpcMsgHdr msgHdr;
    msgHdr.u32FourCC = ZS_IPC_FOURCC;
    msgHdr.u32HdrSize = 3;
    msgHdr.u8aHdr[0] = 0;
    msgHdr.u8aHdr[1] = 0;
    msgHdr.u8aHdr[2] = 0;
    msgHdr.u32PayloadSize = data_size;

    // append data
    if (!wrapper->pub_sp->append((const uint8_t *)&eventHdr, sizeof(eventHdr))) return -3;
    if (!wrapper->pub_sp->append((const uint8_t *)&msgHdr, sizeof(msgHdr))) return -4;
    if (!wrapper->pub_sp->append((const uint8_t *)data, data_size)) return -5;

    // send event
    if (!wrapper->pub_sp->send()) return -6;

    return 0;
}

int zs_ipc_checkEventWithTopic(ZSIPC_EventHandle handle, const char *event_topic,
        const uint8_t *data, size_t data_size, void **ppOutPayloadPtr, uint32_t *pOutPayloadSize)
{
    (void)handle;

    if (!data || !event_topic) return -1;

    // check event header
    if (data_size < sizeof(stZsIpcEventHdr)) return -2;

    const stZsIpcEventHdr *pEventHdr = (const stZsIpcEventHdr *)(data);
    uint32_t u32CheckSize1 = pEventHdr->u32MsgSize + sizeof(stZsIpcEventHdr);
    if (data_size != u32CheckSize1) return -2;

    // only get event with target topic
    std::string topicStr(event_topic);
    std::string eventTopicStr(pEventHdr->szTopic);
    if (eventTopicStr != topicStr) return -2;

    // check msg header
    const stZsIpcMsgHdr *pMsgHdr = (const stZsIpcMsgHdr *)(data + sizeof(stZsIpcEventHdr));
    uint32_t u32CheckSize2 = pMsgHdr->u32PayloadSize + sizeof(stZsIpcEventHdr) + sizeof(stZsIpcMsgHdr);
    if (data_size != u32CheckSize2) return -3;
    if (pMsgHdr->u32FourCC != ZS_IPC_FOURCC) return -3;

    if (ppOutPayloadPtr) *ppOutPayloadPtr = (void *)(data + sizeof(stZsIpcEventHdr) + sizeof(stZsIpcMsgHdr));
    if (pOutPayloadSize) *pOutPayloadSize = pMsgHdr->u32PayloadSize;

    return 0;
}
