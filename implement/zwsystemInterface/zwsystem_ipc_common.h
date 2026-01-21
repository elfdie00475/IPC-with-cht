#ifndef ZWSYSTEM_IPC_COMMON_H
#define ZWSYSTEM_IPC_COMMON_H

#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int value;
    const char* name;
} EnumStrEntry;

static inline const char* enum_to_str(
    int value,
    const EnumStrEntry *map, size_t map_len,
    const char* fallback)
{
    for (size_t i = 0; i < map_len; ++i) {
        if (map[i].value == value) return map[i].name;
    }
    return fallback ? fallback : "";
}

static inline int str_to_enum(
    const char* s,
    const EnumStrEntry* map, size_t map_len,
    int fallback)
{
    if (!s) return fallback;

    for (size_t i = 0; i < map_len; ++i) {
        if (strcmp(map[i].name, s) == 0) return map[i].value;
    }

    return fallback;
}

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define ZWSYSTEM_IPC_STRING_SIZE 256

typedef enum external_storage_health
{
    eHealth_Normal = 0,
    eHealth_NewCard,
    eHealth_Damaged,
    eHealth_NoCard,
    eHealth_Formatting,
    eHealth_Other
} eExternalStorageHealth;

static const EnumStrEntry kExternalStorageHealthMap[] = {
    { eHealth_Normal,     "Normal"     },
    { eHealth_NewCard,    "NewCard"    },
    { eHealth_Damaged,    "Damaged"    },
    { eHealth_NoCard,     "NoCard"     },
    { eHealth_Formatting, "Formatting" },
    { eHealth_Other,      "Other"      }
};

static inline const char* zwsystem_ipc_health_int2str(eExternalStorageHealth v)
{
    return enum_to_str((int)v,
        kExternalStorageHealthMap, ARRAY_SIZE(kExternalStorageHealthMap),
        "Other");
}

static inline eExternalStorageHealth zwsystem_ipc_health_str2int(const char* s)
{
    return (eExternalStorageHealth)str_to_enum(s,
        kExternalStorageHealthMap, ARRAY_SIZE(kExternalStorageHealthMap),
        (int)eHealth_Other);
}

typedef enum camera_status
{
    eStatus_Close = 0,
    eStatus_Normal,
    eStatus_Abnormal,
    eStatus_Sleep
} eCameraStatus;

static const EnumStrEntry kCameraStatusMap[] = {
    { eStatus_Close,     "Close"     },
    { eStatus_Normal,    "Normal"    },
    { eStatus_Abnormal,  "Abnormal"  },
    { eStatus_Sleep,     "Sleep"     }
};

static inline const char* zwsystem_ipc_status_int2str(eCameraStatus v)
{
    return enum_to_str((int)v,
        kCameraStatusMap, ARRAY_SIZE(kCameraStatusMap),
        "Abnormal");
}

static inline eCameraStatus zwsystem_ipc_status_str2int(const char* s)
{
    return (eCameraStatus)str_to_enum(s,
        kCameraStatusMap, ARRAY_SIZE(kCameraStatusMap),
        (int)eStatus_Abnormal);
}

typedef struct bind_camera_report_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char userId[ZWSYSTEM_IPC_STRING_SIZE];
    char name[ZWSYSTEM_IPC_STRING_SIZE];
    char netNo[ZWSYSTEM_IPC_STRING_SIZE];
    char firmwareVer[ZWSYSTEM_IPC_STRING_SIZE];
    eExternalStorageHealth externalStorageHealth;
    char wifiSsid[ZWSYSTEM_IPC_STRING_SIZE];
    int wifiDbm;
    eCameraStatus status;
    char vsDomain[ZWSYSTEM_IPC_STRING_SIZE];
    char vsToken[ZWSYSTEM_IPC_STRING_SIZE];
    char macAddress[ZWSYSTEM_IPC_STRING_SIZE];
    bool activeStatus;
    bool deviceStatus;
    char cameraType[ZWSYSTEM_IPC_STRING_SIZE];
    char model[ZWSYSTEM_IPC_STRING_SIZE];
    bool isCheckHioss;
    char brand[ZWSYSTEM_IPC_STRING_SIZE];
    char chtBarcode[ZWSYSTEM_IPC_STRING_SIZE];
} stBindCameraReportReq;

typedef struct bind_camera_report_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int camSid;
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char chtBarcode[ZWSYSTEM_IPC_STRING_SIZE];
    char tenantId[ZWSYSTEM_IPC_STRING_SIZE];
    char netNo[ZWSYSTEM_IPC_STRING_SIZE];
    char userId[ZWSYSTEM_IPC_STRING_SIZE];
} stBindCameraReportRep;

extern int zwsystem_ipc_bindCameraReport(const stBindCameraReportReq *pReq, stBindCameraReportRep *pRep);

typedef struct camera_register_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
} stCamerRegisterReq;

typedef struct camera_register_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    char publicId[ZWSYSTEM_IPC_STRING_SIZE];
} stCamerRegisterRep;

extern int zwsystem_ipc_cameraRegister(const stCamerRegisterReq *pReq, stCamerRegisterRep *pRep);

typedef struct check_HiOss_status_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char publicIp[ZWSYSTEM_IPC_STRING_SIZE];
    char chtBarcode[ZWSYSTEM_IPC_STRING_SIZE];
} stCheckHiOssStatusReq;

typedef struct check_HiOss_status_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    bool status;
    char obj_description[ZWSYSTEM_IPC_STRING_SIZE];
} stCheckHiOssStatusRep;

extern int zwsystem_ipc_checkHiOssStatus(const stCheckHiOssStatusReq *pReq, stCheckHiOssStatusRep *pRep);

typedef struct get_hamicam_initial_info_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
} stGetHamiCamInitialInfoReq;

typedef struct hamicam_info_st {
    int camSid;
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char chtBarcode[ZWSYSTEM_IPC_STRING_SIZE];
    char tenantId[ZWSYSTEM_IPC_STRING_SIZE];
    char netNo[ZWSYSTEM_IPC_STRING_SIZE];
    char userId[ZWSYSTEM_IPC_STRING_SIZE];
} stHamiCamInfo;

typedef enum flicker_mode
{
    eFlicker_50Hz = 0,
    eFlicker_60Hz,
    eFlicker_Outdoor
} eFlickerMode;

typedef enum image_quality_mode
{
    eImageQuality_Low = 0,
    eImageQuality_Middle,
    eImageQuality_High
} eImageQualityMode;

typedef enum ptz_status
{
    ePtzStatus_None = 0,
    ePtzStatus_Move,
    ePtzStatus_Tour,
    ePtzStatus_Home,
    ePtzStatus_Stay
} ePtzStatus;

typedef enum ptz_tracking_mode
{
    ePtzTracking_Off = 0,
    ePtzTracking_GoToHome,
    ePtzTracking_Stay
} ePtzTrackingMode;

#define ZWSYSTEM_IPC_SCHEDULE_SIZE 10

typedef struct hami_setting_st {
    bool nightMode;
    bool autoNightVision;
    bool statusIndicatorLight;
    bool ifFlipUpDown;
    bool isHd;
    eFlickerMode flicker;
    eImageQualityMode imageQuality;
    bool isMicrophone;
    uint32_t microphoneSensitivity;
    bool isSpeak;
    uint32_t speakVolume;
    uint32_t storageDay;
    bool scheduleOn;
    char scheduleSun[ZWSYSTEM_IPC_SCHEDULE_SIZE];
    char scheduleMon[ZWSYSTEM_IPC_SCHEDULE_SIZE];
    char scheduleTue[ZWSYSTEM_IPC_SCHEDULE_SIZE];
    char scheduleWed[ZWSYSTEM_IPC_SCHEDULE_SIZE];
    char scheduleThu[ZWSYSTEM_IPC_SCHEDULE_SIZE];
    char scheduleFir[ZWSYSTEM_IPC_SCHEDULE_SIZE];
    char scheduleSat[ZWSYSTEM_IPC_SCHEDULE_SIZE];
    uint32_t eventStorageDay;
    bool powerOn;
    bool alertOn;
    bool vmd;
    bool ad;
    uint32_t power;
    ePtzStatus ptzStatus;
    float ptzSpeed;
    uint32_t ptzTourStayTime;
    ePtzTrackingMode humanTracking;
    ePtzTrackingMode petTracking;
} stHamiSetting;

typedef enum sensitivity_mode
{
    eSenMode_Low = 0,
    eSenMode_Middle,
    eSenMode_High
} eSenMode;

typedef enum verify_level
{
    eVerifyLevel_Low = 1,
    eVerifyLevel_High
} eVerifyLevel;

typedef enum fence_direction
{
    eFenceDir_Out2In = 0,
    eFenceDir_In2Out
} eFenceDirection;

#define ZWSYSTEM_FACE_FEATURES_ARRAY_SIZE 20
#define ZWSYSTEM_FACE_FEATURES_SIZE 2048
#define ZWSYSTEM_FENCE_POSITION_SIZE 4

typedef struct position_st
{
    float x;
    float y;
} stPosition;

typedef struct identification_feature_st {
    int id;
    char name[ZWSYSTEM_IPC_STRING_SIZE];
    eVerifyLevel verifyLevel;
    uint8_t faceFeatures[ZWSYSTEM_FACE_FEATURES_SIZE];
    char createTime[ZWSYSTEM_IPC_STRING_SIZE];
    char updateTime[ZWSYSTEM_IPC_STRING_SIZE];
    stPosition fencePos[ZWSYSTEM_FENCE_POSITION_SIZE];
    eFenceDirection fenceDir;
} stIdentificationFeature;

typedef struct hami_ai_setting_st {
    bool vmdAlert;
    bool humanAlert;
    bool petAlert;
    bool adAlert;
    bool fenceAlert;
    bool faceAlert;
    bool fallAlert;
    bool adBabyCryAlert;
    bool adSpeechAlert;
    bool adAlarmAlert;
    bool adDogAlert;
    bool adCatAlert;
    eSenMode vmdSen;
    eSenMode adSen;
    eSenMode humanSen;
    eSenMode faceSen;
    eSenMode fenceSen;
    eSenMode petSen;
    eSenMode adBabyCrySen;
    eSenMode adSpeechSen;
    eSenMode adAlarmSen;
    eSenMode adDogSen;
    eSenMode adCatSen;
    eSenMode fallSen;
    int fallTime;
    uint32_t featuresObjSize;
    stIdentificationFeature features[ZWSYSTEM_FACE_FEATURES_ARRAY_SIZE];
} stHamiAiSetting;

typedef struct hami_system_setting_st {
    char otaDomainName[ZWSYSTEM_IPC_STRING_SIZE];
    int otaQueryInterval;
    char ntpServer[ZWSYSTEM_IPC_STRING_SIZE];
    char bucketName[ZWSYSTEM_IPC_STRING_SIZE];
} stHamiSystemSetting;

typedef struct get_hamicam_initial_info_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    stHamiCamInfo hamiCamInfo;
    stHamiSetting hamiSetting;
    stHamiAiSetting hamiAiSetting;
    stHamiSystemSetting hamiSystemSetting;
} stGetHamiCamInitialInfoRep;

extern int zwsystem_ipc_getHamiCamInitialInfo(const stGetHamiCamInitialInfoReq *pReq, stGetHamiCamInitialInfoRep *pRep);

typedef struct cam_status_by_id_req_st {
    char tenantId[ZWSYSTEM_IPC_STRING_SIZE];
    char netNo[ZWSYSTEM_IPC_STRING_SIZE];
    int camSid;
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char userId[ZWSYSTEM_IPC_STRING_SIZE];
} stCamStatusByIdReq;

typedef struct cam_status_by_id_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    char tenantId[ZWSYSTEM_IPC_STRING_SIZE];
    char netNo[ZWSYSTEM_IPC_STRING_SIZE];
    int camSid;
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char firmwareVer[ZWSYSTEM_IPC_STRING_SIZE];
    char latestVersion[ZWSYSTEM_IPC_STRING_SIZE];
    bool isMicrophone; // 1: open, 0: close;
    uint32_t speakVolume; // 0~10
    eImageQualityMode imageQuality; // 0: low, 1: middle, 2: high
    bool activeStatus; // 0: not start, 1: started
    char name[ZWSYSTEM_IPC_STRING_SIZE];
    eCameraStatus status;
    eExternalStorageHealth externalStorageHealth;
    char externalStorageCapacity[ZWSYSTEM_IPC_STRING_SIZE];
    char externalStorageAvailable[ZWSYSTEM_IPC_STRING_SIZE];
    char wifiSsid[ZWSYSTEM_IPC_STRING_SIZE];
    int wifiDbm;
} stCamStatusByIdRep;

extern int zwsystem_ipc_getCamStatusById(const stCamStatusByIdReq *pReq, stCamStatusByIdRep *pRep);

typedef struct delete_camera_info_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
} stDeleteCameraInfoReq;

typedef struct delete_camera_info_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
} stDeleteCameraInfoRep;

extern int zwsystem_ipc_deleteCameraInfo(const stDeleteCameraInfoReq *pReq, stDeleteCameraInfoRep *pRep);

typedef struct set_timezone_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char tId[ZWSYSTEM_IPC_STRING_SIZE];
} stSetTimezoneReq;

typedef struct set_timezone_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    char tId[ZWSYSTEM_IPC_STRING_SIZE];
} stSetTimezoneRep;

extern int zwsystem_ipc_setTimezone(const stSetTimezoneReq *pReq, stSetTimezoneRep *pRep);

typedef struct get_timezone_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
} stGetTimezoneReq;

#define ZWSYSTEM_IPC_TIMEONE_ARRAY_SIZE 256

typedef struct timezone_object_st {
    char tid[ZWSYSTEM_IPC_STRING_SIZE];
    char displayName[ZWSYSTEM_IPC_STRING_SIZE];
    char baseUtcOffset[ZWSYSTEM_IPC_STRING_SIZE];
    char TZ[ZWSYSTEM_IPC_STRING_SIZE];
} stTimezoneObject;

typedef struct set_timezone_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    char timezone[ZWSYSTEM_IPC_STRING_SIZE];
    uint32_t timezoneObjSize;
    stTimezoneObject timezoneAll[ZWSYSTEM_IPC_TIMEONE_ARRAY_SIZE];
} stGetTimezoneRep;

extern int zwsystem_ipc_getTimezone(const stGetTimezoneReq *pReq, stGetTimezoneRep *pRep);

typedef struct update_camera_name_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char name[ZWSYSTEM_IPC_STRING_SIZE];
} stUpdateCameraNameReq;

typedef struct update_camera_name_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    char name[ZWSYSTEM_IPC_STRING_SIZE];
} stUpdateCameraNameRep;

extern int zwsystem_ipc_updateCameraName(const stUpdateCameraNameReq *pReq, stUpdateCameraNameRep *pRep);

typedef struct set_camera_OSD_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char osdRule[ZWSYSTEM_IPC_STRING_SIZE]; // yyyy MM dd HH mm ss
} stSetCameraOsdReq;

typedef struct set_camera_OSD_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    char osdRule[ZWSYSTEM_IPC_STRING_SIZE];
} stSetCameraOsdRep;

extern int zwsystem_ipc_setCameraOsd(const stSetCameraOsdReq *pReq, stSetCameraOsdRep *pRep);

typedef struct set_camera_HD_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char requestId[ZWSYSTEM_IPC_STRING_SIZE]; // <UDP/Relay>_live_<userId>_<UUID>
    bool isHd;
} stSetCameraHdReq;

typedef struct set_camera_HD_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    char requestId[ZWSYSTEM_IPC_STRING_SIZE];
    bool isHd;
} stSetCameraHdRep;

extern int zwsystem_ipc_setCameraHd(const stSetCameraHdReq *pReq, stSetCameraHdRep *pRep);

typedef struct set_flicker_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    eFlickerMode flicker;
} stSetFlickerReq;

typedef struct set_flicker_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    eFlickerMode flicker;
} stSetFlickerRep;

extern int zwsystem_ipc_setFlicker(const stSetFlickerReq *pReq, stSetFlickerRep *pRep);

typedef struct set_image_quality_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char requestId[ZWSYSTEM_IPC_STRING_SIZE]; // <UDP/Relay>_live_<userId>_<UUID>
    eImageQualityMode imageQuality
} stSetImageQualityReq;

typedef struct set_image_quality_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    char requestId[ZWSYSTEM_IPC_STRING_SIZE]; // <UDP/Relay>_live_<userId>_<UUID>
    eImageQualityMode imageQuality
} stSetImageQualityRep;

extern int zwsystem_ipc_setImageQuality(const stSetImageQualityReq *pReq, stSetImageQualityRep *pRep);

typedef struct set_microphone_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    uint32_t microphoneSensitivity; // 0~10
} stSetMicrophoneReq;

typedef struct set_microphone_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    uint32_t microphoneSensitivity; // 0~10
} stSetMicrophoneRep;

extern int zwsystem_ipc_setMicrophone(const stSetMicrophoneReq *pReq, stSetMicrophoneRep *pRep);

typedef struct set_night_mode_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    bool nightMode;
} stSetNightModeReq;

typedef struct set_night_mode_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    bool nightMode;
} stSetNightModeRep;

extern int zwsystem_ipc_setNightMode(const stSetNightModeReq *pReq, stSetNightModeRep *pRep);

typedef struct set_auto_night_vision_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    bool autoNightVision;
} stSetAutoNightVisionReq;

typedef struct set_auto_night_vision_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    bool autoNightVision;
} stSetAutoNightVisionRep;

extern int zwsystem_ipc_setAutoNightVision(const stSetAutoNightVisionReq *pReq, stSetAutoNightVisionRep *pRep);

typedef struct set_speaker_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    uint32_t speakerVolume; // 0~10
} stSetSpeakerReq;

typedef struct set_speaker_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    uint32_t speakerVolume; // 0~10
} stSetSpeakerRep;

extern int zwsystem_ipc_setSpeaker(const stSetSpeakerReq *pReq, stSetSpeakerRep *pRep);

typedef struct set_flip_updown_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    bool isFlipUpDown;
} stSetFlipUpDownReq;

typedef struct set_flip_updown_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    bool isFlipUpDown;
} stSetFlipUpDownRep;

extern int zwsystem_ipc_setFlipUpDown(const stSetFlipUpDownReq *pReq, stSetFlipUpDownRep *pRep);

typedef struct set_LED_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    bool statusIndicatorLight;
} stSetLedReq;

typedef struct set_LED_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    bool statusIndicatorLight;
} stSetLedRep;

extern int zwsystem_ipc_setLed(const stSetLedReq *pReq, stSetLedRep *pRep);

typedef struct set_camera_power_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    bool cameraPower;
} stSetCameraPowerReq;

typedef struct set_camera_power_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    bool cameraPower;
} stSetCameraPowerRep;

extern int zwsystem_ipc_setCameraPower(const stSetCameraPowerReq *pReq, stSetCameraPowerRep *pRep);

typedef struct get_snapshot_async_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char eventId[ZWSYSTEM_IPC_STRING_SIZE];
} stGetSnapshotAsyncReq;

typedef struct get_snapshot_async_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char eventId[ZWSYSTEM_IPC_STRING_SIZE];
    char snapshotTime[ZWSYSTEM_IPC_STRING_SIZE];
    char filePath[ZWSYSTEM_IPC_STRING_SIZE];
} stGetSnapshotAsyncRep;

extern int zwsystem_ipc_getSnapshotAsync(const stGetSnapshotAsyncReq *pReq, stGetSnapshotAsyncRep *pRep);

typedef struct reboot_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
} stRebootReq;

typedef struct reboot_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
} stRebootRep;

extern int zwsystem_ipc_reboot(const stRebootReq *pReq, stRebootRep *pRep);

typedef struct set_storage_day_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    uint32_t storageDay;
} stSetStorageDayReq;

typedef struct set_storage_day_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    uint32_t storageDay;
} stSetStorageDayRep;

extern int zwsystem_ipc_setStorageDay(const stSetStorageDayReq *pReq, stSetStorageDayRep *pRep);

typedef struct set_event_storage_day_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    uint32_t eventStorageDay;
} stSetEventStorageDayReq;

typedef struct set_event_storage_day_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    uint32_t eventStorageDay;
} stSetEventStorageDayRep;

extern int zwsystem_ipc_setEventStorageDay(const stSetEventStorageDayReq *pReq, stSetEventStorageDayRep *pRep);

typedef struct format_sd_card_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
} stFormatSdCardReq;

typedef struct format_sd_card_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
} stFormatSdCardRep;

extern int zwsystem_ipc_formatSdCard(const stFormatSdCardReq *pReq, stFormatSdCardRep *pRep);

typedef struct ptz_move_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char cmd[ZWSYSTEM_IPC_STRING_SIZE];
    float move_x;
    float move_y;
    float speed_x;
    float speed_y;
    uint32_t timeout;
    bool pan;
    bool home;
} stPtzMoveReq;

typedef struct ptz_move_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
} stPtzMoveRep;

extern int cht_ipc_setPtzControlMove(const stPtzMoveReq *pReq, stPtzMoveRep *pRep);
extern int cht_ipc_setPtzAbsoluteMove(const stPtzMoveReq *pReq, stPtzMoveRep *pRep);
extern int cht_ipc_setPtzRelativeMove(const stPtzMoveReq *pReq, stPtzMoveRep *pRep);
extern int cht_ipc_setPtzContinuousMove(const stPtzMoveReq *pReq, stPtzMoveRep *pRep);
extern int cht_ipc_gotoPtzHome(const stPtzMoveReq *pReq, stPtzMoveRep *pRep);

typedef struct set_ptz_speed_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    float ptzSpeed;
} stSetPtzSpeedReq;

typedef struct set_ptz_speed_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    float ptzSpeed;
} stSetPtzSpeedRep;

extern int cht_ipc_setPtzSpeed(const stSetPtzSpeedReq *pReq, stSetPtzSpeedRep *pRep);

typedef struct get_ptz_status_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
} stGetPtzStatusReq;

typedef struct get_ptz_status_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    uint32_t ptzTourStayTime;
    float ptzSpeed;
    ePtzTrackingMode humanTracking;
    ePtzTrackingMode petTracking;
    ePtzStatus ptzStatus;
    ePtzStatus ptzPetStatus;
} stGetPtzStatusRep;

extern int cht_ipc_getPtzStatus(const stGetPtzStatusReq *pReq, stGetPtzStatusRep *pRep);

typedef struct ptz_tour_go_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char indexSequence[ZWSYSTEM_IPC_STRING_SIZE];
} stPtzTourGoReq;

typedef struct ptz_tour_go_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
} stPtzTourGoRep;

extern int cht_ipc_setPtzTourGo(const stPtzTourGoReq *pReq, stPtzTourGoRep *pRep);

typedef struct ptz_go_preset_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    uint32_t index; // 1~4
} stPtzGoPresetReq;

typedef struct ptz_go_preset_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    uint32_t index; // 1~4
} stPtzGoPresetRep;

extern int cht_ipc_setPtzGoPreset(const stPtzGoPresetReq *pReq, stPtzGoPresetRep *pRep);

typedef struct ptz_set_preset_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    uint32_t index; // 1~4
    bool remove;
    char presetName[ZWSYSTEM_IPC_STRING_SIZE];
} stPtzSetPresetReq;

typedef struct ptz_set_preset_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    uint32_t index; // 1~4
    bool remove;
    char presetName[ZWSYSTEM_IPC_STRING_SIZE];
} stPtzSetPresetRep;

extern int cht_ipc_setPtzPresetPoint(const stPtzSetPresetReq *pReq, stPtzSetPresetRep *pRep);

typedef struct ptz_set_tracking_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    ePtzTrackingMode val;
} stPtzSetTrackingReq;

typedef struct ptz_set_tracking_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    ePtzTrackingMode val;
} stPtzSetTrackingRep;

extern int cht_ipc_setPtzHumanTracking(const stPtzSetTrackingReq *pReq, stPtzSetTrackingRep *pRep);
extern int cht_ipc_setPtzPetTracking(const stPtzSetTrackingReq *pReq, stPtzSetTrackingRep *pRep);

typedef struct default_st {
    int result;
} stDefault;

typedef stDefault stSetPtzHomeReq;

typedef struct set_ptz_home_rep_st {
    int result;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
} stSetPtzHomeRep;

extern int cht_ipc_setPtzHome(const stSetPtzHomeReq *pReq, stSetPtzHomeRep *pRep);

typedef struct get_media_vsrc_req_st {
    char name[ZWSYSTEM_IPC_STRING_SIZE];
} stGetMediaVsrcReq;

typedef struct get_media_vsrc_rep_st {
    int result;
    char name[ZWSYSTEM_IPC_STRING_SIZE];
} stGetMediaVsrcRep;

typedef struct get_media_venc_req_st {
    char name[ZWSYSTEM_IPC_STRING_SIZE];
} stGetMediaVencReq;

typedef struct get_media_venc_rep_st {
    int result;
    char name[ZWSYSTEM_IPC_STRING_SIZE];
} stGetMediaVencRep;

typedef struct get_media_metadata_req_st {
    char name[ZWSYSTEM_IPC_STRING_SIZE];
} stGetMediaMetadataReq;

typedef struct get_media_metadata_rep_st {
    int result;
    char name[ZWSYSTEM_IPC_STRING_SIZE];
} stGetMediaMetadataRep;

typedef struct get_media_all_config_req_st {
    char name[ZWSYSTEM_IPC_STRING_SIZE];
} stGetAllMediaConfigReq;

#define MEDIA_VSRC_MAX_SIZE 2
#define MEDIA_VENC_MAX_SIZE 5
#define MEDIA_METADATA_MAX_SIZE 6
typedef struct get_media_all_config_rep_st {
    int result;
    uint32_t u32VsrcCount;
    stGetMediaVsrcRep vsrc[MEDIA_VSRC_MAX_SIZE];
    uint32_t u32VencCount;
    stGetMediaVencRep venc[MEDIA_VENC_MAX_SIZE];
    stGetMediaMetadataRep metadata[MEDIA_METADATA_MAX_SIZE];
} stGetAllMediaConfigRep;

typedef enum {
    DateTimeType_eManual = 0,
    DateTimeType_eNTP
} DateTimeType;

typedef struct datetime_info_st {
    int result;
    DateTimeType type;
    bool daylightSavings;
    uint32_t offset;
    bool overrideTZ;
    int year;
    int month;
    int day;
    int hours;
    int minutes;
    int seconds;
    char TZStr[ZWSYSTEM_IPC_STRING_SIZE];
} stDateTimeInfo;

typedef stDateTimeInfo stDateTimeInfoReq;
typedef stDateTimeInfo stDateTimeInfoRep;

extern int cht_ipc_getDateTimeInfo(const stDateTimeInfoReq *pReq, stDateTimeInfoRep *pRep);
extern int cht_ipc_setDateTimeInfo(const stDateTimeInfoReq *pReq, stDateTimeInfoRep *pRep);

#ifdef __cplusplus
}
#endif

#endif /* ZWSYSTEM_IPC_COMMON_H */
