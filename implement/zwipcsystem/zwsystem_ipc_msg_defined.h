#ifndef ZWSYSTEM_IPC_MESSAGE_DEFINED_H
#define ZWSYSTEM_IPC_MESSAGE_DEFINED_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)  ((unsigned int)(unsigned char)(ch0) | \
    ((unsigned int)(unsigned char)(ch1) << 8) | ((unsigned int)(unsigned char)(ch2) << 16) | \
    ((unsigned int)(unsigned char)(ch3) << 24 ))
#endif // MAKEFOURCC
#define ZSIPC_FOURCC (MAKEFOURCC('Z','W','S','Y'))

#define ZSIPC_STRING_LEN 128
#define ZSIPC_NAME_LEN 256
#define ZSIPC_PATH_LEN 4096

// struct
// msg header
// -- msg fourCC
// -- msg header size
// -- msg header array
// -- msg payload size (data payload)
// data payload (custom struct)
#define ZSIPC_HEADER_MAX_SIZE 128
typedef struct zsipc_msg_hdr_t
{
    uint32_t u32FourCC;
    uint32_t u32HdrSize;
    uint8_t u8aHdr[ZSIPC_HEADER_MAX_SIZE]; // 0: result, 1: cmd_hi, 2: cmd_low
    uint32_t u32PayloadSize;
} tZSIpcMsgHdr;

typedef enum zsipc_msg_id_e
{
    // for sdchecker reqrep
    eMsgId_SD_GetPartitionList = 0x0101,
    eMsgId_SD_GetPartitionInfo,
    eMsgId_SD_FormatPartition,
    eMsgId_SD_CheckPartitionFormatting
} eZSIpcMsgId;

typedef struct zsipc_message_default_t
{
    int code;
} tZSIpcMsgDefault;

// sdchecker reqrep
typedef tZSIpcMsgDefault tZSIpcMsgGetPartitionListReq;

typedef struct zsipc_msg_getPartitionList_rep_t
{
    int code;
    uint32_t u32ListCount;
} tZSIpcMsgGetPartitionListRep;

typedef struct zsipc_msg_getPartitionInfo_req_t
{
    uint32_t u32Idx;
} tZSIpcMsgGetPartitionInfoReq;

typedef enum zsipc_msg_partition_status_e
{
    ePartStatus_Normal = 0,
    ePartStatus_NewCard,
    ePartStatus_Damaged,
    ePartStatus_NoCard,
    ePartStatus_Formatting,
    ePartStatus_Other,
    ePartStatus_Unmount
} eZSIpcMsgPartStatus;

typedef struct zsipc_msg_getPartitionInfo_rep_t
{
    int code;
    char szDeviceName[ZSIPC_NAME_LEN];
    char szPartitionPath[ZSIPC_PATH_LEN];
    char szMountPath[ZSIPC_PATH_LEN];
    uint64_t u64TotalSize;
    uint64_t u64AvailSize;
    eZSIpcMsgPartStatus eStatus;
} tZSIpcMsgGetPartitionInfoRep;

typedef tZSIpcMsgGetPartitionInfoReq tZSIpcMsgFormatPartitionReq;

typedef tZSIpcMsgDefault tZSIpcMsgFormatPartitionRep;

typedef tZSIpcMsgGetPartitionInfoReq tZSIpcMsgCheckFormattingReq;

typedef struct zsipc_msg_checkFormatting_rep_t
{
    int code;
    bool bFormatting;
} tZSIpcMsgCheckFormattingRep;

#ifdef __cplusplus
}
#endif
#endif /* ZWSYSTEM_IPC_MESSAGE_DEFINED_H */
