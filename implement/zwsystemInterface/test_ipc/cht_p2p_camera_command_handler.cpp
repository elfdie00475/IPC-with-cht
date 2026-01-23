/**
 * @file cht_p2p_camera_api.cpp
 * @brief CHT P2P Camera API實現
 * @date 2025/04/29
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "zwsystem_ipc_client.h"

#include "cht_p2p_camera_command_handler.h"
#include "cht_p2p_agent_payload_defined.h"
#include "camera_parameters_manager.h"

// 內部輔助函數 - 格式化時間戳
static std::string getFormattedTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// 內部輔助函數 - 調試輸出
static void printApiDebug(const std::string &message)
{
    std::cout << "[API-DEBUG " << getFormattedTimestamp() << "] " << message << std::endl;
    std::cout.flush();
}

// 內部輔助函數 - 步驟標題輸出
static void printApiStepHeader(const std::string &step)
{
    std::cout << "\n===== API: " << step << " =====" << std::endl;
    std::cout.flush();
}

// 內部輔助函數 - 驗證JSON回應格式
static bool validateJsonResponse(const std::string &response, std::string &errorMsg)
{
    try
    {
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (parseResult.IsError())
        {
            errorMsg = "JSON解析失敗: " + std::string(GetParseError_En(parseResult.Code()));
            return false;
        }

        if (!responseJson.HasMember("code") && !responseJson.HasMember(PAYLOAD_KEY_RESULT))
        {
            errorMsg = "回應格式錯誤：缺少code或result欄位";
            return false;
        }

        return true;
    }
    catch (const std::exception &e)
    {
        errorMsg = "驗證回應時發生異常: " + std::string(e.what());
        return false;
    }
}

// 內部輔助函數 - 處理初始化資訊錯誤
static void handleInitialInfoError(const std::string &errorMsg)
{
    std::cerr << "處理初始化資訊失敗: " << errorMsg << std::endl;

    // 記錄錯誤到參數管理器
    auto &paramsManager = CameraParametersManager::getInstance();
    paramsManager.addDebugLog("InitialInfo處理錯誤: " + errorMsg, true);

    // 設置錯誤狀態
    paramsManager.setParameter("lastInitError", errorMsg);
    paramsManager.setParameter("lastInitTime", std::to_string(std::time(nullptr)));
}

ChtP2PCameraCommandHandler &ChtP2PCameraCommandHandler::getInstance()
{
    static ChtP2PCameraCommandHandler instance;

    return instance;
}

ChtP2PCameraCommandHandler::ChtP2PCameraCommandHandler()
: m_initialized{false}
{
    initialize();
}

ChtP2PCameraCommandHandler::~ChtP2PCameraCommandHandler()
{
    deinitialize();
}

bool ChtP2PCameraCommandHandler::initialize(void)
{
    if (m_initialized) return true;

    m_initialized = true;
    return true;
}

void ChtP2PCameraCommandHandler::deinitialize()
{
    if (!m_initialized)
    {
        return;
    }

    m_initialized = false;
}

/*
規格要求：
    攝影機報到成功後從回應中提取publicIp
    後續的checkHiOSSstatus將使用正確的publicIp
    保持原有的錯誤處理和日誌輸出
*/
int ChtP2PCameraCommandHandler::bindCamera(const BindCameraConfig& config)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return -1;
    }

    // change wifi first ==> system server
    uint32_t ssid_size = config.wifiSsid.size();
    uint32_t password_size = config.wifiPassword.size();
    if (ssid_size >= ZWSYSTEM_IPC_STRING_SIZE || password_size >= ZWSYSTEM_IPC_STRING_SIZE)
    {
        return -2;
    }

    stChangeWifiReq wifiReq;
    stChangeWifiRep wifiRep;

    snprintf(wifiReq.wifiSsid, ZWSYSTEM_IPC_STRING_SIZE, "%s", config.wifiSsid.c_str());
    snprintf(wifiReq.password, ZWSYSTEM_IPC_STRING_SIZE, "%s", config.wifiPassword.c_str());
    int rc = zwsystem_ipc_changeWifi(wifiReq, &wifiRep);
    if (rc < 0 || wifiRep.code < 0)
    {
        return -3;
    }

    std::string repWifiSsid = std::string(wifiRep.wifiSsid);
    if (config.wifiSsid != repWifiSsid)
    {
        return -4;
    }
    int wifiDdm = wifiRep.wifiDbm;

    // if scucess, call bindCameraReport
    stBindCameraReportReq bindReq;
    stBindCameraReportRep bindRep;
    rc = zwsystem_ipc_bindCameraReport(bindReq, &bindRep);
    if (rc < 0 || bindRep.code < 0)
    {
        return -5;
    }

    auto &paramsManager = CameraParametersManager::getInstance();
    const std::string& camId = paramsManager.getCameraId();
    const std::string& barcode = paramsManager.getCHTBarcode();
    const std::string& firmwareVer = paramsManager.getFirmwareVersion();

    bool bindRes = bindCameraReport(camId, config.userId,
                bindRep.name, config.netNo, firmwareVer,
                zwsystem_ipc_health_int2str(bindRep.externalStorageHealth),
                config.wifiSsid, wifiDdm,
                zwsystem_ipc_status_int2str(bindRep.status),
                bindRep.vsDomain, bindRep.vsToken, bindRep.macAddress,
                std::to_string(bindRep.activeStatus), std::to_string(bindRep.deviceStatus),
                CHT_P2P_AGENT_CAMERA_TYPE,
                bindRep.model,
                std::to_string(bindRep.isCheckHioss),
                bindRep.brand, barcode);
    if (!bindRes) {
        return -5;
    }

    return 0;
}

static void AddString(rapidjson::Document& doc, const char* key, const std::string& val)
{
    auto& alloc = doc.GetAllocator();
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

bool ChtP2PCameraCommandHandler::bindCameraReport(const std::string& camId,
        const std::string &userId, const std::string &name, const std::string &netNo,
        const std::string &firmwareVer, const std::string &externalStorageHealth,
        const std::string &wifiSsid, int wifiDbm, const std::string &status,
        const std::string &vsDomain, const std::string &vsToken, const std::string &macAddress,
        const std::string &activeStatus, const std::string &deviceStatus,
        const std::string &cameraType, const std::string &model,
        const std::string &isCheckHioss, const std::string &brand, const std::string &chtBarcode)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return false;
    }

    auto require_non_empty = [](const std::string& s) { return !s.empty(); };
    if (!require_non_empty(camId) || !require_non_empty(userId) || !require_non_empty(chtBarcode)) {
        return false;
    }
    if (wifiDbm < -150 || wifiDbm > 50) {
        return false;
    }

    // 更新參數管理器中的參數
    // 調試輸出
    printApiDebug("綁定攝影機使用參數:");
    printApiDebug("  camId: " + camId);
    printApiDebug("  chtBarcode: " + chtBarcode);

    try {
        // 構建JSON payload
        rapidjson::Document document;
        document.SetObject();
        auto& allocator = document.GetAllocator();

        AddString(document, PAYLOAD_KEY_CAMID, camId);
        AddString(document, PAYLOAD_KEY_UID, userId);
        AddString(document, PAYLOAD_KEY_NAME, name);
        AddString(document, PAYLOAD_KEY_NETNO, netNo);
        AddString(document, PAYLOAD_KEY_FIRMWARE_VER, firmwareVer);
        AddString(document, PAYLOAD_KEY_EXTERNAL_STORAGE_HEALTH, externalStorageHealth);
        AddString(document, PAYLOAD_KEY_WIFI_SSID, wifiSsid);

        document.AddMember(rapidjson::Value().SetString(PAYLOAD_KEY_WIFI_DBM, allocator), wifiDbm, allocator);

        AddString(document, PAYLOAD_KEY_STATUS, status);
        AddString(document, PAYLOAD_KEY_VSDOMAIN, vsDomain);
        AddString(document, PAYLOAD_KEY_VSTOKEN, vsToken);
        AddString(document, PAYLOAD_KEY_MAC_ADDRESS, macAddress);
        AddString(document, PAYLOAD_KEY_ACTIVE_STATUS, activeStatus);
        AddString(document, PAYLOAD_KEY_DEVICE_STATUS, deviceStatus);
        AddString(document, PAYLOAD_KEY_CAMERA_TYPE, cameraType);
        AddString(document, PAYLOAD_KEY_MODEL, model);
        AddString(document, PAYLOAD_KEY_IS_CHECK_HIOSS, isCheckHioss);
        AddString(document, PAYLOAD_KEY_BRAND, brand);
        AddString(document, PAYLOAD_KEY_CHT_BARCODE, chtBarcode);

        // 轉換為JSON字串
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        if (!document.Accept(writer)) {
            return false;
        }

        // 調試輸出 JSON payload
        printApiDebug("bindCameraReport 發送 JSON payload: " + std::string(buffer.GetString()));

        // 發送命令
        std::string response;
        if (!sendCommand(_BindCameraReport, buffer.GetString(), response)) {
            return false;
        }

        // check response
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析回應JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            return false;
        }

        const rapidjson::Value& rep_dataObj = GetObjectMember(responseJson, PAYLOAD_KEY_DATA);
        int rep_camSid = GetIntMember(rep_dataObj, PAYLOAD_KEY_CAMSID);
        const std::string& rep_camId = GetStringMember(rep_dataObj, PAYLOAD_KEY_CAMID);
        const std::string& rep_barcode = GetStringMember(rep_dataObj, PAYLOAD_KEY_CHT_BARCODE);
        const std::string& rep_tenantId = GetStringMember(rep_dataObj, PAYLOAD_KEY_TENANT_ID);
        const std::string& rep_netNo = GetStringMember(rep_dataObj, PAYLOAD_KEY_NETNO);
        const std::string& rep_userId = GetStringMember(rep_dataObj, PAYLOAD_KEY_UID);

        if (rep_camId != camId || rep_barcode != chtBarcode || rep_userId != userId || rep_netNo != netNo)
        {
            std::cerr << " response parameter is wrong!!!" << std::endl;
            return false;
        }

        // 使用參數管理器獲取參數
        auto &paramsManager = CameraParametersManager::getInstance();
        paramsManager.setTenantId(rep_tenantId);
        paramsManager.setUserId(rep_userId);
        paramsManager.setNetNo(netNo);
        paramsManager.saveToFile();

        return true;

    } catch (const std::exception &e) {
        std::cerr << "bindCameraReport error msg=" << e.what() << std::endl;
        return false;
    }

    return false;
}

int ChtP2PCameraCommandHandler::cameraRegister(void)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return -1;
    }

    auto &paramsManager = CameraParametersManager::getInstance();
    const std::string& camId = paramsManager.getCameraId();
    const std::string& chtBarcode = paramsManager.getCHTBarcode();

    bool regRes = cameraRegister(camId, chtBarcode);
    if (!regRes) {
        return -2;
    }

    return 0;
}

bool ChtP2PCameraCommandHandler::cameraRegister(const std::string& camId, const std::string& chtBarcode)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return -1;
    }

    auto require_non_empty = [](const std::string& s) { return !s.empty(); };
    if (!require_non_empty(camId)) {
        return false;
    }

    try {
        // 構建JSON payload
        rapidjson::Document document;
        document.SetObject();
        const auto& allocator = document.GetAllocator();

        AddString(document, PAYLOAD_KEY_CAMID, camId);
        AddString(document, PAYLOAD_KEY_CHT_BARCODE, chtBarcode);

        // 轉換為JSON字串
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        if (!document.Accept(writer)) {
            return false;
        }

        // 調試輸出 JSON payload
        printApiDebug("cameraRegister 發送 JSON payload: " + std::string(buffer.GetString()));

        // 發送命令
        std::string response;
        if (!sendCommand(_CameraRegister, buffer.GetString(), response)) {
            return false;
        }

        // check response
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析回應JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            return false;
        }

        const rapidjson::Value& rep_dataObj = GetObjectMember(responseJson, PAYLOAD_KEY_DATA);
        const std::string& rep_publicIp = GetStringMember(rep_dataObj, PAYLOAD_KEY_PUBLIC_IP);

        // 使用參數管理器獲取參數
        auto &paramsManager = CameraParametersManager::getInstance();
        paramsManager.setPublicIp(rep_publicIp);
        paramsManager.saveToFile();

        return true;

    } catch (const std::exception &e) {
        std::cerr << "cameraRegister error msg=" << e.what() << std::endl;
        return false;
    }

    return false;
}

int ChtP2PCameraCommandHandler::checkHiOSSstatus(bool& hiOssStatus)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return -1;
    }

    auto &paramsManager = CameraParametersManager::getInstance();
    const std::string& camId = paramsManager.getCameraId();
    const std::string& chtBarcode = paramsManager.getCHTBarcode();
    const std::string& publicIp = paramsManager.getPublicIp();

    bool checkRes = checkHiOSSstatus(camId, chtBarcode, publicIp);
    if(!checkRes) {
        return -2;
    }

    hiOssStatus = (paramsManager.getIsCheckHioss() != "0");

    return 0;
}

bool ChtP2PCameraCommandHandler::checkHiOSSstatus(const std::string& camId,
        const std::string& chtBarcode, const std::string& publicIp)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return false;
    }

#ifdef SIMULATION_MODE
    // 模擬模式下可通過環境變數控制 HiOSS 狀態
    const char *simHiossStatus = getenv("SIM_HIOSS_STATUS");
    bool hiossResult = (simHiossStatus && strcmp(simHiossStatus, "false") == 0) ? false : true;

    printApiDebug("模擬模式 HiOSS 狀態: " + std::string(hiossResult ? "允許" : "受限"));

    auto &paramsManager = CameraParametersManager::getInstance();
    paramsManager.setParameter("hiossStatus", hiossResult ? "1" : "0");
    paramsManager.setIsCheckHioss(hiossResult);
    paramsManager.saveToFile();

    return true;
#endif

    try {
        rapidjson::Document doc;
        doc.SetObject();
        const auto& allocator = doc.GetAllocator();

        AddString(doc, PAYLOAD_KEY_CAMID, camId);
        AddString(doc, PAYLOAD_KEY_CHT_BARCODE, chtBarcode);
        AddString(doc, PAYLOAD_KEY_PUBLIC_IP, publicIp);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        if (!doc.Accept(writer)) {
            return false;
        }

        // 發送命令
        std::string response;
        if (!sendCommand(_CheckHiOSSstatus, buffer.GetString(), response)) {
            return false;
        }

        // check response
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析回應JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            return false;
        }

        const rapidjson::Value& rep_dataObj = GetObjectMember(responseJson, PAYLOAD_KEY_DATA);
        bool rep_status = GetBoolMember(rep_dataObj, PAYLOAD_KEY_STATUS);
        const std::string& rep_desc = GetStringMember(rep_dataObj, PAYLOAD_KEY_DESCRIPTION);

        // 使用參數管理器獲取參數
        auto &paramsManager = CameraParametersManager::getInstance();
        paramsManager.setIsCheckHioss(rep_status);
        paramsManager.saveToFile();

        return true;

    } catch (const std::exception &e) {
        std::cerr << "checkHiOSSstatus error msg=" << e.what() << std::endl;
        return false;
    }

    return false;
}

int ChtP2PCameraCommandHandler::getHamiCameraInitialInfo(void)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return -1;
    }

    auto &paramsManager = CameraParametersManager::getInstance();
    const std::string& camId = paramsManager.getCameraId();
    const std::string& chtBarcode = paramsManager.getCHTBarcode();
    const std::string& tenantId = paramsManager.getTenantId();
    const std::string& netNo = paramsManager.getNetNo();
    const std::string& userId = paramsManager.getUserId();

    bool regRes = getHamiCamInitialInfo(camId, chtBarcode,
            tenantId, netNo, userId);
    if (!regRes) {
        return -2;
    }

    return 0;
}

bool ChtP2PCameraCommandHandler::getHamiCamInitialInfo(const std::string& camId, const std::string& chtBarcode,
        const std::string& tenantId, const std::string& netNo, const std::string& userId)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return -1;
    }

    auto require_non_empty = [](const std::string& s) { return !s.empty(); };
    if (!require_non_empty(camId)) {
        return false;
    }

    printApiStepHeader("開始獲取攝影機初始資訊");

    try {
        // 構建JSON payload
        rapidjson::Document document;
        document.SetObject();
        const auto& allocator = document.GetAllocator();

        AddString(document, PAYLOAD_KEY_CAMID, camId);
        AddString(document, PAYLOAD_KEY_CHT_BARCODE, chtBarcode);

        // 轉換為JSON字串
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        if (!document.Accept(writer)) {
            return false;
        }

        // 調試輸出 JSON payload
        printApiDebug("cameraRegister 發送 JSON payload: " + std::string(buffer.GetString()));

        // 發送命令
        std::string response;
        if (!sendCommand(_GetHamiCamInitialInfo, buffer.GetString(), response)) {
            return false;
        }

        // check response
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析回應JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            return false;
        }

        const rapidjson::Value& rep_dataObj = GetObjectMember(responseJson, PAYLOAD_KEY_DATA);
        const rapidjson::Value& rep_hamiCamInfoObj = GetObjectMember(rep_dataObj, PAYLOAD_KEY_HAMI_CAM_INFO);
        const rapidjson::Value& rep_hamiSettingObj = GetObjectMember(rep_dataObj, PAYLOAD_KEY_HAMI_SETTINGS);
        const rapidjson::Value& rep_hamiAiSettingObj = GetObjectMember(rep_dataObj, PAYLOAD_KEY_HAMI_AI_SETTINGS);
        const rapidjson::Value& rep_hamiSystemSettingObj = GetObjectMember(rep_dataObj, PAYLOAD_KEY_HAMI_SYSTEM_SETTINGS);

        // check camInfo
        int rep_camSid = GetIntMember(rep_hamiCamInfoObj, PAYLOAD_KEY_CAMSID);
        const std::string& rep_camId = GetStringMember(rep_hamiCamInfoObj, PAYLOAD_KEY_CAMID);
        const std::string& rep_barcode = GetStringMember(rep_hamiCamInfoObj, PAYLOAD_KEY_CHT_BARCODE);
        const std::string& rep_tenantId = GetStringMember(rep_hamiCamInfoObj, PAYLOAD_KEY_TENANT_ID);
        const std::string& rep_netNo = GetStringMember(rep_hamiCamInfoObj, PAYLOAD_KEY_NETNO);
        const std::string& rep_userId = GetStringMember(rep_hamiCamInfoObj, PAYLOAD_KEY_UID);
        if ( rep_camId != camId || rep_barcode != chtBarcode ||
             rep_tenantId != tenantId || rep_netNo != netNo ||
             rep_userId != userId) {
            std::cerr << " response parameter is wrong!!!" << std::endl;
            return false;
        }

        // check setting
        int rep_nightMode = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_NIGHT_MODE);
        int rep_autoNightVision = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_AUTO_NIGHT_VISION);
        int rep_statusIndiccatorLight = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_STATUS_INDICATOR_LIGHT);
        int rep_isFlipUpDown = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_IS_FLIP_UP_DOWN);
        int rep_isHd = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_IS_HD);
        int rep_flicker = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_FLICKER);
        int rep_imageQuality = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_IMAGE_QUALITY);
        int rep_isMic = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_IS_MICROPHONE);
        int rep_micSen = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_MICROPHONE_SENSITIVITY);
        int rep_isSpeaker = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_IS_SPEAK);
        int rep_speakerVol = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_SPEAK_VOLUME);
        int rep_storageDay = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_STORAGE_DAY);
        int rep_eventStorageDay = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_EVENT_STORAGE_DAY);

        int rep_powerOn = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_POWER_ON);
        int rep_alertOn = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_ALERT_ON);
        int rep_vmd = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_VMD);
        int rep_ad = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_AD);
        int rep_power = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_POWER);
        int rep_ptzStatus = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_PTZ_STATUS);
        int rep_ptzPetStatus = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_PTZ_PET_STATUS);
        int rep_ptzSpeed = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_PTZ_SPEED);
        int rep_ptzTourStayTime = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_PTZ_TOUR_STAY_TIME);
        int rep_humanTracking = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_HUMAN_TRACKING);
        int rep_petTracking = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_PET_TRACKING);

        int rep_scheduleOn = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_SCHEDULE_ON);
        const std::string& rep_scheduleSun = GetStringMember(rep_hamiSettingObj, PAYLOAD_KEY_SCHEDULE_SUN);
        const std::string& rep_scheduleMon = GetStringMember(rep_hamiSettingObj, PAYLOAD_KEY_SCHEDULE_MON);
        const std::string& rep_scheduleTue = GetStringMember(rep_hamiSettingObj, PAYLOAD_KEY_SCHEDULE_TUE);
        const std::string& rep_scheduleWed = GetStringMember(rep_hamiSettingObj, PAYLOAD_KEY_SCHEDULE_WED);
        const std::string& rep_scheduleThu = GetStringMember(rep_hamiSettingObj, PAYLOAD_KEY_SCHEDULE_THU);
        const std::string& rep_scheduleFri = GetStringMember(rep_hamiSettingObj, PAYLOAD_KEY_SCHEDULE_FRI);
        const std::string& rep_scheduleSat = GetStringMember(rep_hamiSettingObj, PAYLOAD_KEY_SCHEDULE_SAT);


        // check aiSetting


        return true;

    } catch (const std::exception &e) {
        std::cerr << "getHamiCamInitialInfo error msg=" << e.what() << std::endl;
        return false;
    }

    return false;
}

// 添加定期同步狀態的函數
void ChtP2PCameraCommandHandler::scheduledSync()
{
    if (!m_initialized)
    {
        return;
    }

    // 獲取參數管理器
    auto &paramsManager = CameraParametersManager::getInstance();

    // 同步硬體參數
    bool syncResult = paramsManager.syncWithHardware(false); // 只同步重要參數

    if (syncResult)
    {
        // 如果同步成功，保存到文件
        paramsManager.saveToFile();
    }
}

// 添加一個獲取攝影機狀態的方法
bool ChtP2PCameraCommandHandler::getNetworkStatus()
{
    auto &paramsManager = CameraParametersManager::getInstance();

    // 檢查 WiFi 信號強度是否過期 (使用 10 秒的過期時間)
    if (paramsManager.isParameterStale("wifiSignalStrength", std::chrono::milliseconds(10000)))
    {
        // 同步硬體參數
        paramsManager.syncWithHardware(false);
    }

    return true;
}

// 添加一個獲取儲存狀態的方法
bool ChtP2PCameraCommandHandler::getStorageStatus()
{
    auto &paramsManager = CameraParametersManager::getInstance();

    // 檢查存儲參數是否過期 (使用 30 秒的過期時間)
    if (paramsManager.isParameterStale("storageAvailable", std::chrono::milliseconds(30000)) ||
        paramsManager.isParameterStale("storageHealth", std::chrono::milliseconds(30000)))
    {
        // 同步硬體參數
        //paramsManager.syncWithHardware(false);
    }

    return true;
}

bool ChtP2PCameraCommandHandler::reportSnapshot(const std::string &eventId, const std::string &snapshotTime,
                                       const std::string &filePath)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return false;
    }

    // 構建JSON payload
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType &allocator = document.GetAllocator();
    document.AddMember(PAYLOAD_KEY_EVENT_ID, rapidjson::Value(eventId.c_str(), allocator).Move(), allocator);

    const std::string &camId = CameraParametersManager::getInstance().getCameraId();
    document.AddMember(PAYLOAD_KEY_CAMID, rapidjson::Value(camId.c_str(), allocator).Move(), allocator);
    document.AddMember("snapshotTime", rapidjson::Value(snapshotTime.c_str(), allocator).Move(), allocator);
    document.AddMember("filePath", rapidjson::Value(filePath.c_str(), allocator).Move(), allocator);

    // 轉換為JSON字符串
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    // 發送命令
    std::string response;
    return sendCommand(_Snapshot, buffer.GetString(), response);
}

bool ChtP2PCameraCommandHandler::reportRecord(const std::string &eventId, const std::string &fromTime,
                                     const std::string &toTime, const std::string &filePath,
                                     const std::string &thumbnailfilePath)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return false;
    }

    // 構建JSON payload - 符合客戶規格要求
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

    // 獲取攝影機ID
    const std::string &camId = CameraParametersManager::getInstance().getCameraId();

    // 加入所有必要欄位
    document.AddMember(PAYLOAD_KEY_EVENT_ID, rapidjson::Value(eventId.c_str(), allocator).Move(), allocator);
    document.AddMember(PAYLOAD_KEY_CAMID, rapidjson::Value(camId.c_str(), allocator).Move(), allocator);
    document.AddMember("fromTime", rapidjson::Value(fromTime.c_str(), allocator).Move(), allocator);
    document.AddMember("toTime", rapidjson::Value(toTime.c_str(), allocator).Move(), allocator);
    document.AddMember("filePath", rapidjson::Value(filePath.c_str(), allocator).Move(), allocator);
    document.AddMember("thumbnailfilePath", rapidjson::Value(thumbnailfilePath.c_str(), allocator).Move(), allocator);

    // 轉換為JSON字串
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    // 調試輸出JSON payload
    std::cout << "[API-DEBUG] reportRecord 發送 JSON payload: " << buffer.GetString() << std::endl;

    // 發送命令
    std::string response;
    bool result = sendCommand(_Record, buffer.GetString(), response);

    if (result)
    {
        std::cout << "錄影事件回報成功 - EventID: " << eventId << std::endl;
        std::cout << "  影片檔案: " << filePath << std::endl;
        std::cout << "  縮圖檔案: " << thumbnailfilePath << std::endl;
    }
    else
    {
        std::cerr << "錄影事件回報失敗 - EventID: " << eventId << std::endl;
    }

    return result;
}

bool ChtP2PCameraCommandHandler::reportRecognition(const std::string &eventId, const std::string &eventTime,
                                          const std::string &eventType, const std::string &eventClass,
                                          const std::string &videoFilePath, const std::string &snapshotFilePath,
                                          const std::string &audioFilePath, const std::string &coordinate,
                                          const std::string &resultAttributeJson)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return false;
    }

    // 解析結果屬性JSON
    rapidjson::Document resultAttribute;
    resultAttribute.SetObject();

    try
    {
        if (!resultAttributeJson.empty())
        {
            rapidjson::ParseResult parseResult = resultAttribute.Parse(resultAttributeJson.c_str());
            if (parseResult.IsError())
            {
                std::cerr << "解析結果屬性JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
                return false;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "解析結果屬性JSON失敗: " << e.what() << std::endl;
        return false;
    }

    // 構建JSON payload
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType &allocator = document.GetAllocator();
    document.AddMember(PAYLOAD_KEY_EVENT_ID, rapidjson::Value(eventId.c_str(), allocator).Move(), allocator);

    const std::string &camId = CameraParametersManager::getInstance().getCameraId();
    document.AddMember(PAYLOAD_KEY_CAMID, rapidjson::Value(camId.c_str(), allocator).Move(), allocator);

    document.AddMember("eventTime", rapidjson::Value(eventTime.c_str(), allocator).Move(), allocator);
    document.AddMember("eventType", rapidjson::Value(eventType.c_str(), allocator).Move(), allocator);
    document.AddMember("eventClass", rapidjson::Value(eventClass.c_str(), allocator).Move(), allocator);
    document.AddMember("videoFilePath", rapidjson::Value(videoFilePath.c_str(), allocator).Move(), allocator);
    document.AddMember("snapshotFilePath", rapidjson::Value(snapshotFilePath.c_str(), allocator).Move(), allocator);
    document.AddMember("audioFilePath", rapidjson::Value(audioFilePath.c_str(), allocator).Move(), allocator);
    document.AddMember("coordinate", rapidjson::Value(coordinate.c_str(), allocator).Move(), allocator);
    document.AddMember("resultAttribute", resultAttribute, allocator);

    // 轉換為JSON字符串
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    // 發送命令
    std::string response;
    return sendCommand(_Recognition, buffer.GetString(), response);
}

bool ChtP2PCameraCommandHandler::reportStatusEvent(const std::string &eventId, int type, const std::string &camId,
                                          const std::string &statusOrHealth, bool isStatus)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return false;
    }

    // 構建JSON payload
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

    document.AddMember(PAYLOAD_KEY_EVENT_ID, rapidjson::Value(eventId.c_str(), allocator).Move(), allocator);
    document.AddMember("type", type, allocator);

    rapidjson::Value recording(rapidjson::kObjectType);
    recording.AddMember("camId", rapidjson::Value(camId.c_str(), allocator).Move(), allocator);

    // 根據isStatus標誌增加不同的欄位
    if (isStatus)
    {
        recording.AddMember("status", rapidjson::Value(statusOrHealth.c_str(), allocator).Move(), allocator);
    }
    else
    {
        recording.AddMember("externalStorageHealth", rapidjson::Value(statusOrHealth.c_str(), allocator).Move(), allocator);
    }

    document.AddMember("recording", recording, allocator);

    // 轉換為JSON字符串
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    // 發送命令
    std::string response;
    return sendCommand(_StatusEvent, buffer.GetString(), response);
}

bool ChtP2PCameraCommandHandler::sendStreamData(const void *data, size_t dataSize, const std::string &requestId)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return false;
    }

    // 構建metadata JSON
    rapidjson::Document metadata;
    metadata.SetObject();
    rapidjson::Document::AllocatorType &allocator = metadata.GetAllocator();
    metadata.AddMember("requestId", rapidjson::Value(requestId.c_str(), allocator).Move(), allocator);

    // 轉換為JSON字符串
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    metadata.Accept(writer);

    // 發送串流數據
    int result = chtp2p_send_stream_data(data, buffer.GetString());
    return (result == 0);
}

#if 0
void ChtP2PCameraCommandHandler::setInitialInfoCallback(ChtP2PCameraAPI::InitialInfoCallback callback)
{
    m_initialInfoCallback = callback;
}

void ChtP2PCameraCommandHandler::setControlCallback(ChtP2PCameraAPI::ControlCallback callback)
{
    m_controlCallback = callback;
}

void ChtP2PCameraCommandHandler::setAudioDataCallback(ChtP2PCameraAPI::AudioDataCallback callback)
{
    m_audioDataCallback = callback;
}
#endif

// ===== CHT P2P Agent 回調函數實現 =====

void ChtP2PCameraCommandHandler::commandDoneCallback(CHTP2P_CommandType commandType, void *commandHandle,
                                            const char *payload, void *userParam)
{
    std::string payloadStr = payload ? payload : "";
    std::cout << "收到命令完成回調: commandType=" << commandType
              << ", payload=" << payloadStr
              << ", commandHandle=" << commandHandle << std::endl;

    // 打印 m_commandContexts 中的所有鍵
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        std::cout << "m_commandContexts 包含 " << m_commandContexts.size() << " 個項目:" << std::endl;
        for (const auto &pair : m_commandContexts)
        {
            std::cout << "  - 鍵: " << pair.first << std::endl;
        }
    }

    // 尋找對應的命令上下文
    std::shared_ptr<CommandContext> context;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto it = m_commandContexts.find(commandHandle);
        if (it != m_commandContexts.end())
        {
            context = it->second;
            std::cout << "找到對應的命令上下文" << std::endl;
            // 處理完畢後移除
            m_commandContexts.erase(it);
        }
    }

    // 如果找到對應的命令上下文，則通知等待線程
    if (context)
    {
        {
            std::unique_lock<std::mutex> lock(context->mutex);
            context->response = payloadStr;
            context->done = true;
            std::cout << "設置命令完成標誌" << std::endl;
        }
        // 通知等待線程
        context->cv.notify_one();
        std::cout << "通知等待線程" << std::endl;
    }
    else
    {
        std::cerr << "commandDoneCallback 找不到對應的命令上下文" << std::endl;
    }
}

void ChtP2PCameraCommandHandler::controlCallback(CHTP2P_ControlType controlType, void *controlHandle,
                                        const char *payload, void *userParam)
{
    // 獲取ChtP2PCameraCommandHandler實例
    ChtP2PCameraCommandHandler *self = static_cast<ChtP2PCameraCommandHandler *>(userParam);
    if (!self)
    {
        std::cerr << "controlCallback 無效的用戶參數" << std::endl;
        return;
    }

    std::string payloadStr = payload ? payload : "";
    std::cout << "收到控制回調: controlType=" << controlType
              << ", payload=" << payloadStr << std::endl;
#if 0
    // 如果已設置控制回調函數，則調用它
    if (self->m_controlCallback)
    {
        try
        {
            // 調用用戶設置的回調函數獲取回應
            std::string responsePayload = self->m_controlCallback(controlType, payloadStr);

            // 發送控制完成響應
            self->sendControlDone(controlType, controlHandle, responsePayload);
        }
        catch (const std::exception &e)
        {
            std::cerr << "控制回調函數執行異常: " << e.what() << std::endl;

            // 出錯時發送一個錯誤回應
            rapidjson::Document errorResponse;
            errorResponse.SetObject();
            errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, errorResponse.GetAllocator());
            errorResponse.AddMember("description", rapidjson::Value("處理控制請求時發生錯誤", errorResponse.GetAllocator()).Move(), errorResponse.GetAllocator());

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            errorResponse.Accept(writer);
            self->sendControlDone(controlType, controlHandle, buffer.GetString());
        }
    }
    else
    {
        std::cerr << "未設置控制回調函數" << std::endl;

        // 沒有回調處理函數時發送一個錯誤回應
        rapidjson::Document errorResponse;
        errorResponse.SetObject();
        errorResponse.AddMember(PAYLOAD_KEY_RESULT, 0, errorResponse.GetAllocator());
        errorResponse.AddMember("description", rapidjson::Value("未設置控制處理函數", errorResponse.GetAllocator()).Move(), errorResponse.GetAllocator());

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        errorResponse.Accept(writer);
        self->sendControlDone(controlType, controlHandle, buffer.GetString());
    }
#endif
}

void ChtP2PCameraCommandHandler::audioCallback(const char *data, size_t dataSize, const char *metadata, void *userParam)
{
    // 獲取 ChtP2PCameraCommandHandler 實例
    ChtP2PCameraCommandHandler *self = static_cast<ChtP2PCameraCommandHandler *>(userParam);
    if (!self)
    {
        std::cerr << "audioCallback 無效的用戶參數" << std::endl;
        return;
    }

    std::cout << "收到音頻回調: dataSize=" << dataSize << std::endl;
    if (metadata)
    {
        std::cout << ", metadata=" << metadata;
    }
    std::cout << std::endl;
#if 0
    // 如果已設置音頻數據回調函數，則調用它
    if (self->m_audioDataCallback && data)
    {
        try
        {
            // 調用用戶設置的回調函數處理音頻數據
            self->m_audioDataCallback(data, dataSize, ""); // 傳入空字符串作為 metadata
        }
        catch (const std::exception &e)
        {
            std::cerr << "音頻數據回調函數執行異常: " << e.what() << std::endl;
        }
    }
    else
    {
        std::cerr << "未設置音頻數據回調函數或數據為空" << std::endl;
    }
#endif
}

// ===== 幫助函數實現 =====
bool ChtP2PCameraCommandHandler::sendCommand(CHTP2P_CommandType commandType,
        const std::string &payload, std::string &response)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return false;
    }

    // 創建命令上下文
    auto context = std::make_shared<CommandContext>();
    context->done = false;

    // 創建一個有效的指針作為 commandHandle
    static int dummy_handle;
    void *commandHandle = &dummy_handle; // 使用靜態變量的地址作為有效的指針

    // 在發送命令前先存儲上下文
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_commandContexts[commandHandle] = context;
        std::cout << "儲存命令上下文，commandHandle: " << commandHandle << std::endl;
    }

    // 發送命令
    int result = chtp2p_send_command(commandType, &commandHandle, payload.c_str());
    if (result != 0)
    {
        std::cerr << "發送命令失敗，錯誤碼: " << result << std::endl;

        // 移除命令上下文
        std::unique_lock<std::mutex> lock(m_mutex);
        m_commandContexts.erase(commandHandle);

        return false;
    }

    // 等待命令完成
    {
        std::unique_lock<std::mutex> lock(context->mutex);
        std::cout << "等待命令完成，commandHandle: " << commandHandle << std::endl;
        if (!context->cv.wait_for(lock, std::chrono::seconds(10), [&context]()
                                  { return context->done; }))
        {
            // 超時
            std::cerr << "命令執行超時" << std::endl;

            // 移除命令上下文
            std::unique_lock<std::mutex> globalLock(m_mutex);
            m_commandContexts.erase(commandHandle);

            return false;
        }
        std::cout << "命令已完成！" << std::endl;
    }

    // 命令執行完成，獲取回應
    response = context->response;

    // 檢查回應是否成功
    try
    {
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析回應JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            return false;
        }

        auto it = responseJson.FindMember(PAYLOAD_KEY_CODE);
        if (it != responseJson.MemberEnd() && it->value.IsInt()) {
            int code = it->value.GetInt();
            if (code != 0)
            {
                std::cerr << "命令執行失敗，錯誤碼: " << code << std::endl;
                return false;
            }

            return true;
        }

        it = responseJson.FindMember(PAYLOAD_KEY_RESULT);
        if (it != responseJson.MemberEnd() && it->value.IsInt()) {
            int result = it->value.GetInt();
            if (result != 1)
            {
                std::cerr << "命令執行失敗，錯誤碼: " << result << std::endl;
                return false;
            }

            return true;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "解析回應JSON失敗: " << e.what() << std::endl;
        return false;
    }

    return false;
}

bool ChtP2PCameraCommandHandler::sendControlDone(CHTP2P_ControlType controlType, void *controlHandle,
                                        const std::string &payload)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return false;
    }

    // 發送控制完成响應
    int result = chtp2p_send_control_done(controlType, controlHandle, payload.c_str());
    if (result != 0)
    {
        std::cerr << "發送控制完成响應失敗，錯誤碼: " << result << std::endl;
        return false;
    }

    return true;
}

// 實現內部輔助函數
void ChtP2PCameraCommandHandler::handleInitialInfoReceived(const std::string &hamiCamInfo,
                                                  const std::string &hamiSettings,
                                                  const std::string &hamiAiSettings,
                                                  const std::string &hamiSystemSettings)
{
    std::cout << "ChtP2PCameraCommandHandler: 處理初始化資訊..." << std::endl;

    try
    {
        // 獲取參數管理器
        auto &paramsManager = CameraParametersManager::getInstance();

        // 解析並儲存參數
        bool parseResult = paramsManager.parseAndSaveInitialInfo(
            hamiCamInfo, hamiSettings, hamiAiSettings, hamiSystemSettings);

        if (parseResult)
        {
            // 同步到硬體
            if (syncParametersToHardware())
            {
                std::cout << "ChtP2PCameraCommandHandler: 初始化參數處理完成" << std::endl;
            }
            else
            {
                std::cerr << "ChtP2PCameraCommandHandler: 硬體參數同步失敗" << std::endl;
            }
        }
        else
        {
            std::cerr << "ChtP2PCameraCommandHandler: 參數解析失敗" << std::endl;
        }
#if 0
        // 調用用戶設定的回調函數
        if (m_initialInfoCallback)
        {
            m_initialInfoCallback(hamiCamInfo, hamiSettings, hamiAiSettings, hamiSystemSettings);
        }
#endif
    }
    catch (const std::exception &e)
    {
        std::cerr << "ChtP2PCameraCommandHandler: 處理初始化資訊時發生異常: " << e.what() << std::endl;
    }
}

bool ChtP2PCameraCommandHandler::syncParametersToHardware()
{
    try
    {
        auto &paramsManager = CameraParametersManager::getInstance();
        auto &driver = CameraDriver::getInstance();

        // 使用攝影機驅動同步硬體參數
        return driver.syncHardwareFromParameters(paramsManager);
    }
    catch (const std::exception &e)
    {
        std::cerr << "ChtP2PCameraCommandHandler: 同步硬體參數時發生異常: " << e.what() << std::endl;
        return false;
    }
}
