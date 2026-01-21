#ifndef ZWSYSTEM_IPC_DEFINED_H
#define ZWSYSTEM_IPC_DEFINED_H

//#include "zwsystem_ipc_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZWSYSTEM_IPC_NAME "zwsystem_service.ipc"
#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)  ((unsigned int)(unsigned char)(ch0) | \
    ((unsigned int)(unsigned char)(ch1) << 8) | ((unsigned int)(unsigned char)(ch2) << 16) | \
    ((unsigned int)(unsigned char)(ch3) << 24 ))
#endif // MAKEFOURCC
#define ZWSYSTEM_IPC_FOURCC (MAKEFOURCC('Z','W','S','Y'))

typedef enum zwsystem_ipc_command_e {
    _BindCameraReport = 0x0000,  /**綁定攝影機回報*/
    _CameraRegister,            /**攝影機報到*/
    _CheckHiOSSstatus,          /**卡控回報*/
    _GetHamiCamInitialInfo,     /**Camera取得初始值*/
    _Snapshot,                  /**截圖事件回報(定時)*/
    _Record,                    /**全時錄影事件回報(含上傳AWS-S3 檔案路徑)*/
    _Recognition,               /**一辨事件回報(含上傳AWS-S3 檔案路徑)*/
    _StatusEvent,                /**設備異常、正常事件回報*/

    _GetCamStatusById = 0x1000,     /**取得Camera即時硬體資訊(韌體版本+WiFi強度+SD卡狀態)*/
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
    _GetVideoScheduleStream,        /**取得排程影音RTP串流*/
    _StopVideoScheduleStream,       /**停止排程影音RTP串流*/
    _SendAudioStream,              /**傳送雙向語音RTP串流*/
    _StopAudioStream,               /**停止傳送雙向語音RTP串流*/

    // Extend
    _PtzAbsoluteMove,           /**PTZ控制*/
    _PtzRelativeMove,           /**PTZ控制*/
    _PtzContinuousMove,         /**PTZ控制*/
    _SetPtzHome,                /**PTZ控制*/
    _GotoPtzHome,               /**PTZ控制*/

    _GetAllMediaConfigure,
    _GetVideoSourceConfigure,
    _GetVideoEncoderConfigure,
    _GetMetadataConfigure,
} eZwsystemIpcCmd;

#define ZWSYSTEM_IPC_HEADER_SIZE 32
typedef struct zwsystem_ipc_hdr_st {
    uint32_t u32FourCC;
    uint32_t u32HdrSize;
    uint32_t u32PayloadSize;
    uint16_t u16Headers[ZWSYSTEM_IPC_HEADER_SIZE];
    // 0: msg id
    // 1: cmd type
    // 2: result
} stZwsystemIpcHdr;

typedef struct zwsystem_ipc_msg_st {
    stZwsystemIpcHdr stHdr;
    uint8_t *pu8Payload;
} stZwsystemIpcMsg;


static void zwsystem_ipc_msg_init(stZwsystemIpcMsg *m, uint16_t u16MsgId, uint16_t u16CmdType)
{
    m->stHdr.u32FourCC = ZWSYSTEM_IPC_FOURCC;
    m->stHdr.u16Headers[0] = u16MsgId; // 0: msg id
    m->stHdr.u16Headers[1] = u16CmdType; // 1: cmd type
    m->stHdr.u32HdrSize = 2;
    m->stHdr.u32PayloadSize = 0;
    m->pu8Payload = NULL;
}

static void zwsystem_ipc_msg_free(stZwsystemIpcMsg *m)
{
    if (!m) return ;

    m->stHdr.u32FourCC = ZWSYSTEM_IPC_FOURCC;
    m->stHdr.u32HdrSize = 0;
    m->stHdr.u32PayloadSize = 0;
}

static int zwsystem_ipc_msg_checkFourCC(uint32_t u32FourCC)
{
    return ((u32FourCC == ZWSYSTEM_IPC_FOURCC)?1:0);
}

#ifdef __cplusplus
}
#endif
#endif /* ZWSYSTEM_IPC_DEFINED_H */
