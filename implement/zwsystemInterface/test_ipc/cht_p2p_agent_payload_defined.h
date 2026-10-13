#ifndef CHT_P2P_AGENT_PAYLOAD_DEFINED_H
#define CHT_P2P_AGENT_PAYLOAD_DEFINED_H
// cht_p2p_agent_payload_defined.h
#ifdef __cplusplus
extern "C"
{
#endif

#define CHT_P2P_AGENT_CAMERA_TYPE "IPCAM"

// ===== 基本回應結構 =====
#define PAYLOAD_KEY_DATA "data" /**object*/               /**主要資料物件*/
#define PAYLOAD_KEY_CODE "code" /**int*/                  /**回應代碼*/
#define PAYLOAD_KEY_DESCRIPTION "description" /**string*/ /**回應描述*/
#define PAYLOAD_KEY_RESULT "result" /**int*/              /**1:成功, 0 或其他:失敗,不可為空值*/

// ===== 錄影事件相關 =====
#define PAYLOAD_KEY_EVENT_ID "eventId" /**string*/        /**事件唯一識別碼(以epoch time),不可為空值*/
#define PAYLOAD_KEY_FROM_TIME "fromTime" /**string*/      /**錄影檔起始時間如2024-09-19 00:02:30.000,不可為空值*/
#define PAYLOAD_KEY_TO_TIME "toTime" /**string*/          /**錄影檔結束時間如2024-09-19 00:02:30.000,不可為空值*/
#define PAYLOAD_KEY_FILE_PATH "filePath" /**string*/      /**local錄影檔存放位置*/
#define PAYLOAD_KEY_THUMBNAIL_FILE_PATH "thumbnailfilePath" /**string*/ /**Local縮圖檔存放位置*/

// ===== 攝影機基本資訊 =====
#define PAYLOAD_KEY_CAMID "camId" /**string*/ /**攝影機 ID，同 chtBarcode 值*/
#define PAYLOAD_KEY_UID "userId" /**string*/  /**要綁定攝影機的 UID*/
#define PAYLOAD_KEY_NAME "name" /**string*/   /**攝影機名稱(預設值)*/
#define PAYLOAD_KEY_NETNO "netNo" /**string*/ /**電路編號*/
// #define PAYLOAD_KEY_VERSION "Version" /**string*/              /**目前的韌體版本*/
#define PAYLOAD_KEY_FIRMWARE_VER "firmwareVer" /**string*/     /**韌體版本*/
#define PAYLOAD_KEY_LATEST_VERSION "latestVersion" /**string*/ /**最後韌體版本*/
#define PAYLOAD_KEY_CHT_BARCODE "chtBarcode" /**string*/       /**如 27E13A0931001004734*/
// #define PAYLOAD_KEY_UNIQUE_ID "UniqueID" /**string*/           /**唯一 ID(保留)*/
#define PAYLOAD_KEY_TENANT_ID "tenantId" /**string*/ /**租戶 ID*/
#define PAYLOAD_KEY_CAMSID "camSid" /**int*/         /**攝影機 SID*/
// #define PAYLOAD_KEY_DEVICE_ID "DeviceID" /**string*/ /**設備 ID(保留)*/

// ===== 網路與連線資訊 =====
#define PAYLOAD_KEY_MAC_ADDRESS "macAddress" /**string*/ /**攝影機 MAC*/
#define PAYLOAD_KEY_PUBLIC_IP "publicIp" /**string*/     /**攝影機 public ip*/
#define PAYLOAD_KEY_VSDOMAIN "vsDomain" /**string*/      /**攝影機所屬 Video Server domain*/
#define PAYLOAD_KEY_VSTOKEN "vsToken" /**string*/        /**攝影機所屬 Video Server domain 的 token*/

// ===== 設備狀態 =====
#define PAYLOAD_KEY_STATUS "status" /**string*/               /**實際狀態 Close、Normal、Abnormal、Sleep 定義：Close(關閉攝影機)、Normal(上線)、Abnormal(斷線)、Sleep(休眠)*/
#define PAYLOAD_KEY_ACTIVE_STATUS "activeStatus" /**string*/  /**攝影機竣工狀態 0: 未啟用(預設) 1:已啟用*/
#define PAYLOAD_KEY_DEVICE_STATUS "deviceStatus" /**string*/  /**攝影機狀態，開啟1(預設)，關閉0*/
#define PAYLOAD_KEY_CAMERA_TYPE "cameraType" /**string*/      /**攝影機類型 "IPCAM"*/
#define PAYLOAD_KEY_MODEL "model" /**string*/                 /**攝影機型號*/
#define PAYLOAD_KEY_IS_CHECK_HIOSS "isCheckHioss" /**string*/ /**攝影機是否卡控識別，「0」 ：不需要(stage 預設)， 「1」 ：需要(production 預設)*/
#define PAYLOAD_KEY_BRAND "brand" /**string*/                 /**攝影機廠牌*/

// ===== 存儲相關 =====
#define PAYLOAD_KEY_EXTERNAL_STORAGE_HEALTH "externalStorageHealth" /**string*/       /**外部存儲健康狀況(SD Card)值：Normal(正常)、NewCard(新卡)、Damaged(損毀)、NoCard(無插卡)、Formatting(格式化中)、Other*/
#define PAYLOAD_KEY_EXTERNAL_STORAGE_HEALTH_ALT "externalStorageHeath" /**string*/    /**外部存儲健康狀況(SD Card) (拼寫錯誤版本)*/
#define PAYLOAD_KEY_EXTERNAL_STORAGE_CAPACITY "externalStorageCapacity" /**string*/   /**外部存儲容量 (SD Card) 容量 MB*/
#define PAYLOAD_KEY_EXTERNAL_STORAGE_AVAILABLE "externalStorageAvailable" /**string*/ /**外部存儲可用容量 (SD Card) 容量 MB*/

// ===== 音訊設定 =====
#define PAYLOAD_KEY_IS_MICROPHONE "isMicrophone" /**string*/                /**麥克風 1:開啟 0:關閉*/
#define PAYLOAD_KEY_SPEAK_VOLUME "speakVolume" /**string*/                  /**說話音量*/
#define PAYLOAD_KEY_MICROPHONE_SENSITIVITY "microphoneSensitivity" /**int*/ /**麥克風靈敏度 0~10*/
#define PAYLOAD_KEY_IS_SPEAK "isSpeak" /**string*/                          /**開啟語音 1:開啟 0:關閉*/

// ===== 影像設定 =====
#define PAYLOAD_KEY_IMAGE_QUALITY "imageQuality" /**string*/                  /**圖像質量 0:低 1:中 2:高 或 0: Low bitrate 1: Middle bitrate 2:High bitrate*/
#define PAYLOAD_KEY_IS_HD "isHd" /**string*/                                  /**HD 1:開啟 0:關閉*/
#define PAYLOAD_KEY_FLICKER "flicker" /**string*/                             /**閃爍抑制 0: 50Hz 1: 60Hz 2: 戶外*/
#define PAYLOAD_KEY_NIGHT_MODE "nightMode" /**string*/                        /**夜間模式 1:開啟 0:關閉*/
#define PAYLOAD_KEY_AUTO_NIGHT_VISION "autoNightVision" /**string*/           /**自動夜晚模式 1:開啟 0:關閉*/
#define PAYLOAD_KEY_IS_FLIP_UP_DOWN "isFlipUpDown" /**string*/                /**上下翻轉 1:開啟 0:關閉*/
#define PAYLOAD_KEY_STATUS_INDICATOR_LIGHT "statusIndicatorLight" /**string*/ /**狀態指示燈 1:開啟 0:關閉*/
#define PAYLOAD_KEY_CAMERA "Camera" /**string*/                               /**1:開啟 0:關閉*/

// ===== 時區設定 =====
#define PAYLOAD_KEY_TIMEZONE "timezone" /**string*/              /**tid 值，不可為空值*/
#define PAYLOAD_KEY_TIMEZONE_ALL "timezoneAll" /**Object Array*/ /**時區列表*/
#define PAYLOAD_KEY_TID "tId" /**string*/                        /**時區 ID*/
#define PAYLOAD_KEY_DISPLAY_NAME "displayName" /**string*/       /**地區*/
#define PAYLOAD_KEY_BASE_UTC_OFFSET "baseUtcOffset" /**string*/  /**時差, 單位秒數*/

// ===== 主要資料結構 =====
#define PAYLOAD_KEY_HAMI_CAM_INFO "hamiCamInfo" /**object*/               /**HamiCam 基本資訊*/
#define PAYLOAD_KEY_HAMI_SETTINGS "hamiSettings" /**object*/              /**Hami 設定資訊*/
#define PAYLOAD_KEY_HAMI_SYSTEM_SETTINGS "hamiSystemSettings" /**object*/ /**Hami 系統設定資訊*/
#define PAYLOAD_KEY_HAMI_AI_SETTINGS "hamiAiSettings" /**object*/         /**HamiAI設定*/

// ===== hamiSettings 相關參數 =====
#define PAYLOAD_KEY_STORAGE_DAY "storageDay" /**int*/            /**雲存天數*/
#define PAYLOAD_KEY_SCHEDULE_ON "scheduleOn" /**string*/         /**攝影機休眠範圍排程，「0」：關閉，「1」：開啟*/
#define PAYLOAD_KEY_SCHEDULE_SUN "ScheduleSun" /**string*/       /**星期日排程：0840-0000或不設定為""或0000-0000表24小時*/
#define PAYLOAD_KEY_SCHEDULE_MON "scheduleMon" /**string*/       /**星期一排程：0840-0000*/
#define PAYLOAD_KEY_SCHEDULE_TUE "scheduleTue" /**string*/       /**星期二排程：0840-0000*/
#define PAYLOAD_KEY_SCHEDULE_WED "scheduleWed" /**string*/       /**星期三排程：0840-0000*/
#define PAYLOAD_KEY_SCHEDULE_THU "scheduleThu" /**string*/       /**星期四排程：0840-0000*/
#define PAYLOAD_KEY_SCHEDULE_FRI "scheduleFri" /**string*/       /**星期五排程：0840-0000*/
#define PAYLOAD_KEY_SCHEDULE_SAT "scheduleSat" /**string*/       /**星期六排程：0840-0000*/
#define PAYLOAD_KEY_EVENT_STORAGE_DAY "eventStorageDay" /**int*/ /**一辨事件雲存天數*/
#define PAYLOAD_KEY_POWER_ON "powerOn" /**string*/               /**攝影機開機狀態，「0」：關閉，「1」：開啟*/
#define PAYLOAD_KEY_ALERT_ON "alertOn" /**string*/               /**攝影機警報，「0」：關閉，「1」：開啟*/
#define PAYLOAD_KEY_VMD "vmd" /**string*/                        /**動態偵測，「0」：關閉，「1」：開啟*/
#define PAYLOAD_KEY_AD "ad" /**string*/                          /**聲音偵測，「0」：關閉，「1」：開啟*/
#define PAYLOAD_KEY_POWER "power" /**int*/                       /**攝影機電量(使用電池)*/

// ===== PTZ 控制相關 =====
#define PAYLOAD_KEY_PTZ_STATUS "ptzStatus" /**string*/               /**PTZ設定狀態0: 無, 1:自動擺頭, 2: 巡航, 3: 回到原點, 4: 停留*/
#define PAYLOAD_KEY_PTZ_PET_STATUS "ptzPetStatus" /**string*/        /**PTZ設定狀態0: 無, 1:自動擺頭, 2: 巡航, 3: 回到原點, 4: 停留*/
#define PAYLOAD_KEY_PTZ_SPEED "ptzSpeed" /**string*/                 /**PTZ速度 0~2*/
#define PAYLOAD_KEY_PTZ_TOUR_STAY_TIME "ptzTourStayTime" /**string*/ /**巡航停留時間 1~5*/
#define PAYLOAD_KEY_HUMAN_TRACKING "humanTracking" /**string*/       /**人形追蹤1:開啟 0:關閉*/
#define PAYLOAD_KEY_PET_TRACKING "petTracking" /**string*/           /**寵物追蹤1:開啟 0:關閉*/
#define PAYLOAD_KEY_IS_AUTO_ROTATE "IsAutoRotate" /**string*/        /**自動旋轉 (動態偵測取代)*/
#define PAYLOAD_KEY_CMD "cmd" /**string*/                            /**擺頭控制 left/right/up/down/stop/pan*/
#define PAYLOAD_KEY_SPEED "speed" /**int*/                           /**擺頭速度：0~2*/
#define PAYLOAD_KEY_TOUR_STAYTIME "tour_staytime" /**string*/        /**巡航停留時間*/
#define PAYLOAD_KEY_HUMAN_TRACKING_OLD "human_tracking" /**string*/  /**人類追蹤 0:關閉 1:回到 Home 點 2:停留原地*/
#define PAYLOAD_KEY_PET_TRACKING_OLD "pet_tracking" /**string*/      /**寵物追蹤 0:關閉 1:回到 Home 點 2:停留原地*/
#define PAYLOAD_KEY_PTZ_STATUS_OLD "ptz_status" /**string*/          /**PTZ 設定狀態 0: 無, 1:自動擺頭, 2: 巡航, 3: 回到原點, 4: 停留*/
#define PAYLOAD_KEY_INDEX_SEQUENCE "indexSequence" /**string*/       /**巡航路徑以設定點索引 ID 排序，以逗點隔開*/
// #define PAYLOAD_KEY_INDEX "index" /**int*/                           /**設定點索引 ID 1~4*/
#define PAYLOAD_KEY_POSITION_INDEX "index" /**int*/          /**設定點 ID 1~4*/
#define PAYLOAD_KEY_REMOVE "remove" /**string*/              /**1: 清除設定點, 0: 設定*/
#define PAYLOAD_KEY_POSITION_NAME "positionName" /**string*/ /**設定點名稱*/
#define PAYLOAD_KEY_VAL "val" /**int*/                       /**0:關閉 1:回到 Home 點 2:停留原地*/

// ===== AI 設定相關 =====
#define PAYLOAD_KEY_VMD_ALERT "vmdAlert" /**string*/               /**動態檢測告警 1:開啟 0:關閉*/
#define PAYLOAD_KEY_HUMAN_ALERT "humanAlert" /**string*/           /**人形追蹤告警 1:開啟 0:關閉*/
#define PAYLOAD_KEY_PET_ALERT "petAlert" /**string*/               /**寵物追蹤告警 1:開啟 0:關閉*/
#define PAYLOAD_KEY_AD_ALERT "adAlert" /**string*/                 /**聲音偵測告警 1:開啟 0:關閉*/
#define PAYLOAD_KEY_FENCE_ALERT "fenceAlert" /**string*/           /**電子圍籬偵測告警 1:開啟 0:關閉*/
#define PAYLOAD_KEY_FACE_ALERT "faceAlert" /**string*/             /**臉部偵測告警 1:開啟 0:關閉*/
#define PAYLOAD_KEY_FALL_ALERT "fallAlert" /**string*/             /**跌倒偵測告警 1:開啟 0:關閉*/
#define PAYLOAD_KEY_AD_BABY_CRY_ALERT "adBabyCryAlert" /**string*/ /**嬰兒哭泣告警 1:開啟 0:關閉*/
#define PAYLOAD_KEY_AD_SPEECH_ALERT "adSpeechAlert" /**string*/    /**人聲告警 1:開啟 0:關閉*/
#define PAYLOAD_KEY_AD_ALARM_ALERT "adAlarmAlert" /**string*/      /**警報聲告警 1:開啟 0:關閉*/
#define PAYLOAD_KEY_AD_DOG_ALERT "adDogAlert" /**string*/          /**狗叫聲告警 1:開啟 0:關閉*/
#define PAYLOAD_KEY_AD_CAT_ALERT "adCatAlert" /**string*/          /**貓叫聲告警 1:開啟 0:關閉*/

// ===== AI 靈敏度設定 =====
#define PAYLOAD_KEY_VMD_SEN "vmdSen" /**int*/               /**動態偵測靈敏度 0:低 1:中 2:高*/
#define PAYLOAD_KEY_AD_SEN "adSen" /**int*/                 /**聲音偵測靈敏度 0:低 1:中 2:高*/
#define PAYLOAD_KEY_HUMAN_SEN "humanSen" /**int*/           /**人形偵測靈敏度 0:低 1:中 2:高*/
#define PAYLOAD_KEY_FACE_SEN "faceSen" /**int*/             /**人臉偵測靈敏度 0:低 1:中 2:高*/
#define PAYLOAD_KEY_FENCE_SEN "fenceSen" /**int*/           /**電子圍離靈敏度 0:低 1:中 2:高*/
#define PAYLOAD_KEY_PET_SEN "petSen" /**int*/               /**寵物偵測靈敏度 0:低 1:中 2:高*/
#define PAYLOAD_KEY_AD_BABY_CRY_SEN "adBabyCrySen" /**int*/ /**哭泣偵測靈敏度 0:低 1:中 2:高*/
#define PAYLOAD_KEY_AD_SPEECH_SEN "adSpeechSen" /**int*/    /**人聲偵測靈敏度 0:低 1:中 2:高*/
#define PAYLOAD_KEY_AD_ALARM_SEN "adAlarmSen" /**int*/      /**警報聲偵測靈敏度 0:低 1:中 2:高*/
#define PAYLOAD_KEY_AD_DOG_SEN "adDogSen" /**int*/          /**狗叫聲偵測靈敏度 0:低 1:中 2:高*/
#define PAYLOAD_KEY_AD_CAT_SEN "adCatSen" /**int*/          /**貓叫聲偵測靈敏度 0:低 1:中 2:高*/
#define PAYLOAD_KEY_FALL_SEN "fallSen" /**int*/             /**跌倒偵測靈敏度 0:低 1:中 2:高*/
#define PAYLOAD_KEY_FALL_TIME "fallTime" /**int*/           /**跌倒偵測判定時間 1~5 單位秒*/

// ===== 人臉識別相關 =====
#define PAYLOAD_KEY_IDENTIFICATION_FEATURES "identificationFeatures" /**Object array*/ /**人臉資訊與特徵值欄位表(一個租戶最多 20筆，一個租戶的所有攝影機此表會相同)*/
#define PAYLOAD_KEY_ID "id" /**string*/                                                /**人員 ID，由資分 HiFace 後台自動產生*/
#define PAYLOAD_KEY_VERIFY_LEVEL "verifyLevel" /**int*/                                /**人臉辨識門檻值等級，1 是低，2 是高*/
#define PAYLOAD_KEY_FACE_FEATURES "faceFeatures" /**blob*/                             /**人臉特徵值，512 個浮點數，可轉成 2048 個bytes*/
#define PAYLOAD_KEY_CREATE_TIME "createTime" /**string*/                               /**創建時間*/
#define PAYLOAD_KEY_UPDATE_TIME "updateTime" /**string*/                               /**更新時間*/

// ===== 電子圍籬相關 =====
#define PAYLOAD_KEY_FENCE_POS1 "fencePos1" /**object*/ /**電子圍籬座標 1 {"x":"1","y":"50"}*/
#define PAYLOAD_KEY_FENCE_POS2 "fencePos2" /**object*/ /**電子圍籬座標 2*/
#define PAYLOAD_KEY_FENCE_POS3 "fencePos3" /**object*/ /**電子圍籬座標 3*/
#define PAYLOAD_KEY_FENCE_POS4 "fencePos4" /**object*/ /**電子圍籬座標 4*/
#define PAYLOAD_KEY_X "x" /**int*/                     /**X 座標*/
#define PAYLOAD_KEY_Y "y" /**int*/                     /**Y 座標*/
#define PAYLOAD_KEY_FENCE_DIR "fenceDir" /**string*/   /**電子圍籬進入方向 0:進入 1:離開*/

// ===== 系統設定相關 =====
#define PAYLOAD_KEY_OTA_DOMAIN_NAME "otaDomainName" /**string*/    /**OTA domain name*/
#define PAYLOAD_KEY_OTA_QUERY_INTERVAL "otaQueryInterval" /**int*/ /**OTA 查詢更新時間間隔*/
#define PAYLOAD_KEY_NTP_SERVER "ntpServer" /**string*/             /**NTP Server 位址*/
#define PAYLOAD_KEY_BUCKET_NAME "bucketName" /**string*/           /**檔案上傳AWS S3根路徑*/

// ===== 控制相關參數 =====
#define PAYLOAD_KEY_REQUEST_ID "requestId" /**string*/ /**請求ID，格式: <UDP/Relay>_<type>_<userId>_<JWTToken>*/
#define PAYLOAD_KEY_FRAME_TYPE "frameType" /**string*/ /**串流格式類型: rtp, raw*/
#define PAYLOAD_KEY_START_TIME "startTime" /**long*/   /**開始時間 (Epoch timestamp)*/

// ===== WiFi 設定相關 =====
#define PAYLOAD_KEY_WIFI_SSID "wifiSsid" /**string*/         /**攝影機連線 wifi 的 SSID*/
#define PAYLOAD_KEY_WIFI_DBM "wifiDbm" /**int*/              /**WiFi 訊號強度，以 dBm 表示*/
#define PAYLOAD_KEY_WIFI_PASSWORD "wifiPassword" /**string*/ /**WiFi 密碼*/
#define PAYLOAD_KEY_PSWD "pswd" /**string*/                  /**密碼 (Base64編碼)*/
// #define PAYLOAD_KEY_SSID "SSID" /**string*/                  /**WiFiSSID*/
// #define PAYLOAD_KEY_PSWD_CAPS "PSWD" /**string*/             /**WiFi 密碼*/

// ===== OTA 升級相關 =====
#define PAYLOAD_KEY_UPGRADE_MODE "upgradeMode" /**string*/ /**更新時機 0:立即(等待 5 秒讓控制指令先回應至CHT-API), 1:稍後閒置時*/
#define PAYLOAD_KEY_FILE_URL "fileURL" /**string*/         /**韌體檔下載 URL*/

// ===== OSD 相關參數 =====
#define PAYLOAD_KEY_OSD_RULE "osdRule" /**string*/              /**OSD 規則設定*/
#define PAYLOAD_KEY_OSD_FORMAT "osdFormat" /**string*/          /**OSD 格式設定*/
#define PAYLOAD_KEY_DATE_FORMAT "dateFormat" /**string*/        /**日期格式*/
#define PAYLOAD_KEY_TIME_FORMAT "timeFormat" /**string*/        /**時間格式*/
#define PAYLOAD_KEY_SHOW_DATE "showDate" /**bool*/              /**顯示日期*/
#define PAYLOAD_KEY_SHOW_TIME "showTime" /**bool*/              /**顯示時間*/
#define PAYLOAD_KEY_SHOW_CAMERA_NAME "showCameraName" /**bool*/ /**顯示攝影機名稱*/
#define PAYLOAD_KEY_POSITION "position" /**int*/                /**位置 0:右下角, 1:左下角, 2:右上角, 3:左上角*/

// ===== 錄影和截圖相關 =====
#define PAYLOAD_KEY_EVENT_ID "eventId" /**string*/                      /**事件唯一識別碼*/
#define PAYLOAD_KEY_SNAPSHOT_TIME "snapshotTime" /**string*/            /**截圖時間,不可為空值*/
#define PAYLOAD_KEY_FILE_PATH "filePath" /**string*/                    /**local 截圖檔存放位置,包含檔名如CamID_yyyyMMddHHmmssZZZ.jpg,其中時間格式至毫秒*/
#define PAYLOAD_KEY_THUMBNAIL_FILE_PATH "thumbnailfilePath" /**string*/ /**縮圖檔案路徑*/
#define PAYLOAD_KEY_FROM_TIME "fromTime" /**string*/                    /**錄影檔起始時間如 2023-01-01 00:02:30.000,不可為空值*/
#define PAYLOAD_KEY_TO_TIME "toTime" /**string*/                        /**錄影檔結束時間如 2023-01-01 00:02:50.000,不可為空值*/
#define PAYLOAD_KEY_EVENT_TIME "eventTime" /**string*/                  /**事件發生時間：不可為空值*/
#define PAYLOAD_KEY_EVENT_TYPE "eventType" /**string*/                  /**辨識類型：不可為空值*/
#define PAYLOAD_KEY_EVENT_CLASS "eventClass" /**string*/                /**辨識對象：不可為空值*/
#define PAYLOAD_KEY_VIDEO_FILE_PATH "videoFilePath" /**string*/         /**local 錄影檔存放位置,包含檔名如 CamID_yyyyMMddHHmmssZZZ.mp4*/
#define PAYLOAD_KEY_SNAPSHOT_FILE_PATH "snapshotFilePath" /**string*/   /**local 截圖檔存放位置,包含檔名如 CamID_yyyyMMddHHmmssZZZ.jpg*/
#define PAYLOAD_KEY_AUDIO_FILE_PATH "audioFilePath" /**string*/         /**local 音檔存放位置,包含檔名如 AAC 格式 CamID_yyyyMMddHHmmssZZZ.aac*/
#define PAYLOAD_KEY_COORDINATE "coordinate" /**string*/                 /**經緯度，可為空值*/
#define PAYLOAD_KEY_RESULT_ATTRIBUTE "resultAttribute" /**object*/      /**結果屬性物件*/
#define PAYLOAD_KEY_FID_RESULT "fidresult" /**string*/                  /**人臉辨識結果*/
#define PAYLOAD_KEY_TYPE "type" /**int*/                                /**設備異常事件類型：不可為空值*/
#define PAYLOAD_KEY_RECORDING "recording" /**object*/                   /**錄影物件*/
#define PAYLOAD_KEY_CAM_STATUS "camStatus" /**string*/                  /**實際狀態 Close、Normal、Abnormal、Sleep*/

// ===== 串流相關 =====
#define PAYLOAD_KEY_VIDEO "video" /**object*/         /**視訊物件*/
#define PAYLOAD_KEY_AUDIO "audio" /**object*/         /**音訊物件*/
#define PAYLOAD_KEY_CODEC "codec" /**int*/            /**編碼格式 視訊: 0:_MJPG,1:_MPEG4,2:_H264,3:_H263,4:_H265 音訊: 11:_G711, 12:_G729, 13:_AAC*/
#define PAYLOAD_KEY_WIDTH "width" /**int*/            /**影像解析度寬pixel*/
#define PAYLOAD_KEY_HEIGHT "height" /**int*/          /**影像解析度高pixel*/
#define PAYLOAD_KEY_FPS "fps" /**int*/                /**1~30*/
#define PAYLOAD_KEY_BIT_RATE "bitRate" /**int*/       /**單位 kbps*/
#define PAYLOAD_KEY_SAMPLE_RATE "sampleRate" /**int*/ /**單位 kHz*/
#define PAYLOAD_KEY_SDP "sdp" /**string*/

#ifdef __cplusplus
}
#endif

#endif // CHT_P2P_AGENT_PAYLOAD_DEFINED_H
