#ifndef CHT_P2P_CAMERA_STREAMING_HANDLER_H
#define CHT_P2P_CAMERA_STREAMING_HANDLER_H

#include <mutex>

#include "cht_p2p_agent_c.h"

class ChtP2PCameraStreamingHandler
{
public:
    /**
     * @brief 建構函式
     */
    ChtP2PCameraStreamingHandler();

    /**
     * @brief 解構函式
     */
    ~ChtP2PCameraStreamingHandler();

    static ChtP2PCameraStreamingHandler &getInstance();

    bool initialize(void);

    void deinitialize(void);

public:
    // CHT P2P Agent回調處理函數
    void audioCallback(const char *data, size_t dataSize, const char *metadata, void *userParam);

private:
    // 成員變量
    bool m_initialized;       // 初始化狀態
    std::mutex m_mutex;       // 互斥鎖
};

#endif // CHT_P2P_CAMERA_STREAMING_HANDLER_H
