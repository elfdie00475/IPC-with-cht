#include "cht_p2p_camera_streaming_handler.h"


ChtP2PCameraStreamingHandler &ChtP2PCameraStreamingHandler::getInstance()
{
    static ChtP2PCameraStreamingHandler instance;

    return instance;
}

ChtP2PCameraStreamingHandler::ChtP2PCameraStreamingHandler()
: m_initialized{false}
{
    initialize();
}

ChtP2PCameraStreamingHandler::~ChtP2PCameraStreamingHandler()
{
    deinitialize();
}

bool ChtP2PCameraStreamingHandler::initialize(void)
{
    if (m_initialized) return true;

    m_initialized = true;
    return true;
}

void ChtP2PCameraStreamingHandler::deinitialize()
{
    if (!m_initialized)
    {
        return;
    }

    m_initialized = false;
}

void ChtP2PCameraStreamingHandler::audioCallback(const char *data, size_t dataSize, const char *metadata, void *userParam)
{

}
