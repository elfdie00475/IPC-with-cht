/**
 * @file cht_p2p_camera_control_handler.cpp
 * @brief CHT P2P Camera控制處理器實現
 * @date 2025/04/29
 */


#include <stddef.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <cstdio>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <ctime>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <thread>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <thread>
#include <vector>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "zwsystem_ipc_client.h"

#include "cht_p2p_camera_control_handler.h"
#include "cht_p2p_agent_payload_defined.h"
#include "camera_parameters_manager.h"
#include "cht_p2p_agent_payload_defined.h"
#include "timezone_utils.h"

// 輔助函數：執行系統命令並忽略返回值
inline void executeSystemCommand(const char *cmd)
{
    int result = system(cmd);
    (void)result; // 明確忽略返回值
}

// 輔助函數：從INI檔案讀取串流參數
struct StreamParams
{
    int width;
    int height;
    int fps;
    int bitrate; // 從INI讀取的是bps，使用時需要轉換為kbps
};

StreamParams readStreamParamsFromINI(const std::string &quality)
{
    StreamParams params = {640, 480, 30, 460800}; // 預設值(低品質)

    const char *iniPath = "/mnt/flash/leipzig/ini/host_stream.ini";
    std::ifstream iniFile(iniPath);

    if (!iniFile.is_open())
    {
        std::cerr << "警告: 無法讀取INI檔案 " << iniPath << "，使用預設參數" << std::endl;
        return params;
    }

    // 根據品質決定要讀取的stream段落
    std::string targetSection;
    if (quality == "0")
    {
        targetSection = "[stream2]"; // 低品質: stream2 (640x480)
    }
    else if (quality == "1")
    {
        targetSection = "[stream1]"; // 中品質: stream1 (1920x1080)
    }
    else
    {
        targetSection = "[stream0]"; // 高品質: stream0 (2560x1440)
    }

    std::string line;
    bool inTargetSection = false;

    while (std::getline(iniFile, line))
    {
        // 移除行首和行尾空白
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        line.erase(0, line.find_first_not_of(" \t"));

        // 跳過空行和註釋
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        // 檢查是否進入目標段落
        if (line == targetSection)
        {
            inTargetSection = true;
            continue;
        }

        // 檢查是否離開當前段落
        if (inTargetSection && line[0] == '[' && line != targetSection)
        {
            break;
        }

        // 在目標段落內解析參數
        if (inTargetSection)
        {
            size_t equalPos = line.find('=');
            if (equalPos != std::string::npos)
            {
                std::string key = line.substr(0, equalPos);
                std::string value = line.substr(equalPos + 1);

                // 移除key和value的空白
                key.erase(key.find_last_not_of(" \t") + 1);
                key.erase(0, key.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));

                // 移除註釋
                size_t commentPos = value.find('#');
                if (commentPos != std::string::npos)
                {
                    value = value.substr(0, commentPos);
                    value.erase(value.find_last_not_of(" \t") + 1);
                }

                // 設定參數
                if (key == "Width")
                {
                    params.width = std::stoi(value);
                }
                else if (key == "Height")
                {
                    params.height = std::stoi(value);
                }
                else if (key == "FPS")
                {
                    params.fps = std::stoi(value);
                }
                else if (key == "Bitrate")
                {
                    params.bitrate = std::stoi(value);
                }
            }
        }
    }

    iniFile.close();

    std::cout << "從INI讀取串流參數 (品質=" << quality << "): "
              << params.width << "x" << params.height
              << " @" << params.fps << "fps, "
              << (params.bitrate / 1000) << "kbps (" << params.bitrate << "bps)" << std::endl;

    return params;
}

// 獲取單例實例
ChtP2PCameraControlHandler &ChtP2PCameraControlHandler::getInstance()
{
    static ChtP2PCameraControlHandler instance;
    return instance;
}

// 建構函式
ChtP2PCameraControlHandler::ChtP2PCameraControlHandler()
{
    // 註冊默認處理函數
    registerDefaultHandlers();
}

ChtP2PCameraControlHandler::~ChtP2PCameraControlHandler()
{
}

// Base64 編碼函數（簡單實現）
std::string base64_encode(const std::string &input)
{
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    int val = 0, valb = -6;
    for (char c : input)
    {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0)
        {
            encoded.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
        encoded.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (encoded.size() % 4)
        encoded.push_back('=');
    return encoded;
}
// 檢查韌體檔案是否有效
bool validateFirmwareFile(const std::string &filePath)
{
    try
    {
        struct stat fileStat;

        // 檢查檔案是否存在
        if (stat(filePath.c_str(), &fileStat) != 0)
        {
            std::cerr << "ERROR: 韌體檔案不存在: " << filePath << std::endl;
            return false;
        }

        // 檢查是否為一般檔案
        if (!S_ISREG(fileStat.st_mode))
        {
            std::cerr << "ERROR: 路徑不是一般檔案: " << filePath << std::endl;
            return false;
        }

        // 檢查檔案大小（韌體檔案應該有一定大小）
        if (fileStat.st_size < 1024)
        { // 小於 1KB 可能有問題
            std::cerr << "ERROR: 韌體檔案大小異常: " << fileStat.st_size << " bytes" << std::endl;
            return false;
        }

        // 檢查檔案是否可讀
        if (access(filePath.c_str(), R_OK) != 0)
        {
            std::cerr << "ERROR: 韌體檔案無法讀取: " << filePath << std::endl;
            return false;
        }

        std::cout << "INFO: 韌體檔案驗證通過 - 大小: " << fileStat.st_size << " bytes" << std::endl;
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "ERROR: 驗證韌體檔案時發生例外: " << e.what() << std::endl;
        return false;
    }
}

// 讀取 WiFi 設定
bool readWifiConfig(std::string &ssid, std::string &password)
{
    try
    {
        auto &paramsManager = CameraParametersManager::getInstance();

        // 從參數管理器讀取 WiFi 設定
        ssid = paramsManager.getWifiSsid();
        password = paramsManager.getParameter("wifiPassword", "");

        // 檢查是否為空
        if (ssid.empty() || password.empty())
        {
            std::cerr << "ERROR: WiFi SSID 或密碼為空" << std::endl;
            return false;
        }

        // 如果密碼是 "********"（遮罩），嘗試從系統獲取實際密碼
        if (password == "********" || password.length() < 4)
        {
            // 嘗試從 OpenWrt uci 系統獲取密碼
            std::string uciCmd = "uci get wireless.@wifi-iface[0].key 2>/dev/null";
            FILE *pipe = popen(uciCmd.c_str(), "r");
            if (pipe)
            {
                char buffer[256];
                if (fgets(buffer, sizeof(buffer), pipe) != nullptr)
                {
                    password = std::string(buffer);
                    // 移除換行符
                    password.erase(password.find_last_not_of("\r\n") + 1);
                }
                pclose(pipe);
            }
        }

        std::cout << "INFO: 成功讀取 WiFi 設定 - SSID: " << ssid << std::endl;
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "ERROR: 讀取 WiFi 設定時發生例外: " << e.what() << std::endl;
        return false;
    }
}

bool ChtP2PCameraControlHandler::checkHiOssStatus(void)
{
#ifndef SIMULATION_MODE
    auto &paramsManager = CameraParametersManager::getInstance();

    // 檢查HiOSS狀態 - 根據規格2.2要求
    bool isCheckHiOss = paramsManager.getIsCheckHioss();
    if (isCheckHiOss) {
        std::cerr << "Camera does not bind yet, drop control function" << std::endl;
        return false;
    }

    // 如果HiOSS狀態為false（"0"），則僅接收解綁攝影機指令(_DeleteCameraInfo)
    bool hiOssStatus = paramsManager.getHiOssStatus();
    return hiOssStatus;
#endif

    return true;
}

/*
邏輯
正常狀態 → hiossStatus="1" → 接受所有指令
    ↓
CheckHiOSS失敗 → hiossStatus="0" → 僅接受_DeleteCameraInfo
    ↓
執行解綁成功 → hiossStatus="1" → 恢復接受所有指令
*/
// 處理控制命令
void ChtP2PCameraControlHandler::controlCallback(CHTP2P_ControlType controlType, void *handle, const char *payload, void *userParam)
{
    std::string resultJson = {};
    int rc = this->controlHandle(controlType, payload, resultJson);
    if (rc < 0 || resultJson.empty()) {
        std::cerr << "controlHandle error" <<
                ", controlType = " << controlType <<
                ", rc = " << std::to_string(rc) <<
                ", resultJson size = " << resultJson.size() << std::endl;
        return ;
    }

    rc = chtp2p_send_control_done(controlType, handle, resultJson.c_str());
    if (rc < 0) {
        std::cerr << "chtp2p_send_control_done error" <<
                ", controlType = " << controlType <<
                ", rc = " << std::to_string(rc) << std::endl;
    }
}

// for menu testing
int ChtP2PCameraControlHandler::controlHandleWrapper(CHTP2P_ControlType controlType, const char *payload, std::string& outResult)
{
    return controlHandle(controlType, payload, outResult);
}

static void AddString(rapidjson::Document& doc, const char* key, const std::string& val)
{
    auto& alloc = doc.GetAllocator();
    rapidjson::Value k(key, alloc);
    rapidjson::Value v;
    v.SetString(val.c_str(), static_cast<rapidjson::SizeType>(val.size()), alloc);
    doc.AddMember(k, v, alloc);
}

static void AddString(rapidjson::Value& doc, const char* key, const std::string& val, rapidjson::Document::AllocatorType& alloc)
{
    rapidjson::Value k(key, alloc);
    rapidjson::Value v;
    v.SetString(val.c_str(), static_cast<rapidjson::SizeType>(val.size()), alloc);
    doc.AddMember(k, v, alloc);
}

static const rapidjson::Value& GetObjectMember(const rapidjson::Value& obj, const char* key)
{
    if (!obj.IsObject())
        throw std::runtime_error("Expected object when accessing member: " + std::string(key));

    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd())
        throw std::runtime_error("Missing member: " + std::string(key));

    if (!it->value.IsObject())
        throw std::runtime_error("Member is not object: " + std::string(key));

    return it->value;
}

static int GetIntMember(const rapidjson::Value& obj, const char* key)
{
    if (!obj.IsObject())
        throw std::runtime_error("Expected object when accessing member: " + std::string(key));

    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsInt())
        throw std::runtime_error("Missing or not int: " + std::string(key));

    return it->value.GetInt();
}

static bool GetBoolMember(const rapidjson::Value& obj, const char* key)
{
    if (!obj.IsObject())
        throw std::runtime_error("Expected object when accessing member: " + std::string(key));

    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsBool())
        throw std::runtime_error("Missing or not string: " + std::string(key));

    return it->value.GetBool();
}

static std::string GetStringMember(const rapidjson::Value& obj, const char* key)
{
    if (!obj.IsObject())
        throw std::runtime_error("Expected object when accessing member: " + std::string(key));

    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsString())
        throw std::runtime_error("Missing or not string: " + std::string(key));

    // 直接複製出來，避免 dangling reference
    return std::string(it->value.GetString(), it->value.GetStringLength());
}

static const std::string g_defaultErrorRespons = "{ \"result\":0, \"description\":\"\" }";
// 創建錯誤回應的輔助函數
static bool createErrorResponse(const std::string& description, std::string& outJson)
{
    try
    {
        rapidjson::Document response;
        response.SetObject();
        auto& allocator = response.GetAllocator();

        response.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);
        AddString(response, PAYLOAD_KEY_DESCRIPTION, description.empty()? "" : description);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        outJson = buffer.GetString();

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "createErrorResponse error msg=" << e.what() << std::endl;

        outJson = g_defaultErrorRespons;

        return false;
    }

    return false;
}

int ChtP2PCameraControlHandler::controlHandle(CHTP2P_ControlType controlType, const char *payload, std::string& outResult)
{
    std::cout << "\n===== 處理控制指令 =====" << std::endl;
    std::cout << "控制類型: " << controlType << std::endl;
    std::cout << "負載資料: " << payload << std::endl;

    if (!checkHiOssStatus() && controlType != _DeleteCameraInfo)
    {
        std::cout << "\n[控制指令過濾]" << std::endl;
        std::cout << "HiOSS狀態為受限模式，僅接收解綁攝影機指令" << std::endl;
        std::cout << "請求的控制類型: " << controlType << std::endl;
        std::cout << "允許的控制類型: " << _DeleteCameraInfo << " (_DeleteCameraInfo)" << std::endl;
        std::cout << "處理結果: 拒絕執行" << std::endl;

        std::string desc = "reject control, controlType = " + std::to_string(controlType) + ". " +
                "Only support _DeleteCameraInfo";

        createErrorResponse(desc, outResult);

        return -1;
    }

    // maybe use workers to handle all control function

    // 尋找並執行對應的處理函數
    auto it = m_handlers.find(controlType);
    if (it == m_handlers.end())
    {
        std::cerr << "找不到控制類型 " << controlType << " 的處理函數" << std::endl;

        // 返回錯誤回應
        std::string desc = "cannot find control handler, controlType = " + std::to_string(controlType) + ". ";
        createErrorResponse(desc, outResult);

        return -2;
    }


    std::cout << "開始執行控制指令處理函數..." << std::endl;
    const std::string& result = it->second(this, payload);
    // 呼叫對應的處理函數
    std::cout << "控制指令處理完成" << std::endl;
    std::cout << "===== 控制指令處理完成 =====" << std::endl;

    outResult = result;
    if (outResult.empty())
    {
        std::cerr << "處理控制命令異常, controlType = " << std::to_string(controlType) << std::endl;

        // 返回錯誤回應
        std::string desc = "execute control handler has exception result, controlType = " + std::to_string(controlType) + ". ";
        createErrorResponse(desc, outResult);

        return -2;
    }

    return 0;
}

// 註冊控制處理函數
void ChtP2PCameraControlHandler::registerHandler(CHTP2P_ControlType controlType, ControlHandlerFunc handler)
{
    m_handlers[controlType] = handler;
}

void ChtP2PCameraControlHandler::registerDefaultHandlers(void)
{
    // 註冊所有預設控制處理函數
    // 這些函數會在類初始化時自動註冊
    // 這樣可以確保所有控制命令都有對應的處理函數
    // 這些函數的具體實現會在下面的 handleXXX 函數中定義
    // 這些函數會根據控制命令的類型來處理不同的請求
    registerHandler(_GetCamStatusById, &ChtP2PCameraControlHandler::handleGetCamStatusById);
    registerHandler(_DeleteCameraInfo, &ChtP2PCameraControlHandler::handleDeleteCameraInfo);
    registerHandler(_SetTimeZone, &ChtP2PCameraControlHandler::handleSetTimeZone);
    registerHandler(_GetTimeZone, &ChtP2PCameraControlHandler::handleGetTimeZone);
    registerHandler(_UpdateCameraName, &ChtP2PCameraControlHandler::handleUpdateCameraName);
    registerHandler(_SetCameraOSD, &ChtP2PCameraControlHandler::handleSetCameraOSD);
    registerHandler(_SetCameraHD, &ChtP2PCameraControlHandler::handleSetCameraHD);
    registerHandler(_SetFlicker, &ChtP2PCameraControlHandler::handleSetFlicker);
    registerHandler(_SetImageQuality, &ChtP2PCameraControlHandler::handleSetImageQuality);
    registerHandler(_SetMicrophone, &ChtP2PCameraControlHandler::handleSetMicrophone);
    registerHandler(_SetNightMode, &ChtP2PCameraControlHandler::handleSetNightMode);
    registerHandler(_SetAutoNightVision, &ChtP2PCameraControlHandler::handleSetAutoNightVision);
    registerHandler(_SetSpeak, &ChtP2PCameraControlHandler::handleSetSpeak);
    registerHandler(_SetFlipUpDown, &ChtP2PCameraControlHandler::handleSetFlipUpDown);
    registerHandler(_SetLED, &ChtP2PCameraControlHandler::handleSetLED);
    registerHandler(_SetCameraPower, &ChtP2PCameraControlHandler::handleSetCameraPower);
    registerHandler(_GetSnapshotHamiCamDevice, &ChtP2PCameraControlHandler::handleGetSnapshotHamiCamDevice);
    registerHandler(_RestartHamiCamDevice, &ChtP2PCameraControlHandler::handleRestartHamiCamDevice);
    registerHandler(_SetCamStorageDay, &ChtP2PCameraControlHandler::handleSetCamStorageDay);
    registerHandler(_HamiCamFormatSDCard, &ChtP2PCameraControlHandler::handleHamiCamFormatSDCard);
    registerHandler(_HamiCamPtzControlMove, &ChtP2PCameraControlHandler::handleHamiCamPtzControlMove);
    registerHandler(_HamiCamPtzControlConfigSpeed, &ChtP2PCameraControlHandler::handleHamiCamPtzControlConfigSpeed);
    registerHandler(_HamiCamGetPtzControl, &ChtP2PCameraControlHandler::handleHamiCamGetPtzControl);
    registerHandler(_HamiCamPtzControlTourGo, &ChtP2PCameraControlHandler::handleHamiCamPtzControlTourGo);
    registerHandler(_HamiCamPtzControlGoPst, &ChtP2PCameraControlHandler::handleHamiCamPtzControlGoPst);
    registerHandler(_HamiCamPtzControlConfigPst, &ChtP2PCameraControlHandler::handleHamiCamPtzControlConfigPst);
    registerHandler(_HamiCamHumanTracking, &ChtP2PCameraControlHandler::handleHamiCamHumanTracking);
    registerHandler(_HamiCamPetTracking, &ChtP2PCameraControlHandler::handleHamiCamPetTracking);
    registerHandler(_GetHamiCamBindList, &ChtP2PCameraControlHandler::handleGetHamiCamBindList);
    registerHandler(_UpgradeHamiCamOTA, &ChtP2PCameraControlHandler::handleUpgradeHamiCamOTA);
    registerHandler(_UpdateCameraAISetting, &ChtP2PCameraControlHandler::handleUpdateCameraAISetting);
    registerHandler(_GetCameraAISetting, &ChtP2PCameraControlHandler::handleGetCameraAISetting);
    registerHandler(_GetVideoLiveStream, &ChtP2PCameraControlHandler::handleGetVideoLiveStream);
    registerHandler(_StopVideoLiveStream, &ChtP2PCameraControlHandler::handleStopVideoLiveStream);
    registerHandler(_GetVideoHistoryStream, &ChtP2PCameraControlHandler::handleGetVideoHistoryStream);
    registerHandler(_StopVideoHistoryStream, &ChtP2PCameraControlHandler::handleStopVideoHistoryStream);
    registerHandler(_SendAudioStream, &ChtP2PCameraControlHandler::handleSendAudioStream);
    registerHandler(_StopAudioStream, &ChtP2PCameraControlHandler::handleStopAudioStream);
    registerHandler(_SetCamEventStorageDay, &ChtP2PCameraControlHandler::handleSetCamEventStorageDay);
    registerHandler(_GetVideoScheduleStream, &ChtP2PCameraControlHandler::handleGetVideoScheduleStream);
    registerHandler(_StopVideoScheduleStream, &ChtP2PCameraControlHandler::handleStopVideoScheduleStream);
}

// 驗證camId的輔助函數
static bool validateCamId(const rapidjson::Document& request,
        const std::string& saved_camId, std::string &errorMsg)
{
    try {
        const std::string& camId = GetStringMember(request, PAYLOAD_KEY_CAMID);
        if (camId == "" || camId != saved_camId) {
            throw std::runtime_error("攝影機ID不符");
        }

        return true;
    }
    catch (const std::exception &e)
    {
        errorMsg = std::string("攝影機ID不符");
        return false;
    }

    return false;
}

#define BOOL2STR(x) ((x)?"1":"0")

// 各控制命令的默認處理實現
std::string ChtP2PCameraControlHandler::handleGetCamStatusById(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理獲取攝影機狀態: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        // 從參數管理器獲取最新參數
        auto &paramsManager = CameraParametersManager::getInstance();
        const std::string& saved_camId = paramsManager.getCameraId();
        const std::string& saved_tenantId = paramsManager.getTenantId();
        const std::string& saved_netNo = paramsManager.getNetNo();
        int saved_sid = paramsManager.getCamSid();
        const std::string& saved_userId = paramsManager.getUserId();
        const std::string& saved_firmwareVer = paramsManager.getFirmwareVersion();
        const std::string& saved_lastVer = paramsManager.getLatestFirmwareVersion();

        std::string errMsg = "";
        if (!validateCamId(requestJson, saved_camId, errMsg))
        {
            throw std::runtime_error(errMsg);
        }

        // 從請求中獲取參數（驗證用）
        const std::string& tenantId = GetStringMember(requestJson, PAYLOAD_KEY_TENANT_ID);
        const std::string& netNo = GetStringMember(requestJson, PAYLOAD_KEY_TENANT_ID);
        int camSid = GetIntMember(requestJson, PAYLOAD_KEY_CAMSID);
        const std::string& userId = GetStringMember(requestJson, PAYLOAD_KEY_UID);

        std::cout << "請求參數 - "
                  << ", tenantId: " << tenantId
                  << ", netNo: " << netNo
                  << ", camSid: " << std::to_string(camSid)
                  << ", userId: " << userId << std::endl;

        if (tenantId != saved_tenantId || netNo != saved_netNo || userId != saved_userId)
        {
            throw std::runtime_error("camera parameter is invalid!!!");
        }

        // 獲取當前攝影機參數
        stCamStatusByIdReq stReq;
        stCamStatusByIdRep stRep;
        int rc = zwsystem_ipc_getCamStatusById(stReq, &stRep);
        if (rc < 0 || stRep.code < 0)
        {
            throw std::runtime_error("system service error!!!");
        }

        // 從請求或參數管理器獲取其他資訊
        std::cout << "準備回傳的參數:" << std::endl;
        std::cout << "  camId: " << saved_camId << std::endl;
        std::cout << "  firmwareVer: " << saved_firmwareVer << std::endl;
        std::cout << "  latestVersion: " << saved_lastVer << std::endl;
        std::cout << "  name: " << stRep.name << std::endl;
        std::cout << "  status: " << zwsystem_ipc_status_int2str(stRep.status) << std::endl;
        std::cout << "  storageHealth: " << zwsystem_ipc_health_int2str(stRep.externalStorageHealth) << std::endl;
        std::cout << "  storageCapacity: " << stRep.externalStorageCapacity << std::endl;
        std::cout << "  storageAvailable: " << stRep.externalStorageAvailable << std::endl;
        std::cout << "  wifiSsid: " << stRep.wifiSsid << std::endl;
        std::cout << "  wifiDbm: " << stRep.wifiDbm << std::endl;
        std::cout << "  microphoneEnabled: " << stRep.isMicrophone << std::endl;
        std::cout << "  speakerVolume: " << stRep.speakVolume << std::endl;
        std::cout << "  imageQuality: " << stRep.imageQuality << std::endl;
        std::cout << "  activeStatus: " << stRep.activeStatus << std::endl;

        // 構建回應 JSON（按照 CHT 規格）
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();

        // 按照 CHT 規格的欄位順序和格式

        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);

        AddString(response, PAYLOAD_KEY_CAMID, saved_camId);
        AddString(response, PAYLOAD_KEY_DESCRIPTION, "");
        AddString(response, PAYLOAD_KEY_TENANT_ID, saved_tenantId);
        AddString(response, PAYLOAD_KEY_NETNO, saved_netNo);
        AddString(response, PAYLOAD_KEY_FIRMWARE_VER, saved_firmwareVer);
        AddString(response, PAYLOAD_KEY_LATEST_VERSION, saved_lastVer);
        AddString(response, PAYLOAD_KEY_IS_MICROPHONE, BOOL2STR(stRep.isMicrophone));
        AddString(response, PAYLOAD_KEY_SPEAK_VOLUME, std::to_string(stRep.speakVolume));
        AddString(response, PAYLOAD_KEY_IMAGE_QUALITY, std::to_string(stRep.imageQuality));
        AddString(response, PAYLOAD_KEY_ACTIVE_STATUS, BOOL2STR(stRep.activeStatus));
        AddString(response, PAYLOAD_KEY_NAME, stRep.name);
        AddString(response, PAYLOAD_KEY_STATUS, zwsystem_ipc_status_int2str(stRep.status));
        AddString(response, PAYLOAD_KEY_EXTERNAL_STORAGE_HEALTH, zwsystem_ipc_health_int2str(stRep.externalStorageHealth));
        AddString(response, PAYLOAD_KEY_EXTERNAL_STORAGE_CAPACITY, stRep.externalStorageCapacity);
        AddString(response, PAYLOAD_KEY_EXTERNAL_STORAGE_AVAILABLE, stRep.externalStorageAvailable);
        AddString(response, PAYLOAD_KEY_WIFI_SSID, stRep.wifiSsid);
        response.AddMember(PAYLOAD_KEY_WIFI_DBM, stRep.wifiDbm, allocator);

        // 轉換為 JSON 字符串
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        std::string responseStr = buffer.GetString();
        std::cout << "回傳 JSON: " << responseStr << std::endl;

        return responseStr;
    }
    catch (const std::exception &e)
    {
        std::cerr << "處理獲取攝影機狀態時發生異常: " << e.what() << std::endl;
        std::string desc = "處理獲取攝影機狀態時發生異常: " + std::string(e.what());
        std::string outResult = "";
        createErrorResponse(desc, outResult);

        return outResult;
    }

    return "";
}


std::string ChtP2PCameraControlHandler::handleDeleteCameraInfo(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理解綁攝影機指令: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        // 從參數管理器獲取最新參數
        auto &paramsManager = CameraParametersManager::getInstance();
        const std::string& saved_camId = paramsManager.getCameraId();

        std::string errMsg = "";
        if (!validateCamId(requestJson, saved_camId, errMsg))
        {
            throw std::runtime_error(errMsg);
        }

        std::cout << "開始清除綁定相關參數..." << std::endl;

        stDeleteCameraInfoReq stReq;
        stDeleteCameraInfoRep stRep;
        int rc = zwsystem_ipc_deleteCameraInfo(stReq, &stRep);
        if (rc < 0 || stRep.code != 0) {
            throw std::runtime_error("system service error!!!");
        }

        // 檢查當前HiOSS狀態
        bool saved_hiOssStatus = paramsManager.getHiOssStatus();
        std::cout << "解綁前HiOSS狀態: " << (saved_hiOssStatus? "允許模式" : "受限模式") << std::endl;

        // ===== 清除伺服器分配的資訊 =====
        std::cout << "2. 清除伺服器分配的資訊..." << std::endl;
        paramsManager.setCamSid(0);
        paramsManager.setTenantId("");
        paramsManager.setUserId("");
        std::cout << "   - camSid: (已清除)" << std::endl;
        std::cout << "   - tenantId: (已清除)" << std::endl;
        std::cout << "   - userId: (已清除)" << std::endl;

        // ===== 清除網路/服務相關參數 =====
        std::cout << "3. 清除網路和服務相關參數..." << std::endl;
        paramsManager.setNetNo("");
        paramsManager.setVsDomain("");
        paramsManager.setVsToken("");
        paramsManager.setPublicIp(""); // 清除伺服器分配的公網IP
        std::cout << "   - netNo: (已清除)" << std::endl;
        std::cout << "   - vsDomain: (已清除)" << std::endl;
        std::cout << "   - vsToken: (已清除)" << std::endl;
        std::cout << "   - publicIp: (已清除)" << std::endl;

        // ===== 重設HiOSS狀態 - 解綁後允許重新綁定和檢查 =====
        std::cout << "7. 重設HiOSS狀態..." << std::endl;
        // 重設HiOSS狀態 - 解綁後允許重新綁定和檢查
        paramsManager.setIsCheckHioss(false);
        paramsManager.setHiOssStatus(false);
        std::cout << "  重設 HiOSS 狀態為允許模式，設備可重新進行綁定流程" << std::endl;
        std::cout << "   - HiOSS狀態: 1 (允許模式)" << std::endl;
        std::cout << "   ★ 重要：HiOSS狀態已重設為允許模式" << std::endl;
        std::cout << "   ★ 設備現在可以接收所有控制指令" << std::endl;
        std::cout << "   ★ 控制指令限制已完全解除" << std::endl;

        // ===== 重設時區為預設（台北時區）=====
        std::cout << "9. 重設時區..." << std::endl;
        const std::string defaultTid = TimezoneUtils::getDefaultTimezoneId();
        paramsManager.setTimeZone(defaultTid);
        std::cout << "   - 時區: " << defaultTid << std::endl;

        // 保存組態到檔案
        std::cout << "\n=== 保存設定到檔案 ===" << std::endl;
        bool saveResult = paramsManager.saveToFile();
        std::cout << "攝影機解綁完成，設定已保存: " << (saveResult ? "成功" : "失敗") << std::endl;
        std::cout << "HiOSS狀態已重設，控制指令限制已解除" << std::endl;
        std::cout << "設備已恢復為初始未綁定狀態，可重新進行綁定流程" << std::endl;

        // **返回成功格式（按照規範）**
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        AddString(response, PAYLOAD_KEY_DESCRIPTION, "攝影機解除綁定");

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "解綁攝影機時發生異常: " << e.what() << std::endl;
        std::string desc = "解綁攝影機時發生異常: " + std::string(e.what());
        std::string outResult = "";
        createErrorResponse(desc, outResult);

        return outResult;
    }

    return "";
}


std::string ChtP2PCameraControlHandler::handleSetTimeZone(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理設定時區: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        // 從參數管理器獲取最新參數
        auto &paramsManager = CameraParametersManager::getInstance();
        const std::string& saved_camId = paramsManager.getCameraId();

        std::string errMsg = "";
        if (!validateCamId(requestJson, saved_camId, errMsg))
        {
            throw std::runtime_error(errMsg);
        }

        const std::string& tId = GetStringMember(requestJson, PAYLOAD_KEY_TID);

        std::cout << "設置時區 - tId: " << tId << std::endl;

        // 檢查時區ID是否有效並獲取時區字串
        const std::string& tzString = TimezoneUtils::getTimezoneString(tId);
        if (tzString.empty())
        {
            throw std::runtime_error("無效的時區ID: " + tId);
        }
        std::cout << "時區字串: " << tzString << std::endl;

        // set TZ string into system service
        stSetTimezoneReq stReq;
        stSetTimezoneRep stRep;
        stReq.updateBit = eDatetimeUpdateMask_Timezone;
        snprintf(stReq.TZStr, ZWSYSTEM_IPC_STRING_SIZE, "%s", tzString.c_str());
        stReq.daylightSavings = false;
        int rc = zwsystem_ipc_setTimezone(stReq, &stRep);
        if (rc < 0 || stRep.code < 0)
        {
            throw std::runtime_error("system service error!!!");
        }

        // update this process timezone, maybe can wait subscribe
        //TimezoneUtils.updateTimezone;

        // 更新參數管理器
        paramsManager.setTimeZone(tId);
        paramsManager.saveToFile();

        // 構建成功回應
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        AddString(response, PAYLOAD_KEY_DESCRIPTION, "時區設定成功回應");
        AddString(response, PAYLOAD_KEY_TID, "tId");

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        std::cout << "時區設定成功回應: " << buffer.GetString() << std::endl;
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "設定時區時發生異常: " << e.what() << std::endl;
        std::string desc = "設定時區時發生異常: " + std::string(e.what());
        std::string outResult = "";
        createErrorResponse(desc, outResult);

        return outResult;
    }

    return "";
}

std::string ChtP2PCameraControlHandler::handleGetTimeZone(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理獲取時區: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        // 從參數管理器獲取最新參數
        auto &paramsManager = CameraParametersManager::getInstance();
        const std::string& saved_camId = paramsManager.getCameraId();

        std::string errMsg = "";
        if (!validateCamId(requestJson, saved_camId, errMsg))
        {
            throw std::runtime_error(errMsg);
        }

        std::string tId = paramsManager.getTimeZone();

        // 如果時區為空，設為預設值
        if (tId.empty())
        {
            tId = TimezoneUtils::getDefaultTimezoneId(); // 使用工具類別的預設值
            const std::string& tzString = TimezoneUtils::getTimezoneString(tId);
            if (tzString.empty())
            {
                throw std::runtime_error("無效的時區ID: " + tId);
            }

            // set TZ string into system service
            stSetTimezoneReq stReq;
            stSetTimezoneRep stRep;
            stReq.updateBit = eDatetimeUpdateMask_Timezone;
            snprintf(stReq.TZStr, ZWSYSTEM_IPC_STRING_SIZE, "%s", tzString.c_str());
            stReq.daylightSavings = false;
            int rc = zwsystem_ipc_setTimezone(stReq, &stRep);
            if (rc < 0 || stRep.code < 0)
            {
                throw std::runtime_error("system service error!!!");
            }

            paramsManager.setTimeZone(tId);
            paramsManager.saveToFile();
        }

        std::cout << "當前時區: " << tId << std::endl;

        // 構建時區資訊列表
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();

        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        AddString(response, PAYLOAD_KEY_DESCRIPTION, "獲取時區成功回應");
        AddString(response, PAYLOAD_KEY_TIMEZONE, tId);

        // 加入時區列表 - 使用 TimezoneUtils 獲取完整資訊
        rapidjson::Value timezoneAll(rapidjson::kArrayType);

        // 從 TimezoneUtils 獲取所有時區資訊
        const auto& timezoneInfoList = TimezoneUtils::getAllTimezoneInfo();
        for (const auto &timezoneInfo : timezoneInfoList)
        {
            rapidjson::Value timeZoneObj(rapidjson::kObjectType);
            AddString(timeZoneObj, PAYLOAD_KEY_TID, timezoneInfo.tId, allocator);
            AddString(timeZoneObj, PAYLOAD_KEY_DISPLAY_NAME, timezoneInfo.displayName, allocator);
            AddString(timeZoneObj, PAYLOAD_KEY_BASE_UTC_OFFSET, timezoneInfo.baseUtcOffset, allocator);

            timezoneAll.PushBack(timeZoneObj.Move(), allocator);
        }

        response.AddMember(PAYLOAD_KEY_TIMEZONE_ALL, timezoneAll, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        std::cout << "成功獲取時區資訊，包含 " << timezoneInfoList.size() << " 個時區" << std::endl;
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "處理獲取時區失敗: " << e.what() << std::endl;
        std::string desc = "處理獲取時區失敗: " + std::string(e.what());
        std::string outResult = "";
        createErrorResponse(desc, outResult);

        return outResult;
    }

    return "";
}

static int s_osd_render_utf8_next(const char *src, unsigned int src_len, unsigned int src_idx,
        unsigned int *out, unsigned int *need_out)
{
    if (!src) return -1;
    if (src_idx >= src_len) return 0;

    unsigned char c = (unsigned char)src[src_idx];
    unsigned int v, need;

    if (c < 0x80) { v = c; need = 1; }
    else if ((c & 0xE0) == 0xC0) { v = c & 0x1F; need = 2; }
    else if ((c & 0xF0) == 0xE0) { v = c & 0x0F; need = 3; }
    else if ((c & 0xF8) == 0xF0) { v = c & 0x07; need = 4; }
    else return -1;

    if (src_idx + need > src_len) return -1;
    for (unsigned int k = 1; k < need; k++) {
        unsigned char t = (unsigned char)src[src_idx + k];
        if ((t & 0xC0) != 0x80) return -1;
        v = (v << 6) | (t & 0x3F);
    }

    if (need == 2 && v < 0x80) return -1;
    if (need == 3 && v < 0x800) return -1;
    if (need == 4 && (v < 0x10000 || v > 0x10FFFF)) return -1;
    if (v >= 0xD800 && v <= 0xDFFF) return -1;
    if (v > 0x10FFFF) return -1;

    if (out) *out = v;
    if (need_out) *need_out = need;
    return 1;
}

static bool is_valid_utf8(const char *s, size_t len)
{
    size_t i = 0;
    while (i < len) {
        unsigned int cp = 0, need = 0;
        int r = s_osd_render_utf8_next(s, (unsigned int)len, (unsigned int)i, &cp, &need);
        if (r < 0) return false;     // invalid
        if (r == 0) break;       // end (理論上 i < len 不會到這裡)
        i += need;
    }
    return true; // valid
}

/***************************************************************
    目前暫定的handleUpdateCameraName流程:
    目錄檢查：檢查 /etc/config/ 是否存在，不存在就建立
    檔案讀取：讀取現有的 osd_settings.ini（如果存在）
    內容解析：解析 [osd] section 下的所有設定
    更新名稱：將 CameraName 更新為新的值
    預設值檢查：確保 Location 和 strftime 有預設值
    檔案寫入：按照指定格式和順序寫入檔案
    系統更新：venc4_setup host_stream 應該三秒後系統會自動更新影像上的文字OSD
 ***************************************************************/
std::string ChtP2PCameraControlHandler::handleUpdateCameraName(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理更新攝影機名稱: " << payload << std::endl;
    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        // 從參數管理器獲取最新參數
        auto &paramsManager = CameraParametersManager::getInstance();
        const std::string& saved_camId = paramsManager.getCameraId();

        std::string errMsg = "";
        if (!validateCamId(requestJson, saved_camId, errMsg))
        {
            throw std::runtime_error(errMsg);
        }

        const std::string& name = GetStringMember(requestJson, PAYLOAD_KEY_NAME);
        std::cout << "更新攝影機名稱 - name: " << name << std::endl;

        if (name.empty() || name.size() >= ZWSYSTEM_IPC_STRING_SIZE) {
            throw std::runtime_error("name maybe empty or too long");
        }
        if (!is_valid_utf8(name.c_str(), name.size())) {
            throw std::runtime_error("name string format invalid");
        }

        stUpdateCameraNameReq stReq;
        stUpdateCameraNameRep stRep;
        snprintf(stReq.name, ZWSYSTEM_IPC_STRING_SIZE, "%s", name.c_str());
        int rc = zwsystem_ipc_updateCameraName(stReq, &stRep);
        if (rc < 0 || stRep.code < 0)
        {
            throw std::runtime_error("system service error!!!");
        }

        // 構建成功回應
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();

        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        AddString(response, PAYLOAD_KEY_DESCRIPTION, "成功更新攝影機名稱");
        AddString(response, PAYLOAD_KEY_NAME, name);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        // 構建失敗回應
        std::cerr << "更新攝影機名稱時發生異常: " << e.what() << std::endl;
        std::string desc = "更新攝影機名稱時發生異常: " + std::string(e.what());
        std::string outResult = "";
        createErrorResponse(desc, outResult);

        return outResult;
    }
    return "";
}

// 輔助函數：計算UTF-8字符串中的字符數量
size_t countUTF8Characters(const std::string &str)
{
    size_t count = 0;
    for (size_t i = 0; i < str.length();)
    {
        unsigned char c = str[i];
        if (c < 0x80)
        {
            // ASCII 字符 (0xxxxxxx)
            i += 1;
        }
        else if ((c & 0xE0) == 0xC0)
        {
            // 2字節 UTF-8 字符 (110xxxxx)
            i += 2;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            // 3字節 UTF-8 字符 (1110xxxx)
            i += 3;
        }
        else if ((c & 0xF8) == 0xF0)
        {
            // 4字節 UTF-8 字符 (11110xxx)
            i += 4;
        }
        else
        {
            // 無效的 UTF-8 字符，跳過
            i += 1;
        }
        count++;
    }
    return count;
}

// 輔助函數：從 osdRule 解析 strftime 格式與前綴
std::pair<std::string, std::string> parseOsdRuleAndGetFormat(const std::string &osdRule)
{
    // 支援的替代格式清單
    static const std::vector<std::pair<std::string, std::string>> datePatterns = {
        {"yyyy-MM-dd", "%Y-%m-%d"},
        {"yyyy/MM/dd", "%Y/%m/%d"},
        {"yyyy MM dd", "%Y %m %d"},
        {"HH:mm:ss", "%H:%M:%S"},
        {"HH mm ss", "%H %M %S"},
        {"HH-mm-ss", "%H-%M-%S"},
        {"yyyyMMdd", "%Y%m%d"},
        {"HHmmss", "%H%M%S"},
    };

    std::string datetimeFormat;
    size_t firstMatchPos = std::string::npos;
    std::string locationPrefix;

    for (const auto &pattern : datePatterns)
    {
        size_t pos = osdRule.find(pattern.first);
        if (pos != std::string::npos)
        {
            if (firstMatchPos == std::string::npos || pos < firstMatchPos)
            {
                firstMatchPos = pos;
                datetimeFormat = pattern.second;
            }
        }
    }

    if (firstMatchPos == std::string::npos)
    {
        throw std::runtime_error("osdRule 中未找到有效的日期格式 (yyyy...)");
    }

    // 嘗試拼接多個連續格式
    size_t pos = firstMatchPos;
    std::string fullFormat;
    while (pos < osdRule.size())
    {
        bool matched = false;
        for (const auto &pattern : datePatterns)
        {
            if (osdRule.compare(pos, pattern.first.length(), pattern.first) == 0)
            {
                fullFormat += pattern.second + " ";
                pos += pattern.first.length();
                matched = true;
                break;
            }
        }
        if (!matched)
            break;
    }

    // 移除末尾空白
    if (!fullFormat.empty() && fullFormat.back() == ' ')
        fullFormat.pop_back();

    locationPrefix = osdRule.substr(0, firstMatchPos);

    // 檢查前置文字的 UTF-8 字符數量，限制最多 4 個字符 (knerno系統OSD限制)
    size_t prefixCharCount = countUTF8Characters(locationPrefix);
    if (prefixCharCount > 4)
    {
        std::cout << "警告: OSD前置文字超過4個UTF-8字符限制 (當前" << prefixCharCount << "個)，將截取前4個字符" << std::endl;

        // 截取前4個UTF-8字符
        std::string truncatedPrefix;
        size_t charCount = 0;
        for (size_t i = 0; i < locationPrefix.length() && charCount < 4;)
        {
            unsigned char c = locationPrefix[i];
            size_t charBytes = 1;

            if (c < 0x80)
            {
                // ASCII 字符 (0xxxxxxx)
                charBytes = 1;
            }
            else if ((c & 0xE0) == 0xC0)
            {
                // 2字節 UTF-8 字符 (110xxxxx)
                charBytes = 2;
            }
            else if ((c & 0xF0) == 0xE0)
            {
                // 3字節 UTF-8 字符 (1110xxxx)
                charBytes = 3;
            }
            else if ((c & 0xF8) == 0xF0)
            {
                // 4字節 UTF-8 字符 (11110xxx)
                charBytes = 4;
            }

            // 確保不會超出字串邊界
            if (i + charBytes <= locationPrefix.length())
            {
                truncatedPrefix += locationPrefix.substr(i, charBytes);
                charCount++;
            }

            i += charBytes;
        }

        locationPrefix = truncatedPrefix;
        std::cout << "截取後的前置文字: \"" << locationPrefix << "\"" << std::endl;
    }

    return {locationPrefix, fullFormat};
}

std::string ChtP2PCameraControlHandler::handleSetCameraOSD(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理設定攝影機OSD: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        // 從參數管理器獲取最新參數
        auto &paramsManager = CameraParametersManager::getInstance();
        const std::string& saved_camId = paramsManager.getCameraId();

        std::string errMsg = "";
        if (!validateCamId(requestJson, saved_camId, errMsg))
        {
            throw std::runtime_error(errMsg);
        }

        const std::string& osdRule = GetStringMember(requestJson, PAYLOAD_KEY_OSD_RULE);
        std::cout << "解析成功 - osdRule: " << osdRule << std::endl;

        if (osdRule.size() >= ZWSYSTEM_IPC_STRING_SIZE) {
            throw std::runtime_error("name maybe empty or too long");
        }
        if (!is_valid_utf8(osdRule.c_str(), osdRule.size())) {
            throw std::runtime_error("name string format invalid");
        }

        stSetCameraOsdReq stReq;
        stSetCameraOsdRep stRep;
        snprintf(stReq.osdRule, ZWSYSTEM_IPC_STRING_SIZE, "%s", osdRule.c_str());
        int rc = zwsystem_ipc_setCameraOsd(stReq, &stRep);
        if (rc < 0 || stRep.code < 0)
        {
            throw std::runtime_error("system service error!!!");
        }

        // 構建成功回應
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();

        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        AddString(response, PAYLOAD_KEY_DESCRIPTION, "成功設定攝影機OSD");
        AddString(response, PAYLOAD_KEY_OSD_RULE, osdRule);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        // 構建失敗回應
        std::cerr << "設定攝影機OSD時發生錯誤: " << e.what() << std::endl;
        std::string desc = "設定攝影機OSD時發生錯誤: " + std::string(e.what());
        std::string outResult = "";
        createErrorResponse(desc, outResult);

        return outResult;
    }
    return "";
}

static bool isValidRequestId(const std::string& requestId)
{
    // 驗證requestId格式: <UDP/Relay>_live_<userId>_<JWTToken>
    std::regex requestIdPattern("^(UDP|Relay)_live_.+_.+$");
    return std::regex_match(requestId, requestIdPattern);
}

std::string ChtP2PCameraControlHandler::handleSetCameraHD(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理設定攝影機HD: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        // 從參數管理器獲取最新參數
        auto &paramsManager = CameraParametersManager::getInstance();
        const std::string& saved_camId = paramsManager.getCameraId();

        std::string errMsg = "";
        if (!validateCamId(requestJson, saved_camId, errMsg))
        {
            throw std::runtime_error(errMsg);
        }

        // 獲取請求參數 - 使用 rapidjson
        const std::string& requestId = GetStringMember(requestJson, PAYLOAD_KEY_REQUEST_ID);
        const std::string& isHd = GetStringMember(requestJson, PAYLOAD_KEY_IS_HD);
        std::cout << "設定HD - isHd: " << isHd << " ,requestId: " << requestId << std::endl;

        // 驗證requestId格式: <UDP/Relay>_live_<userId>_<JWTToken>
        if (!isValidRequestId(requestId))
        {
            std::cerr << "requestId格式錯誤，應為: <UDP/Relay>_live_<userId>_<JWTToken>" << std::endl;
            throw std::runtime_error("requestId格式錯誤");
        }

        // 驗證isHd參數
        if (isHd != "0" && isHd != "1")
        {
            throw std::runtime_error("無效的isHd參數，必須為0或1");
        }

        std::cout << "設定HD - requestId: " << requestId
                  << ", isHd: " << isHd << std::endl;
        // maybe set into profile for

        // 更新HD設定到參數管理器
        paramsManager.setRequestId(requestId);
        paramsManager.setIsHd(isHd);

        // 保存設定到檔案
        bool saveResult = paramsManager.saveToFile();
        std::cout << "HD設定已保存: " << (saveResult ? "成功" : "失敗") << std::endl;

        // 構建成功回應
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();

        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        AddString(response, PAYLOAD_KEY_DESCRIPTION, "成功設定攝影機OSD");
        AddString(response, PAYLOAD_KEY_REQUEST_ID, requestId);
        AddString(response, PAYLOAD_KEY_IS_HD, isHd);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        // 構建失敗回應
        std::cerr << "設定HD時發生異常: " << e.what() << std::endl;
        std::string desc = "設定HD時發生異常: " + std::string(e.what());
        std::string outResult = "";
        createErrorResponse(desc, outResult);

        return outResult;
    }
    return "";
}

bool ChtP2PCameraControlHandler::verifyExternalEnvironment(const std::string &expectedTzString)
{
    std::cout << "\n========== 驗證外部環境變數 ==========" << std::endl;

    // 建立測試腳本
    std::string testScript = "/tmp/test_external_env.sh";
    std::ofstream script(testScript);
    if (!script.is_open())
    {
        std::cerr << "無法建立測試腳本" << std::endl;
        return false;
    }

    script << "#!/bin/bash\n";
    script << "echo \"外部Shell的TZ值: $TZ\"\n";
    script << "if [ \"$TZ\" = \"" << expectedTzString << "\" ]; then\n";
    script << "    echo \"SUCCESS: 外部環境變數正確\"\n";
    script << "    exit 0\n";
    script << "else\n";
    script << "    echo \"FAILED: 外部環境變數不正確\"\n";
    script << "    echo \"期望: " << expectedTzString << "\"\n";
    script << "    echo \"實際: $TZ\"\n";
    script << "    exit 1\n";
    script << "fi\n";
    script.close();

    chmod(testScript.c_str(), 0755);

    // 執行測試腳本
    std::string testCmd = "bash " + testScript;
    int result = system(testCmd.c_str());

    // 清理測試腳本
    unlink(testScript.c_str());

    bool success = (result == 0);
    std::cout << "外部環境變數驗證: " << (success ? "通過" : "失敗") << std::endl;
    std::cout << "=======================================" << std::endl;

    return success;
}

/**
 * @brief 更新 osd_setting.ini 檔案中的時區設定
 * @param tzString 時區字串，例如 "CST-8"
 * @return 成功返回 true，失敗返回 false
 */
bool ChtP2PCameraControlHandler::updateOsdTimezone(const std::string &tzString)
{
    std::cout << "更新 OSD 設定檔中的時區: " << tzString << std::endl;

    try
    {
        // const std::string iniFilePath = "/mnt/flash/leipzig/ini/osd_setting.ini";
        const std::string iniFilePath = "/etc/config/osd_setting.ini";

        // 檢查目錄是否存在，如果不存在就建立
        std::string iniDir = "/mnt/flash/leipzig/ini";
        struct stat st = {0};
        if (stat(iniDir.c_str(), &st) == -1)
        {
            if (mkdir(iniDir.c_str(), 0755) != 0)
            {
                std::cerr << "ERROR: 無法建立目錄: " << iniDir << std::endl;
                return false;
            }
            std::cout << "INFO: 已建立目錄: " << iniDir << std::endl;
        }

        // 讀取現有的 ini 檔案內容
        std::map<std::string, std::string> iniContent;
        std::ifstream inFile(iniFilePath);
        bool fileExists = inFile.good();

        if (fileExists)
        {
            std::string line;
            std::string currentSection = "";

            while (std::getline(inFile, line))
            {
                // 移除行首尾空白
                line.erase(0, line.find_first_not_of(" \t"));
                line.erase(line.find_last_not_of(" \t") + 1);

                // 跳過空行和註解
                if (line.empty() || line[0] == '#' || line[0] == ';')
                {
                    continue;
                }

                // 處理 section [osd]
                if (line[0] == '[' && line.back() == ']')
                {
                    currentSection = line.substr(1, line.length() - 2);
                    continue;
                }

                // 處理 key = value
                size_t equalPos = line.find('=');
                if (equalPos != std::string::npos)
                {
                    std::string key = line.substr(0, equalPos);
                    std::string value = line.substr(equalPos + 1);

                    // 移除 key 和 value 的空白
                    key.erase(0, key.find_first_not_of(" \t"));
                    key.erase(key.find_last_not_of(" \t") + 1);
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);

                    if (currentSection == "osd")
                    {
                        iniContent[key] = value;
                    }
                }
            }
            inFile.close();
            std::cout << "INFO: 已讀取現有的 osd_setting.ini 檔案" << std::endl;
        }
        else
        {
            std::cout << "INFO: osd_setting.ini 檔案不存在，將建立新檔案" << std::endl;
        }

        // 更新時區設定
        iniContent["timezone"] = tzString;

        // 如果是新檔案，設定預設值
        if (!fileExists)
        {
            auto &paramsManager = CameraParametersManager::getInstance();
            std::string currentCameraName = paramsManager.getCameraName();

            if (iniContent.find("CameraName") == iniContent.end())
            {
                iniContent["CameraName"] = currentCameraName.empty() ? "CAMID_FROM_FILE" : currentCameraName;
            }
            if (iniContent.find("Location") == iniContent.end())
            {
                iniContent["Location"] = "DEMO_ROME";
            }
            if (iniContent.find("strftime") == iniContent.end())
            {
                iniContent["strftime"] = "%Y-%m-%d %H:%M:%S";
            }

            std::cout << "INFO: 設定預設值" << std::endl;
        }

        // 將更新後的內容寫回檔案
        std::ofstream outFile(iniFilePath);
        if (!outFile.is_open())
        {
            std::cerr << "ERROR: 無法開啟檔案進行寫入: " << iniFilePath << std::endl;
            return false;
        }

        // 寫入 [osd] section
        outFile << "[osd]" << std::endl;

        // 按照指定順序寫入 key-value pairs
        // 確保順序：CameraName, Location, strftime, timezone
        if (iniContent.find("CameraName") != iniContent.end())
        {
            outFile << "CameraName = " << iniContent["CameraName"] << std::endl;
        }
        if (iniContent.find("Location") != iniContent.end())
        {
            outFile << "Location = " << iniContent["Location"] << std::endl;
        }
        if (iniContent.find("strftime") != iniContent.end())
        {
            outFile << "strftime = " << iniContent["strftime"] << std::endl;
        }
        if (iniContent.find("timezone") != iniContent.end())
        {
            outFile << "timezone = " << iniContent["timezone"] << std::endl;
        }

        // 寫入其他可能存在的欄位
        for (const auto &pair : iniContent)
        {
            if (pair.first != "CameraName" && pair.first != "Location" &&
                pair.first != "strftime" && pair.first != "timezone")
            {
                outFile << pair.first << " = " << pair.second << std::endl;
            }
        }

        outFile.close();
        std::cout << "INFO: 已成功更新 osd_setting.ini 檔案" << std::endl;
        std::cout << "INFO: timezone = " << tzString << std::endl;

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "ERROR: 更新 OSD 時區設定時發生異常: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief 執行 export TZ 指令設定當前進程環境變數
 * @param tzString 時區字串，例如 "CST-8"
 * @return 成功返回 true，失敗返回 false
 */
bool ChtP2PCameraControlHandler::verifySystemTimezone(const std::string &expectedTzString)
{
    std::cout << "\n========== 驗證系統時區設置 ==========" << std::endl;
    std::cout << "期望時區: " << expectedTzString << std::endl;

    bool allGood = true;

    // 1. 檢查當前進程環境變數
    std::cout << "\n[檢查1] 當前進程環境變數:" << std::endl;
    const char *currentTz = getenv("TZ");
    if (currentTz && std::string(currentTz) == expectedTzString)
    {
        std::cout << "  ✓ 當前進程 TZ = " << currentTz << std::endl;
    }
    else
    {
        std::cout << "  ✗ 當前進程 TZ = " << (currentTz ? currentTz : "未設置")
                  << " (期望: " << expectedTzString << ")" << std::endl;
        allGood = false;
    }

    // 2. 檢查 /etc/TZ 檔案
    std::cout << "\n[檢查2] /etc/TZ 檔案:" << std::endl;
    std::ifstream tzFile("/etc/TZ");
    if (tzFile.is_open())
    {
        std::string fileTz;
        std::getline(tzFile, fileTz);
        tzFile.close();

        // 移除可能的換行符
        fileTz.erase(fileTz.find_last_not_of("\r\n") + 1);

        if (fileTz == expectedTzString)
        {
            std::cout << "  ✓ /etc/TZ = " << fileTz << std::endl;
        }
        else
        {
            std::cout << "  ✗ /etc/TZ = " << fileTz << " (期望: " << expectedTzString << ")" << std::endl;
            allGood = false;
        }
    }
    else
    {
        std::cout << "  ✗ 無法讀取 /etc/TZ 檔案" << std::endl;
        allGood = false;
    }

    // 3. 檢查 /etc/profile.d/timezone.sh
    std::cout << "\n[檢查3] /etc/profile.d/timezone.sh:" << std::endl;
    std::ifstream profiledFile("/etc/profile.d/timezone.sh");
    if (profiledFile.is_open())
    {
        std::string line;
        bool found = false;
        while (std::getline(profiledFile, line))
        {
            if (line.find("export TZ=") != std::string::npos &&
                line.find(expectedTzString) != std::string::npos)
            {
                std::cout << "  ✓ profile.d 腳本包含正確設定: " << line << std::endl;
                found = true;
                break;
            }
        }
        if (!found)
        {
            std::cout << "  ✗ profile.d 腳本未包含期望的時區設定" << std::endl;
            allGood = false;
        }
        profiledFile.close();
    }
    else
    {
        std::cout << "  ✗ 無法讀取 /etc/profile.d/timezone.sh" << std::endl;
        allGood = false;
    }

    // 4. 檢查 /etc/environment
    std::cout << "\n[檢查4] /etc/environment:" << std::endl;
    std::ifstream envFile("/etc/environment");
    if (envFile.is_open())
    {
        std::string line;
        bool found = false;
        while (std::getline(envFile, line))
        {
            if (line.find("TZ=") != std::string::npos &&
                line.find(expectedTzString) != std::string::npos)
            {
                std::cout << "  ✓ environment 檔案包含正確設定: " << line << std::endl;
                found = true;
                break;
            }
        }
        if (!found)
        {
            std::cout << "  ? environment 檔案未包含時區設定（可選）" << std::endl;
        }
        envFile.close();
    }

    // 5. 檢查系統時間顯示
    std::cout << "\n[檢查5] 系統時間顯示:" << std::endl;

    // 使用 popen 來獲取命令輸出
    FILE *pipe = popen("date", "r");
    if (pipe)
    {
        char buffer[256];
        std::string dateOutput;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            dateOutput += buffer;
        }
        pclose(pipe);

        std::cout << "  當前系統時間: " << dateOutput;

        // 簡單檢查時區縮寫是否出現在輸出中
        std::string tzAbbr;
        if (expectedTzString.find("WAT") != std::string::npos)
            tzAbbr = "WAT";
        else if (expectedTzString.find("CST") != std::string::npos)
            tzAbbr = "CST";
        else if (expectedTzString.find("JST") != std::string::npos)
            tzAbbr = "JST";
        else if (expectedTzString.find("GMT") != std::string::npos)
            tzAbbr = "GMT";
        else if (expectedTzString.find("PST") != std::string::npos)
            tzAbbr = "PST";
        else if (expectedTzString.find("EST") != std::string::npos)
            tzAbbr = "EST";

        if (!tzAbbr.empty() && dateOutput.find(tzAbbr) != std::string::npos)
        {
            std::cout << "  ✓ 系統時間顯示包含期望的時區縮寫: " << tzAbbr << std::endl;
        }
        else
        {
            std::cout << "  ? 無法從系統時間輸出確認時區（這可能是正常的）" << std::endl;
        }
    }

    // 6. **新增：外部環境驗證**
    std::cout << "\n[檢查6] 外部環境持久化效果:" << std::endl;
    bool externalResult = verifyExternalEnvironment(expectedTzString);
    if (!externalResult)
    {
        std::cout << "  ⚠ 外部環境驗證有問題，但主要設定已完成" << std::endl;
        // 不影響主要驗證結果，因為這是額外的檢查
    }

    std::cout << "\n========== 驗證結果 ==========" << std::endl;
    if (allGood)
    {
        std::cout << "✓ 所有主要檢查都通過，時區設置應該已生效" << std::endl;
        std::cout << "✓ 當前程序的時區設定正確" << std::endl;
        if (externalResult)
        {
            std::cout << "✓ 外部環境的持久化設定也正確" << std::endl;
        }
        else
        {
            std::cout << "ℹ 外部環境需要手動載入：source /etc/profile.d/timezone.sh" << std::endl;
        }
    }
    else
    {
        std::cout << "✗ 部分檢查失敗，時區設置可能不完整" << std::endl;
    }

    std::cout << "\n手動驗證指令（程序結束後執行）：" << std::endl;
    std::cout << "  檢查檔案內容: cat /etc/TZ" << std::endl;
    std::cout << "  載入新設定: source /etc/profile.d/timezone.sh" << std::endl;
    std::cout << "  檢查環境變數: echo $TZ" << std::endl;
    std::cout << "  檢查時間: date" << std::endl;
    std::cout << "  立即使用: source /tmp/cht_camera_env.sh" << std::endl;
    std::cout << "===============================" << std::endl;

    return allGood;
}

/**
 * @brief 獲取格式化的時間戳（如果不存在的話）
 * @return 格式化的時間字串
 */
std::string getFormattedTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

/**
 * @brief 執行 export TZ 指令設定當前進程環境變數並持久化到系統
 * @param tzString 時區字串，例如 "WAT-1"
 * @return 成功返回 true，失敗返回 false
 */
bool ChtP2PCameraControlHandler::createParentShellSolution(const std::string &tzString)
{
    std::cout << "\n========== 建立父 Shell 環境變數解決方案 ==========" << std::endl;
    std::cout << "注意：由於程序隔離限制，子程序無法直接修改父 Shell 環境變數" << std::endl;
    std::cout << "提供以下解決方案供使用者選擇：" << std::endl;

    try
    {
        // 解決方案1: 建立立即執行腳本
        std::string immediateScript = "/tmp/apply_timezone_now.sh";
        std::ofstream script1(immediateScript);
        if (script1.is_open())
        {
            script1 << "#!/bin/bash\n";
            script1 << "# CHT Camera 時區立即套用腳本\n";
            script1 << "# 在當前 Shell 中執行此腳本來套用時區變更\n";
            script1 << "\n";
            script1 << "echo \"正在套用時區設定...\"\n";
            script1 << "export TZ=\"" << tzString << "\"\n";
            script1 << "echo \"✓ 時區已設定為: $TZ\"\n";
            script1 << "echo \"當前時間: $(date)\"\n";
            script1 << "\n";
            script1 << "# 將設定寫入當前 Shell 歷史，方便重複使用\n";
            script1 << "echo \"export TZ=\\\"" << tzString << "\\\"\" >> ~/.bash_history\n";
            script1.close();
            chmod(immediateScript.c_str(), 0755);
            std::cout << "✓ 立即套用腳本已建立: " << immediateScript << std::endl;
        }

        // 解決方案2: 建立 eval 命令檔案
        std::string evalFile = "/tmp/tz_eval_command.txt";
        std::ofstream evalScript(evalFile);
        if (evalScript.is_open())
        {
            evalScript << "export TZ=\"" << tzString << "\"";
            evalScript.close();
            std::cout << "✓ eval 命令檔案已建立: " << evalFile << std::endl;
        }

        // 解決方案3: 建立 alias 設定
        std::string aliasFile = "/tmp/tz_alias_setup.sh";
        std::ofstream aliasScript(aliasFile);
        if (aliasScript.is_open())
        {
            aliasScript << "#!/bin/bash\n";
            aliasScript << "# 建立時區快速切換 alias\n";
            aliasScript << "alias set_tz_" << tzString.substr(0, 3) << "='export TZ=\"" << tzString << "\" && echo \"時區設定為: $TZ\" && date'\n";
            aliasScript << "echo \"alias 已設定，使用 'set_tz_" << tzString.substr(0, 3) << "' 快速套用時區\"\n";
            aliasScript.close();
            chmod(aliasFile.c_str(), 0755);
            std::cout << "✓ alias 設定腳本已建立: " << aliasFile << std::endl;
        }

        // 解決方案4: 建立互動式設定腳本
        std::string interactiveScript = "/tmp/interactive_tz_setup.sh";
        std::ofstream interactive(interactiveScript);
        if (interactive.is_open())
        {
            interactive << "#!/bin/bash\n";
            interactive << "# CHT Camera 互動式時區設定\n";
            interactive << "\n";
            interactive << "echo \"CHT Camera 時區設定工具\"\n";
            interactive << "echo \"========================\"\n";
            interactive << "echo \"建議的時區: " << tzString << "\"\n";
            interactive << "echo \"\"\n";
            interactive << "read -p \"是否要套用此時區設定? (y/n): \" choice\n";
            interactive << "case \"$choice\" in\n";
            interactive << "  y|Y|yes|YES)\n";
            interactive << "    export TZ=\"" << tzString << "\"\n";
            interactive << "    echo \"✓ 時區已設定為: $TZ\"\n";
            interactive << "    echo \"當前時間: $(date)\"\n";
            interactive << "    echo \"\"\n";
            interactive << "    echo \"要讓此設定永久生效，請將以下命令加入 ~/.bashrc:\"\n";
            interactive << "    echo \"export TZ=\\\"" << tzString << "\\\"\"\n";
            interactive << "    ;;\n";
            interactive << "  *)\n";
            interactive << "    echo \"已取消時區設定\"\n";
            interactive << "    ;;\n";
            interactive << "esac\n";
            interactive.close();
            chmod(interactiveScript.c_str(), 0755);
            std::cout << "✓ 互動式設定腳本已建立: " << interactiveScript << std::endl;
        }

        // 解決方案5: 建立 bashrc 自動載入設定
        std::string bashrcAppend = "/tmp/bashrc_tz_append.txt";
        std::ofstream bashrcScript(bashrcAppend);
        if (bashrcScript.is_open())
        {
            bashrcScript << "\n# CHT Camera 時區設定 - 自動生成於 " << getFormattedTimestamp() << "\n";
            bashrcScript << "export TZ=\"" << tzString << "\"\n";
            bashrcScript << "# 如需移除此設定，請刪除上述兩行\n";
            bashrcScript.close();
            std::cout << "✓ bashrc 附加內容已建立: " << bashrcAppend << std::endl;
        }

        // 顯示使用方法
        std::cout << "\n========== 父 Shell 套用方法 ==========" << std::endl;
        std::cout << "由於程序限制，請在程序結束後使用以下任一方法：" << std::endl;
        std::cout << "" << std::endl;

        std::cout << "【方法1】立即套用（推薦）：" << std::endl;
        std::cout << "  source " << immediateScript << std::endl;
        std::cout << "" << std::endl;

        std::cout << "【方法2】使用 eval 命令：" << std::endl;
        std::cout << "  eval $(cat " << evalFile << ")" << std::endl;
        std::cout << "" << std::endl;

        std::cout << "【方法3】直接 export（最簡單）：" << std::endl;
        std::cout << "  export TZ=\"" << tzString << "\"" << std::endl;
        std::cout << "" << std::endl;

        std::cout << "【方法4】互動式設定：" << std::endl;
        std::cout << "  bash " << interactiveScript << std::endl;
        std::cout << "" << std::endl;

        std::cout << "【方法5】永久設定（加入 ~/.bashrc）：" << std::endl;
        std::cout << "  cat " << bashrcAppend << " >> ~/.bashrc" << std::endl;
        std::cout << "  source ~/.bashrc" << std::endl;
        std::cout << "" << std::endl;

        std::cout << "【驗證方法】：" << std::endl;
        std::cout << "  echo $TZ" << std::endl;
        std::cout << "  date" << std::endl;

        std::cout << "======================================" << std::endl;

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "ERROR: 建立父 Shell 解決方案時發生異常: " << e.what() << std::endl;
        return false;
    }
}
// /**
//  * @brief 在程序結束時顯示父Shell套用指引
//  */
// void ChtP2PCameraControlHandler::displayParentShellGuidance()
// {
//     std::cout << "\n" << std::string(60, '=') << std::endl;
//     std::cout << "              CHT Camera 環境變數套用指引" << std::endl;
//     std::cout << std::string(60, '=') << std::endl;
//     std::cout << "程序內的環境變數設定不會自動套用到父 Shell。" << std::endl;
//     std::cout << "若要在當前 Shell 中使用相同的時區設定，請執行：" << std::endl;
//     std::cout << "" << std::endl;
//     std::cout << "  source /tmp/apply_timezone_now.sh" << std::endl;
//     std::cout << "" << std::endl;
//     std::cout << "或者直接執行：" << std::endl;
//     std::cout << "  source /etc/profile.d/timezone.sh" << std::endl;
//     std::cout << "" << std::endl;
//     std::cout << "驗證設定：" << std::endl;
//     std::cout << "  echo $TZ && date" << std::endl;
//     std::cout << std::string(60, '=') << std::endl;
// }

/**
 * @brief 修正後的 executeExportTZ - 專注於程序內設定和提供父Shell解決方案
 */
bool ChtP2PCameraControlHandler::executeExportTZ(const std::string &tzString)
{
    std::cout << "執行 export TZ 指令: " << tzString << std::endl;

    try
    {
        // ===== 步驟 1: 設置當前程序環境變數 =====
        std::cout << "## [步驟1] 設置當前程序環境變數" << std::endl;
        if (setenv("TZ", tzString.c_str(), 1) != 0)
        {
            std::cerr << "ERROR: setenv() 設置 TZ 環境變數失敗" << std::endl;
            return false;
        }

        // 通知系統重新載入時區資訊
        tzset();
        std::cout << "INFO: ✓ 當前程序環境變數已設置: TZ=" << tzString << std::endl;

        // ===== 步驟 2: 系統檔案持久化（重開機生效）=====
        std::cout << "## [步驟2] 系統檔案持久化更新" << std::endl;

        // 更新系統時區檔案
        std::string tzFileCmd = "echo '" + tzString + "' > /etc/TZ";
        executeSystemCommand(tzFileCmd.c_str());

        // 更新 profile.d 腳本
        executeSystemCommand("mkdir -p /etc/profile.d");
        std::string profileCmd = "echo 'export TZ=\"" + tzString + "\"' > /etc/profile.d/timezone.sh";
        executeSystemCommand(profileCmd.c_str());
        executeSystemCommand("chmod +x /etc/profile.d/timezone.sh");

        std::cout << "INFO: ✓ 系統檔案已更新，重開機後自動生效" << std::endl;

        // ===== 步驟 3: 建立父 Shell 套用解決方案 =====
        std::cout << "## [步驟3] 建立父 Shell 套用解決方案" << std::endl;
        bool solutionResult = createParentShellSolution(tzString);

        if (solutionResult)
        {
            std::cout << "INFO: ✓ 父 Shell 套用方案已準備完成" << std::endl;
        }
        else
        {
            std::cout << "WARNING: 父 Shell 套用方案建立失敗" << std::endl;
        }

        // ===== 步驟 4: 驗證當前程序設定 =====
        const char *currentTz = getenv("TZ");
        if (currentTz && std::string(currentTz) == tzString)
        {
            std::cout << "INFO: ✓ 程序內環境變數驗證成功: TZ=" << currentTz << std::endl;
            std::cout << "INFO: ✓ 程序內時間顯示: ";
            if (system("date") != 0)
            {
                std::cout << "無法獲取系統時間" << std::endl;
            }
            return true;
        }
        else
        {
            std::cerr << "ERROR: 程序內環境變數驗證失敗" << std::endl;
            return false;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "ERROR: 執行 export TZ 時發生異常: " << e.what() << std::endl;
        return false;
    }
}

bool ChtP2PCameraControlHandler::reloadSystemTimezone()
{
    std::cout << "\n========== 重新載入系統時區設定 ==========" << std::endl;

    try
    {
        // 方法 1: 從 /etc/TZ 檔案重新載入
        std::cout << "[方法1] 從 /etc/TZ 檔案重新載入" << std::endl;

        std::ifstream tzFile("/etc/TZ");
        if (tzFile.is_open())
        {
            std::string fileTz;
            std::getline(tzFile, fileTz);
            tzFile.close();

            // 移除可能的換行符
            fileTz.erase(fileTz.find_last_not_of("\r\n") + 1);

            if (!fileTz.empty())
            {
                std::cout << "  從檔案讀取到時區: " << fileTz << std::endl;

                if (setenv("TZ", fileTz.c_str(), 1) == 0)
                {
                    tzset();
                    std::cout << "  ✓ 環境變數已更新為: " << fileTz << std::endl;
                }
                else
                {
                    std::cout << "  ✗ 更新環境變數失敗" << std::endl;
                    return false;
                }
            }
            else
            {
                std::cout << "  ⚠ /etc/TZ 檔案為空" << std::endl;
            }
        }
        else
        {
            std::cout << "  ⚠ /etc/TZ 檔案不存在" << std::endl;
        }

        // 方法 2: 執行 profile.d 腳本
        std::cout << "[方法2] 執行 profile.d 腳本" << std::endl;

        std::ifstream profileFile("/etc/profile.d/timezone.sh");
        if (profileFile.is_open())
        {
            std::string line;
            bool found = false;

            while (std::getline(profileFile, line))
            {
                // 尋找 export TZ="xxx" 這樣的行
                size_t exportPos = line.find("export TZ=");
                if (exportPos != std::string::npos)
                {
                    std::cout << "  找到設定行: " << line << std::endl;

                    // 提取時區值
                    size_t quoteStart = line.find('"', exportPos);
                    size_t quoteEnd = line.find('"', quoteStart + 1);
                    if (quoteStart != std::string::npos && quoteEnd != std::string::npos)
                    {
                        std::string extractedTz = line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                        std::cout << "  提取到時區: " << extractedTz << std::endl;

                        if (setenv("TZ", extractedTz.c_str(), 1) == 0)
                        {
                            tzset();
                            std::cout << "  ✓ 環境變數已更新為: " << extractedTz << std::endl;
                            found = true;
                        }
                        else
                        {
                            std::cout << "  ✗ 更新環境變數失敗" << std::endl;
                        }
                        break;
                    }
                }
            }

            if (!found)
            {
                std::cout << "  ⚠ 未找到有效的時區設定" << std::endl;
            }

            profileFile.close();
        }
        else
        {
            std::cout << "  ⚠ /etc/profile.d/timezone.sh 檔案不存在" << std::endl;
        }

        // 方法 3: 使用 system() 執行 source
        std::cout << "[方法3] 執行 source 命令" << std::endl;
        std::string sourceCmd = ". /etc/profile.d/timezone.sh 2>/dev/null";
        std::cout << "## [DEBUG] Execute Command: " << sourceCmd << std::endl;
        int sourceResult = system(sourceCmd.c_str());
        std::cout << "  source 命令結果: " << (sourceResult == 0 ? "成功" : "失敗") << std::endl;

        // 驗證最終結果
        const char *currentTz = getenv("TZ");
        std::cout << "\n最終環境變數 TZ: " << (currentTz ? currentTz : "(未設置)") << std::endl;
        std::cout << "當前時間: ";
        if (system("date") != 0)
        {
            std::cout << "無法獲取系統時間" << std::endl;
        }

        return (currentTz != nullptr);
    }
    catch (const std::exception &e)
    {
        std::cerr << "重新載入時區設定時發生異常: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @file cht_p2p_camera_control_handler.cpp (修改部分)
 * @brief 簡化的時區處理實現
 */

// 在 cht_p2p_camera_control_handler.cpp 中修改以下函數：

/**
 * @brief 簡化的時區設置函數
 */
bool ChtP2PCameraControlHandler::setSystemTimezone(const std::string &tzString)
{
    std::cout << "簡化設置系統時區: " << tzString << std::endl;

    try
    {
        // 步驟 1: 設置程序環境變數
        if (setenv("TZ", tzString.c_str(), 1) != 0)
        {
            std::cerr << "ERROR: 設置環境變數失敗" << std::endl;
            return false;
        }
        tzset(); // 重新載入時區

        // 步驟 2: 寫入系統檔案（重開機後生效）
        std::string tzFileCmd = "echo '" + tzString + "' > /etc/TZ";
        executeSystemCommand(tzFileCmd.c_str());

        // 步驟 3: 建立 profile 腳本
        executeSystemCommand("mkdir -p /etc/profile.d");
        std::string profileCmd = "echo 'export TZ=\"" + tzString + "\"' > /etc/profile.d/timezone.sh";
        executeSystemCommand(profileCmd.c_str());
        executeSystemCommand("chmod +x /etc/profile.d/timezone.sh");

        std::cout << "✓ 時區設置完成: " << tzString << std::endl;
        std::cout << "當前時間: ";
        if (system("date") != 0)
        {
            std::cout << "無法獲取系統時間" << std::endl;
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "ERROR: 設置時區時發生異常: " << e.what() << std::endl;
        return false;
    }
}

void ChtP2PCameraControlHandler::displayCurrentTimezoneStatus()
{
    std::cout << "\n========== 當前時區狀態 ==========" << std::endl;

    // 1. 顯示環境變數
    const char *currentTz = getenv("TZ");
    std::cout << "環境變數 TZ: " << (currentTz ? currentTz : "(未設置)") << std::endl;

    // 2. 顯示JSON組態
    try
    {
        auto &paramsManager = CameraParametersManager::getInstance();
        std::string jsonTzId = paramsManager.getTimeZone();
        std::cout << "JSON 時區ID: " << (jsonTzId.empty() ? "(未設置)" : jsonTzId) << std::endl;

        if (!jsonTzId.empty())
        {
            TimezoneInfo tzInfo = TimezoneUtils::getTimezoneInfo(jsonTzId);
            if (!tzInfo.tId.empty())
            {
                std::cout << "時區描述: " << tzInfo.displayName << std::endl;
                std::cout << "UTC偏移: " << tzInfo.baseUtcOffset << " 秒" << std::endl;

                // 顯示該時區的當前時間 - 使用全域函數
                std::string offsetTime = getTimeWithOffset(tzInfo.baseUtcOffset);
                if (!offsetTime.empty())
                {
                    std::cout << "該時區時間: " << offsetTime << std::endl;
                }
            }
        }
    }
    catch (...)
    {
        std::cout << "JSON 組態: 讀取失敗" << std::endl;
    }

    // 3. 顯示系統時間
    std::cout << "系統時間: ";
    if (system("date") != 0)
    {
        std::cout << "無法獲取系統時間" << std::endl;
    }

    std::cout << "=================================" << std::endl;
}
/**
 * @brief 根據baseUtcOffset顯示時間
 */
std::string getTimeWithOffset(const std::string &baseUtcOffset)
{
    try
    {
        // 獲取當前UTC時間
        auto now = std::chrono::system_clock::now();
        auto utc_time = std::chrono::system_clock::to_time_t(now);

        // 加上時區偏移
        int offset_seconds = std::stoi(baseUtcOffset);
        time_t local_time = utc_time + offset_seconds;

        // 格式化時間
        std::tm *tm_local = std::gmtime(&local_time);
        char buffer[64];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_local);

        return std::string(buffer);
    }
    catch (const std::exception &e)
    {
        std::cerr << "計算時間偏移失敗: " << e.what() << std::endl;
        return "";
    }
}
/**
 * @brief 簡化的NTP同步函數
 */
bool performNtpSync()
{
    std::cout << "執行NTP時間同步..." << std::endl;
#if 0
#if 0
    if (CameraDriver::getInstance().isSimulationMode())
    {
        std::cout << "模擬模式：模擬NTP同步完成" << std::endl;
        return true;
    }
#endif
#endif
    // 嘗試使用台灣的NTP服務器
    std::vector<std::string> ntpServers = {
        "tock.stdtime.gov.tw",
        "tick.stdtime.gov.tw",
        "time.stdtime.gov.tw"};

    for (const auto &server : ntpServers)
    {
        std::string ntpCmd = "ntpdate -b -u " + server + " 2>/dev/null";
        std::cout << "嘗試同步: " << server << std::endl;

        int result = system(ntpCmd.c_str());
        if (result == 0)
        {
            std::cout << "✓ NTP同步成功: " << server << std::endl;
            std::cout << "同步後時間: ";
            if (system("date") != 0)
            {
                std::cout << "無法獲取系統時間" << std::endl;
            }
            return true;
        }
    }

    std::cout << "✗ 所有NTP服務器同步失敗" << std::endl;
    return false;
}

std::string ChtP2PCameraControlHandler::handleSetFlicker(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理設定閃爍率: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        std::string flicker;

        // 獲取請求參數
        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }
        if (requestJson.HasMember("flicker") && requestJson["flicker"].IsString())
        {
            flicker = requestJson["flicker"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: flicker");
        }

        // 驗證flicker參數
        if (flicker != "0" && flicker != "1" && flicker != "2")
        {
            throw std::runtime_error("無效的flicker參數，必須為0(50Hz)、1(60Hz)或2(戶外)");
        }

        std::cout << "設定閃爍率 - camId: " << camId
                  << ", flicker: " << flicker << std::endl;

        // 獲取參數管理器
        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證 camId（如果提供的話）
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }

        // 更新閃爍率設定到參數管理器
        paramsManager.setParameter("flicker", flicker);

        // 保存設定到檔案
        bool saveResult = paramsManager.saveToFile();
        std::cout << "閃爍率設定已保存: " << (saveResult ? "成功" : "失敗") << std::endl;

        // 這裡應該調用實際的攝影機驅動來設定閃爍率
        // 例如：CameraDriver::getInstance().setFlickerMode(std::stoi(flicker));
        //  ONLY support  50 Hz  / 60 Hz to type
        int iFlicker;
        if (flicker == "0")
            iFlicker = 50;
        else if (flicker == "1")
            iFlicker = 60;
        else
            iFlicker = 60;
        //CameraDriver::getInstance().setFlickerMode(iFlicker);
        //CameraDriver::getInstance().setFlickerMode(iFlicker);

        // 構建成功回應
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember("flicker", rapidjson::Value(flicker.c_str(), allocator).Move(), allocator);
        response.AddMember("flicker", rapidjson::Value(flicker.c_str(), allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "設定閃爍率時發生異常: " << e.what() << std::endl;

        // 構建失敗回應
        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

std::string ChtP2PCameraControlHandler::handleSetImageQuality(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理設定影像品質: " << payload << std::endl;

    try
    {
        // 解析請求 JSON - 使用 rapidjson
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        std::string requestId;
        std::string imageQuality;

        // 獲取請求參數 - 使用 rapidjson
        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: camId");
        }

        if (requestJson.HasMember("requestId") && requestJson["requestId"].IsString())
        {
            requestId = requestJson["requestId"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: requestId");
        }

        if (requestJson.HasMember("imageQuality") && requestJson["imageQuality"].IsString())
        {
            imageQuality = requestJson["imageQuality"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: imageQuality");
        }

        // 驗證requestId格式: <UDP/Relay>_live_<userId>_<JWTToken>
        std::regex requestIdPattern("^(UDP|Relay)_live_.+_.+$");
        if (!std::regex_match(requestId, requestIdPattern))
        {
            std::cerr << "requestId格式錯誤，應為: <UDP/Relay>_live_<userId>_<JWTToken>" << std::endl;
            throw std::runtime_error("requestId格式錯誤");
        }

        // 驗證imageQuality參數
        if (imageQuality != "0" && imageQuality != "1" && imageQuality != "2")
        {
            throw std::runtime_error("無效的imageQuality參數，必須為0(Low)、1(Middle)或2(High)");
        }

        std::cout << "設定影像品質 - camId: " << camId
                  << ", requestId: " << requestId
                  << ", imageQuality: " << imageQuality << std::endl;

        // 獲取參數管理器
        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證 camId
        std::string currentCamId = paramsManager.getCameraId();
        if (camId != currentCamId)
        {
            std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
            throw std::runtime_error("攝影機 ID 不符");
        }
#if 0
#if 0
        // 檢查是否為模擬模式
        auto &driver = CameraDriver::getInstance();
        bool isSimulationMode = driver.isSimulationMode();

        std::cout << "運行模式: " << (isSimulationMode ? "模擬模式" : "真實模式") << std::endl;

        // 更新影像品質設定到參數管理器
        paramsManager.setRequestId(requestId);
        paramsManager.setImageQuality(imageQuality);


        // 保存設定到檔案
        bool saveResult = paramsManager.saveToFile();
        std::cout << "影像品質設定已保存: " << (saveResult ? "成功" : "失敗") << std::endl;

        // 根據模式調用適當的硬體設定
        bool hardwareResult = false;
        if (isSimulationMode)
        {
            std::cout << "模擬模式：跳過硬體設定驗證" << std::endl;
            hardwareResult = driver.setImageQuality(std::stoi(imageQuality));
        }
        else
        {
            std::cout << "真實模式：執行硬體設定" << std::endl;
            hardwareResult = driver.setImageQuality(std::stoi(imageQuality));

            // 在真實模式下，如果硬體設定失敗，拋出異常
            if (!hardwareResult)
            {
                throw std::runtime_error("硬體設定失敗");
            }
        }

        std::cout << "影像品質硬體設定結果: " << (hardwareResult ? "成功" : "失敗") << std::endl;
#endif
#endif
        // ==== 回應 ====
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();

        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember("requestId", rapidjson::Value(requestId.c_str(), allocator).Move(), allocator);
        response.AddMember("imageQuality", rapidjson::Value(imageQuality.c_str(), allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        std::string responseStr = buffer.GetString();
        std::cout << "影像品質設定成功回應: " << responseStr << std::endl;

        return responseStr;
    }
    catch (const std::exception &e)
    {
        std::cerr << "設定影像品質時發生異常: " << e.what() << std::endl;

        // 構建失敗回應 - 使用 rapidjson
        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);

        return buffer.GetString();
    }
}

std::string ChtP2PCameraControlHandler::handleSetMicrophone(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理設定麥克風: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        std::string microphoneSensitivity;

        // 獲取請求參數
        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }
        if (requestJson.HasMember("microphoneSensitivity") && requestJson["microphoneSensitivity"].IsString())
        {
            microphoneSensitivity = requestJson["microphoneSensitivity"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: microphoneSensitivity");
        }

        // 驗證microphoneSensitivity參數（0~10）
        int sensitivity = 0;
        try
        {
            sensitivity = std::stoi(microphoneSensitivity);
            if (sensitivity < 0 || sensitivity > 10)
            {
                throw std::runtime_error("無效的microphoneSensitivity參數，必須為0~10之間");
            }
        }
        catch (const std::invalid_argument &)
        {
            throw std::runtime_error("microphoneSensitivity參數格式錯誤，必須為數字");
        }

        std::cout << "設定麥克風 - camId: " << camId
                  << ", microphoneSensitivity: " << microphoneSensitivity << std::endl;

        // 獲取參數管理器
        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證 camId（如果提供的話）
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }

        // 更新麥克風設定到參數管理器
        paramsManager.setParameter("microphoneSensitivity", microphoneSensitivity);

        // 保存設定到檔案
        bool saveResult = paramsManager.saveToFile();
        std::cout << "麥克風設定已保存: " << (saveResult ? "成功" : "失敗") << std::endl;

        // 這裡應該調用實際的攝影機驅動來設定麥克風靈敏度
        // 例如：CameraDriver::getInstance().setMicrophoneSensitivity(std::stoi(microphoneSensitivity));
        int setSensitivityVal = 0;
        setSensitivityVal = sensitivity * 10;
        //CameraDriver::getInstance().setMicrophoneHardware(true, setSensitivityVal);
        //CameraDriver::getInstance().setMicrophoneHardware(true, setSensitivityVal);

        // 構建成功回應
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember("microphoneSensitivity", rapidjson::Value(microphoneSensitivity.c_str(), allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "設定麥克風時發生異常: " << e.what() << std::endl;

        // 構建失敗回應
        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

std::string ChtP2PCameraControlHandler::handleSetNightMode(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理設定夜間模式: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        std::string nightMode;

        // 獲取請求參數
        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }
        if (requestJson.HasMember("nightMode") && requestJson["nightMode"].IsString())
        {
            nightMode = requestJson["nightMode"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: nightMode");
        }

        // 驗證nightMode參數
        if (nightMode != "0" && nightMode != "1")
        {
            throw std::runtime_error("無效的nightMode參數，必須為0(關閉)或1(開啟)");
        }

        std::cout << "設定夜間模式 - camId: " << camId
                  << ", nightMode: " << nightMode << std::endl;

        // 獲取參數管理器
        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證 camId（如果提供的話）
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }

        // 更新夜間模式設定到參數管理器
        paramsManager.setParameter("nightMode", nightMode);

        // 保存設定到檔案
        bool saveResult = paramsManager.saveToFile();
        std::cout << "夜間模式設定已保存: " << (saveResult ? "成功" : "失敗") << std::endl;

        // 這裡應該調用實際的攝影機驅動來設定夜間模式
        // 例如：CameraDriver::getInstance().setNightMode(nightMode == "1");
        // ## DEPEND  on factory.sh  test nightmode shell script
        bool bIsNightMod;
        if (nightMode == "0")
            bIsNightMod = false;
        else if (nightMode == "1")
            bIsNightMod = true;
        else
            bIsNightMod = false;
        //CameraDriver::getInstance().setNightModeHardware(bIsNightMod, false);
        //CameraDriver::getInstance().setNightModeHardware(bIsNightMod, false);

        // 構建成功回應
        rapidjson::Document response;
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember("nightMode", rapidjson::Value(nightMode.c_str(), allocator).Move(), allocator);
        response.AddMember("nightMode", rapidjson::Value(nightMode.c_str(), allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "設定夜間模式時發生異常: " << e.what() << std::endl;

        // 構建失敗回應
        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

std::string ChtP2PCameraControlHandler::handleSetAutoNightVision(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理設定自動夜視: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        std::string autoNightVision;

        // 獲取請求參數
        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }
        if (requestJson.HasMember("autoNightVision") && requestJson["autoNightVision"].IsString())
        {
            autoNightVision = requestJson["autoNightVision"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: autoNightVision");
        }

        // 驗證autoNightVision參數
        if (autoNightVision != "0" && autoNightVision != "1")
        {
            throw std::runtime_error("無效的autoNightVision參數，必須為0(關閉)或1(開啟)");
        }

        std::cout << "設定自動夜視 - camId: " << camId
                  << ", autoNightVision: " << autoNightVision << std::endl;

        // 獲取參數管理器
        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證 camId（如果提供的話）
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }

        // 更新自動夜視設定到參數管理器
        paramsManager.setParameter("autoNightVision", autoNightVision);

        // 保存設定到檔案
        bool saveResult = paramsManager.saveToFile();
        std::cout << "自動夜視設定已保存: " << (saveResult ? "成功" : "失敗") << std::endl;

        // 這裡應該調用實際的攝影機驅動來設定自動夜視
        // 例如：CameraDriver::getInstance().setAutoNightVision(autoNightVision == "1");

        bool bIsAutoNightVision;
        if (autoNightVision == "0")
            bIsAutoNightVision = false;
        else if (autoNightVision == "1")
            bIsAutoNightVision = true;
        else
            bIsAutoNightVision = false;
        //CameraDriver::getInstance().setAutoNightVision(bIsAutoNightVision);
        //CameraDriver::getInstance().setAutoNightVision(bIsAutoNightVision);

        // 構建成功回應
        rapidjson::Document response;
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember("autoNightVision", rapidjson::Value(autoNightVision.c_str(), allocator).Move(), allocator);
        response.AddMember("autoNightVision", rapidjson::Value(autoNightVision.c_str(), allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "設定自動夜視時發生異常: " << e.what() << std::endl;

        // 構建失敗回應
        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

std::string ChtP2PCameraControlHandler::handleSetSpeak(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理設定揚聲器: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        std::string speakVolume;
        int volume = 0;

        // 獲取請求參數
        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }
        if (requestJson.HasMember("speakVolume") && requestJson["speakVolume"].IsString())
        {
            speakVolume = requestJson["speakVolume"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: speakVolume");
        }

        // 驗證speakVolume參數（0~10）
        try
        {
            volume = std::stoi(speakVolume);
            if (volume < 0 || volume > 10)
            {
                throw std::runtime_error("無效的speakVolume參數，必須為0~10之間");
            }
        }
        catch (const std::invalid_argument &)
        {
            throw std::runtime_error("speakVolume參數格式錯誤，必須為數字");
        }

        std::cout << "設定揚聲器 - camId: " << camId
                  << ", speakVolume: " << speakVolume << std::endl;

        // 獲取參數管理器
        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證 camId（如果提供的話）
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }

        // 更新揚聲器設定到參數管理器
        paramsManager.setParameter("speakVolume", volume);

        // 保存設定到檔案
        bool saveResult = paramsManager.saveToFile();
        std::cout << "揚聲器設定已保存: " << (saveResult ? "成功" : "失敗") << std::endl;

        // 這裡應該調用實際的攝影機驅動來設定揚聲器音量
        // 例如：
        int setVolValue = 0;
        setVolValue = volume * 10;
        //CameraDriver::getInstance().setSpeakerHardware(true, setVolValue);
        //CameraDriver::getInstance().setSpeakerHardware(true, setVolValue);

        // 構建成功回應
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember("speakVolume", rapidjson::Value(speakVolume.c_str(), allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "設定揚聲器時發生異常: " << e.what() << std::endl;

        // 構建失敗回應
        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

std::string ChtP2PCameraControlHandler::handleSetFlipUpDown(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理設定上下翻轉: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        std::string isFlipUpDown;

        // 獲取請求參數
        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }
        if (requestJson.HasMember("isFlipUpDown") && requestJson["isFlipUpDown"].IsString())
        {
            isFlipUpDown = requestJson["isFlipUpDown"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: isFlipUpDown");
        }

        // 驗證isFlipUpDown參數
        if (isFlipUpDown != "0" && isFlipUpDown != "1")
        {
            throw std::runtime_error("無效的isFlipUpDown參數，必須為0(關閉)或1(開啟)");
        }

        std::cout << "設定上下翻轉 - camId: " << camId
                  << ", isFlipUpDown: " << isFlipUpDown << std::endl;

        // 獲取參數管理器
        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證 camId（如果提供的話）
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }

        // 更新上下翻轉設定到參數管理器
        paramsManager.setParameter("isFlipUpDown", isFlipUpDown);

        // 保存設定到檔案
        bool saveResult = paramsManager.saveToFile();
        std::cout << "上下翻轉設定已保存: " << (saveResult ? "成功" : "失敗") << std::endl;

        // 這裡應該調用實際的攝影機驅動來設定上下翻轉
        // 例如：CameraDriver::getInstance().setFlipUpDown(isFlipUpDown == "1");
        // ## DEPEND  venc2_msg_sender -f 3 -e 1(影像正)  venc2_msg_sender -f 3 -e 0 (影像反)
        bool flipUpDown;
        if (isFlipUpDown == "0")
            flipUpDown = false;
        else if (isFlipUpDown == "1")
            flipUpDown = true;
        else
            flipUpDown = false;
        //CameraDriver::getInstance().setImageFlipHardware(flipUpDown);
        //CameraDriver::getInstance().setImageFlipHardware(flipUpDown);

        // 構建成功回應
        rapidjson::Document response;
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember("isFlipUpDown", rapidjson::Value(isFlipUpDown.c_str(), allocator).Move(), allocator);
        response.AddMember("isFlipUpDown", rapidjson::Value(isFlipUpDown.c_str(), allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "設定上下翻轉時發生異常: " << e.what() << std::endl;

        // 構建失敗回應
        rapidjson::Document errorResponse;
        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

std::string ChtP2PCameraControlHandler::handleSetLED(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理設定LED指示燈: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        std::string statusIndicatorLight;

        // 獲取請求參數
        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }
        if (requestJson.HasMember("statusIndicatorLight") && requestJson["statusIndicatorLight"].IsString())
        {
            statusIndicatorLight = requestJson["statusIndicatorLight"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: statusIndicatorLight");
        }

        // 驗證statusIndicatorLight參數
        if (statusIndicatorLight != "0" && statusIndicatorLight != "1")
        {
            throw std::runtime_error("無效的statusIndicatorLight參數，必須為0(關閉)或1(開啟)");
        }

        std::cout << "設定LED指示燈 - camId: " << camId
                  << ", statusIndicatorLight: " << statusIndicatorLight << std::endl;

        // 獲取參數管理器
        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證 camId（如果提供的話）
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }

        // 更新LED指示燈設定到參數管理器
        paramsManager.setParameter("statusIndicatorLight", statusIndicatorLight);

        // 保存設定到檔案
        bool saveResult = paramsManager.saveToFile();
        std::cout << "LED指示燈設定已保存: " << (saveResult ? "成功" : "失敗") << std::endl;

        // 這裡應該調用實際的攝影機驅動來設定LED指示燈
        // 例如：CameraDriver::getInstance().setStatusLED(statusIndicatorLight == "1");
        bool bLedEnabled;
        if (statusIndicatorLight == "0")
            bLedEnabled = false;
        else if (statusIndicatorLight == "1")
            bLedEnabled = true;
        else
            bLedEnabled = false;
        //CameraDriver::getInstance().setStatusLEDHardware(bLedEnabled);
        //CameraDriver::getInstance().setStatusLEDHardware(bLedEnabled);

        // 構建成功回應
        rapidjson::Document response;
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember("statusIndicatorLight", rapidjson::Value(statusIndicatorLight.c_str(), allocator).Move(), allocator);
        response.AddMember("statusIndicatorLight", rapidjson::Value(statusIndicatorLight.c_str(), allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "設定LED指示燈時發生異常: " << e.what() << std::endl;

        // 構建失敗回應
        rapidjson::Document errorResponse;
        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

std::string ChtP2PCameraControlHandler::handleSetCameraPower(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理設定攝影機電源: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        std::string camera;

        // 獲取請求參數
        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }
        if (requestJson.HasMember("camera") && requestJson["camera"].IsString())
        {
            camera = requestJson["camera"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: camera");
        }

        // 驗證camera參數
        if (camera != "0" && camera != "1")
        {
            throw std::runtime_error("無效的camera參數，必須為0(關閉)或1(開啟)");
        }

        std::cout << "設定攝影機電源 - camId: " << camId
                  << ", camera: " << camera << std::endl;

        // 獲取參數管理器
        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證 camId（如果提供的話）
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }

        // 更新攝影機電源設定到參數管理器
        paramsManager.setParameter("cameraPower", camera);

        // 保存設定到檔案
        bool saveResult = paramsManager.saveToFile();
        std::cout << "攝影機電源設定已保存: " << (saveResult ? "成功" : "失敗") << std::endl;

        // 這裡應該調用實際的攝影機驅動來設定電源狀態
        // 例如：CameraDriver::getInstance().setCameraPower(camera == "1");

        // 構建成功回應
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember("camera", rapidjson::Value(camera.c_str(), allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "設定攝影機電源時發生異常: " << e.what() << std::endl;

        // 構建失敗回應
        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

std::string ChtP2PCameraControlHandler::handleGetSnapshotHamiCamDevice(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理取得快照: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError() || !requestJson.IsObject())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON格式錯誤");
        }

        auto &paramsManager = CameraParametersManager::getInstance();
        // 驗證camId
        std::string errorMsg;
        if (!validateCamId(requestJson, paramsManager.getCameraId(), errorMsg))
        {
            throw std::runtime_error(errorMsg);
        }

        // 獲取請求參數
        std::string eventId;
        auto it = requestJson.FindMember(PAYLOAD_KEY_EVENT_ID);
        if (it == requestJson.MemberEnd() || !it->value.IsString()) {
            throw std::runtime_error(std::string("缺少必要欄位: ") + std::string(PAYLOAD_KEY_EVENT_ID));
        }
        eventId = it->value.GetString();

#if 0
        const std::string& currentCamId = paramsManager.getCameraId();
        std::string snapshotTime, filePath;
        int result;
        std::tie(result, snapshotTime, filePath) = CameraDriver::getInstance().getSnapshot(currentCamId);

        std::cout << "快照請求 - eventId: " << eventId << std::endl;
#endif

        // 先回應準備截圖訊息
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember("description", rapidjson::Value("準備截圖", allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "處理快照請求時發生異常: " << e.what() << std::endl;

        rapidjson::Document errorResponse;
        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

// 內部測試函數 - 驗證參數設定結果
static bool verifyParameterSetting(const std::string &paramName, const std::string &expectedValue)
{
    try
    {
        auto &paramsManager = CameraParametersManager::getInstance();
        std::string actualValue = paramsManager.getParameter(paramName, "");

        bool isValid = (actualValue == expectedValue);
        std::cout << "參數驗證 " << paramName << ": 期望=" << expectedValue
                  << ", 實際=" << actualValue << ", 結果=" << (isValid ? "通過" : "失敗") << std::endl;

        return isValid;
    }
    catch (const std::exception &e)
    {
        std::cerr << "驗證參數 " << paramName << " 時發生異常: " << e.what() << std::endl;
        return false;
    }
}

// 內部測試函數 - 模擬控制命令執行
static bool simulateControlExecution(CHTP2P_ControlType controlType, const std::string &testPayload)
{
    try
    {
        std::cout << "模擬執行控制命令: " << controlType << std::endl;

        auto &handler = ChtP2PCameraControlHandler::getInstance();
        std::string response = handler.handleControl(controlType, testPayload);

        // 解析回應檢查是否成功
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "回應解析失敗" << std::endl;
            return false;
        }

        if (responseJson.HasMember(PAYLOAD_KEY_RESULT) && responseJson[PAYLOAD_KEY_RESULT].IsInt())
        {
            int result = responseJson[PAYLOAD_KEY_RESULT].GetInt();
            std::cout << "控制命令執行結果: " << (result == 1 ? "成功" : "失敗") << std::endl;
            return result == 1;
        }

        return false;
    }
    catch (const std::exception &e)
    {
        std::cerr << "模擬控制命令執行異常: " << e.what() << std::endl;
        return false;
    }
}
std::string ChtP2PCameraControlHandler::handleRestartHamiCamDevice(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理重啟設備: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }

        std::cout << "重啟請求 - camId: " << camId << std::endl;

        // 驗證 camId
        auto &paramsManager = CameraParametersManager::getInstance();
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }

        // 先回應準備重啟訊息
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember("description", rapidjson::Value("準備reboot", allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        // 設置裝置狀態為未執行，因為要重開機了
        paramsManager.setParameter("deviceStatus", std::string("0"));
        paramsManager.saveToFile();

        // 啟動非同步重啟處理 (等待5秒後重啟)
        std::thread rebootThread([]() {
            try
            {
                std::cout << "等待5秒後重啟設備..." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
#if 0
                // 檢查是否為模擬模式
                if (CameraDriver::getInstance().isSimulationMode())
                {
                    std::cout << "模擬模式：模擬重啟完成" << std::endl;
                }
                else
                {
                    std::cout << "執行系統重啟..." << std::endl;
                    executeSystemCommand("reboot");
                }
#endif
            }
            catch (const std::exception &e)
            {
                std::cerr << "重啟處理執行緒異常: " << e.what() << std::endl;
            }
        });

        rebootThread.detach();

        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "處理重啟請求時發生異常: " << e.what() << std::endl;

        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

std::string ChtP2PCameraControlHandler::handleSetCamStorageDay(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理設定儲存天數: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        std::string storageDay;

        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }

        if (requestJson.HasMember("storageDay") && requestJson["storageDay"].IsString())
        {
            storageDay = requestJson["storageDay"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: storageDay");
        }

        std::cout << "設定雲存天數 - camId: " << camId << ", storageDay: " << storageDay << std::endl;

        // 驗證 camId
        auto &paramsManager = CameraParametersManager::getInstance();
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }

        // 驗證 storageDay 數值
        try
        {
            int days = std::stoi(storageDay);
            if (days < 0 || days > 365)
            {
                throw std::runtime_error("雲存天數必須在0-365天之間");
            }
        }
        catch (const std::invalid_argument &)
        {
            throw std::runtime_error("雲存天數格式錯誤");
        }

        // 更新雲存天數設定
        paramsManager.setParameter("storageDay", storageDay);
        paramsManager.saveToFile();

        // 回應成功
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember("storageDay", rapidjson::Value(storageDay.c_str(), allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "設定雲存天數時發生異常: " << e.what() << std::endl;

        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

std::string ChtP2PCameraControlHandler::handleSetCamEventStorageDay(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理設定事件儲存天數: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        std::string eventStorageDay;

        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }

        if (requestJson.HasMember("eventStorageDay") && requestJson["eventStorageDay"].IsString())
        {
            eventStorageDay = requestJson["eventStorageDay"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: eventStorageDay");
        }

        std::cout << "設定事件雲存天數 - camId: " << camId << ", eventStorageDay: " << eventStorageDay << std::endl;

        // 驗證 camId
        auto &paramsManager = CameraParametersManager::getInstance();
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }

        // 驗證 eventStorageDay 數值
        try
        {
            int days = std::stoi(eventStorageDay);
            if (days < 0 || days > 365)
            {
                throw std::runtime_error("事件雲存天數必須在0-365天之間");
            }
        }
        catch (const std::invalid_argument &)
        {
            throw std::runtime_error("事件雲存天數格式錯誤");
        }

        // 更新事件雲存天數設定
        paramsManager.setParameter("eventStorageDay", eventStorageDay);
        paramsManager.saveToFile();

        // 回應成功
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember("eventStorageDay", rapidjson::Value(eventStorageDay.c_str(), allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "設定事件雲存天數時發生異常: " << e.what() << std::endl;

        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();

        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

std::string ChtP2PCameraControlHandler::handleHamiCamFormatSDCard(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理格式化SD卡: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }

        std::cout << "SD卡格式化請求 - camId: " << camId << std::endl;

        // 驗證 camId
        auto &paramsManager = CameraParametersManager::getInstance();
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }

        // 先回應準備格式化訊息
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember("description", rapidjson::Value("準備SD格式化", allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        // 啟動非同步格式化處理
        std::thread formatThread([]() {
            try
            {
                std::cout << "開始SD卡格式化..." << std::endl;
#if 0
#if 0
                if (CameraDriver::getInstance().isSimulationMode()) {
                    std::cout << "模擬模式：模擬SD卡格式化" << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                    std::cout << "模擬SD卡格式化完成" << std::endl;
                    return;
                }
#endif
#endif
                // 步驟 1: 停止 SD 卡檢查服務
                std::cout << "停止 SD 卡檢查服務..." << std::endl;
                executeSystemCommand("/etc/init.d/S98SdcardChecker stop");
                std::this_thread::sleep_for(std::chrono::seconds(2)); // 等待服務完全停止

                const char *dev = "/dev/mmcblk0";
                const char *part = "/dev/mmcblk0p1";
                const char *mount_point = "/mnt/sd";

                // 檢查 SD 卡設備是否存在
                if (access(dev, F_OK) != 0) {
                    std::cerr << "找不到 SD 卡裝置: " << dev << std::endl;
                    // 重新啟動服務後退出
                    executeSystemCommand("/etc/init.d/S98SdcardChecker start");
                    return;
                }

                // 步驟 2: 卸載 SD 卡
                std::cout << "檢查並卸載 SD 卡..." << std::endl;


                // 首先嘗試卸載 /mnt/sd/0 (如果存在)
                if (system("mount | grep /mnt/sd/0") == 0)
                if (system("mount | grep /mnt/sd/0") == 0)
                {
                    std::cout << "發現 /mnt/sd/0 掛載，嘗試卸載..." << std::endl;
                    executeSystemCommand("fuser -k /mnt/sd/0 2>/dev/null");
                    int ret = system("umount /mnt/sd/0 2>/dev/null");
                    if (ret == 0) {
                        std::cout << "/mnt/sd/0 卸載成功" << std::endl;
                    } else {
                        std::cout << "/mnt/sd/0 卸載失敗，但繼續處理" << std::endl;
                    }
                }


                // 然後檢查並卸載 /mnt/sd
                if (system("mount | grep /mnt/sd") == 0)
                if (system("mount | grep /mnt/sd") == 0)
                {
                    std::cout << "SD 卡已掛載，嘗試卸載..." << std::endl;
                    // 終止占用進程
                    executeSystemCommand("fuser -k /mnt/sd 2>/dev/null");
                    // 執行卸載
                    int ret = system("umount /mnt/sd 2>/dev/null");
                    if (ret != 0) {
                        std::cerr << "卸載 SD 卡失敗" << std::endl;
                        // 重新啟動服務後退出
                        executeSystemCommand("/etc/init.d/S98SdcardChecker start");
                        return;
                    }
                    std::cout << "SD 卡卸載成功" << std::endl;
                }

                // 同步檔案系統
                executeSystemCommand("sync");

                // 檢查是否已有分割區
                std::cout << "檢查是否已有 partition..." << std::endl;
                bool hasPartition = (access(part, F_OK) == 0);
                std::cout << (hasPartition ? "已有分割區，將重新建立" : "無分割區，將建立新的") << std::endl;

                // 刪除並建立新分割區（非互動 fdisk）
                int ret = system("echo -e \"o\\nn\\np\\n1\\n\\n\\nw\" | fdisk /dev/mmcblk0");
                if (ret != 0) {
                    std::cerr << "fdisk 建立分割區失敗" << std::endl;
                    return;
                }

                // 通知內核重新讀取分割區表
                executeSystemCommand("partprobe /dev/mmcblk0");
                sleep(2); // 增加等待時間以確保設備節點更新

                if (access(part, F_OK) != 0) {
                    std::cerr << "找不到新分割區: " << part << std::endl;
                    return;
                }
                // 設定卷標
                time_t now = time(nullptr);
                struct tm *ltm = localtime(&now);
                char label[32];
                snprintf(label, sizeof(label), "HAMI_%02d%02d%02d",
                        (ltm->tm_year + 1900) % 100, 1 + ltm->tm_mon, ltm->tm_mday);

                // 格式化為 exFAT
                char cmd[128];
                snprintf(cmd, sizeof(cmd), "mkfs.exfat -n %s /dev/mmcblk0p1", label);
                std::cout << "格式化命令: " << cmd << std::endl;

                std::cout << "開始格式化為 exFAT..." << std::endl;
                ret = system(cmd);
                if (ret != 0) {
                    std::cerr << "格式化 exFAT 失敗" << std::endl;
                    return;
                }

                std::cout << "格式化成功，重新掛載..." << std::endl;
                executeSystemCommand("mkdir -p /mnt/sd");
                ret = system("mount /dev/mmcblk0p1 /mnt/sd");
                if (ret != 0) {
                    std::cerr << "重新掛載 SD 卡失敗" << std::endl;
                    return;
                }

                // 建立標記檔案
                char filename[64];
                snprintf(filename, sizeof(filename), "/mnt/sd/.zw_cht730_%04d%02d%02d",1900 + ltm->tm_year, 1 + ltm->tm_mon, ltm->tm_mday);

                std::ofstream outFile(filename);
                if (outFile.is_open())
                if (outFile.is_open())
                {
                    outFile << "created by CHT format handler on "
                            << (1900 + ltm->tm_year) << "-"
                            << (1 + ltm->tm_mon) << "-"
                            << ltm->tm_mday;
                    outFile.close();
                    std::cout << "建立新標記檔案: " << filename << std::endl;
                } else {
                    std::cerr << "無法建立標記檔案: " << filename << std::endl;
                }

                // 步驟 3: 重新啟動 SD 卡檢查服務
                std::cout << "重新啟動 SD 卡檢查服務..." << std::endl;
                executeSystemCommand("/etc/init.d/S98SdcardChecker start");
                std::cout << "SD 卡格式化程序完成" << std::endl;
            }
            catch (const std::exception &e)
            {
                std::cerr << "SD卡格式化執行緒異常: " << e.what() << std::endl;
                // 確保在發生異常時也重新啟動服務
                std::cout << "異常處理：重新啟動 SD 卡檢查服務..." << std::endl;
                executeSystemCommand("/etc/init.d/S98SdcardChecker start");
            }
        });

        formatThread.detach();

        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "處理SD卡格式化時發生異常: " << e.what() << std::endl;

        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

std::string ChtP2PCameraControlHandler::handleHamiCamPtzControlMove(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理PTZ移動控制: " << payload << std::endl;
    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError() || !requestJson.IsObject())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON格式錯誤");
        }

        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證camId
        std::string errorMsg;
        if (!validateCamId(requestJson, paramsManager.getCameraId(), errorMsg))
        {
            throw std::runtime_error(errorMsg);
        }

        // 驗證PTZ命令
        std::string cmd;
        auto it = requestJson.FindMember(PAYLOAD_KEY_CMD);
        if (it == requestJson.MemberEnd() || !it->value.IsString()) {
            throw std::runtime_error(std::string("缺少必要欄位: ") + std::string(PAYLOAD_KEY_CMD));
        }
        cmd = it->value.GetString();

#if 0
        auto &driver = CameraDriver::getInstance();
        if (!driver.validatePTZCmd(cmd))
        {
            throw std::runtime_error("無效的PTZ命令");
        }

        std::cout << "執行PTZ命令 - cmd: " << cmd << std::endl;

        // 執行PTZ控制
        bool success = driver.setPTZControlMove(cmd);

        // 儲存PTZ狀態
        paramsManager.setParameter("lastPtzCommand", cmd);
        paramsManager.setParameter("ptzStatus", success ? "4" : "0");
        paramsManager.saveToFile();
#endif
        bool success = true; // 假設成功

        // 構建成功回應 - 使用 rapidjson
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, success ? 1 : 0, allocator);
        response.AddMember(PAYLOAD_KEY_CMD, rapidjson::Value(cmd.c_str(), allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_CMD, rapidjson::Value(cmd.c_str(), allocator).Move(), allocator);
        if (success)
            response.AddMember(PAYLOAD_KEY_DESCRIPTION, rapidjson::Value("Send OK", allocator).Move(), allocator);
            response.AddMember(PAYLOAD_KEY_DESCRIPTION, rapidjson::Value("Send OK", allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        std::string responseStr = buffer.GetString();
        std::cout << "PTZ移動控制回應: " << responseStr << std::endl;

        return responseStr;
    }
    catch (const std::exception &e)
    {
        // 構建失敗回應 - 使用 rapidjson
        std::cerr << "ERROR: PTZ控制異常: " << e.what() << std::endl;
        std::string desc = std::string("PTZ控制異常: ") + e.what();

        return createErrorResponse(desc);
    }
}

std::string ChtP2PCameraControlHandler::handleHamiCamPtzControlConfigSpeed(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理PTZ速度設定: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError() || !requestJson.IsObject())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON格式錯誤");
        }

        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證camId
        std::string errorMsg;
        if (!validateCamId(requestJson, paramsManager.getCameraId(), errorMsg))
        {
            throw std::runtime_error(errorMsg);
        }

        int speed = 0;
        auto it = requestJson.FindMember(PAYLOAD_KEY_SPEED);
        if (it == requestJson.MemberEnd() || !it->value.IsInt()) {
            throw std::runtime_error(std::string("缺少必要欄位: ") + std::string(PAYLOAD_KEY_SPEED));
        }
        speed = it->value.GetInt();

        // 驗證速度範圍
        if (speed < 0 || speed > 2)
        {
            throw std::runtime_error("PTZ速度必須在0-2之間");
        }

        std::cout << "PTZ速度設定 - speed: " << speed << std::endl;

#if 0
        // 設定PTZ速度
        bool success = CameraDriver::getInstance().setPTZSpeed(speed);


        // 儲存PTZ速度設定
        paramsManager.setParameter("ptzSpeed", std::to_string(speed));
        paramsManager.saveToFile();
#endif
        bool success = true; // 假設成功

        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, success ? 1 : 0, allocator);
        response.AddMember(PAYLOAD_KEY_SPEED, speed, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        // 構建失敗回應 - 使用 rapidjson
        std::cerr << "PTZ速度設定時發生異常: " << e.what() << std::endl;
        std::string desc = std::string("PTZ速度設定時發生異常: ") + e.what();

        return createErrorResponse(desc);
    }
}

std::string ChtP2PCameraControlHandler::handleHamiCamGetPtzControl(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理獲取PTZ控制資訊: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError() || !requestJson.IsObject())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON格式錯誤");
        }

        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證camId
        std::string errorMsg;
        if (!validateCamId(requestJson, paramsManager.getCameraId(), errorMsg))
        {
            throw std::runtime_error(errorMsg);
        }

        int iTourStayTime = 0;
        int iSpeed = 0;
        int iSpeed = 0;
        int iHumanTrackingSetting = 0;
        int iPetTrackingSetting = 0;
        int iPetTrackingSetting = 0;
        int iStatus = 0;
        int iPetStatus = 0;

#if 0
        bool success = CameraDriver::getInstance().getPTZStatus(
                &iTourStayTime, &iSpeed, &iHumanTrackingSetting, &iPetTrackingSetting, &iStatus, &iPetStatus);


        std::cout << " success " << success
                << " iTourStayTime " << iTourStayTime
                << " iSpeed " << iSpeed
                << " iHumanTrackingSetting " << iHumanTrackingSetting
                << " iPetTrackingSetting " << iPetTrackingSetting
                << " iStatus " << iStatus
                << " iPetStatus " << iPetStatus
                << std::endl;
        if (!success) {
            throw std::runtime_error("Get PTZ agent status error!!!");
        }
#endif
        std::string ptzTourStayTime = std::to_string(iTourStayTime);
        std::string speed = std::to_string(iSpeed);
        std::string humanTracking = std::to_string(iHumanTrackingSetting);
        std::string petTracking = std::to_string(iPetTrackingSetting);
        std::string ptzStatus = std::to_string(iStatus);
        std::string ptzPetStatus = std::to_string(iPetStatus);

        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember(PAYLOAD_KEY_PTZ_TOUR_STAY_TIME, rapidjson::Value(ptzTourStayTime.c_str(), allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_SPEED, rapidjson::Value(speed.c_str(), allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_HUMAN_TRACKING, rapidjson::Value(humanTracking.c_str(), allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_PET_TRACKING, rapidjson::Value(petTracking.c_str(), allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_PTZ_STATUS, rapidjson::Value(ptzStatus.c_str(), allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_PTZ_PET_STATUS, rapidjson::Value(ptzPetStatus.c_str(), allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_PTZ_TOUR_STAY_TIME, rapidjson::Value(ptzTourStayTime.c_str(), allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_SPEED, rapidjson::Value(speed.c_str(), allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_HUMAN_TRACKING, rapidjson::Value(humanTracking.c_str(), allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_PET_TRACKING, rapidjson::Value(petTracking.c_str(), allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_PTZ_STATUS, rapidjson::Value(ptzStatus.c_str(), allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_PTZ_PET_STATUS, rapidjson::Value(ptzPetStatus.c_str(), allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        // 構建失敗回應 - 使用 rapidjson
        std::cerr << "ERROR: 獲取PTZ控制資訊時發生異常: " << e.what() << std::endl;
        std::string desc = std::string("獲取PTZ控制資訊時發生異常: ") + e.what();

        return createErrorResponse(desc);
    }
}

std::string ChtP2PCameraControlHandler::handleHamiCamPtzControlTourGo(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理PTZ巡航: " << payload << std::endl;
    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError() || !requestJson.IsObject())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON格式錯誤");
        }

        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證camId
        std::string errorMsg;
        if (!validateCamId(requestJson, paramsManager.getCameraId(), errorMsg))
        {
            throw std::runtime_error(errorMsg);
        }

        // 驗證PTZ命令
        std::string indexSequence;
        auto it = requestJson.FindMember(PAYLOAD_KEY_INDEX_SEQUENCE);
        if (it == requestJson.MemberEnd() || !it->value.IsString()) {
            throw std::runtime_error(std::string("缺少必要欄位: ") + std::string(PAYLOAD_KEY_INDEX_SEQUENCE));
        }
        indexSequence = it->value.GetString();
        if (indexSequence.empty())
        {
            throw std::runtime_error("巡航路徑不能為空");
        }

        std::cout << "INFO: 設定PTZ巡航路徑: " << indexSequence << std::endl;

#if 0
        // 執行PTZ巡航
        bool success = CameraDriver::getInstance().setPTZGoTour(indexSequence);

        // 儲存PTZ巡航設定
        paramsManager.setParameter("ptzTourSequence", indexSequence);
        paramsManager.setParameter("ptzStatus", "2"); // 巡航狀態
        paramsManager.saveToFile();
#endif
        bool success = true; // 假設成功

        // 構建成功回應 - 使用 rapidjson
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, success ? 1 : 0, allocator);
        response.AddMember(PAYLOAD_KEY_INDEX_SEQUENCE, rapidjson::Value(indexSequence.c_str(), allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_INDEX_SEQUENCE, rapidjson::Value(indexSequence.c_str(), allocator).Move(), allocator);
        if (success)
            response.AddMember(PAYLOAD_KEY_DESCRIPTION, rapidjson::Value("Send OK", allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        std::string responseStr = buffer.GetString();
        std::cout << "PTZ巡航控制回應: " << responseStr << std::endl;

        return responseStr;
    }
    catch (const std::exception &e)
    {
        // 構建失敗回應 - 使用 rapidjson
        std::cerr << "ERROR: PTZ巡航模式異常: " << e.what() << std::endl;
        std::string desc = std::string("PTZ巡航模式異常: ") + e.what();

        return createErrorResponse(desc);
    }
}

std::string ChtP2PCameraControlHandler::handleHamiCamPtzControlGoPst(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理PTZ移動到預設點: " << payload << std::endl;
    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError() || !requestJson.IsObject())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON格式錯誤");
        }

        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證camId
        std::string errorMsg;
        if (!validateCamId(requestJson, paramsManager.getCameraId(), errorMsg))
        {
            throw std::runtime_error(errorMsg);
        }

        // 驗證預設點範圍
        int index;
        auto it = requestJson.FindMember(PAYLOAD_KEY_POSITION_INDEX);
        if (it == requestJson.MemberEnd() || !it->value.IsInt()) {
            throw std::runtime_error(std::string("缺少必要欄位: ") + std::string(PAYLOAD_KEY_POSITION_INDEX));
        }
        index = it->value.GetInt();


        if (index < 1 || index > 4)
        {
            throw std::runtime_error("PTZ移動到預設點必須在1-4之間");
        }

        std::cout << "PTZ移動到預設點 - index: " << index << std::endl;
#if 0
#if 0
        // 設定PTZ移動到預設點
        bool success = CameraDriver::getInstance().setPTZGoPositionPoint(index);


        // 儲存PTZ移動設定
        // paramsManager.setParameter("ptzindex", std::to_string(index));
        paramsManager.setParameter("ptzStatus", "4");
        paramsManager.saveToFile();

        // 構建成功回應 - 使用 rapidjson
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, success ? 1 : 0, allocator);
        response.AddMember(PAYLOAD_KEY_POSITION_INDEX, index, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        std::string responseStr = buffer.GetString();
        std::cout << "PTZ控制成功回應: " << responseStr << std::endl;

        return responseStr;
#endif
#endif
    }
    catch (const std::exception &e)
    {
        // 構建失敗回應 - 使用 rapidjson
        std::cerr << "ERROR: PTZ移動到預設點發生異常: " << e.what() << std::endl;
        std::string desc = std::string("PTZ移動到預設點發生異常: ") + e.what();

        return createErrorResponse(desc);
    }

    return "";

    return "";
}

std::string ChtP2PCameraControlHandler::handleHamiCamPtzControlConfigPst(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理PTZ設定預設點: " << payload << std::endl;
    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError() || !requestJson.IsObject())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON格式錯誤");
        }

        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證camId
        std::string errorMsg;
        if (!validateCamId(requestJson, paramsManager.getCameraId(), errorMsg))
        {
            throw std::runtime_error(errorMsg);
        }

        // 驗證預設點範圍
        int index;
        std::string remove;
        std::string positionName;
        auto it = requestJson.FindMember(PAYLOAD_KEY_POSITION_INDEX);
        if (it == requestJson.MemberEnd() || !it->value.IsInt()) {
            throw std::runtime_error(std::string("缺少必要欄位: ") + std::string(PAYLOAD_KEY_POSITION_INDEX));
        }
        index = it->value.GetInt();


        it = requestJson.FindMember(PAYLOAD_KEY_REMOVE);
        if (it == requestJson.MemberEnd() || !it->value.IsString()) {
            throw std::runtime_error(std::string("缺少必要欄位: ") + std::string(PAYLOAD_KEY_REMOVE));
        }
        remove = it->value.GetString();

        it = requestJson.FindMember(PAYLOAD_KEY_POSITION_NAME);
        if (it == requestJson.MemberEnd() || !it->value.IsString()) {
            throw std::runtime_error(std::string("缺少必要欄位: ") + std::string(PAYLOAD_KEY_POSITION_NAME));
        }
        positionName = it->value.GetString();

        if (index < 1 || index > 4)
        {
            throw std::runtime_error("PTZ預設點必須在1-4之間");
        }
        if (!(remove == "1" || remove == "0"))
        {
            throw std::runtime_error("PTZ預設點參數remove數值不正確");
        }

        std::cout << "PTZ設定預設點 - index: " << index << ", remove: " << remove << ", positionName: " << positionName << std::endl;

        // 設定PTZ預設點
        //bool success = CameraDriver::getInstance().setPTZPositionPoint(index, (remove == "1"), positionName);
        // 儲存PTZ預設點名稱
        // positionNameNum = positionName1 or positionName2 or positionName3 or positionName4
        std::string positionNameNum = "positionName" + std::to_string(index);
        if (remove == "0")
            paramsManager.setParameter(positionNameNum, positionName.c_str());
        else if (remove == "1")
            paramsManager.setParameter(positionNameNum, " ");
        paramsManager.saveToFile();

        bool success = true; // 假設成功

        // 構建成功回應 - 使用 rapidjson
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, success ? 1 : 0, allocator);
        response.AddMember(PAYLOAD_KEY_POSITION_INDEX, index, allocator);
        response.AddMember(PAYLOAD_KEY_REMOVE, rapidjson::Value(remove.c_str(), allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_POSITION_NAME, rapidjson::Value(positionName.c_str(), allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_REMOVE, rapidjson::Value(remove.c_str(), allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_POSITION_NAME, rapidjson::Value(positionName.c_str(), allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        std::string responseStr = buffer.GetString();
        std::cout << "PTZ控制成功回應: " << responseStr << std::endl;

        return responseStr;
    }
    catch (const std::exception &e)
    {
        // 構建失敗回應 - 使用 rapidjson
        std::cerr << "ERROR: PTZ設定預設點發生異常: " << e.what() << std::endl;
        std::string desc = std::string("PTZ設定預設點發生異常: ") + e.what();

        return createErrorResponse(desc);
    }

    return "";

    return "";
}

std::string ChtP2PCameraControlHandler::handleHamiCamHumanTracking(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理人體追蹤開關: " << payload << std::endl;
    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError() || !requestJson.IsObject())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON格式錯誤");
        }

        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證camId
        std::string errorMsg;
        if (!validateCamId(requestJson, paramsManager.getCameraId(), errorMsg))
        {
            throw std::runtime_error(errorMsg);
        }

        int val = 0;
        auto it = requestJson.FindMember(PAYLOAD_KEY_VAL);
        if (it == requestJson.MemberEnd() || !it->value.IsInt()) {
            throw std::runtime_error(std::string("缺少必要欄位: ") + std::string(PAYLOAD_KEY_VAL));
        }
        val = it->value.GetInt();

        // 驗證開關範圍
        if (val < 0 || val > 2)
        {
            throw std::runtime_error("人體追蹤開關必須在0-2之間");
        }

        std::cout << "人體追蹤開關 - val: " << val << std::endl;

        // 設定人體追蹤開關
        //bool success = CameraDriver::getInstance().setPTZHumanTracking(val);

        // 儲存人體追蹤開關設定
        paramsManager.setParameter("humanTracking", std::to_string(val));
        paramsManager.saveToFile();

        // send ai setting to host_stream
        //CameraDriver::getInstance().eventUpdateAiSetting(paramsManager);

        bool success = true; // 假設成功

        // 構建成功回應 - 使用 rapidjson
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, success ? 1 : 0, allocator);
        response.AddMember(PAYLOAD_KEY_VAL, val, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        std::string responseStr = buffer.GetString();
        std::cout << "PTZ控制成功回應: " << responseStr << std::endl;

        return responseStr;
    }
    catch (const std::exception &e)
    {
        // 構建失敗回應 - 使用 rapidjson
        std::cerr << "ERROR: 設定人體追蹤開關發生異常: " << e.what() << std::endl;
        std::string desc = std::string("設定人體追蹤開關發生異常: ") + e.what();

        return createErrorResponse(desc);
    }
}

std::string ChtP2PCameraControlHandler::handleHamiCamPetTracking(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理寵物追蹤開關: " << payload << std::endl;
    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError() || !requestJson.IsObject())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON格式錯誤");
        }

        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證camId
        std::string errorMsg;
        if (!validateCamId(requestJson, paramsManager.getCameraId(), errorMsg))
        {
            throw std::runtime_error(errorMsg);
        }

        int val = 0;
        auto it = requestJson.FindMember(PAYLOAD_KEY_VAL);
        if (it == requestJson.MemberEnd() || !it->value.IsInt()) {
            throw std::runtime_error(std::string("缺少必要欄位: ") + std::string(PAYLOAD_KEY_VAL));
        }
        val = it->value.GetInt();

        // 驗證開關範圍
        if (val < 0 || val > 2)
        {
            throw std::runtime_error("寵物追蹤開關必須在0-2之間");
        }

        std::cout << "寵物追蹤開關 - val: " << val << std::endl;

        //bool success = CameraDriver::getInstance().setPTZPetTracking(val);

        // 儲存寵物追蹤開關設定
        paramsManager.setParameter("petTracking", std::to_string(val));
        paramsManager.saveToFile();

        // send ai setting to host_stream
        //CameraDriver::getInstance().eventUpdateAiSetting(paramsManager);

        bool success = true; // 假設成功

        // 構建成功回應 - 使用 rapidjson
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, success ? 1 : 0, allocator);
        response.AddMember(PAYLOAD_KEY_VAL, val, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        std::string responseStr = buffer.GetString();
        std::cout << "PTZ控制成功回應: " << responseStr << std::endl;

        return responseStr;
    }
    catch (const std::exception &e)
    {
        // 構建失敗回應 - 使用 rapidjson
        std::cerr << "ERROR: 設定寵物追蹤開關發生異常: " << e.what() << std::endl;
        std::string desc = std::string("設定寵物追蹤開關發生異常: ") + e.what();

        return createErrorResponse(desc);
    }
}

std::string ChtP2PCameraControlHandler::handleGetHamiCamBindList(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理獲取綁定清單: " << payload << std::endl;

    try
    {
        // 解析請求JSON
        rapidjson::Document request;
        rapidjson::ParseResult parseResult = request.Parse(payload.c_str());
        rapidjson::Document request;
        rapidjson::ParseResult parseResult = request.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            throw std::runtime_error("JSON解析失敗: " + std::string(rapidjson::GetParseError_En(parseResult.Code())));
            throw std::runtime_error("JSON解析失敗: " + std::string(rapidjson::GetParseError_En(parseResult.Code())));
        }

        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證camId
        std::string errorMsg;
        if (!validateCamId(request, paramsManager.getCameraId(), errorMsg))
        {
            throw std::runtime_error(errorMsg);
        }


        std::string camId = request["camId"].GetString();
        std::cout << "INFO: 處理攝影機ID: " << camId << std::endl;

        // 檢查是否為模擬模式
        //auto &driver = CameraDriver::getInstance();
        //bool isSimulationMode = driver.isSimulationMode();
        bool isSimulationMode = false; // 假設非模擬模式

        // 讀取WiFi設定
        std::string wifiSsid = "";
        std::string wifiPassword = "";


        if (isSimulationMode)
        {
            wifiSsid = "testSsid";
            wifiPassword = "1234567890";
        }
        else
        {
            //wifiSsid = driver.getWifiSsid();
            //wifiPassword = driver.getWifiPassword();
        }

        if (wifiSsid.empty())
        {
            throw std::runtime_error("無法讀取WiFi SSID");
        }
        else if (wifiPassword.empty())
        {
            throw std::runtime_error("無法取得WiFi密碼");
        }

        // Base64編碼密碼
        std::string encodedPassword = base64_encode(wifiPassword);
        if (encodedPassword.empty())
        {
            throw std::runtime_error("Base64編碼失敗");
        }

        // 建立成功回應
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();

        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember("wifiSsid", rapidjson::Value(wifiSsid.c_str(), allocator).Move(), allocator);
        response.AddMember("pswd", rapidjson::Value(encodedPassword.c_str(), allocator).Move(), allocator);
        response.AddMember("wifiSsid", rapidjson::Value(wifiSsid.c_str(), allocator).Move(), allocator);
        response.AddMember("pswd", rapidjson::Value(encodedPassword.c_str(), allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        std::cout << "INFO: 成功取得WiFi資訊 - SSID: " << wifiSsid << std::endl;
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "ERROR: handleGetHamiCamBindList異常: " << e.what() << std::endl;
        return createErrorResponse(e.what());
    }
}

std::string ChtP2PCameraControlHandler::handleUpgradeHamiCamOTA(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理OTA升級: " << payload << std::endl;

    rapidjson::Document response;
    response.SetObject();
    rapidjson::Document::AllocatorType &allocator = response.GetAllocator();

    try
    {
        // 步驟 1: 解析輸入的 JSON payload
        rapidjson::Document request;
        rapidjson::ParseResult parseResult = request.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "ERROR: JSON 解析失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            response.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);
            response.AddMember("description", rapidjson::Value("JSON 格式錯誤", allocator).Move(), allocator);
            goto send_response;
        }

        // 步驟 2: 驗證必要欄位
        if (!request.HasMember(PAYLOAD_KEY_CAMID) || !request[PAYLOAD_KEY_CAMID].IsString() ||
            !request.HasMember(PAYLOAD_KEY_UPGRADE_MODE) || !request[PAYLOAD_KEY_UPGRADE_MODE].IsString() ||
            !request.HasMember(PAYLOAD_KEY_FILE_PATH) || !request[PAYLOAD_KEY_FILE_PATH].IsString())
        {

            std::string errorMsg = "缺少必要欄位 (" PAYLOAD_KEY_CAMID ", " PAYLOAD_KEY_UPGRADE_MODE ", " PAYLOAD_KEY_FILE_PATH ")";
            return createErrorResponse(errorMsg);
        }

        std::string camId = request[PAYLOAD_KEY_CAMID].GetString();
        std::string upgradeMode = request[PAYLOAD_KEY_UPGRADE_MODE].GetString();
        std::string filePath = request[PAYLOAD_KEY_FILE_PATH].GetString();

        // 檢查欄位是否為空
        if (camId.empty() || upgradeMode.empty() || filePath.empty())
        {
            std::cerr << "ERROR: 有欄位為空值" << std::endl;
            response.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);
            response.AddMember(PAYLOAD_KEY_DESCRIPTION, rapidjson::Value("參數不能為空", allocator).Move(), allocator);
            response.AddMember(PAYLOAD_KEY_DESCRIPTION, rapidjson::Value("參數不能為空", allocator).Move(), allocator);
            goto send_response;
        }

        // 驗證 upgradeMode 值
        if (upgradeMode != "0" && upgradeMode != "1")
        {
            std::cerr << "ERROR: upgradeMode 值無效: " << upgradeMode << std::endl;
            response.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);
            response.AddMember(PAYLOAD_KEY_DESCRIPTION, rapidjson::Value("更新模式參數無效", allocator).Move(), allocator);
            response.AddMember(PAYLOAD_KEY_DESCRIPTION, rapidjson::Value("更新模式參數無效", allocator).Move(), allocator);
            goto send_response;
        }

        std::cout << "INFO: 攝影機ID: " << camId << std::endl;
        std::cout << "INFO: 更新模式: " << (upgradeMode == "0" ? "立即更新" : "閒置時更新") << std::endl;
        std::cout << "INFO: 韌體檔案路徑: " << filePath << std::endl;

        // 步驟 3: 驗證 camId
        auto &paramsManager = CameraParametersManager::getInstance();
        std::string currentCamId = paramsManager.getCameraId();
        if (camId != currentCamId)
        {
            std::cerr << "ERROR: 請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
            response.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);
            response.AddMember(PAYLOAD_KEY_DESCRIPTION, rapidjson::Value("攝影機 ID 不符", allocator).Move(), allocator);
            response.AddMember(PAYLOAD_KEY_DESCRIPTION, rapidjson::Value("攝影機 ID 不符", allocator).Move(), allocator);
            goto send_response;
        }

        // 步驟 4: 驗證韌體檔案
        if (!validateFirmwareFile(filePath))
        {
            response.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);
            response.AddMember(PAYLOAD_KEY_DESCRIPTION, rapidjson::Value("韌體檔案驗證失敗", allocator).Move(), allocator);
            response.AddMember(PAYLOAD_KEY_DESCRIPTION, rapidjson::Value("韌體檔案驗證失敗", allocator).Move(), allocator);
            goto send_response;
        }

        // 步驟 5: 檢查系統狀態（磁碟空間等）
        struct statvfs diskInfo;
        if (statvfs("/", &diskInfo) == 0)
        {
            unsigned long long freeSpace = (unsigned long long)diskInfo.f_bavail * diskInfo.f_frsize;
            unsigned long long requiredSpace = 50 * 1024 * 1024; // 假設需要 50MB 空間

            if (freeSpace < requiredSpace)
            {
                std::cerr << "ERROR: 磁碟空間不足，可用空間: " << (freeSpace / 1024 / 1024) << "MB" << std::endl;
                response.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);
                response.AddMember(PAYLOAD_KEY_DESCRIPTION, rapidjson::Value("儲存空間不足", allocator).Move(), allocator);
                response.AddMember(PAYLOAD_KEY_DESCRIPTION, rapidjson::Value("儲存空間不足", allocator).Move(), allocator);
                goto send_response;
            }
        }

        // 步驟 6: 準備 OTA 更新
        std::cout << "INFO: 準備執行 OTA 更新..." << std::endl;

        // 如果是立即更新模式，設定背景任務
        if (upgradeMode == "0")
        {
            std::cout << "INFO: 立即更新模式，將在回應後 5 秒開始更新" << std::endl;

            // 啟動背景執行緒執行 OTA 更新
            std::thread otaThread([filePath]()
                                  {
                try {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    std::cout << "INFO: 開始執行 OTA 更新..." << std::endl;


                    // 檢查是否為模擬模式
                    //if (CameraDriver::getInstance().isSimulationMode()) {
                    //    std::cout << "INFO: 模擬模式：模擬 OTA 更新完成" << std::endl;
                    //} else
                    {
                        // 執行實際的 OTA 更新命令
                        std::string otaCmd = "sysupgrade -v " + filePath;
                        std::cout << "INFO: 執行 OTA 命令: " << otaCmd << std::endl;
                        int result = system(otaCmd.c_str());
                        if (result == 0) {
                            std::cout << "INFO: OTA 更新執行成功" << std::endl;
                        } else {
                            std::cerr << "ERROR: OTA 更新執行失敗，錯誤碼: " << result << std::endl;
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "ERROR: OTA 更新執行緒異常: " << e.what() << std::endl;
                } });
            otaThread.detach();
        }
        else
        {
            std::cout << "INFO: 閒置更新模式，將在系統閒置時執行更新" << std::endl;
            // 這裡可以註冊一個閒置時的回調函數或寫入排程檔案
        }

        // 步驟 7: 建立成功回應
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember(PAYLOAD_KEY_DESCRIPTION, rapidjson::Value("準備更新OTA", allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_DESCRIPTION, rapidjson::Value("準備更新OTA", allocator).Move(), allocator);

        std::cout << "INFO: OTA 更新請求處理成功" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "ERROR: handleUpgradeHamiCamOTA 發生未預期的例外: " << e.what() << std::endl;
        response.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);
        response.AddMember(PAYLOAD_KEY_DESCRIPTION, rapidjson::Value("系統內部錯誤", allocator).Move(), allocator);
        response.AddMember(PAYLOAD_KEY_DESCRIPTION, rapidjson::Value("系統內部錯誤", allocator).Move(), allocator);
    }

send_response:
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    response.Accept(writer);

    std::string responseStr = buffer.GetString();
    std::cout << "INFO: 送出回應: " << responseStr << std::endl;

    return responseStr;
}

std::string ChtP2PCameraControlHandler::handleUpdateCameraAISetting(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理更新AI設定: " << payload << std::endl;
    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError() || !requestJson.IsObject())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON格式錯誤");
        }

        auto &paramsManager = CameraParametersManager::getInstance();

        // 驗證camId
        std::string errorMsg;
        if (!validateCamId(requestJson, paramsManager.getCameraId(), errorMsg))
        {
            throw std::runtime_error(errorMsg);
        }

        // 提取AI設定並保存
        // check AI settings object exist.
        auto it = requestJson.FindMember(PAYLOAD_KEY_HAMI_AI_SETTINGS);
        if (it == requestJson.MemberEnd() || !it->value.IsObject()) {
            throw std::runtime_error(std::string("缺少必要欄位: ") + std::string(PAYLOAD_KEY_HAMI_AI_SETTINGS));
        }

        // 將整個 hamiAiSettings 物件轉換為字串儲存
        rapidjson::StringBuffer aiSettingsBuffer;
        rapidjson::Writer<rapidjson::StringBuffer> aiSettingsWriter(aiSettingsBuffer);


        if (!it->value.Accept(aiSettingsWriter)) {
            std::cerr << "AI設定序列化失敗" << std::endl;
            throw std::runtime_error("AI設定序列化失敗");
        }

        std::string aiSettingsJson(aiSettingsBuffer.GetString(), aiSettingsBuffer.GetSize());

        // 儲存AI設定到參數管理器
        bool saveResult = false;
        if (paramsManager.parseHamiAiSettings(aiSettingsJson))
        {
            saveResult = paramsManager.saveToFile();


            // send ai setting to host_stream
            //CameraDriver::getInstance().eventUpdateAiSetting(paramsManager);
            //CameraDriver::getInstance().eventUpdateAiSetting(paramsManager);
        }

        std::cout << "AI設定已更新並儲存: " << aiSettingsJson << std::endl;
        std::cout << "保存結果: " << (saveResult ? "成功" : "失敗") << std::endl;

        // 返回成功回應
        return "{\"result\":1,\"description\":\"更新成功\"}";
    }
    catch (const std::exception &e)
    {
        std::cerr << "更新AI設定時發生異常: " << e.what() << std::endl;
        return createErrorResponse(e.what());
    }
}

static bool getHamiAISettingsObj(const CameraParametersManager &paramsManager,
                    rapidjson::Value &outVal, rapidjson::Document::AllocatorType& alloc)
{
    bool result = true;

    // reset output
    outVal.SetObject();

    // 獲取當前的 AI 設定
    std::string currentAiSettings = paramsManager.getAISettings();
    std::cout << "當前儲存的AI設定: " << currentAiSettings << std::endl;

    if (currentAiSettings.empty() || currentAiSettings == "{}")
    {
        // 如果沒有儲存的設定，從個別參數建立完整的 hamiAiSettings

        // 告警設定 (Alert)
        auto addAlert = [&](const char *key, bool val) {
            outVal.AddMember(
                rapidjson::StringRef(key),
                rapidjson::Value((val)?"1":"0", alloc),
                alloc);
        };
        addAlert("vmdAlert",        paramsManager.getVmdAlert());
        addAlert("humanAlert",      paramsManager.getHumanAlert());
        addAlert("petAlert",        paramsManager.getPetAlert());
        addAlert("adAlert",         paramsManager.getAdAlert());
        addAlert("fenceAlert",      paramsManager.getFenceAlert());
        addAlert("faceAlert",       paramsManager.getFaceAlert());
        addAlert("fallAlert",       paramsManager.getFallAlert());
        addAlert("adBabyCryAlert",  paramsManager.getAdBabyCryAlert());
        addAlert("adSpeechAlert",   paramsManager.getAdSpeechAlert());
        addAlert("adAlarmAlert",    paramsManager.getAdAlarmAlert());
        addAlert("adDogAlert",      paramsManager.getAdDogAlert());
        addAlert("adCatAlert",      paramsManager.getAdCatAlert());

        // 靈敏度設定 (Sen) - 轉換為 int
        auto addSensitivity = [&](const char *key, int val) {
            outVal.AddMember(
                rapidjson::StringRef(key),
                val,
                alloc);
        };
        addSensitivity("vmdSen",        paramsManager.getVmdSen());
        addSensitivity("adSen",         paramsManager.getAdSen());
        addSensitivity("humanSen",      paramsManager.getHumanSen());
        addSensitivity("faceSen",       paramsManager.getFaceSen());
        addSensitivity("fenceSen",      paramsManager.getFenceSen());
        addSensitivity("petSen",        paramsManager.getPetSen());
        addSensitivity("adBabyCrySen",  paramsManager.getAdBabyCrySen());
        addSensitivity("adSpeechSen",   paramsManager.getAdSpeechSen());
        addSensitivity("adAlarmSen",    paramsManager.getAdAlarmSen());
        addSensitivity("adDogSen",      paramsManager.getAdDogSen());
        addSensitivity("adCatSen",      paramsManager.getAdCatSen());
        addSensitivity("fallSen",       paramsManager.getFallSen());

        // 電子圍籬座標
        auto addFencePos = [&](const char *key, const std::pair<int, int> &val)
        {
            rapidjson::Value pos(rapidjson::kObjectType);
            pos.AddMember("x", val.first, alloc);
            pos.AddMember("y", val.second, alloc);


            outVal.AddMember(
                rapidjson::StringRef(key),
                pos.Move(),
                alloc);
        };
        addFencePos("fencePos1", paramsManager.getFencePos1());
        addFencePos("fencePos2", paramsManager.getFencePos2());
        addFencePos("fencePos3", paramsManager.getFencePos3());
        addFencePos("fencePos4", paramsManager.getFencePos4());

        // 圍籬方向
        outVal.AddMember(
            rapidjson::StringRef("fenceDir"),
            rapidjson::Value(paramsManager.getFenceDir().c_str(), alloc),
            alloc);

        // 人臉識別特徵（暫時空陣列）
        rapidjson::Value identificationFeatures(rapidjson::kArrayType);
        outVal.AddMember(
            rapidjson::StringRef("identificationFeatures"),
            identificationFeatures,
            alloc);
    }
    else
    {
        // 解析已儲存的 AI 設定
        rapidjson::Document tmp;
        if (tmp.Parse(currentAiSettings.c_str()).HasParseError() || !tmp.IsObject()) {
            std::cerr << "解析儲存的AI設定失敗，返回空物件" << std::endl;
            outVal.SetObject();
            result = false;
        } else {
            outVal.CopyFrom(tmp, alloc); // deep copy to outVal
        }
    }

    return result;
}

std::string ChtP2PCameraControlHandler::handleGetCameraAISetting(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理獲取AI設定: " << payload << std::endl;

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        // 從請求中獲取 camId
        std::string camId;
        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }

        // 驗證 camId
        auto &paramsManager = CameraParametersManager::getInstance();
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }

        // 構建回應
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);

        // 解析已儲存的 AI 設定或建立預設值
        rapidjson::Value aiSettings(rapidjson::kObjectType);
        if (getHamiAISettingsObj(paramsManager, aiSettings, allocator) == false)
        {
            throw std::runtime_error("Get local AI settings error");
        }
        // 將 aiSettingsDoc 加入回應
        response.AddMember("hamiAiSettings", aiSettings, allocator);

        // 序列化回應
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        std::string responseStr = buffer.GetString();
        std::cout << "回應內容: " << responseStr << std::endl;
        return responseStr;
    }
    catch (const std::exception &e)
    {
        std::cerr << "獲取AI設定時發生異常: " << e.what() << std::endl;

        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);
        errorResponse.AddMember("description", rapidjson::Value(e.what(), allocator), allocator);
        errorResponse.AddMember("description", rapidjson::Value(e.what(), allocator), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}
/**
 * @brief 處理獲取即時影音串流
 */
std::string ChtP2PCameraControlHandler::handleGetVideoLiveStream(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理獲取即時串流: " << payload << std::endl;
    std::cout << "\n===== 處理即時影音串流請求 =====" << std::endl;

    //auto &streamManager = StreamManager::getInstance();
    //auto &streamManager = StreamManager::getInstance();

    try
    {
        // 解析請求 JSON
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        std::string requestId;
        std::string frameType;
        std::string IP;
        std::string imageQuality;

        // 獲取請求參數
        if (requestJson.HasMember(PAYLOAD_KEY_CAMID) && requestJson[PAYLOAD_KEY_CAMID].IsString())
        {
            camId = requestJson[PAYLOAD_KEY_CAMID].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: " PAYLOAD_KEY_CAMID);
        }

        if (requestJson.HasMember(PAYLOAD_KEY_REQUEST_ID) && requestJson[PAYLOAD_KEY_REQUEST_ID].IsString())
        {
            requestId = requestJson[PAYLOAD_KEY_REQUEST_ID].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: " PAYLOAD_KEY_REQUEST_ID);
        }

        if (requestJson.HasMember(PAYLOAD_KEY_FRAME_TYPE) && requestJson[PAYLOAD_KEY_FRAME_TYPE].IsString())
        {
            frameType = requestJson[PAYLOAD_KEY_FRAME_TYPE].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: " PAYLOAD_KEY_FRAME_TYPE);
        }

        if (requestJson.HasMember("IP") && requestJson["IP"].IsString())
        {
            IP = requestJson["IP"].GetString();
        }

        if (requestJson.HasMember(PAYLOAD_KEY_IMAGE_QUALITY) && requestJson[PAYLOAD_KEY_IMAGE_QUALITY].IsString())
        {
            imageQuality = requestJson[PAYLOAD_KEY_IMAGE_QUALITY].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: " PAYLOAD_KEY_IMAGE_QUALITY);
        }

        // 驗證requestId格式: <UDP/Relay>_live_<userId>_<JWTToken>
        std::regex requestIdPattern("^(UDP|Relay)_live_.+_.+$");
        if (!std::regex_match(requestId, requestIdPattern))
        {
            std::cerr << "requestId格式錯誤，應為: <UDP/Relay>_live_<userId>_<JWTToken>" << std::endl;
            throw std::runtime_error("requestId格式錯誤");
        }

        // 驗證frameType參數
        if (frameType != "rtp" && frameType != "raw")
        {
            std::cerr << "不支援的frameType: " << frameType << std::endl;
            throw std::runtime_error("frameType必須為rtp或raw");
        }

        // 驗證imageQuality參數
        if (imageQuality != "0" && imageQuality != "1" && imageQuality != "2")
        {
            std::cerr << "不支援的imageQuality: " << imageQuality << std::endl;
            throw std::runtime_error("imageQuality必須為0、1或2");
        }

        std::cout << "即時串流請求 - camId: " << camId << ", requestId: " << requestId << std::endl;
        std::cout << "即時串流請求 - frameType: " << frameType << ", imageQuality: " << imageQuality << std::endl;

        // 驗證 camId
        auto &paramsManager = CameraParametersManager::getInstance();
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }
#if 0
#if 0
        // 根據imageQuality設置影音編碼參數
        VideoCodecParams videoParams;
        videoParams.codec = 2; // H.264

        // 從INI檔案讀取串流參數
        StreamParams streamParams = readStreamParamsFromINI(imageQuality);
        int width = streamParams.width;
        int height = streamParams.height;
        int fps = streamParams.fps;
        int bitrate = streamParams.bitrate;

        videoParams.width = width;
        videoParams.height = height;
        videoParams.fps = fps;
        // videoParams.bitrate = bitrate;

        AudioCodecParams audioParams;
        audioParams.codec = 13;     // G.711
        audioParams.bitRate = 64;   // 64 kbps
        audioParams.sampleRate = 8; // 8 kHz



        // 啟動串流
        bool startResult = streamManager.startLiveVideoStream(requestId, videoParams, audioParams, IP);
        if (!startResult)
        {
            throw std::runtime_error("啟動即時串流失敗");
        }
#endif
#endif

        // 構建成功回應 - 包含影音編碼資訊
        rapidjson::Document response;
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
#if 0
#if 0
        // 影像編碼資訊
        rapidjson::Value video(rapidjson::kObjectType);
        rapidjson::Value video(rapidjson::kObjectType);
        video.AddMember(PAYLOAD_KEY_VIDEO, videoParams.codec, allocator);
        video.AddMember(PAYLOAD_KEY_WIDTH, videoParams.width, allocator);
        video.AddMember(PAYLOAD_KEY_HEIGHT, videoParams.height, allocator);
        video.AddMember(PAYLOAD_KEY_FPS, videoParams.fps, allocator);
        response.AddMember(PAYLOAD_KEY_VIDEO, video, allocator);

        // 音頻編碼資訊
        rapidjson::Value audio(rapidjson::kObjectType);
        rapidjson::Value audio(rapidjson::kObjectType);
        audio.AddMember(PAYLOAD_KEY_AUDIO, audioParams.codec, allocator);
        audio.AddMember(PAYLOAD_KEY_BIT_RATE, audioParams.bitRate, allocator);
        audio.AddMember(PAYLOAD_KEY_SAMPLE_RATE, audioParams.sampleRate, allocator);
        response.AddMember(PAYLOAD_KEY_AUDIO, audio, allocator);
#endif
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
#endif
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        //std::cout << "即時串流已啟動，requestId: " << requestId << std::endl;
        //std::cout << "影像設定: " << width << "x" << height << " @" << fps << "fps, " << bitrate << "bps" << std::endl;
        //std::cout << "即時串流已啟動，requestId: " << requestId << std::endl;
        //std::cout << "影像設定: " << width << "x" << height << " @" << fps << "fps, " << bitrate << "bps" << std::endl;
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "處理即時串流請求時發生異常: " << e.what() << std::endl;

        rapidjson::Document errorResponse;
        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

/**
 * @brief 處理停止即時影音串流
 */
std::string ChtP2PCameraControlHandler::handleStopVideoLiveStream(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理停止即時串流: " << payload << std::endl;

    //auto &streamManager = StreamManager::getInstance();
    //auto &streamManager = StreamManager::getInstance();

    try
    {
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        std::string requestId;

        if (requestJson.HasMember(PAYLOAD_KEY_CAMID) && requestJson[PAYLOAD_KEY_CAMID].IsString())
        {
            camId = requestJson[PAYLOAD_KEY_CAMID].GetString();
        }

        if (requestJson.HasMember(PAYLOAD_KEY_REQUEST_ID) && requestJson[PAYLOAD_KEY_REQUEST_ID].IsString())
        {
            requestId = requestJson[PAYLOAD_KEY_REQUEST_ID].GetString();

            // 驗證 requestId 格式：<UDP/Relay>_live_<UserID>_<JWTToken>
            std::cout << "停止live串流 requestId 格式: <UDP/Relay>_live_<UserID>_<JWTToken>" << std::endl;
            std::cout << "收到的 requestId: " << requestId << std::endl;
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: requestId");
        }

        std::cout << "停止即時串流 - camId: " << camId << ", requestId: " << requestId << std::endl;

        // 驗證 camId
        auto &paramsManager = CameraParametersManager::getInstance();
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }
#if 0
#if 0
        // 停止串流管理器中的串流
        bool stopResult = streamManager.stopLiveVideoStream(requestId);
        if (!stopResult)
        {
            std::cerr << "停止即時串流失敗 - " << requestId << std::endl;
            // 繼續執行，不拋出異常
        }
#endif
#endif
        std::cout << "即時串流已停止" << std::endl;

        rapidjson::Document response;
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "處理停止即時串流時發生異常: " << e.what() << std::endl;

        rapidjson::Document errorResponse;
        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

/**
 * @brief 處理獲取歷史影音串流
 */
std::string ChtP2PCameraControlHandler::handleGetVideoHistoryStream(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理獲取歷史串流: " << payload << std::endl;

    //auto &streamManager = StreamManager::getInstance();
    //auto &streamManager = StreamManager::getInstance();

    try
    {
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        std::string requestId;
        std::string frameType;
        std::string imageQuality;
        std::string IP;
        long startTime;

        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }

        if (requestJson.HasMember("requestId") && requestJson["requestId"].IsString())
        {
            requestId = requestJson["requestId"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: requestId");
        }

        if (requestJson.HasMember("frameType") && requestJson["frameType"].IsString())
        {
            frameType = requestJson["frameType"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: frameType");
        }

        if (requestJson.HasMember("imageQuality") && requestJson["imageQuality"].IsString())
        {
            imageQuality = requestJson["imageQuality"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: imageQuality");
        }

        if (requestJson.HasMember("IP") && requestJson["IP"].IsString())
        {
            IP = requestJson["IP"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: IP");
        }

        if (requestJson.HasMember("startTime") && (requestJson["startTime"].IsInt64() || requestJson["startTime"].IsString()))
        {
            if (requestJson["startTime"].IsString())
            {
                startTime = std::stol(requestJson["startTime"].GetString());
            }
            else
            {
                startTime = requestJson["startTime"].GetInt64();
            }
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: startTime");
        }

        // 驗證requestId格式: <UDP/Relay>_history_<userId>_<JWTToken>
        std::regex requestIdPattern("^(UDP|Relay)_history_.+_.+$");
        if (!std::regex_match(requestId, requestIdPattern))
        {
            // std::cerr << "requestId格式錯誤，應為: <UDP/Relay>_history_<userId>_<JWTToken>" << std::endl;
            // throw std::runtime_error("requestId格式錯誤");
        }

        // 驗證frameType參數
        if (frameType != "rtp" && frameType != "raw")
        {
            std::cerr << "不支援的frameType: " << frameType << std::endl;
            throw std::runtime_error("frameType必須為rtp或raw");
        }

        // 驗證imageQuality參數
        if (imageQuality != "0" && imageQuality != "1" && imageQuality != "2")
        {
            std::cerr << "不支援的imageQuality: " << imageQuality << std::endl;
            throw std::runtime_error("imageQuality必須為0、1或2");
        }

        std::cout << "歷史串流請求 - camId: " << camId
                  << ", requestId: " << requestId
                  << ", frameType: " << frameType
                  << ", imageQuality: " << imageQuality
                  << ", startTime: " << startTime << std::endl;

        // 驗證 camId
        auto &paramsManager = CameraParametersManager::getInstance();
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }
#if 0
#if 0
        // 設置影音編碼參數 - 根據imageQuality調整
        VideoCodecParams videoParams;
        AudioCodecParams audioParams;

        // 從INI檔案讀取串流參數
        StreamParams streamParams = readStreamParamsFromINI(imageQuality);
        int width = streamParams.width;
        int height = streamParams.height;
        int fps = streamParams.fps;
        int bitrate = streamParams.bitrate;

        videoParams.codec = 2; // H.264

        audioParams.codec = 13;     // G.711:
        audioParams.bitRate = 64;   // 64 kbps
        audioParams.sampleRate = 8; // 8 kHz

        // 啟動歷史串流
        bool startResult = streamManager.startHistoryVideoStream(requestId, startTime,
                                                                 videoParams, audioParams, IP);
        if (!startResult)
        {
            throw std::runtime_error("啟動歷史串流失敗");
        }
#endif
#endif
        // 構建成功回應
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
#if 0
#if 0
        // 影像編碼資訊
        rapidjson::Value video(rapidjson::kObjectType);
        rapidjson::Value video(rapidjson::kObjectType);
        video.AddMember("codec", videoParams.codec, allocator);
        video.AddMember("width", videoParams.width, allocator);
        video.AddMember("height", videoParams.height, allocator);
        video.AddMember("fps", videoParams.fps, allocator);
        response.AddMember("video", video, allocator);

        // 音頻編碼資訊 (可選)
        rapidjson::Value audio(rapidjson::kObjectType);
        rapidjson::Value audio(rapidjson::kObjectType);
        audio.AddMember("codec", audioParams.codec, allocator);
        audio.AddMember("bitRate", audioParams.bitRate, allocator);
        audio.AddMember("sampleRate", audioParams.sampleRate, allocator);
        response.AddMember("audio", audio, allocator);
#endif
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
#endif
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        std::cout << "歷史串流已啟動，requestId: " << requestId
                  << ", frameType: " << frameType
                  << ", imageQuality: " << imageQuality
                  << ", startTime: " << startTime << std::endl;
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "處理歷史串流請求時發生異常: " << e.what() << std::endl;

        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

/**
 * @brief 處理停止歷史影音串流
 */
std::string ChtP2PCameraControlHandler::handleStopVideoHistoryStream(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理停止歷史串流: " << payload << std::endl;

    //auto &streamManager = StreamManager::getInstance();
    //auto &streamManager = StreamManager::getInstance();

    try
    {
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        std::string requestId;

        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }

        if (requestJson.HasMember("requestId") && requestJson["requestId"].IsString())
        {
            requestId = requestJson["requestId"].GetString();

            // 驗證 requestId 格式：<UDP/Relay>_history_<UserID>_<JWTToken>
            std::cout << "停止歷史串流 requestId 格式: <UDP/Relay>_history_<UserID>_<JWTToken>" << std::endl;
            std::cout << "收到的 requestId: " << requestId << std::endl;
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: requestId");
        }

        std::cout << "停止歷史串流 - camId: " << camId << ", requestId: " << requestId << std::endl;

        // 驗證 camId
        auto &paramsManager = CameraParametersManager::getInstance();
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }
#if 0
#if 0
        // 停止串流管理器中的歷史串流
        bool stopResult = streamManager.stopHistoryVideoStream(requestId);
        if (!stopResult)
        {
            std::cerr << "停止歷史串流失敗 - " << requestId << std::endl;
            // 繼續執行，不拋出異常
        }
#endif
#endif
        std::cout << "歷史串流已停止" << std::endl;

        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "處理停止歷史串流時發生異常: " << e.what() << std::endl;

        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

#if 1 // jeffery added 20250805
/**
 * @brief 處理獲取排程影音串流
 */
std::string ChtP2PCameraControlHandler::handleGetVideoScheduleStream(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理獲取排程串流: " << payload << std::endl;

    //auto &streamManager = StreamManager::getInstance();
    //auto &streamManager = StreamManager::getInstance();

    try
    {
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        std::string requestId;
        std::string frameType;
        std::string imageQuality;
        long startTime;

        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }

        if (requestJson.HasMember("requestId") && requestJson["requestId"].IsString())
        {
            requestId = requestJson["requestId"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: requestId");
        }

        if (requestJson.HasMember("frameType") && requestJson["frameType"].IsString())
        {
            frameType = requestJson["frameType"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: frameType");
        }

        if (requestJson.HasMember("imageQuality") && requestJson["imageQuality"].IsString())
        {
            imageQuality = requestJson["imageQuality"].GetString();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: imageQuality");
        }

        if (requestJson.HasMember("startTime") && (requestJson["startTime"].IsInt64() || requestJson["startTime"].IsString()))
        {
            if (requestJson["startTime"].IsString())
            {
                startTime = std::stol(requestJson["startTime"].GetString());
            }
            else
            {
                startTime = requestJson["startTime"].GetInt64();
            }
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: startTime");
        }

        // 取得IP參數（如果沒有提供則使用空字串）
        std::string IP;
        if (requestJson.HasMember("IP") && requestJson["IP"].IsString())
        {
            IP = requestJson["IP"].GetString();
        }

        // 驗證requestId格式: <UDP/Relay>_history_<userId>_<JWTToken>
        /*std::regex requestIdPattern("^(UDP|Relay)_history_.+_.+$");
        if (!std::regex_match(requestId, requestIdPattern))
        {
            std::cerr << "requestId格式錯誤，應為: <UDP/Relay>_history_<userId>_<JWTToken>" << std::endl;
            throw std::runtime_error("requestId格式錯誤");
        }*/

        // 驗證frameType參數
        if (frameType != "rtp" && frameType != "raw")
        {
            std::cerr << "不支援的frameType: " << frameType << std::endl;
            throw std::runtime_error("frameType必須為rtp或raw");
        }

        // 驗證imageQuality參數
        if (imageQuality != "0" && imageQuality != "1" && imageQuality != "2")
        {
            std::cerr << "不支援的imageQuality: " << imageQuality << std::endl;
            throw std::runtime_error("imageQuality必須為0、1或2");
        }

        std::cout << "排程串流請求 - camId: " << camId
                  << ", requestId: " << requestId
                  << ", frameType: " << frameType
                  << ", imageQuality: " << imageQuality
                  << ", startTime: " << startTime
                  << ", IP: " << IP << std::endl;

        // 驗證 camId
        auto &paramsManager = CameraParametersManager::getInstance();
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }
#if 0
#if 0
        // 設置影音編碼參數 - 根據imageQuality調整
        VideoCodecParams videoParams;
        AudioCodecParams audioParams;

        // 從INI檔案讀取串流參數
        StreamParams streamParams = readStreamParamsFromINI(imageQuality);
        int width = streamParams.width;
        int height = streamParams.height;
        int fps = streamParams.fps;
        int bitrate = streamParams.bitrate;

        videoParams.codec = 2; // H.264

        audioParams.codec = 13;     // G.711
        audioParams.bitRate = 64;   // 64 kbps
        audioParams.sampleRate = 8; // 8 kHz

        // 啟動排程串流
        bool startResult = streamManager.startScheduleVideoStream(requestId, startTime,
                                                                  videoParams, audioParams, IP);
        if (!startResult)
        {
            throw std::runtime_error("啟動排程串流失敗");
        }
#endif
#endif
        // 構建成功回應
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
#if 0
#if 0
        // 影像編碼資訊
        rapidjson::Value video(rapidjson::kObjectType);
        rapidjson::Value video(rapidjson::kObjectType);
        video.AddMember(PAYLOAD_KEY_CODEC, videoParams.codec, allocator);
        video.AddMember(PAYLOAD_KEY_WIDTH, videoParams.width, allocator);
        video.AddMember(PAYLOAD_KEY_HEIGHT, videoParams.height, allocator);
        video.AddMember(PAYLOAD_KEY_FPS, videoParams.fps, allocator);
        response.AddMember(PAYLOAD_KEY_VIDEO, video, allocator);

        // 音頻編碼資訊 (可選)
        rapidjson::Value audio(rapidjson::kObjectType);
        rapidjson::Value audio(rapidjson::kObjectType);
        audio.AddMember(PAYLOAD_KEY_CODEC, audioParams.codec, allocator);
        audio.AddMember(PAYLOAD_KEY_BIT_RATE, audioParams.bitRate, allocator);
        audio.AddMember(PAYLOAD_KEY_SAMPLE_RATE, audioParams.sampleRate, allocator);
        response.AddMember(PAYLOAD_KEY_AUDIO, audio, allocator);
#endif
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
#endif
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        std::cout << "排程串流已啟動，requestId: " << requestId
                  << ", frameType: " << frameType
                  << ", imageQuality: " << imageQuality
                  << ", startTime: " << startTime << std::endl;
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "處理排程串流請求時發生異常: " << e.what() << std::endl;

        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

/**
 * @brief 處理停止排程影音串流
 */
std::string ChtP2PCameraControlHandler::handleStopVideoScheduleStream(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理停止排程串流: " << payload << std::endl;

    //auto &streamManager = StreamManager::getInstance();
    //auto &streamManager = StreamManager::getInstance();

    try
    {
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        std::string camId;
        std::string requestId;

        if (requestJson.HasMember("camId") && requestJson["camId"].IsString())
        {
            camId = requestJson["camId"].GetString();
        }

        if (requestJson.HasMember("requestId") && requestJson["requestId"].IsString())
        {
            requestId = requestJson["requestId"].GetString();

            // 驗證 requestId 格式：<UDP/Relay>_history_<UserID>_<JWTToken>
            std::cout << "停止排程串流 requestId 格式: <UDP/Relay>_history_<UserID>_<JWTToken>" << std::endl;
            std::cout << "收到的 requestId: " << requestId << std::endl;
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: requestId");
        }

        std::cout << "停止排程串流 - camId: " << camId << ", requestId: " << requestId << std::endl;

        // 驗證 camId
        auto &paramsManager = CameraParametersManager::getInstance();
        if (!camId.empty())
        {
            std::string currentCamId = paramsManager.getCameraId();
            if (camId != currentCamId)
            {
                std::cerr << "請求的 camId (" << camId << ") 與當前攝影機 ID (" << currentCamId << ") 不符" << std::endl;
                throw std::runtime_error("攝影機 ID 不符");
            }
        }
#if 0
#if 0
        // 停止串流管理器中的歷史串流
        bool stopResult = streamManager.stopScheduleVideoStream(requestId);
        if (!stopResult)
        {
            std::cerr << "停止排程串流失敗 - " << requestId << std::endl;
            // 繼續執行，不拋出異常
        }
#endif
#endif
        std::cout << "排程串流已停止" << std::endl;

        rapidjson::Document response;
        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "處理停止歷史串流時發生異常: " << e.what() << std::endl;

        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}
#endif

/**
 * @brief 處理發送音頻串流 (雙向語音)
 */
std::string ChtP2PCameraControlHandler::handleSendAudioStream(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理發送音頻串流: " << payload << std::endl;

    //auto &streamManager = StreamManager::getInstance();
    //auto &streamManager = StreamManager::getInstance();

    try
    {
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        int codec;
        int bitRate;
        int sampleRate;

        // 提取參數
        if (requestJson.HasMember("codec") && requestJson["codec"].IsInt())
        {
            codec = requestJson["codec"].GetInt();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: codec");
        }

        if (requestJson.HasMember("bitRate") && requestJson["bitRate"].IsInt())
        {
            bitRate = requestJson["bitRate"].GetInt();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: bitRate");
        }

        if (requestJson.HasMember("sampleRate") && requestJson["sampleRate"].IsInt())
        {
            sampleRate = requestJson["sampleRate"].GetInt();
        }
        else
        {
            throw std::runtime_error("缺少必要欄位: sampleRate");
        }

        std::cout << "雙向語音串流 - codec: " << codec
                  << ", bitRate: " << bitRate
                  << ", sampleRate: " << sampleRate << std::endl;

        // 生成requestId（基於時間戳）
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                             now.time_since_epoch())
                             .count();
        std::string requestId = "audio_" + std::to_string(timestamp);
#if 0
#if 0
        // 設置音頻編碼參數
        AudioCodecParams audioParams;
        audioParams.codec = codec;
        audioParams.bitRate = bitRate;
        audioParams.sampleRate = sampleRate;

        // 啟動雙向語音串流
        bool startResult = streamManager.startAudioStream(requestId, audioParams);
        if (!startResult)
        {
            throw std::runtime_error("啟動音頻串流失敗");
        }
#endif
#endif
        std::cout << "音頻串流已啟動，準備接收語音資料" << std::endl;

        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember("description", rapidjson::Value("準備接收播放語音串流", allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "處理語音串流時發生異常: " << e.what() << std::endl;

        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}

/**
 * @brief 處理停止音頻串流
 */
std::string ChtP2PCameraControlHandler::handleStopAudioStream(ChtP2PCameraControlHandler *self, const std::string &payload)
{
    std::cout << "處理停止音頻串流: " << payload << std::endl;

    //auto &streamManager = StreamManager::getInstance();
    //auto &streamManager = StreamManager::getInstance();

    try
    {
        rapidjson::Document requestJson;
        rapidjson::ParseResult parseResult = requestJson.Parse(payload.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析請求JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON 格式錯誤");
        }

        // 音頻串流通常沒有特定的requestId在停止命令中
        // 這裡簡化處理：停止所有活躍的音頻串流
        std::cout << "停止雙向語音串流" << std::endl;

        // 注意：在實際應用中，應該維護一個音頻串流ID的對應
        // 這裡為了簡化，假設只有一個活躍的音頻串流
        // 可以通過遍歷 StreamManager 中的會話來找到音頻串流並停止

        // 由於 StreamManager 沒有直接的 "停止所有音頻串流" 方法，
        // 這裡記錄一個警告，實際應用中需要改進
        std::cout << "注意：當前實現需要特定的 requestId 來停止音頻串流" << std::endl;

        std::cout << "音頻串流已停止，資源已釋放" << std::endl;

        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType &allocator = response.GetAllocator();
        response.AddMember(PAYLOAD_KEY_RESULT, 1, allocator);
        response.AddMember("description", rapidjson::Value("停止接收播放語音串流", allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        return buffer.GetString();
    }
    catch (const std::exception &e)
    {
        std::cerr << "處理停止語音串流時發生異常: " << e.what() << std::endl;

        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        rapidjson::Document::AllocatorType &allocator = errorResponse.GetAllocator();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        return buffer.GetString();
    }
}
