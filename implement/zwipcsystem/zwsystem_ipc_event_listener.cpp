#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "zwsystem_ipc_event.h"
#include "zwsystem_ipc_event_defined.h"

static void event_process(void *user_data, const uint8_t *data, size_t data_size, uint8_t **response, size_t * response_size)
{
    (void)user_data;
    (void)response;
    (void)response_size;

    printf("data %p data_size %ld\n", data, data_size);
    if (!data) return ;
    if (data_size < (sizeof(tZSIpcEventHdr) + sizeof(tZSIpcMsgHdr))) return ;

    printf("read event header: \n");
    tZSIpcEventHdr *pEventHdr = (tZSIpcEventHdr *)data;
    printf("  topic: %s\n", pEventHdr->szTopic);
    printf("  seqId: %d\n", pEventHdr->u32SeqId);
    printf("  UTC string: %s\n", pEventHdr->szUtcString);
    printf("  local timestamp: %ld\n", pEventHdr->u64LocalTimestampNs);
    printf("  mono timestamp: %ld\n", pEventHdr->u64MonoTimestampNs);
    printf("  message size: %d\n", pEventHdr->u32MsgSize);

    printf("msg event header: \n");
    tZSIpcMsgHdr *pMsgHdr = (tZSIpcMsgHdr *)(data + sizeof(tZSIpcEventHdr));
    printf("  CC: 0x%x\n", pMsgHdr->u32FourCC);
    printf("  header size: %d\n", pMsgHdr->u32HdrSize);
    printf("  payload size: %d\n", pMsgHdr->u32PayloadSize);
}

int main(void)
{
    ZSIPC_EventHandle handle = zsipc_createEventHandle();
    zsipc_startListenEvent(handle, event_process, NULL, 4);
    zsipc_subscribeEvent(handle, "", 0);

    while (true) {
        sleep(10);
    }

    zsipc_stopListenEvent(handle);
    zsipc_freeEventHandle(&handle);

    return 0;
}
