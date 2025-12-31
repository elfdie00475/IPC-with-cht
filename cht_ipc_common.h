#ifndef LLT_CHT_IPC_COMMON_H
#define LLT_CHT_IPC_COMMON_H

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#include <zwp2pagent/cht_p2p_agent_c.h>

#define CHT_IPC_NAME "system_service.ipc"
#define CHT_IPC_STRING_SIZE 256

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)  ((unsigned int)(unsigned char)(ch0) | \
    ((unsigned int)(unsigned char)(ch1) << 8) | ((unsigned int)(unsigned char)(ch2) << 16) | \
    ((unsigned int)(unsigned char)(ch3) << 24 ))
#endif // MAKEFOURCC
#define CHT_IPC_FOURCC (MAKEFOURCC('C','H','T','1'))

typedef struct cht_ipc_hdr_st {
    uint32_t u32FourCC;
    uint16_t u16CmdType;
    uint16_t u16Id; // bit1: 0: req, 1: res
    uint32_t u32PayloadSize;
    int result;
} stChtIpcHdr;

#ifdef __cplusplus
extern "C" {
#endif

static void cht_ipc_hdr_init(stChtIpcHdr *m, uint16_t u16Id, uint16_t u16CmdType)
{
    m->u32FourCC = CHT_IPC_FOURCC;
    m->u16CmdType = u16CmdType;
    m->u16Id = u16Id;
    m->u32PayloadSize = 0;
    m->result = 0;
}

static void cht_ipc_hdr_free(stChtIpcHdr *m)
{
    if (!m) return ;

    m->result = 0;
    m->u16Id = 0xFFFF;
    m->u16CmdType = 0xFFFF;
    m->u32PayloadSize = 0;
}

static int cht_ipc_hdr_checkFourCC(uint32_t u32FourCC)
{
    printf("%x %x\n", u32FourCC, CHT_IPC_FOURCC);
    return ((u32FourCC == CHT_IPC_FOURCC)?1:0);
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace llt {

struct ChtIpcHdr {
    stChtIpcHdr m{};
    ChtIpcHdr(uint16_t u16Id, uint16_t u16CmdType) { 
        cht_ipc_hdr_init(&m, u16Id, u16CmdType); 
    }
    ~ChtIpcHdr() { cht_ipc_hdr_free(&m); }
};

} // namespace llt
#endif

#endif /* LLT_CHT_IPC_COMMON_H */
