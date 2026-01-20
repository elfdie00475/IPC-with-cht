#ifndef LLT_NNGIPC_IPCPUBLISHHANDLER_H
#define LLT_NNGIPC_IPCPUBLISHHANDLER_H

#include <memory>
#include <string>
#include <vector>

#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>

namespace llt {
namespace nngipc {

class PublishHandler
{
public:
    static std::shared_ptr<PublishHandler> create(const char *ipc_name, bool proxyMode = false);

public:
    ~PublishHandler();

    bool init(void);

    bool release(void);

    bool append(const uint8_t *payload, size_t payload_len);

    bool send(void);

private:
    PublishHandler(const char *ipc_name, bool proxyMode);

    const std::string m_ipcName;
    nng_socket m_sock;
    nng_msg *m_msg;
    bool m_init;
    bool m_proxyMode;

}; // class PublishHandler

} // namespace nngipc
} // namespace llt

#endif /* LLT_NNGIPC_IPCPUBLISHHANDLER_H */
