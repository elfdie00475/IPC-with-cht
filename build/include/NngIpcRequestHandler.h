#ifndef LLT_NNGIPC_IPCREQUESTHANDLER_H
#define LLT_NNGIPC_IPCREQUESTHANDLER_H

#include <memory>
#include <string>
#include <vector>

#include <nng/nng.h>
#include <nng/protocol/reqrep0/req.h>

namespace llt {
namespace nngipc {

class RequestHandler
{
public:
    static std::shared_ptr<RequestHandler> create(const char *ipc_name);

public:
    ~RequestHandler();

    bool init(void);

    bool release(void);

    bool append(const uint8_t *payload, size_t payload_len);

    bool send(void);

    bool recv(uint8_t **payload, size_t *payload_len);

private:
    RequestHandler(const char *ipc_name);

    const std::string m_ipcName;
    nng_socket m_sock;
    nng_msg *m_msg;
    bool m_init;

}; // class RequestHandler

} // namespace nngipc
} // namespace llt

#endif /* LLT_NNGIPC_IPCREQUESTHANDLER_H */