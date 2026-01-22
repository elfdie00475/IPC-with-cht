// CameraParametersManager.h
// 2025/07/14 modify
//
// GetHamiCamInitialInfo 日誌控制說明：
// 參數 "enableGetHamiCamInitialInfoLog" 控制是否輸出 GetHamiCamInitialInfo 的完整回應資料
// - "1": 啟用詳細日誌（預設值）
// - "0": 停用詳細日誌
//
// 範例用法：
// paramsManager.setParameter("enableGetHamiCamInitialInfoLog", "1"); // 啟用
// paramsManager.setParameter("enableGetHamiCamInitialInfoLog", "0"); // 停用
//
#ifndef CAMERA_PARAMETERS_MANAGER_H
#define CAMERA_PARAMETERS_MANAGER_H

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <mutex>
#include <chrono>

/**
 * @brief 攝影機參數管理器類 - 統一管理攝影機參數
 */
class CameraParametersManager
{
public:
    /**
     * @brief 解析並儲存 GetHamiCamInitialInfo 的所有參數
     * @param hamiCamInfo 攝影機基本資訊 JSON
     * @param hamiSettings 攝影機設定 JSON
     * @param hamiAiSettings AI設定 JSON
     * @param hamiSystemSettings 系統設定 JSON
     * @return 成功返回true，失敗返回false
     */
    bool parseAndSaveInitialInfo(const std::string &hamiCamInfo,
                                 const std::string &hamiSettings,
                                 const std::string &hamiAiSettings,
                                 const std::string &hamiSystemSettings);

    /**
     * @brief 解析並儲存 hamiCamInfo 參數
     */
    bool parseHamiCamInfo(const std::string &jsonStr);

    /**
     * @brief 解析並儲存 hamiSettings 參數
     */
    bool parseHamiSettings(const std::string &jsonStr);

    /**
     * @brief 解析並儲存 hamiAiSettings 參數
     */
    bool parseHamiAiSettings(const std::string &jsonStr);

    /**
     * @brief 解析並儲存 hamiSystemSettings 參數
     */
    bool parseHamiSystemSettings(const std::string &jsonStr);

    /**
     * @brief 解析人臉識別特徵陣列
     */
    bool parseIdentificationFeatures(const std::string &jsonArrayStr);

    /**
     * @brief 獲取單例實例
     * @return 返回參數管理器單例
     */
    static CameraParametersManager &getInstance();

    /**
     * @brief 初始化參數管理器
     * @param configFilePath 配置檔案路徑
     * @return 成功返回true，失敗返回false
     */
    bool initialize(const std::string &configFilePath);

    /**
     * @brief 初始化參數管理器和條碼配置
     * @param configFilePath 配置檔案路徑
     * @param barcodeConfigPath 條碼配置檔案路徑
     * @return 成功返回true，失敗返回false
     */
    bool initialize(const std::string &configFilePath, const std::string &barcodeConfigPath);

    /**
     * @brief 初始化條碼配置
     * @param barcodeConfigPath 條碼配置檔案路徑
     * @return 成功返回true，失敗返回false
     */
    bool initializeBarcode(const std::string &barcodeConfigPath);

    /**
     * @brief 保存條碼配置到檔案
     * @param path 檔案路徑
     * @return 成功返回true，失敗返回false
     */
    bool saveBarcodeToFile(const std::string &path);

    /**
     * @brief 檢查配置文件是否存在
     * @return 如果配置文件和條碼文件都存在則返回true，否則返回false
     */
    bool configFilesExist() const;

    /**
     * @brief 生成隨機camId和barcode
     * @return 包含隨機生成的camId和barcode的pair
     */
    std::pair<std::string, std::string> generateRandomCamIdAndBarcode();

    /**
     * @brief 檢查是否已完成綁定
     * @return 如果已綁定返回true，否則返回false
     */
    bool isBound() const;

    /**
     * @brief 生成默認條碼配置
     * @return 包含默認CHTBarcode的鍵值
     */
    std::string generateDefaultBarcode() const;

    /**
     * @brief 根據MAC地址生成相機名稱
     * @return 生成的相機名稱字串
     */
    std::string generateCameraNameFromMac();

    // ===== 基本參數設置函數 =====
    void setCameraId(const std::string &cameraId);
    void setCHTBarcode(const std::string &barcode);
    void setPublicIp(const std::string &ip);
    void setCameraName(const std::string &name);
    void setOsdRule(const std::string &osdRule);
    void setAISettings(const std::string &aiSettings);
    void setCamSid(const std::string &camSid);
    void setTenantId(const std::string &tenantId);
    void setNetNo(const std::string &netNo);
    void setVsDomain(const std::string &domain);
    void setVsToken(const std::string &token);
    void setActiveStatus(const std::string &status);
    void setDeviceStatus(const std::string &status);
    void setCameraType(const std::string &type);
    void setModel(const std::string &model);
    void setIsCheckHioss(const std::string &value);
    void setBrand(const std::string &brand);
    void setTimeZone(const std::string &timezone);
    void setUserId(const std::string &userId);

    void setRequestId(const std::string& requestId);
    void setIsHd(const std::string& isHd);
    void setImageQuality(const std::string& imageQuality);

    // ===== 基本參數獲取函數 =====
    std::string getCameraId() const;
    std::string getCHTBarcode() const;
    std::string getCamSid() const;
    std::string getTenantId() const;
    std::string getPublicIp() const;
    std::string getCameraName() const;
    std::string getOsdRule() const;
    std::string getWifiSsid() const;


    std::string getFirmwareVersion() const;
    std::string getCameraStatus() const;
    long getStorageCapacity() const;
    long getStorageAvailable() const;
    std::string getStorageHealth() const;
    bool getMicrophoneEnabled() const;
    int getSpeakerVolume() const;
    std::string getActiveStatus() const;
    std::string getDeviceStatus() const;
    std::string getAISettings() const;
    std::string getMacAddress() const;
    std::string getTimeZone() const;
    std::string getUserId() const;

    std::string getRequestId() const;
    std::string getIsHd() const;
    std::string getImageQuality() const;

    // 新增原本 dataStorage 的方法
    const std::string &getNetNo() const;
    const std::string &getVsDomain() const;
    const std::string &getVsToken() const;
    const std::string &getCameraType() const;
    const std::string &getModel() const;
    const std::string &getIsCheckHioss() const;
    const std::string &getBrand() const;

    // ===== hamiSettings 相關 getter 函數 =====
    std::string getNightMode() const;
    std::string getAutoNightVision() const;
    std::string getStatusIndicatorLight() const;
    std::string getIsFlipUpDown() const;
    std::string getFlicker() const;
    std::string getImageQualityStr() const;
    std::string getIsMicrophone() const;
    int getMicrophoneSensitivity() const;
    std::string getIsSpeak() const;
    int getSpeakVolume() const;
    int getStorageDay() const;
    std::string getScheduleOn() const;
    std::string getScheduleSun() const;
    std::string getScheduleMon() const;
    std::string getScheduleTue() const;
    std::string getScheduleWed() const;
    std::string getScheduleThu() const;
    std::string getScheduleFri() const;
    std::string getScheduleSat() const;
    int getEventStorageDay() const;
    std::string getPowerOn() const;
    std::string getAlertOn() const;
    std::string getVmd() const;
    std::string getAd() const;
    int getPower() const;
    std::string getLastPtzCommand() const;
    std::string getPtzStatus() const;
    std::string getPtzSpeed() const;
    std::string getPtzTourStayTime() const;
    int getHumanTracking() const;
    int getPetTracking() const;
    std::string getPtzTourSequence() const;
    std::string getPositionName1() const;
    std::string getPositionName2() const;
    std::string getPositionName3() const;
    std::string getPositionName4() const;

    // ===== hamiAiSettings 相關 getter 函數 =====
    bool getVmdAlert() const;
    bool getHumanAlert() const;
    bool getPetAlert() const;
    bool getAdAlert() const;
    bool getFenceAlert() const;
    bool getFaceAlert() const;
    bool getFallAlert() const;
    bool getAdBabyCryAlert() const;
    bool getAdSpeechAlert() const;
    bool getAdAlarmAlert() const;
    bool getAdDogAlert() const;
    bool getAdCatAlert() const;
    int getVmdSen() const;
    int getAdSen() const;
    int getHumanSen() const;
    int getFaceSen() const;
    int getFenceSen() const;
    int getPetSen() const;
    int getAdBabyCrySen() const;
    int getAdSpeechSen() const;
    int getAdAlarmSen() const;
    int getAdDogSen() const;
    int getAdCatSen() const;
    int getFallSen() const;
    int getFallTime() const;
    std::string getFenceDir() const;

    // ===== 電子圍籬座標相關函數 =====
    std::pair<int, int> getFencePos1() const;
    std::pair<int, int> getFencePos2() const;
    std::pair<int, int> getFencePos3() const;
    std::pair<int, int> getFencePos4() const;

    // ===== hamiSystemSettings 相關 getter 函數 =====
    std::string getOtaDomainName() const;
    int getOtaQueryInterval() const;
    std::string getNtpServer() const;
    std::string getBucketName() const;

    // ===== 人臉識別特徵相關函數 =====
    struct IdentificationFeature
    {
        std::string id;
        std::string name;
        std::string faceFeatures;
        int verifyLevel;
        std::string createTime;
        std::string updateTime;
    };

    std::vector<IdentificationFeature> getIdentificationFeatures() const;
    bool addIdentificationFeature(const IdentificationFeature &feature);
    bool removeIdentificationFeature(const std::string &id);
    bool updateIdentificationFeature(const std::string &id, const IdentificationFeature &feature);
    bool updateIdentificationFeature(const std::string& aiSettingJson); // from _GetHamiCamInitialInfo or _UpdateCameraAISetting

    /**
     * @brief 解析並儲存初始化資訊後同步硬體
     * @param hamiCamInfo 攝影機基本資訊 JSON
     * @param hamiSettings 攝影機設定 JSON
     * @param hamiAiSettings AI設定 JSON
     * @param hamiSystemSettings 系統設定 JSON
     * @return 成功返回true，失敗返回false
     */
    bool parseAndSaveInitialInfoWithSync(const std::string &hamiCamInfo,
                                         const std::string &hamiSettings,
                                         const std::string &hamiAiSettings,
                                         const std::string &hamiSystemSettings);

    /**
     * @brief 驗證參數有效性
     * @param key 參數鍵名
     * @param value 參數值
     * @return 成功返回true，失敗返回false
     */
    bool validateParameter(const std::string &key, const std::string &value);

    /**
     * @brief 生成調試日誌條目
     * @param message 日誌訊息
     * @param logToFile 是否寫入檔案
     */
    void addDebugLog(const std::string &message, bool logToFile = false);

    // ===== 泛型參數操作 =====
    template <typename T>
    T getParameter(const std::string &key, const T &defaultValue) const;

    template <typename T>
    void setParameter(const std::string &key, const T &value);

    bool hasParameter(const std::string &key) const;
    bool removeParameter(const std::string &key);
    std::map<std::string, std::string> getAllParameters() const;

    // 特殊版本，針對字符串參數的直接存取
    std::string getParameter(const std::string &key, const std::string &defaultValue) const;

    // ===== 檔案操作 =====
    bool saveToFile(const std::string &configFilePath = "");
    bool loadFromFile(const std::string &configFilePath = "");

    // ===== 參數過期檢查 =====
    std::chrono::system_clock::time_point getParameterUpdateTime(const std::string &key) const;
    bool isParameterStale(const std::string &key, std::chrono::milliseconds maxAge) const;

    // ===== 參數變更通知 =====
    typedef std::function<void(const std::string &key, const std::string &value)> ParameterChangeCallback;
    int registerParameterChangeCallback(const std::string &key, ParameterChangeCallback callback);
    bool unregisterParameterChangeCallback(int callbackId);

    bool isFirstBinding() const;
    void setParameterString(const std::string &key, const char *value)
    {
        setParameter(key, std::string(value));
    }

    // NTP 相關方法
    void setNtpServer(const std::string &ntpServer);
    bool syncTimeWithNtp(const std::string &customNtpServer = "");
    bool updateSystemTimeFromNtp();
    bool initializeTimezoneWithNtpSync();

    // ===== 系統檔案讀取方法 =====
    /**
     * @brief 從 /etc/config/hami_uid 讀取 userId
     * @return 成功返回 userId 字串，失敗返回空字串
     */
    std::string loadUserIdFromHamiUidFile();

    /**
     * @brief 從 /etc/config/wpa_supplicant.conf 讀取 WiFi 資訊
     * @param wifiSsid 輸出參數：WiFi SSID
     * @param wifiPassword 輸出參數：WiFi 密碼
     * @return 成功返回 true，失敗返回 false
     */
    bool loadWifiInfoFromSupplicantFile(std::string &wifiSsid, std::string &wifiPassword);

private:
    /**
     * @brief 構造函數，私有，實現單例模式
     */
    CameraParametersManager();

    /**
     * @brief 初始化默認參數
     */
    void initializeDefaultParameters();

    std::string getChtBarcodeFromUbootExport();
    std::string getethaddrFromUbootExport();
    std::string getFirmwareDefVersion();

    /**
     * @brief 通知參數變更
     * @param key 參數鍵名
     * @param value 參數值
     */
    void notifyParameterChanged(const std::string &key, const std::string &value);

    // 參數映射表
    mutable std::recursive_mutex m_mutex;
    std::map<std::string, std::string> m_parameters;

    // 參數更新時間
    std::map<std::string, std::chrono::system_clock::time_point> m_parameterUpdateTimes;

    // 配置檔案路徑
    std::string m_configFilePath;

    // 條碼配置路徑
    std::string m_barcodeConfigPath;

    // 初始化標誌
    bool m_initialized;

    // 參數變更回調
    struct CallbackInfo
    {
        int id;
        std::string key; // 空字符串表示所有參數
        ParameterChangeCallback callback;
    };
    std::vector<CallbackInfo> m_callbacks;
    int m_nextCallbackId;

    // 人臉識別特徵存儲
    std::vector<IdentificationFeature> m_identificationFeatures;
};

#endif // CAMERA_PARAMETERS_MANAGER_H
