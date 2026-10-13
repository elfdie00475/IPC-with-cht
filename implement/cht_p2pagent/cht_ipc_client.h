#ifndef CHT_IPC_CLIENT_H
#define CHT_IPC_CLIENT_H

#include "cht_ipc_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cam_status_by_id_req_st {
    char tenantId[CHT_IPC_STRING_SIZE];
    char netNo[CHT_IPC_STRING_SIZE];
    int camSid;
    char camId[CHT_IPC_STRING_SIZE];
    char userId[CHT_IPC_STRING_SIZE];
} stCamStatusByIdReq;

typedef struct cam_status_by_id_rep_st {
    int result;
    char tenantId[CHT_IPC_STRING_SIZE];
    char netNo[CHT_IPC_STRING_SIZE];
    int camSid;
    char camId[CHT_IPC_STRING_SIZE];
    char firmwareVer[CHT_IPC_STRING_SIZE];
    char latestVersion[CHT_IPC_STRING_SIZE];
    int isMicrophone; // 1: open, 0: close;
    int speakVolume; // 0~10
    int imageQuality; // 0: low, 1: middle, 2: high
    int activeStatus; // 0: not start, 1: started
    char description[CHT_IPC_STRING_SIZE];
    char name[CHT_IPC_STRING_SIZE];
    char status[CHT_IPC_STRING_SIZE];
    char externalStorageHealth[CHT_IPC_STRING_SIZE];
    char externalStorageCapacity[CHT_IPC_STRING_SIZE];
    char externalStorageAvailable[CHT_IPC_STRING_SIZE];
    char wifiSsid[CHT_IPC_STRING_SIZE];
    int wifiDbm;
} stCamStatusByIdRep;

extern int cht_ipc_getCamStatusById(const stCamStatusByIdReq *pReq, stCamStatusByIdRep *pRep);

#ifdef __cplusplus
}
#endif
#endif /* CHT_IPC_CLIENT_H */
