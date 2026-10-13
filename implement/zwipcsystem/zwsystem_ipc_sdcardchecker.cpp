#include <string.h>
#include <sys/time.h>

#include <string>

#include <nngipc.h>

#include "zwsystem_ipc_sdcardchecker.h"

#define ZWSTORAGE_IPC_NAME "zs_sdcardchecker.ipc"

using namespace llt::nngipc;

struct SDCardCheckerWrapper {
    std::shared_ptr<ResponseHandler> res_sp;
    std::shared_ptr<RequestHandler> req_sp;

    SDCardCheckerWrapper() : res_sp{nullptr}, req_sp{nullptr} {}
};

ZSIPC_SDCardCheckerHandle zsipc_createSdCardCheckerHandle(void)
{
    auto wrapper = new (std::nothrow) SDCardCheckerWrapper();
    if (!wrapper) return NULL;

    return (ZSIPC_SDCardCheckerHandle)wrapper;
}

void zsipc_freeSdCardCheckerHandle(ZSIPC_SDCardCheckerHandle *pHandle)
{
    if (!pHandle || !(*pHandle)) return;

    auto wrapper = (SDCardCheckerWrapper *)(*pHandle);
    if (wrapper->res_sp) {
        wrapper->res_sp->stop();
        wrapper->res_sp.reset();
    }

    delete wrapper;
    *pHandle = NULL;
}

int zsipc_startSdCardCheckerMsgReceiver(ZSIPC_SDCardCheckerHandle handle,
        ZSIPC_OutputCallback cb, void *cb_param, uint32_t worker_num)
{
    if (!handle || !cb) return -1;

    auto wrapper = (SDCardCheckerWrapper *)handle;
    if (wrapper->res_sp) return -2;

    if (worker_num == 0) worker_num = 1;

    wrapper->res_sp = ResponseHandler::create(ZWSTORAGE_IPC_NAME, worker_num, cb, cb_param);
    if (!wrapper->res_sp) return -3;

    if (!wrapper->res_sp->start()) {
        wrapper->res_sp.reset();
        wrapper->res_sp = nullptr;
        return -4;
    }

    return 0;
}

int zsipc_stopSdCardCheckerMsgReceiver(ZSIPC_SDCardCheckerHandle handle)
{
    if (!handle) return -1;

    auto wrapper = (SDCardCheckerWrapper *)handle;
    if (wrapper->res_sp) return -2;

    if (wrapper->res_sp) {
        wrapper->res_sp->stop();
        wrapper->res_sp.reset();
        wrapper->res_sp = nullptr;
    }

    return 0;
}

int zsipc_checkMsgWithId(ZSIPC_SDCardCheckerHandle handle, eZSIpcMsgId uMsgId,
        const uint8_t *data, size_t data_size, void **ppOutPayloadPtr, uint32_t *pOutPayloadSize)
{
    (void)handle;

    if (!data) return -1;

    // check msg header
    if (data_size < sizeof(tZSIpcMsgHdr)) return -2;

    const tZSIpcMsgHdr *pMsgHdr = (const tZSIpcMsgHdr *)(data);
    uint32_t u32CheckSize1 = pMsgHdr->u32PayloadSize + sizeof(tZSIpcMsgHdr);
    if (data_size != u32CheckSize1) return -2;
    if (pMsgHdr->u32FourCC != ZSIPC_FOURCC) return -2;

    // check msg id
    uint32_t u32HdrSize = pMsgHdr->u32HdrSize;
    if (u32HdrSize != 3) return -3;
    uint16_t u16MsgId = ((pMsgHdr->u8aHdr[1] << 8) | pMsgHdr->u8aHdr[2]);
    if (u16MsgId != (uint16_t)uMsgId) return -3;

    if (ppOutPayloadPtr) *ppOutPayloadPtr = (void *)(data + sizeof(tZSIpcMsgHdr));
    if (pOutPayloadSize) *pOutPayloadSize = pMsgHdr->u32PayloadSize;

    return 0;
}

int zsipc_createResponseMsg(ZSIPC_SDCardCheckerHandle handle, eZSIpcMsgId uMsgId,
        const uint8_t *data, size_t data_size, uint8_t **ppOutData, size_t *puOutDataSize)
{
    (void)handle;

    if (!data || data_size == 0) return -1;

    // malloc out data space
    uint32_t u32OutDataSize = data_size + sizeof(tZSIpcMsgHdr);
    uint8_t *pu8OutData = (uint8_t *)malloc(u32OutDataSize);
    if (!pu8OutData) return -2;

    uint8_t *ptr = pu8OutData;
    // write msg header
    tZSIpcMsgHdr *pHdr = (tZSIpcMsgHdr *)ptr;
    pHdr->u32FourCC = ZSIPC_FOURCC;
    pHdr->u32HdrSize = 3;
    pHdr->u8aHdr[0] = 0;
    pHdr->u8aHdr[1] = (uint8_t)((uMsgId >> 8) & 0xFF);
    pHdr->u8aHdr[2] = (uint8_t)((uMsgId >> 0) & 0xFF);
    pHdr->u32PayloadSize = data_size;

    // copy data
    ptr += sizeof(tZSIpcMsgHdr);
    memcpy(ptr, data, data_size);

    if (ppOutData) *ppOutData = pu8OutData;
    if (puOutDataSize) *puOutDataSize = u32OutDataSize;

    return 0;
}

template<typename ReqType, typename RepType>
static int zsipc_executeReqRep(eZSIpcMsgId uMsgId, const ReqType& tReq, RepType *ptRep)
{
    int rc = 0;
    const uint8_t *data = (const uint8_t *)&tReq;
    size_t data_size = sizeof(ReqType);
    uint8_t *recv = NULL;
    size_t recv_size = 0;
    size_t rep_size = sizeof(RepType);

    // init msg header
    tZSIpcMsgHdr msgHdr;
    msgHdr.u32FourCC = ZSIPC_FOURCC;
    msgHdr.u32HdrSize = 3;
    msgHdr.u8aHdr[0] = 0;
    msgHdr.u8aHdr[1] = (uint8_t)((uint16_t)uMsgId >> 8);
    msgHdr.u8aHdr[2] = (uint8_t)((uint16_t)uMsgId >> 0);
    msgHdr.u32PayloadSize = data_size;

    do {
        // create request handler
        auto rep_handler = RequestHandler::create(ZWSTORAGE_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        // append data
        if (!rep_handler->append((const uint8_t *)&msgHdr, sizeof(msgHdr))) { rc = -4; break; }
        if (!rep_handler->append(data, data_size)) { rc = -5; break; }

        // send event
        if (!rep_handler->send()) { rc = -6; break; }

        // check recv
        if (!rep_handler->recv(&recv, &recv_size)) { rc = -7; break; }
        if (!recv || recv_size < sizeof(tZSIpcMsgHdr)) { rc = -8; break; }

        // check msg header
        tZSIpcMsgHdr *pMsgHdr = (tZSIpcMsgHdr *)recv;
        uint32_t u32CheckSize1 = pMsgHdr->u32PayloadSize + sizeof(tZSIpcMsgHdr);
        if (recv_size != u32CheckSize1) { rc = -9; break; }
        if (pMsgHdr->u32FourCC != ZSIPC_FOURCC) { rc = -10; break; }

        // check msg id
        uint32_t u32HdrSize = pMsgHdr->u32HdrSize;
        if (u32HdrSize != 3) { rc = -11; break; }
        uint16_t u16MsgId = ((pMsgHdr->u8aHdr[1] << 8) | pMsgHdr->u8aHdr[2]);
        if (u16MsgId != (uint16_t)uMsgId) { rc = -12; break; }

        // check msg result
        uint8_t u8MsgResult = pMsgHdr->u8aHdr[0];
        if (u8MsgResult != 0) { rc = -13; break; }

        // check payload size
        uint8_t *pPayload = recv + sizeof(tZSIpcMsgHdr);
        uint32_t u32PayloadSize = pMsgHdr->u32PayloadSize;
        if (u32PayloadSize != rep_size) { rc = -14; break; }

        if (ptRep) {
            memcpy(ptRep, pPayload, u32PayloadSize);
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    return rc;
}

int zsipc_getPartitionList(ZSIPC_SDCardCheckerHandle handle,
        tZSIpcMsgGetPartitionListReq stReq, tZSIpcMsgGetPartitionListRep *pRep)
{
    (void)handle;
    return zsipc_executeReqRep<tZSIpcMsgGetPartitionListReq, tZSIpcMsgGetPartitionListRep>(
                eMsgId_SD_GetPartitionList, stReq, pRep);
}

int zsipc_getPartitionInfo(ZSIPC_SDCardCheckerHandle handle,
        tZSIpcMsgGetPartitionInfoReq stReq, tZSIpcMsgGetPartitionInfoRep *pRep)
{
    (void)handle;
    return zsipc_executeReqRep<tZSIpcMsgGetPartitionInfoReq, tZSIpcMsgGetPartitionInfoRep>(
                eMsgId_SD_GetPartitionInfo, stReq, pRep);
}

int zsipc_formatPartition(ZSIPC_SDCardCheckerHandle handle,
        tZSIpcMsgFormatPartitionReq stReq, tZSIpcMsgFormatPartitionRep *pRep)
{
    (void)handle;
    return zsipc_executeReqRep<tZSIpcMsgFormatPartitionReq, tZSIpcMsgFormatPartitionRep>(
                eMsgId_SD_FormatPartition, stReq, pRep);
}

int zsipc_checkPartitionFormatting(ZSIPC_SDCardCheckerHandle handle,
        tZSIpcMsgCheckFormattingReq stReq, tZSIpcMsgCheckFormattingRep *pRep)
{
    (void)handle;
    return zsipc_executeReqRep<tZSIpcMsgCheckFormattingReq, tZSIpcMsgCheckFormattingRep>(
                eMsgId_SD_CheckPartitionFormatting, stReq, pRep);
}
