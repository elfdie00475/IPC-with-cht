#ifndef LLT_CHT_IPC_COMMON_H
#define LLT_CHT_IPC_COMMON_H

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#include <zwp2pagent/cht_p2p_agent_c.h>

#define CHT_IPC_NAME "system_service.ipc"
#define CHT_IPC_HEADER_SIZE 32
#define CHT_IPC_STRING_SIZE 256

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)  ((unsigned int)(unsigned char)(ch0) | \
    ((unsigned int)(unsigned char)(ch1) << 8) | ((unsigned int)(unsigned char)(ch2) << 16) | \
    ((unsigned int)(unsigned char)(ch3) << 24 ))
#endif // MAKEFOURCC
#define CHT_IPC_FOURCC (MAKEFOURCC('C','H','T','1'))

typedef struct cht_ipc_hdr_st {
    uint32_t u32FourCC;
    uint32_t u32HdrSize;
    uint32_t u32PayloadSize;
    uint16_t u16Headers[CHT_IPC_HEADER_SIZE];
    // 0: msg id
    // 1: cmd type
    // 2: result
} stChtIpcHdr;

typedef struct cht_ipc_msg_st {
    stChtIpcHdr stHdr;
    uint8_t *pu8Payload;
} stChtIpcMsg;

#ifdef __cplusplus
extern "C" {
#endif

static void cht_ipc_msg_init(stChtIpcMsg *m, uint16_t u16MsgId, uint16_t u16CmdType)
{
    m->stHdr.u32FourCC = CHT_IPC_FOURCC;
    m->stHdr.u16Headers[0] = u16MsgId; // 0: msg id
    m->stHdr.u16Headers[1] = u16CmdType; // 1: cmd type
    m->stHdr.u32HdrSize = 2;
    m->stHdr.u32PayloadSize = 0;
    m->pu8Payload = NULL;
}

static void cht_ipc_msg_free(stChtIpcMsg *m)
{
    if (!m) return ;

    m->stHdr.u32FourCC = CHT_IPC_FOURCC;
    m->stHdr.u32HdrSize = 0;
    m->stHdr.u32PayloadSize = 0;
}

static int cht_ipc_msg_checkFourCC(uint32_t u32FourCC)
{
    return ((u32FourCC == CHT_IPC_FOURCC)?1:0);
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace llt {

struct ChtIpcMsg {
    stChtIpcMsg m{};
    ChtIpcMsg(uint16_t u16MsgId, uint16_t u16CmdType) { 
        cht_ipc_msg_init(&m, u16MsgId, u16CmdType); 
    }
    ~ChtIpcMsg() { cht_ipc_msg_free(&m); }
};

} // namespace llt
#endif

#endif /* LLT_CHT_IPC_COMMON_H */
