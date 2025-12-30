#ifndef __CHT_IPC_CLIENT_H__
#define __CHT_IPC_CLIENT_H__

#define CHT_IPC_STRING_SIZE 256

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
    char tenantId[CHT_IPC_STRING_SIZE];
    char netNo[CHT_IPC_STRING_SIZE];
    int camSid;
    char camId[CHT_IPC_STRING_SIZE];
    char userId[CHT_IPC_STRING_SIZE];
} stCamStatusByIdRep;

extern int cht_ipc_getCamStatusById(const stCamStatusByIdReq *pReq, stCamStatusByIdRep *pRep);

#ifdef __cplusplus
}
#endif
#endif /* __CHT_IPC_CLIENT_H__ */
