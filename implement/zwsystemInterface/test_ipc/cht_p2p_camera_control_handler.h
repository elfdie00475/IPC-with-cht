/**
 * @file cht_p2p_camera_control_handler.h
 * @brief CHT P2P Camera控制處理器 - 處理P2P Agent發送給Camera的控制指令
 * @date 2025/07/17
 */

#ifndef CHT_P2P_CAMERA_CONTROL_HANDLER_H
#define CHT_P2P_CAMERA_CONTROL_HANDLER_H

#include <string>
#include <map>
#include <functional>
#include <chrono>
#include <future>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "cht_p2p_agent_c.h"

#include "camera_parameters_manager.h"

/**
 * @brief CHT P2P Camera控制處理器類
 */
class ChtP2PCameraControlHandler
{
public:
    typedef std::function<std::string(ChtP2PCameraControlHandler *self, const std::string &)> ControlHandlerFunc;

    // ===== 單例模式 =====
    static ChtP2PCameraControlHandler &getInstance();

    ~ChtP2PCameraControlHandler();

    // ===== 核心處理函數 =====
    std::string handleControl(CHTP2P_ControlType controlType, const std::string &payload);
    void registerHandler(CHTP2P_ControlType controlType, ControlHandlerFunc handler);
    void registerDefaultHandlers();

    // ===== 時區管理函數 =====
    static bool setSystemTimezone(const std::string &tzString);
    static bool verifySystemTimezone(const std::string &expectedTzString);
    static bool reloadSystemTimezone();
    static void displayCurrentTimezoneStatus();
    bool updateOsdTimezone(const std::string &tzString);
    static bool executeExportTZ(const std::string &tzString);
    static bool verifyExternalEnvironment(const std::string &expectedTzString);
    static bool createParentShellSolution(const std::string &tzString);

private:
    ChtP2PCameraControlHandler();
    std::map<CHTP2P_ControlType, ControlHandlerFunc> m_handlers;

    // ===== 基本狀態與管理 =====
    static std::string handleGetCamStatusById(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleDeleteCameraInfo(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleUpdateCameraName(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleGetHamiCamBindList(ChtP2PCameraControlHandler *self, const std::string &payload);

    // ===== 時區控制 =====
    static std::string handleSetTimeZone(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleGetTimeZone(ChtP2PCameraControlHandler *self, const std::string &payload);

    // ===== 影像設定 =====
    static std::string handleSetCameraOSD(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleSetCameraHD(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleSetFlicker(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleSetImageQuality(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleSetNightMode(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleSetAutoNightVision(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleSetFlipUpDown(ChtP2PCameraControlHandler *self, const std::string &payload);

    // ===== 音頻控制 =====
    static std::string handleSetMicrophone(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleSetSpeak(ChtP2PCameraControlHandler *self, const std::string &payload);

    // ===== 系統控制 =====
    static std::string handleSetLED(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleSetCameraPower(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleGetSnapshotHamiCamDevice(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleRestartHamiCamDevice(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleUpgradeHamiCamOTA(ChtP2PCameraControlHandler *self, const std::string &payload);

    // ===== 存儲管理 =====
    static std::string handleSetCamStorageDay(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleSetCamEventStorageDay(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleHamiCamFormatSDCard(ChtP2PCameraControlHandler *self, const std::string &payload);

    // ===== PTZ控制 =====
    static std::string handleHamiCamPtzControlMove(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleHamiCamPtzControlConfigSpeed(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleHamiCamGetPtzControl(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleHamiCamPtzControlTourGo(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleHamiCamPtzControlGoPst(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleHamiCamPtzControlConfigPst(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleHamiCamHumanTracking(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleHamiCamPetTracking(ChtP2PCameraControlHandler *self, const std::string &payload);

    // ===== AI設定 =====
    static std::string handleUpdateCameraAISetting(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleGetCameraAISetting(ChtP2PCameraControlHandler *self, const std::string &payload);

    // ===== 串流控制 =====
    static std::string handleGetVideoLiveStream(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleStopVideoLiveStream(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleGetVideoHistoryStream(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleStopVideoHistoryStream(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleSendAudioStream(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleStopAudioStream(ChtP2PCameraControlHandler *self, const std::string &payload);
#if 1 // jeffery added 20250805
    static std::string handleGetVideoScheduleStream(ChtP2PCameraControlHandler *self, const std::string &payload);
    static std::string handleStopVideoScheduleStream(ChtP2PCameraControlHandler *self, const std::string &payload);
#endif
};

// ===== 全域輔助函數 =====
std::string getTimeWithOffset(const std::string &baseUtcOffset);
bool performNtpSync();

#endif // CHT_P2P_CAMERA_CONTROL_HANDLER_H
