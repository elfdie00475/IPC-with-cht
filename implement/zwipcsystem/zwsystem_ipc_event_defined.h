#ifndef ZWSYSTEM_IPC_EVENT_DEFINED_H
#define ZWSYSTEM_IPC_EVENT_DEFINED_H

#include <stdint.h>
#include <stdbool.h>

#include "zwsystem_ipc_msg_defined.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZSIPC_TOPIC_LEN 32
#define ZSIPC_TOPIC_REC_PREFIX                 "rec."
#define ZSIPC_TOPIC_REC_STATUS_PREFIX          "rec.status."
#define ZSIPC_TOPIC_REC_STATUS_STARTED         "rec.status.started"
#define ZSIPC_TOPIC_REC_STATUS_STOPPED         "rec.status.stopped"
#define ZSIPC_TOPIC_REC_STATUS_CONFIGCHANGED   "rec.status.config_changed"
#define ZSIPC_TOPIC_REC_ERROR                  "rec.error"
#define ZSIPC_TOPIC_VSRC_PREFIX                 "vsrc."
#define ZSIPC_TOPIC_VSRC_STATUS_PREFIX          "vsrc.status."
#define ZSIPC_TOPIC_VSRC_STATUS_INITDONE        "vsrc.status.init_done"
#define ZSIPC_TOPIC_VSRC_STATUS_CLOSING         "vsrc.status.closing"
#define ZSIPC_TOPIC_VSRC_ERROR                  "vsrc.error"
#define ZSIPC_TOPIC_VENC_PREFIX                 "venc."
#define ZSIPC_TOPIC_VENC_STATUS_PREFIX          "venc.status."
#define ZSIPC_TOPIC_VENC_STATUS_STARTED         "venc.status.started"
#define ZSIPC_TOPIC_VENC_STATUS_STOPPED         "venc.status.stopped"
#define ZSIPC_TOPIC_VENC_STATUS_CONFIGCHANGED   "venc.status.config_changed"
#define ZSIPC_TOPIC_VENC_ERROR                  "venc.error"
#define ZSIPC_TOPIC_STORAGE_PREFIX             "stor."
#define ZSIPC_TOPIC_STORAGE_AUTO_REMOVE        "stor.auto_remove"
#define ZSIPC_TOPIC_STORAGE_BLOCK_INFO         "stor.blk_info"
#define ZSIPC_TOPIC_STORAGE_PRE_UNMOUNT        "stor.pre_unmount"
#define ZSIPC_TOPIC_STORAGE_FORMAT_STARTING    "stor.format_starting"
#define ZSIPC_TOPIC_STORAGE_FORMAT_ENDED       "stor.format_ended"
#define ZSIPC_TOPIC_STORAGE_ERROR              "stor.error"

// struct
// event header
// -- event tag
// -- event seq id
// -- event UTC (local time) string, include offset, e.g. 2026/02/03 15:34:04 +08:00
// -- event monotonic time（ns/us), epoch time
// -- event payload size (msg header)
// msg header
// -- msg fourCC
// -- msg header size
// -- msg header array
// -- msg payload size (data payload)
// data payload (custom struct)
typedef struct zsipc_event_hdr_t
{
    char szTopic[ZSIPC_TOPIC_LEN];
    uint32_t u32SeqId;
    char szUtcString[ZSIPC_STRING_LEN];
    uint64_t u64LocalTimestampNs;
    uint64_t u64MonoTimestampNs;
    uint32_t u32MsgSize;
} tZSIpcEventHdr;

typedef struct zsipc_event_rec_started_t
{
    uint64_t u64StartTimeStampMs;
    char szFilename[ZSIPC_NAME_LEN];
} tZSIpcEvtRecStarted;

typedef struct zsipc_event_rec_stopped_t
{
    uint64_t u64StartTimeStampMs;
    char szFilename[ZSIPC_NAME_LEN];
    bool hasRecordFile;
    char szRecordFilePath[ZSIPC_PATH_LEN];
    bool hasSnapshotFile;
    char szSnapshotFilePath[ZSIPC_PATH_LEN];
    bool hasAudioFile;
    char szAudioFilePath[ZSIPC_PATH_LEN];
} tZSIpcEvtRecStopped;

typedef struct zsipc_vsrc_config_t
{
    uint32_t u32MaxWidth;
    uint32_t u32MaxHeight;
    uint32_t u32PosX;
    uint32_t u32PosY;
    uint32_t u32Width;
    uint32_t u32Height;
} tZSIpcVsrcConfig;

typedef enum zsipc_venc_codec_type_e
{
	eVencCodecType_H264 = 0,    //! H.264 Codec
	eVencCodecType_H265,        //! H.265 Codec
	eVencCodecType_MJPG,        //! MJPG Codec
	eVencCodecType_NONE = 9,    //! No codec
} eZSIpcVencCodecType;

typedef struct zsipc_venc_config_t
{
    uint32_t u32Idx;
    eZSIpcVencCodecType eCodec;
    uint32_t u32Width;
    uint32_t u32Height;
    uint32_t u32Bitrate;
    float fFps;
    uint32_t u32Gop;
    uint32_t u32Qp;
    uint32_t u32MinIQp;
    uint32_t u32MaxIQp;
    uint32_t u32MinPQp;
    uint32_t u32MaxPQp;
    uint32_t u32KeepRatio;
} tZSIpcVencConfig;

#define ZSIPC_VENC_MAX_NUM 5

typedef struct zsipc_event_vsrc_init_done_t
{
    // vsrc
    tZSIpcVsrcConfig stVsrcConfig;
    // venc
    uint32_t u32VencNum;
    tZSIpcVencConfig stVencConfig[ZSIPC_VENC_MAX_NUM];
} tZSIpcEvtVsrcInitDone;

typedef struct zsipc_event_default_t
{
    int code;
} tZSIpcEvtDefault;

typedef tZSIpcEvtDefault tZSIpcEvtVsrcClosing;

typedef struct zsipc_event_venc_started_t
{
    tZSIpcVencConfig stVencConfig;
} tZSIpcEvtVencStarted;

typedef tZSIpcEvtDefault tZSIpcEvtVencStopped;

typedef tZSIpcEvtDefault tZSIpcEvtStorageAutoRemove;

typedef enum zsipc_storage_status_type_e
{
	eStorStatusType_Other = 0,
    eStorStatusType_Normal,
    eStorStatusType_NonInit,
    eStorStatusType_Damaged,
    eStorStatusType_NoMount,
    eStorStatusType_Formatting
} eZSIpcStorageStatusType;

typedef struct zsipc_event_storage_block_info_t
{
    char szMountPath[ZSIPC_PATH_LEN];
    uint64_t u64TotalSize;
    uint64_t u64FreeSize;
    uint64_t u64AvailSize;
    eZSIpcStorageStatusType eStatus;
} tZSIpcEvtStorageBlockInfo;

typedef tZSIpcEvtDefault tZSIpcEvtStoragePreUnmount;

typedef tZSIpcEvtDefault tZSIpcEvtStorageFormatStarting;

typedef tZSIpcEvtDefault tZSIpcEvtStorageFormatEnded;

#ifdef __cplusplus
}
#endif
#endif /* ZWSYSTEM_IPC_EVENT_DEFINED_H */
