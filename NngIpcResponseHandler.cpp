#include <stdlib.h>
#include <string.h>

#include <string>

#include "NngIpcResponseHandler.h"

#ifndef NNGIPC_DIR_PATH
#define NNGIPC_DIR_PATH "/tmp/nngipc"
#endif

namespace llt {
namespace nngipc {

static const uint32_t gc_maxWorkerNum = 8;

static void process_worker(void *args);
static void free_worker(Worker **ppWorker);

static Worker *alloc_worker(nng_socket sock, OutputCallback callback)
{
    Worker *pWorker = NULL;
    int rv = 0;

    do {
        if ((pWorker = (Worker *)nng_alloc(sizeof(Worker))) == NULL) {
            fprintf(stderr, "%s: %s\n", "nng_alloc", nng_strerror(NNG_ENOMEM));
            break;
        }

        if ((rv = nng_aio_alloc(&pWorker->aio, process_worker, pWorker)) != 0) {
            fprintf(stderr, "%s: %s\n", "nng_aio_alloc", nng_strerror(rv));
            break;
        }

        if ((rv = nng_ctx_open(&pWorker->ctx, sock)) != 0) {
            fprintf(stderr, "%s: %s\n", "nng_ctx_open", nng_strerror(rv));
            break;
        }

        pWorker->cb = callback;
        pWorker->state = Worker::INIT;
        pWorker->stopping = false;

        return pWorker;
    } while (false);

    free_worker(&pWorker);

    return NULL;
}

static void free_worker(Worker **ppWorker)
{
    if (!ppWorker || !(*ppWorker)) return ;

    Worker *pWorker = (*ppWorker);
    pWorker->stopping = true;

    if (pWorker->aio) { // wait aio stop work
        nng_aio_free(pWorker->aio);
        pWorker->aio = NULL;
    }

    nng_ctx_close(pWorker->ctx);

    if (pWorker) {
        nng_free(pWorker, sizeof(Worker));
    }
    
    *ppWorker = NULL;
}

static void process_worker(void *args)
{
    Worker *pWorker = (Worker *)args;
    if (!pWorker || !pWorker->aio) return ;

    nng_msg *msg = NULL;
    int rv = nng_aio_result(pWorker->aio);
    uint32_t when = 0;

    if (rv != 0 || pWorker->stopping) {
        fprintf(stderr, "%s: %s\n", "process_worker", nng_strerror(rv));
        if (rv == NNG_ECANCELED || rv == NNG_ECLOSED || pWorker->stopping) {
            msg = nng_aio_get_msg(pWorker->aio);
            if (msg) nng_msg_free(msg);

            return ;
        } else if (rv != 0) {
            if (pWorker->state == Worker::SEND) {
                msg = nng_aio_get_msg(pWorker->aio);
                if (msg) nng_msg_free(msg);
            }
            pWorker->state = Worker::ERROR;
            nng_sleep_aio(1000, pWorker->aio);
            return ;
        }
    }

    switch (pWorker->state) {
    case Worker::INIT:
        {
            pWorker->state = Worker::RECV;
            nng_ctx_recv(pWorker->ctx, pWorker->aio);
        }
        break;
    case Worker::RECV:
        {
            msg = nng_aio_get_msg(pWorker->aio);
            const uint8_t *req_payload = (const uint8_t *)nng_msg_body(msg);
            size_t req_len = nng_msg_len(msg);
            uint8_t *rep_payload = NULL;
            size_t rep_len = 0;

            if (pWorker->cb) {
                // pass msg to handle
                pWorker->cb(req_payload, req_len, &rep_payload, &rep_len);
            }

            nng_msg_clear(msg);

            if (rep_payload && rep_len) {
                rv = nng_msg_append(msg, rep_payload, rep_len);
            } else {
                rv = 0;
            }

            if (rep_payload) free(rep_payload);

            if (rv != 0) {
                fprintf(stderr, "%s: %s\n", "process_worker", nng_strerror(rv));
                nng_msg_free(msg);
                pWorker->state = Worker::ERROR;
                nng_sleep_aio(1000, pWorker->aio);
                break;
            }

            nng_aio_set_msg(pWorker->aio, msg);
            pWorker->state = Worker::SEND;
            nng_ctx_send(pWorker->ctx, pWorker->aio);
        }
        break;
    case Worker::SEND:
    case Worker::ERROR:
        {
            pWorker->state = Worker::RECV;
            nng_ctx_recv(pWorker->ctx, pWorker->aio);
        }
        break;
    default:
        {
            fprintf(stderr, "%s: %s\n", "process_worker", nng_strerror(NNG_ESTATE));
            pWorker->state = Worker::ERROR;
            nng_sleep_aio(1000, pWorker->aio);
        }
        break;
    }
}

static void stop_worker(void *args)
{
    Worker *pWorker = (Worker *)args;
    if (!pWorker) return ;

    pWorker->stopping = true;

    if (pWorker->aio) nng_aio_stop(pWorker->aio);
}

std::shared_ptr<ResponseHandler> ResponseHandler::create(
    const char *ipc_name, uint32_t worker_num, OutputCallback cb)
{
    if (!ipc_name || strlen(ipc_name) == 0) {
        return nullptr;
    }

    if (worker_num == 0) worker_num = 1;
    else if (worker_num > gc_maxWorkerNum) worker_num = gc_maxWorkerNum;

    const auto& handler = std::shared_ptr<ResponseHandler>(new ResponseHandler(ipc_name, worker_num, cb));
    if (!handler) {
        return nullptr;
    }

    if (!handler->init()) {
        return nullptr;
    }

    return handler;
}

ResponseHandler::ResponseHandler(
    const char *ipc_name, uint32_t worker_num, OutputCallback cb)
: m_ipcName{std::string(ipc_name)},
  m_workerNum{worker_num},
  m_outputCb{cb},
  m_init{false}
{
    m_sock.id = 0;
}

ResponseHandler::~ResponseHandler()
{
    stop();
    release();
}

bool ResponseHandler::init(void)
{
    // create ipc folder
    std::string cmd = std::string("mkdir -p ") + NNGIPC_DIR_PATH;
    system(cmd.c_str());
    
    /*  Create the socket. */
    int rv = 0;
    if ((rv = nng_rep0_open(&m_sock)) != 0) {
        fprintf(stderr, "%s: %s\n", "nng_rep0_open", nng_strerror(rv));
        return false;
    }

    for (uint32_t i = 0; i < m_workerNum; i++) {
        Worker *worker = alloc_worker(m_sock, m_outputCb);
        if (worker) m_workers.push_back(worker);
    }

    m_init = true;
    return true;
}

bool ResponseHandler::start(void)
{
    if (!m_init) return false;

    std::string url = std::string("ipc://") + std::string(NNGIPC_DIR_PATH) + "/" + m_ipcName;
    int rv = 0;
    if ((rv = nng_listen(m_sock, url.c_str(), NULL, 0)) != 0) {
        fprintf(stderr, "%s: %s url %s\n", "nng_listen", nng_strerror(rv), url.c_str());
        return false;
    }

    for (auto& worker : m_workers) {
        process_worker((void *)worker);
    }

    return true;
}

bool ResponseHandler::stop(void)
{
    for (auto& worker : m_workers) {
        stop_worker(&worker);
    }

    return true;
}

bool ResponseHandler::release(void)
{
    for (auto& worker : m_workers) {
        free_worker(&worker);
        worker->ctx = NNG_CTX_INITIALIZER;
    }

    nng_close(m_sock);
    m_sock = NNG_SOCKET_INITIALIZER;

    m_init = false;
    return true;
}

} // namespace ipc
} // namespace llt
