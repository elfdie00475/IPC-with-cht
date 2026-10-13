#include <stdlib.h>
#include <string.h>

#include <string>

#include <nng/nng.h>

#include "NngIpcAioWorker.h"

namespace llt::nngipc {

std::shared_ptr<AioWorker> AioWorker::create(nng_socket sock, TYPE type,
    OutputCallback cb, void *cb_param)
{
    if (sock.id == 0) return nullptr;

    if (type != TYPE::Response && type != TYPE::Subscribe) return nullptr;

    const auto& worker = std::shared_ptr<AioWorker>(new AioWorker(sock, type, cb, cb_param));
    if (!worker) {
        return nullptr;
    }

    if (!worker->init()) {
        return nullptr;
    }

    return worker;
}

AioWorker::AioWorker(nng_socket sock, TYPE type,
    OutputCallback cb, void *cb_param)
: m_sock{sock},
  m_cb{cb},
  m_cbParam{cb_param},
  m_state{STATE::INIT},
  m_type{type},
  m_stopping{false}
{
}

AioWorker::~AioWorker()
{
    release();
}

static bool worker_free_wrapper(nng_aio **ppAio, nng_ctx *pCtx)
{
    if (ppAio && *ppAio) {
        nng_aio_free(*ppAio);
        *ppAio = NULL;
    }

    if (pCtx && pCtx->id != 0) {
        nng_ctx_close(*pCtx);
    }

    if (pCtx) {
        *pCtx = NNG_CTX_INITIALIZER;
    }

    return true;
}

bool AioWorker::init(void)
{
    int rv = 0;

    nng_aio *pAio = NULL;
    nng_ctx ctx   = NNG_CTX_INITIALIZER;

    do {

        if ((rv = nng_aio_alloc(&pAio, NULL, NULL)) != 0) {
            fprintf(stderr, "%s: %s\n", "nng_aio_alloc", nng_strerror(rv));
            break;
        }

        if ((rv = nng_ctx_open(&ctx, m_sock)) != 0) {
            fprintf(stderr, "%s: %s\n", "nng_ctx_open", nng_strerror(rv));
            break;
        }

        m_aio = pAio;
        m_ctx = ctx;

        return true;
    } while (false);

    worker_free_wrapper(&pAio, &ctx);

    return false;
}

void AioWorker::start(void)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (!m_aio || m_stopping || m_thread.joinable()) return;
    m_thread = std::thread(&AioWorker::workerLoop, this);
}

void AioWorker::workerLoop(void)
{
    process();
    while (!m_stopping.load()) {
        nng_aio_wait(m_aio);
        process();
    }
}

void AioWorker::process(void)
{
    if (!m_aio) return ;

    nng_msg *msg = NULL;
    STATE curr_state = STATE::ERROR;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        curr_state = m_state;
    }

    int rv = nng_aio_result(m_aio);
    if (rv != 0 || m_stopping.load()) {
        fprintf(stderr, "%s: %s\n", "process_worker", nng_strerror(rv));
        if (rv == NNG_ECANCELED || rv == NNG_ECLOSED || m_stopping.load()) {
            msg = nng_aio_get_msg(m_aio);
            nng_aio_set_msg(m_aio, NULL);
            if (msg) nng_msg_free(msg);

            return ;
        } else if (rv != 0) {
            if (curr_state == STATE::SEND) {
                msg = nng_aio_get_msg(m_aio);
                nng_aio_set_msg(m_aio, NULL);
                if (msg) nng_msg_free(msg);
            }
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_state = STATE::ERROR;
            }
            nng_sleep_aio(1000, m_aio);
            return ;
        }
    }
    switch (curr_state) {
    case STATE::INIT:
        {
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_state = STATE::RECV;
            }
            nng_ctx_recv(m_ctx, m_aio);
        }
        break;
    case STATE::RECV:
        {
            msg = nng_aio_get_msg(m_aio);
            // The callback owns the received message until it is either sent
            // as a response or freed. Do not leave a freed message in the aio.
            nng_aio_set_msg(m_aio, NULL);
            if (!msg) {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_state = STATE::ERROR;
                nng_sleep_aio(1000, m_aio);
                break;
            }
            const uint8_t *req_payload = (const uint8_t *)nng_msg_body(msg);
            size_t req_len = nng_msg_len(msg);
            uint8_t *rep_payload = NULL;
            size_t rep_len = 0;

            if (m_cb) {
                // pass msg to handle
                m_cb(m_cbParam, req_payload, req_len, &rep_payload, &rep_len);
            }

            {
                // stop() may run while the user callback is executing. Do not
                // submit another NNG operation after the worker has been stopped.
                std::lock_guard<std::mutex> lock(m_stateMutex);
                if (m_stopping) {
                    if (rep_payload) free(rep_payload);
                    nng_msg_free(msg);
                    return;
                }
            }

            nng_msg_clear(msg);

            if (rep_payload && rep_len) {
                rv = nng_msg_append(msg, rep_payload, rep_len);

                if (rep_payload) free(rep_payload);

                if (rv != 0) {
                    fprintf(stderr, "%s: %s\n", "process_worker", nng_strerror(rv));
                    nng_msg_free(msg);
                    {
                        std::lock_guard<std::mutex> lock(m_stateMutex);
                        m_state = STATE::ERROR;
                    }
                    nng_sleep_aio(1000, m_aio);
                    break;
                }

                nng_aio_set_msg(m_aio, msg);
                {
                    std::lock_guard<std::mutex> lock(m_stateMutex);
                    m_state = STATE::SEND;
                }
                nng_ctx_send(m_ctx, m_aio);
            } else {
                if (rep_payload) free(rep_payload);
                // Subscribers have no response; each received event must be
                // released before the next recv overwrites the aio slot.
                nng_msg_free(msg);
                {
                    std::lock_guard<std::mutex> lock(m_stateMutex);
                    m_state = STATE::RECV;
                }
                nng_ctx_recv(m_ctx, m_aio);
            }
        }
        break;
    case STATE::SEND:
    case STATE::ERROR:
        {
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_state = STATE::RECV;
            }
            nng_ctx_recv(m_ctx, m_aio);
        }
        break;
    default:
        {
            fprintf(stderr, "%s: %s\n", "process_worker", nng_strerror(NNG_ESTATE));
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_state = STATE::ERROR;
            }
            nng_sleep_aio(1000, m_aio);
        }
        break;
    }
}

void AioWorker::stop(void)
{
    std::thread worker;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_stopping.store(true);
        worker = std::move(m_thread);
    }

    if (m_aio) {
        nng_aio_stop(m_aio);
        // A canceled recv/send may still own a message. Detach it only after
        // the application callback has returned and the worker has stopped.
    }
    if (worker.joinable()) worker.join();
    if (m_aio) {
        nng_msg *msg = nng_aio_get_msg(m_aio);
        nng_aio_set_msg(m_aio, NULL);
        if (msg) nng_msg_free(msg);
    }
}


bool AioWorker::subscribe(const std::string& subscribe_str)
{
    int rv = 0;
    if ((rv = nng_sub0_ctx_subscribe(m_ctx, subscribe_str.c_str(), subscribe_str.size())) != 0) {
        fprintf(stderr, "%s: %s\n", "nng_setopt NNG_OPT_SUB_SUBSCRIBE", nng_strerror(rv));
        return false;
    }

    return true;
}

bool AioWorker::unsubscribe(const std::string& subscribe_str)
{
    if (subscribe_str.empty()) return true;

    int rv = 0;
    if ((rv = nng_sub0_ctx_unsubscribe(m_ctx, subscribe_str.c_str(), subscribe_str.size())) != 0) {
        fprintf(stderr, "%s: %s\n", "nng_setopt NNG_OPT_SUB_SUBSCRIBE", nng_strerror(rv));
        return false;
    }

    return true;
}

bool AioWorker::release(void)
{
    stop();

    return worker_free_wrapper(&m_aio, &m_ctx);
}

} // namespace llt::nngipc
