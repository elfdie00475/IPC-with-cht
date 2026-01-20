#ifndef CHT_P2P_AGENT_H
#define CHT_P2P_AGENT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief 影像壓縮類型
     */
    typedef enum
    {
        _MJPG = 0,
        _MPEG4,
        _H264,
        _H263,
        _H265
    } CHTP2P_VideoCodecType;

    /**
     * @brief 語音壓縮類型
     */
    typedef enum
    {
        _G711 = 11,
        _G729,
        _AAC
    } CHTP2P_AudioCodecType;

    /**
     * @brief 命令類型
     */
    typedef enum
    {
        _BindCameraReport = 0x1000,  /**綁定攝影機回報*/
        _CameraRegister,            /**攝影機報到*/
        _CheckHiOSSstatus,          /**卡控回報*/
        _GetHamiCamInitialInfo,     /**Camera取得初始值*/
        _Snapshot,                  /**截圖事件回報(定時)*/
        _Record,                    /**全時錄影事件回報(含上傳AWS-S3 檔案路徑)*/
        _Recognition,               /**一辨事件回報(含上傳AWS-S3 檔案路徑)*/
        _StatusEvent                /**設備異常、正常事件回報*/
    } CHTP2P_CommandType;

    /**
     * @brief 控制類型
     */
    typedef enum
    {
        _GetCamStatusById = 0x2000,    /**取得Camera即時硬體資訊(韌體版本+WiFi強度+SD卡狀態)*/
        _DeleteCameraInfo,             /**解綁攝影機*/
        _SetTimeZone,                  /**設定攝影機時區*/
        _GetTimeZone,                  /**取得攝影機時區*/
        _UpdateCameraName,             /**修改攝影機名稱*/
        _SetCameraOSD,                 /**更新OSD顯示格式*/
        _SetCameraHD,                  /**更新攝影機HD*/
        _SetFlicker,                   /**更新閃爍率*/
        _SetImageQuality,              /**更新影像品質*/
        _SetMicrophone,                /**更新麥克風*/
        _SetNightMode,                 /**更新夜間模式*/
        _SetAutoNightVision,           /**設定夜間模式自動*/
        _SetSpeak,                     /**更新揚聲器*/
        _SetFlipUpDown,                /**更新上下翻轉180翻轉*/
        _SetLED,                       /**更新LED指示燈*/
        _SetCameraPower,               /**攝影機關閉*/
        _GetSnapshotHamiCamDevice,     /**取得攝影機快照(非同步)*/
        _RestartHamiCamDevice,         /**攝影機重新開機*/
        _SetCamStorageDay,             /**更新雲存天數*/
        _SetCamEventStorageDay,       /**更新事件雲存天數*/
        _HamiCamFormatSDCard,          /**攝影機SD卡格式化*/
        _HamiCamPtzControlMove,        /**PTZ控制*/
        _HamiCamPtzControlConfigSpeed, /**PTZ設定擺頭速度*/
        _HamiCamGetPtzControl,         /**取得PTZ設定與狀態資訊*/
        _HamiCamPtzControlTourGo,      /**PTZ巡航模式*/
        _HamiCamPtzControlGoPst,       /**PTZ移動到設定點*/
        _HamiCamPtzControlConfigPst,   /**PTZ設定點*/
        _HamiCamHumanTracking,         /**PTZ人類追蹤*/
        _HamiCamPetTracking,           /**PTZ寵物追蹤*/
        _GetHamiCamBindList,           /**取得wifi密碼*/
        _UpgradeHamiCamOTA,            /**更新OTA*/
        _UpdateCameraAISetting,        /**更新攝影機AI設定資訊*/
        _GetCameraAISetting,           /**取得攝影機AI設定資訊*/
        _GetVideoLiveStream,           /**取得即時影音RTP串流*/
        _StopVideoLiveStream,          /**停止即時影音RTP串流*/
        _GetVideoHistoryStream,        /**取得歷史影音RTP串流*/
        _StopVideoHistoryStream,       /**停止歷史影音RTP串流*/
#if 1 //jeffery added 20250805
        _GetVideoScheduleStream,        /**取得排程影音RTP串流*/
        _StopVideoScheduleStream,       /**停止排程影音RTP串流*/
#endif
        _SendAudioStream,              /**傳送雙向語音RTP串流*/
        _StopAudioStream                /**停止傳送雙向語音RTP串流*/
    } CHTP2P_ControlType;


    /**
     * @brief 命令完成回呼，由Camera端發起命令叫P2P Agent做事的完成回呼
     * @param commandType     命令類型
     * @param commandHandle   命令Handle指標，命令完成帶回
     * @param payload         命令完成攜帶的資料 (JSON格式，例如綁定攝影機回報後取得設備設定資料)
     * @param userParam       使用者自定義參數 (初始化時設定)
     */
    typedef void (*CHTP2P_CommandDoneCallback)(
        CHTP2P_CommandType commandType,
        void *commandHandle,
        const char *payload,
        void *userParam);

    /**
     * @brief 控制回呼，由P2P Agent端發起的控制請求
     * @param controlType     控制類型
     * @param controlHandle   控制Handle指標，控制完成時回呼要帶回
     * @param payload         控制攜帶的資料 (JSON格式，例如P2P伺服器轉送的控制指令)
     * @param userParam       使用者自定義參數 (初始化時設定)
     */
    typedef void (*CHTP2P_ControlCallback)(
        CHTP2P_ControlType controlType,
        void *controlHandle,
        const char *payload,
        void *userParam);

    /**
     * @brief 雙向語音資料回呼
     * @param data       語音資料
     * @param dataSize   data的長度
     * @param metadata   元數據
     * @param userParam  使用者自定義參數 (初始化時設定)
     */
    typedef void (*CHTP2P_AudioCallback)(
        const char*      data,
        size_t           dataSize,
        const char*      metadata,
        void*           userParam
    );


    /**
     * @brief 初始化參數
     */
    typedef struct
    {
        const char *camId;                              /**Camera裝置ID */
        const char *chtBarcode;                         /**25碼~32碼，1234567890123456789012345*/
        CHTP2P_CommandDoneCallback commandDoneCallback; /**命令完成回呼函式*/
        CHTP2P_ControlCallback controlCallback;         /**控制回呼函式*/
        CHTP2P_AudioCallback audioCallback;             /**語音資料回呼函式*/
        void *userParam;                                /**回呼時帶入的參數 */
    } CHTP2P_Config;

    /**
     * @brief 初始化P2P Agent
     */
    int chtp2p_initialize(const CHTP2P_Config *config);

    /**
     * @brief 停止P2P Agent
     */
    void chtp2p_deinitialize(void);

    /**
     * @brief 單一API：Camera叫P2P Agent做事
     * @param commandType     命令類型
     * @param commandHandle   命令Handle指標，P2P Agent生成，命令完成時回呼要帶回
     * @param payload   主要資料 (JSON格式，例如檔案路徑、bucketName、時間字串…)
     * @return 0=成功, 非0=失敗
     * @param output commandHandle
     */
    int chtp2p_send_command(CHTP2P_CommandType commandType, void **commandHandle, const char *payload);

    /**
     * @brief 單一API：由P2P Agent端發起的控制請求，Camera完成時回應
     * @param control   控制
     * @param controlHandle   控制Handle指標帶回
     * @param payload   主要資料 (JSON格式，例如非同步截圖回報檔案路徑、時間字串…)
     * @return 0=成功, 非0=失敗
     */
    int chtp2p_send_control_done(CHTP2P_ControlType controlType, void *controlHandle, const char *payload);

    /**
     * @brief 傳送串流 (例如上行語音、視訊)，內部自決是否Relay
     * @param data  要傳送的串流資料(音訊/視訊)(RTP影音串流)
     * @param metadata    額外資訊(可為NULL)，(JSON格式，例如串流請求ID…)
     * @return 0=成功, 非0=失敗
     */
    int chtp2p_send_stream_data(const void *data, const char *metadata);

#ifdef __cplusplus
}
#endif

#endif // CHT_P2P_AGENT_H
