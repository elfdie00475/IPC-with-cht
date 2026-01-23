/**
 * @file cht_p2p_camera_api.h
 * @brief CHT P2P Camera API - IP Camera與CHT P2P Agent SDK整合的API介面
 * @date 2025/04/29
 */

#ifndef CHT_P2P_CAMERA_COMMAND_HANDLER_H
#define CHT_P2P_CAMERA_COMMAND_HANDLER_H

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "cht_p2p_agent_c.h"

struct BindCameraConfig {
    std::string userId;
    std::string netNo;
    std::string wifiSsid;
    std::string wifiPassword; // base64?
};

/**
 * @brief CHT P2P Camera API類，IP Camera用此類調用CHT P2P Agent功能
 */
class ChtP2PCameraCommandHandler
{
public:
    /**
     * @brief 建構函式
     */
    ChtP2PCameraCommandHandler();

    /**
     * @brief 解構函式
     */
    ~ChtP2PCameraCommandHandler();

    static ChtP2PCameraCommandHandler &getInstance();

    bool initialize(void);

    void deinitialize(void);

    int bindCamera(const BindCameraConfig& config);
    int cameraRegister(void);
    int checkHiOSSstatus(bool& hiOssStatus);
    int getHamiCameraInitialInfo(void);

    // 事件回報
    bool reportSnapshot(const std::string &eventId, const std::string &snapshotTime, const std::string &filePath);
    bool reportRecord(const std::string &eventId, const std::string &fromTime, const std::string &toTime,
                      const std::string &filePath, const std::string &thumbnailfilePath);
    bool reportRecognition(const std::string &eventId, const std::string &eventTime,
                           const std::string &eventType, const std::string &eventClass,
                           const std::string &videoFilePath, const std::string &snapshotFilePath,
                           const std::string &audioFilePath, const std::string &coordinate,
                           const std::string &resultAttributeJson);
    bool reportStatusEvent(const std::string &eventId, int type, const std::string &camId,
                           const std::string &statusOrHealth, bool isStatus);
    // 參數管理器輔助函數
    void scheduledSync();
    bool getNetworkStatus();
    bool getStorageStatus();

    // 串流相關
    bool sendStreamData(const void *data, size_t dataSize, const std::string &requestId);

    // 回調設置
    //void setInitialInfoCallback(ChtP2PCameraAPI::InitialInfoCallback callback);
    //void setControlCallback(ChtP2PCameraAPI::ControlCallback callback);
    //void setAudioDataCallback(ChtP2PCameraAPI::AudioDataCallback callback);

public:
    // CHT P2P Agent回調處理函數
    void commandDoneCallback(CHTP2P_CommandType commandType, void *commandHandle, const char *payload, void *userParam);

private:
    // 攝影機註冊相關
    bool bindCameraReport(const std::string& camId,
        const std::string &userId, const std::string &name, const std::string &netNo,
        const std::string &firmwareVer, const std::string &externalStorageHealth,
        const std::string &wifiSsid, int wifiDbm, const std::string &status,
        const std::string &vsDomain, const std::string &vsToken, const std::string &macAddress,
        const std::string &activeStatus, const std::string &deviceStatus,
        const std::string &cameraType, const std::string &model,
        const std::string &isCheckHioss, const std::string &brand,
        const std::string &chtBarcode);

    bool cameraRegister(const std::string& camId, const std::string& chtBarcode);

    bool checkHiOSSstatus(const std::string& camId, const std::string& chtBarcode, const std::string& publicIp);

    bool getHamiCamInitialInfo(const std::string& camId, const std::string& chtBarcode,
            const std::string& tenantId, const std::string& netNo, const std::string& userId);

    static void controlCallback(CHTP2P_ControlType controlType, void *controlHandle, const char *payload, void *userParam);
    static void audioCallback(const char *data, size_t dataSize, const char *metadata, void *userParam);

    // 命令處理幫助函數
    bool sendCommand(CHTP2P_CommandType commandType, const std::string &payload, std::string &response);
    bool sendControlDone(CHTP2P_ControlType controlType, void *controlHandle, const std::string &payload);

    // 成員變量
    bool m_initialized;       // 初始化狀態
    std::mutex m_mutex;       // 互斥鎖

    // 回調函數
    //ChtP2PCameraAPI::InitialInfoCallback m_initialInfoCallback;
    //ChtP2PCameraAPI::ControlCallback m_controlCallback;
    //ChtP2PCameraAPI::AudioDataCallback m_audioDataCallback;

    // 命令回應同步處理
    struct CommandContext
    {
        std::mutex mutex;
        std::condition_variable cv;
        bool done;
        std::string response;
    };
    std::map<void *, std::shared_ptr<CommandContext>> m_commandContexts;

    void handleInitialInfoReceived(const std::string &hamiCamInfo,
                                   const std::string &hamiSettings,
                                   const std::string &hamiAiSettings,
                                   const std::string &hamiSystemSettings);

    bool syncParametersToHardware();
};

#endif // CHT_P2P_CAMERA_COMMAND_HANDLER_H
