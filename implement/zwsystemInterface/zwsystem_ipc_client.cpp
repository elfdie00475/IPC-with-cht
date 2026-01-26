#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <memory>
#include <cstdint>

#include <nngipc.h>

#include "zwsystem_ipc_client.h"
#include "zwsystem_ipc_defined.h"

using namespace llt;

static uint16_t g_u16MsgId = 0;
static pthread_mutex_t g_IdMutex = PTHREAD_MUTEX_INITIALIZER;

static int ipc_client_getMsgId(void)
{
    uint16_t u16TmpId = g_u16MsgId;

    pthread_mutex_lock(&g_IdMutex);
    g_u16MsgId++;
    if (g_u16MsgId == 0) g_u16MsgId++;
    u16TmpId = g_u16MsgId;
    pthread_mutex_unlock(&g_IdMutex);

    return u16TmpId;
}

template<typename ReqType, typename RepType>
static int ipc_client_executeReqRep(eZwsystemIpcCmd ipc_cmd_id, const ReqType& stReq, RepType *pRep)
{
    int rc = 0;
    stZwsystemIpcMsg ipcReqMsg;
    uint8_t *recv = NULL;
    size_t recv_size = 0;
    size_t req_size = sizeof(ReqType);
    size_t rep_size = sizeof(RepType);

    zwsystem_ipc_msg_init(&ipcReqMsg, ((ipc_client_getMsgId() << 1) | 0), ipc_cmd_id);
    ipcReqMsg.stHdr.u32PayloadSize = req_size;

    do {
        bool res = false;

        auto rep_handler = nngipc::RequestHandler::create(ZWSYSTEM_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&ipcReqMsg, sizeof(stZwsystemIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)&stReq, req_size);
        if (!res) { rc = -3; break; }
        res = rep_handler->send();
        if (!res) { rc = -4; break; }

        res = rep_handler->recv(&recv, &recv_size);
        // check res and header;
        if (!res || !recv || recv_size < sizeof(stZwsystemIpcHdr)) {
            rc = -5; break;
        }
        stZwsystemIpcHdr *pIpcRepHdr = (stZwsystemIpcHdr *)recv;
        if ( zwsystem_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3 ) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;

        if (ipc_result != 0 ||
            u16CmdType != ipc_cmd_id ||
            u32PayloadSize != rep_size) { rc = -6; break; }

        if (pRep) {
            stZwsystemIpcMsg *pIpcRepMsg = (stZwsystemIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, rep_size);
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    zwsystem_ipc_msg_free(&ipcReqMsg);

    return rc;
}

int zwsystem_ipc_bindCameraReport(stBindCameraReportReq stReq, stBindCameraReportRep *pRep)
{
    return ipc_client_executeReqRep<stBindCameraReportReq, stBindCameraReportRep>(
                _BindCameraReport, stReq, pRep);
}

int zwsystem_ipc_changeWifi(stChangeWifiReq stReq, stChangeWifiRep *pRep)
{
    return ipc_client_executeReqRep<stChangeWifiReq, stChangeWifiRep>(
                _ChangeWifi, stReq, pRep);
}

int zwsystem_ipc_setHamiCamInitialInfo(stSetHamiCamInitialInfoReq stReq, stSetHamiCamInitialInfoRep *pRep)
{
    return ipc_client_executeReqRep<stSetHamiCamInitialInfoReq, stSetHamiCamInitialInfoRep>(
                _SetHamiCamInitialInfo, stReq, pRep);
}

int zwsystem_ipc_getCamStatusById(stCamStatusByIdReq stReq, stCamStatusByIdRep *pRep)
{
    return ipc_client_executeReqRep<stCamStatusByIdReq, stCamStatusByIdRep>(
                _GetCamStatusById, stReq, pRep);
}

int zwsystem_ipc_deleteCameraInfo(stDeleteCameraInfoReq stReq, stDeleteCameraInfoRep *pRep)
{
    return ipc_client_executeReqRep<stDeleteCameraInfoReq, stDeleteCameraInfoRep>(
                _DeleteCameraInfo, stReq, pRep);
}

int zwsystem_ipc_setTimezone(stSetTimezoneReq stReq, stSetTimezoneRep *pRep)
{
    return ipc_client_executeReqRep<stSetTimezoneReq, stSetTimezoneRep>(
                _SetTimeZone, stReq, pRep);
}

int zwsystem_ipc_getTimezone(stGetTimezoneReq stReq, stGetTimezoneRep *pRep)
{
    return ipc_client_executeReqRep<stGetTimezoneReq, stGetTimezoneRep>(
                _GetTimeZone, stReq, pRep);
}

int zwsystem_ipc_updateCameraName(stUpdateCameraNameReq stReq, stUpdateCameraNameRep *pRep)
{
    return ipc_client_executeReqRep<stUpdateCameraNameReq, stUpdateCameraNameRep>(
                _UpdateCameraName, stReq, pRep);
}

int zwsystem_ipc_setCameraOsd(stSetCameraOsdReq stReq, stSetCameraOsdRep *pRep)
{
    return ipc_client_executeReqRep<stSetCameraOsdReq, stSetCameraOsdRep>(
                _SetCameraOSD, stReq, pRep);
}
#if 0
int cht_ipc_setPtzControlMove(const stPtzMoveReq *pReq, stPtzMoveRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _HamiCamPtzControlMove);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stPtzMoveReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stPtzMoveReq));
        if (!res) { rc = -3; break; }
        res = rep_handler->send();
        if (!res) { rc = -4; break; }

        size_t recv_size = 0;
        res = rep_handler->recv(&recv, &recv_size);
        // check res and header;
        if (!res || !recv || recv_size < sizeof(stChtIpcHdr)) {
            rc = -5; break;
        }
        stChtIpcHdr *pIpcRepHdr = (stChtIpcHdr *)recv;
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _HamiCamPtzControlMove ||
            u32PayloadSize != sizeof(stPtzMoveRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stPtzMoveRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_setPtzAbsoluteMove(const stPtzMoveReq *pReq, stPtzMoveRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _PtzAbsoluteMove);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stPtzMoveReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stPtzMoveReq));
        if (!res) { rc = -3; break; }
        res = rep_handler->send();
        if (!res) { rc = -4; break; }

        size_t recv_size = 0;
        res = rep_handler->recv(&recv, &recv_size);
        // check res and header;
        if (!res || !recv || recv_size < sizeof(stChtIpcHdr)) {
            rc = -5; break;
        }
        stChtIpcHdr *pIpcRepHdr = (stChtIpcHdr *)recv;
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _PtzAbsoluteMove ||
            u32PayloadSize != sizeof(stPtzMoveRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stPtzMoveRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_setPtzRelativeMove(const stPtzMoveReq *pReq, stPtzMoveRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _PtzRelativeMove);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stPtzMoveReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stPtzMoveReq));
        if (!res) { rc = -3; break; }
        res = rep_handler->send();
        if (!res) { rc = -4; break; }

        size_t recv_size = 0;
        res = rep_handler->recv(&recv, &recv_size);
        // check res and header;
        if (!res || !recv || recv_size < sizeof(stChtIpcHdr)) {
            rc = -5; break;
        }
        stChtIpcHdr *pIpcRepHdr = (stChtIpcHdr *)recv;
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _PtzRelativeMove ||
            u32PayloadSize != sizeof(stPtzMoveRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stPtzMoveRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_setPtzContinuousMove(const stPtzMoveReq *pReq, stPtzMoveRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _PtzContinuousMove);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stPtzMoveReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stPtzMoveReq));
        if (!res) { rc = -3; break; }
        res = rep_handler->send();
        if (!res) { rc = -4; break; }

        size_t recv_size = 0;
        res = rep_handler->recv(&recv, &recv_size);
        // check res and header;
        if (!res || !recv || recv_size < sizeof(stChtIpcHdr)) {
            rc = -5; break;
        }
        stChtIpcHdr *pIpcRepHdr = (stChtIpcHdr *)recv;
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _PtzContinuousMove ||
            u32PayloadSize != sizeof(stPtzMoveRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stPtzMoveRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_setNightMode(const stNightModeReq *pReq, stNightModeRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _SetNightMode);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stNightModeReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stNightModeReq));
        if (!res) { rc = -3; break; }
        res = rep_handler->send();
        if (!res) { rc = -4; break; }

        size_t recv_size = 0;
        res = rep_handler->recv(&recv, &recv_size);
        // check res and header;
        if (!res || !recv || recv_size < sizeof(stChtIpcHdr)) {
            rc = -5; break;
        }
        stChtIpcHdr *pIpcRepHdr = (stChtIpcHdr *)recv;
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _SetNightMode ||
            u32PayloadSize != sizeof(stNightModeRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stNightModeRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_setAutoNightMode(const stNightModeReq *pReq, stNightModeRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _SetAutoNightVision);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stNightModeReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stNightModeReq));
        if (!res) { rc = -3; break; }
        res = rep_handler->send();
        if (!res) { rc = -4; break; }

        size_t recv_size = 0;
        res = rep_handler->recv(&recv, &recv_size);
        // check res and header;
        if (!res || !recv || recv_size < sizeof(stChtIpcHdr)) {
            rc = -5; break;
        }
        stChtIpcHdr *pIpcRepHdr = (stChtIpcHdr *)recv;
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _SetAutoNightVision ||
            u32PayloadSize != sizeof(stNightModeRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stNightModeRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_setPtzHome(const stSetPtzHomeReq *pReq, stSetPtzHomeRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _SetPtzHome);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stSetPtzHomeReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stSetPtzHomeReq));
        if (!res) { rc = -3; break; }
        res = rep_handler->send();
        if (!res) { rc = -4; break; }

        size_t recv_size = 0;
        res = rep_handler->recv(&recv, &recv_size);
        // check res and header;
        if (!res || !recv || recv_size < sizeof(stChtIpcHdr)) {
            rc = -5; break;
        }
        stChtIpcHdr *pIpcRepHdr = (stChtIpcHdr *)recv;
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _SetPtzHome ||
            u32PayloadSize != sizeof(stSetPtzHomeRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stSetPtzHomeRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_gotoPtzHome(const stPtzMoveReq *pReq, stPtzMoveRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _GotoPtzHome);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stPtzMoveReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stPtzMoveReq));
        if (!res) { rc = -3; break; }
        res = rep_handler->send();
        if (!res) { rc = -4; break; }

        size_t recv_size = 0;
        res = rep_handler->recv(&recv, &recv_size);
        // check res and header;
        if (!res || !recv || recv_size < sizeof(stChtIpcHdr)) {
            rc = -5; break;
        }
        stChtIpcHdr *pIpcRepHdr = (stChtIpcHdr *)recv;
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _GotoPtzHome ||
            u32PayloadSize != sizeof(stPtzMoveRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stPtzMoveRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_getDateTimeInfo(const stDateTimeInfoReq *pReq, stDateTimeInfoRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _GetTimeZone);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stDateTimeInfoReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stDateTimeInfoReq));
        if (!res) { rc = -3; break; }
        res = rep_handler->send();
        if (!res) { rc = -4; break; }

        size_t recv_size = 0;
        res = rep_handler->recv(&recv, &recv_size);
        // check res and header;
        if (!res || !recv || recv_size < sizeof(stChtIpcHdr)) {
            rc = -5; break;
        }
        stChtIpcHdr *pIpcRepHdr = (stChtIpcHdr *)recv;
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _GetTimeZone ||
            u32PayloadSize != sizeof(stDateTimeInfoRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stDateTimeInfoRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_setDateTimeInfo(const stDateTimeInfoReq *pReq, stDateTimeInfoRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _SetTimeZone);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stDateTimeInfoReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stDateTimeInfoReq));
        if (!res) { rc = -3; break; }
        res = rep_handler->send();
        if (!res) { rc = -4; break; }

        size_t recv_size = 0;
        res = rep_handler->recv(&recv, &recv_size);
        // check res and header;
        if (!res || !recv || recv_size < sizeof(stChtIpcHdr)) {
            rc = -5; break;
        }
        stChtIpcHdr *pIpcRepHdr = (stChtIpcHdr *)recv;
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _SetTimeZone ||
            u32PayloadSize != sizeof(stDateTimeInfoRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stDateTimeInfoRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}
#endif

class ZwsystemSubListener
{
public:
    ZwsystemSubListener(zwsystem_sub_callback callback, void *userParam)
        : m_callback{callback}, m_userParam{userParam}
    {
    }

    ~ZwsystemSubListener() {
        if (m_subscriber) {
            m_subscriber->stop();
            m_subscriber = nullptr;
        }
    }

    int init(void) {
        std::shared_ptr<nngipc::SubscribeHandler> subscriber =
        nngipc::SubscribeHandler::create(ZWSYSTEM_SUBSCRIBE_NAME, 1,
            &ZwsystemSubListener::onMessage, this);
        if (!subscriber || !subscriber->start()) {
            return -2; // failed to create subscriber
        }

        subscriber->subscribe("");

        m_subscriber = subscriber;

        return 0;
    }

    void handleEvent(eZwsystemSubSystemEventType eventType,
            const uint8_t *data, size_t dataSize)
    {
        if (m_callback) {
            m_callback(m_userParam, eventType, data, dataSize);
        }
    }

private:
    static void onMessage(void *userParam,
            const uint8_t *data, size_t dataSize,
            uint8_t **responseData, size_t *responseDataSize)
    {
        (void)responseData;
        (void)responseDataSize;

        if (!data || dataSize < sizeof(stZwsystemSubHdr) + sizeof(stZwsystemIpcHdr)) {
            return ;
        }

        const stZwsystemIpcHdr *pIpcHdr = zwsystem_sub_msg_getIpcHdr((stZwsystemSubMsg *)data);
        if (!pIpcHdr) {
            return ;
        }

        if ( zwsystem_ipc_msg_checkFourCC(pIpcHdr->u32FourCC) != 1 ||
            pIpcHdr->u32HdrSize < 3 ) {
            return ;
        }
        if ( pIpcHdr->u16Headers[2] != 0 ) {
            return ;
        }

        const char *eventPrefix = zwsystem_sub_msg_getEventPrefix((stZwsystemSubMsg *)data);
        if (!eventPrefix) {
            return ;
        }

        eZwsystemSubSystemEventType eventType = eSystemEventType_Unknown;
        if (strncmp(eventPrefix,
                ZWSYSTEM_SUBSCRIBE_SOURCE_SNAPSHOT,
                ZWSYSTEM_SUBSCRIBE_PREFIX_LEN)) {
            eventType = eSystemEventType_Snapshot;
        } else if (strncmp(eventPrefix,
                ZWSYSTEM_SUBSCRIBE_SOURCE_RECORD,
                ZWSYSTEM_SUBSCRIBE_PREFIX_LEN)) {
            eventType = eSystemEventType_Record;
        } else if (strncmp(eventPrefix,
                ZWSYSTEM_SUBSCRIBE_SOURCE_RECOGNITION,
                ZWSYSTEM_SUBSCRIBE_PREFIX_LEN)) {
            eventType = eSystemEventType_Recognition;
        } else if (strncmp(eventPrefix,
                ZWSYSTEM_SUBSCRIBE_SOURCE_STATUS,
                ZWSYSTEM_SUBSCRIBE_PREFIX_LEN)) {
            eventType = eSystemEventType_StatusEvent;
        } else {
            eventType = eSystemEventType_Unknown;
        }

        ZwsystemSubListener *listener = static_cast<ZwsystemSubListener *>(userParam);
        if (!listener) {
            return ;
        }
        listener->handleEvent(eventType, data, dataSize);
    }

private:
    zwsystem_sub_callback m_callback;
    void *m_userParam;
    std::shared_ptr<nngipc::SubscribeHandler> m_subscriber;
} ;

static std::shared_ptr<ZwsystemSubListener> g_listener = nullptr;
int zwsystem_sub_subscribeSystemEvent(zwsystem_sub_callback callback, void *userParam)
{
    if (g_listener) {
        return 0; // already subscribed
    }

    const auto listener = std::make_shared<ZwsystemSubListener>(callback, userParam);
    if (!listener || listener->init() < 0) {
        return -1; // failed to init listener
    }

    g_listener = listener;

    return 0;
}

int zwsystem_sub_unsubscribeSystemEvent(void)
{
    if (g_listener) {
        g_listener.reset();
        g_listener = nullptr;
    }

    return 0;
}
