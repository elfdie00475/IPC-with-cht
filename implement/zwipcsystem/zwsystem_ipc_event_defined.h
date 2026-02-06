#ifndef ZWSYSTEM_IPC_EVENT_DEFINED_H
#define ZWSYSTEM_IPC_EVENT_DEFINED_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)  ((unsigned int)(unsigned char)(ch0) | \
    ((unsigned int)(unsigned char)(ch1) << 8) | ((unsigned int)(unsigned char)(ch2) << 16) | \
    ((unsigned int)(unsigned char)(ch3) << 24 ))
#endif // MAKEFOURCC
#define ZS_IPC_FOURCC (MAKEFOURCC('Z','W','S','Y'))

#define ZS_IPC_EVENT_TOPIC_LEN 32
#define ZS_IPC_EVENT_RECORED_PREFIX                 "rec."
#define ZS_IPC_EVENT_RECORED_STATUS_PREFIX          "rec.status."
#define ZS_IPC_EVENT_RECORED_STATUS_STARTED         "rec.status.started"
#define ZS_IPC_EVENT_RECORED_STATUS_STOPPED         "rec.status.stopped"
#define ZS_IPC_EVENT_RECORED_STATUS_CONFIGCHANGED   "rec.status.config_changed"
#define ZS_IPC_EVENT_RECORED_ERROR                  "rec.error"
#define ZS_IPC_EVENT_VIDEO_SOURCE_PREFIX                 "vsrc."
#define ZS_IPC_EVENT_VIDEO_SOURCE_STATUS_PREFIX          "vsrc.status."
#define ZS_IPC_EVENT_VIDEO_SOURCE_STATUS_INITDONE        "vsrc.status.init_done"
#define ZS_IPC_EVENT_VIDEO_SOURCE_STATUS_CLOSING         "vsrc.status.closing"
#define ZS_IPC_EVENT_VIDEO_SOURCE_ERROR                  "vsrc.error"
#define ZS_IPC_EVENT_VIDEO_ENCODE_PREFIX                 "venc."
#define ZS_IPC_EVENT_VIDEO_ENCODE_STATUS_PREFIX          "venc.status."
#define ZS_IPC_EVENT_VIDEO_ENCODE_STATUS_STARTED         "venc.status.started"
#define ZS_IPC_EVENT_VIDEO_ENCODE_STATUS_STOPPED         "venc.status.stopped"
#define ZS_IPC_EVENT_VIDEO_ENCODE_STATUS_CONFIGCHANGED   "venc.status.config_changed"
#define ZS_IPC_EVENT_VIDEO_ENCODE_ERROR                  "venc.error"

#define ZS_IPC_EVNET_STORAGE_PREFIX         "stor."
#define ZS_IPC_EVENT_STORAGE_STATUS         "stor.status"
#define ZS_IPC_EVENT_STORAGE_ERROR          "stor.error"
// maybe into status event
//#define ZS_IPC_EVENT_STORAGE_LOW            "stor.low"
//#define ZS_IPC_EVENT_STORAGE_FULL           "stor.full"
//#define ZS_IPC_EVENT_STORAGE_FORMAT_STARTED "stor.format_started"
//#define ZS_IPC_EVENT_STORAGE_FORMAT_ENDED   "stor.format_ended"
//#define ZS_IPC_EVENT_STORAGE_AUTO_REMOVED   "stor.auto_removed"

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
#define ZS_IPC_STRING_LEN 128

typedef struct zs_ipc_event_hdr_st
{
    char szTopic[ZS_IPC_EVENT_TOPIC_LEN];
    uint32_t u32SeqId;
    char szUtcString[ZS_IPC_STRING_LEN];
    uint64_t u64LocalTimestampNs;
    uint64_t u64MonoTimestampNs;
    uint32_t u32MsgSize;
} stZsIpcEventHdr;

#define ZS_IPC_HEADER_MAX_SIZE 128

typedef struct zs_ipc_msg_hdr_st
{
    uint32_t u32FourCC;
    uint32_t u32HdrSize;
    uint8_t u8aHdr[ZS_IPC_HEADER_MAX_SIZE]; // 0: result, 1: cmd_hi, 2: cmd_low
    uint32_t u32PayloadSize;
} stZsIpcMsgHdr;

#define ZS_IPC_NAME_LEN 256
#define ZS_IPC_PATH_LEN 4096

typedef struct zs_ipc_event_rec_status_started_st
{
    uint64_t u64StartTimeStampMs;
    char szFilename[ZS_IPC_NAME_LEN];
} stZsIpcEventRecStatusStarted;

typedef struct zs_ipc_event_rec_status_stopped_st
{
    uint64_t u64StartTimeStampMs;
    char szFilename[ZS_IPC_NAME_LEN];
    bool hasRecordFile;
    char szRecordFilePath[ZS_IPC_PATH_LEN];
    bool hasSnapshotFile;
    char szSnapshotFilePath[ZS_IPC_PATH_LEN];
    bool hasAudioFile;
    char szAudioFilePath[ZS_IPC_PATH_LEN];
} stZsIpcEventRecStatusStopped;

typedef struct zs_ipc_video_source_config_st
{
    uint32_t u32MaxWidth;
    uint32_t u32MaxHeight;
    uint32_t u32PosX;
    uint32_t u32PosY;
    uint32_t u32Width;
    uint32_t u32Height;
} stZsIpcVsrcConfig;

typedef enum video_encoder_codec_type_e
{
	eVencCodecType_H264 = 0,    //! H.264 Codec
	eVencCodecType_H265,        //! H.265 Codec
	eVencCodecType_MJPG,        //! MJPG Codec
	eVencCodecType_NONE = 9,    //! No codec
} eVencCodecType;

typedef struct zs_ipc_video_encoder_config_st
{
    uint32_t u32Idx;
    eVencCodecType eCodec;
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
} stZsIpcVencConfig;

#define ZS_IPC_VENC_MAX_NUM 5

typedef struct zs_ipc_event_vsrc_status_init_done_st
{
    // vsrc
    stZsIpcVsrcConfig stVsrcConfig;
    // venc
    uint32_t u32VencNum;
    stZsIpcVencConfig stVencConfig[ZS_IPC_VENC_MAX_NUM];
} stZsIpcEventVsrcStatusInitDone;

typedef struct zs_ipc_event_default_st
{
    int code;
} stZsIpcDefault;

typedef stZsIpcDefault stZsIpcEventVsrcStatusClosing;

typedef struct zs_ipc_event_venc_status_started_st
{
    stZsIpcVencConfig stVencConfig;
} stZsIpcEventVencStatusStarted;

typedef stZsIpcDefault stZsIpcEventVencStatusStopped;

#ifdef __cplusplus
}
#endif
#endif /* ZWSYSTEM_IPC_EVENT_DEFINED_H */
