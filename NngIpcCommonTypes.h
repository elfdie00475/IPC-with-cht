#ifndef LLT_IPC_COMMON_DEFINED_H
#define LLT_IPC_COMMON_DEFINED_H

#include <stdint.h>

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
#define ZSIPC_NAME_LEN   256
#define ZSIPC_PATH_LEN   4096

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
#define ZSIPC_TOPIC_LEN  32
typedef struct
{
    char     szTopic[ZSIPC_TOPIC_LEN];
    uint32_t u32SeqId;
    char     szUtcString[ZSIPC_STRING_LEN];
    uint64_t u64LocalTimestampNs;
    uint64_t u64MonoTimestampNs;
    uint32_t u32MsgSize;
} ZSIPCEventHdr;

// struct
// msg header
// -- msg fourCC
// -- msg header size
// -- msg header array
// -- msg payload size (data payload)
// data payload (custom struct)
#define ZSIPC_HEADER_MAX_SIZE 128
typedef struct
{
    uint32_t u32FourCC;
    uint32_t u32HdrSize;
    uint8_t  au8Hdr[ZSIPC_HEADER_MAX_SIZE]; // 0: result, 1: cmd_hi, 2: cmd_low
    uint32_t u32PayloadSize;
} ZSIPCMsgHdr;

typedef struct
{
    int code;
} ZSIPCDefaultMsg;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LLT_IPC_COMMON_DEFINED_H
