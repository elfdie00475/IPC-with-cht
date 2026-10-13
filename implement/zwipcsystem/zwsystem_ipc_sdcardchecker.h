#ifndef ZWSYSTEM_IPC_SDCARDCHECKER_H
#define ZWSYSTEM_IPC_SDCARDCHECKER_H

#include <stdint.h>

#include "zwsystem_ipc_msg_defined.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ZSIPC_OUTPUTCALLBACK_DEFINED
#define ZSIPC_OUTPUTCALLBACK_DEFINED
typedef void (*ZSIPC_OutputCallback) (void *, const uint8_t *, size_t, uint8_t **, size_t *);
#endif // ZSIPC_OUTPUTCALLBACK_DEFINED

typedef void *ZSIPC_SDCardCheckerHandle;

ZSIPC_SDCardCheckerHandle zsipc_createSdCardCheckerHandle(void);
void zsipc_freeSdCardCheckerHandle(ZSIPC_SDCardCheckerHandle *pHandle);
int zsipc_startSdCardCheckerMsgReceiver(ZSIPC_SDCardCheckerHandle handle,
        ZSIPC_OutputCallback cb, void *cb_param, uint32_t worker_num);
int zsipc_stopSdCardCheckerMsgReceiver(ZSIPC_SDCardCheckerHandle handle);

// for service
int zsipc_checkMsgWithId(ZSIPC_SDCardCheckerHandle handle, eZSIpcMsgId uMsgId,
        const uint8_t *data, size_t data_size, void **ppOutPayloadPtr, uint32_t *pOutPayloadSize);
int zsipc_createResponseMsg(ZSIPC_SDCardCheckerHandle handle, eZSIpcMsgId uMsgId,
        const uint8_t *data, size_t data_size, uint8_t **ppOutData, size_t *puOutDataSize);

// for client
int zsipc_getPartitionList(ZSIPC_SDCardCheckerHandle handle,
        tZSIpcMsgGetPartitionListReq stReq, tZSIpcMsgGetPartitionListRep *pRep);
int zsipc_getPartitionInfo(ZSIPC_SDCardCheckerHandle handle,
        tZSIpcMsgGetPartitionInfoReq stReq, tZSIpcMsgGetPartitionInfoRep *pRep);
int zsipc_formatPartition(ZSIPC_SDCardCheckerHandle handle,
        tZSIpcMsgFormatPartitionReq stReq, tZSIpcMsgFormatPartitionRep *pRep);
int zsipc_checkPartitionFormatting(ZSIPC_SDCardCheckerHandle handle,
        tZSIpcMsgCheckFormattingReq stReq, tZSIpcMsgCheckFormattingRep *pRep);

#ifdef __cplusplus
}
#endif

#endif /* ZWSYSTEM_IPC_SDCARDCHECKER_H */
