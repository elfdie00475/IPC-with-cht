#ifndef ZWSYSTEM_IPC_COMMON_H
#define ZWSYSTEM_IPC_COMMON_H

#include <stddef.h>
#include <string.h>
#include <stdint.h>

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
    eStatus_Sleep,
    eStatus_Unknown
} eCameraStatus;

static const EnumStrEntry kCameraStatusMap[] = {
    { eStatus_Close,    "Close"     },
    { eStatus_Normal,   "Normal"    },
    { eStatus_Abnormal, "Abnormal"  },
    { eStatus_Sleep,    "Sleep"     },
    { eStatus_Unknown,  "Unknown"   }
};

static inline const char* zwsystem_ipc_status_int2str(eCameraStatus v)
{
    return enum_to_str((int)v,
        kCameraStatusMap, ARRAY_SIZE(kCameraStatusMap),
        "Unknown");
}

static inline eCameraStatus zwsystem_ipc_status_str2int(const char* s)
{
    return (eCameraStatus)str_to_enum(s,
        kCameraStatusMap, ARRAY_SIZE(kCameraStatusMap),
        (int)eStatus_Unknown);
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

typedef struct camera_register_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
} stCamerRegisterReq;

typedef struct camera_register_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    char publicId[ZWSYSTEM_IPC_STRING_SIZE];
} stCamerRegisterRep;

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
} stIdentificationFeature;

typedef enum hami_ai_setting_update_mask {
    eAiSettingUpdateMask_VmdAlert       = (1u << 0),
    eAiSettingUpdateMask_HumanAlert     = (1u << 1),
    eAiSettingUpdateMask_PetAlert       = (1u << 2),
    eAiSettingUpdateMask_AdAlert        = (1u << 3),
    eAiSettingUpdateMask_FenceAlert     = (1u << 4),
    eAiSettingUpdateMask_FaceAlert      = (1u << 5),
    eAiSettingUpdateMask_FallAlert      = (1u << 6),
    eAiSettingUpdateMask_AdBabyCryAlert = (1u << 7),
    eAiSettingUpdateMask_AdSpeechAlert  = (1u << 8),
    eAiSettingUpdateMask_AdAlarmAlert   = (1u << 9),
    eAiSettingUpdateMask_AdDogAlert     = (1u << 10),
    eAiSettingUpdateMask_AdCatAlert     = (1u << 11),
    eAiSettingUpdateMask_VmdSen         = (1u << 12),
    eAiSettingUpdateMask_AdSen          = (1u << 13),
    eAiSettingUpdateMask_HumanSen       = (1u << 14),
    eAiSettingUpdateMask_FaceSen        = (1u << 15),
    eAiSettingUpdateMask_FenceSen       = (1u << 16),
    eAiSettingUpdateMask_PetSen         = (1u << 17),
    eAiSettingUpdateMask_AdBabySen      = (1u << 18),
    eAiSettingUpdateMask_AdSpeechSen    = (1u << 19),
    eAiSettingUpdateMask_AdAlarmSen     = (1u << 20),
    eAiSettingUpdateMask_AdDogSen       = (1u << 21),
    eAiSettingUpdateMask_AdCatSen       = (1u << 22),
    eAiSettingUpdateMask_FallSen        = (1u << 23),
    eAiSettingUpdateMask_FallTime       = (1u << 24),
    eAiSettingUpdateMask_Features       = (1u << 25),
    eAiSettingUpdateMask_FencePos       = (1u << 26),
    eAiSettingUpdateMask_FenceDir       = (1u << 27),
    eAiSettingUpdateMask_ALL            = 0xFFFFFFFFu
} eAiSettingUpdateMaskBit;

typedef enum fence_pos_update_mask {
    eFencePosUpdateMask_FencePos_1     = (1u << 1),
    eFencePosUpdateMask_FencePos_2     = (1u << 2),
    eFencePosUpdateMask_FencePos_3     = (1u << 3),
    eFencePosUpdateMask_FencePos_4     = (1u << 4),
    eFencePosUpdateMask_ALL            = 0xFFFFFFFFu
} eFencePosUpdateMaskBit;

typedef struct hami_ai_setting_st {
    eAiSettingUpdateMaskBit updateBit; // eAiSettingUpdateMaskBit
    eFencePosUpdateMaskBit fencePosUpdateBit; // eFencePosUpdateMaskBit
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
    stPosition fencePos[ZWSYSTEM_FENCE_POSITION_SIZE];
    eFenceDirection fenceDir;
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

typedef struct delete_camera_info_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
} stDeleteCameraInfoReq;

typedef struct delete_camera_info_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
} stDeleteCameraInfoRep;

typedef enum datetime_type {
    DateTimeType_eManual = 0,
    DateTimeType_eNTP
} eDateTimeType;

typedef struct datetime_info_st {
    eDateTimeType type;
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

typedef struct timezone_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char tId[ZWSYSTEM_IPC_STRING_SIZE];

    stDateTimeInfo dateTimeInfo;
} stTimezoneReq;

#define ZWSYSTEM_IPC_TIMEONE_ARRAY_SIZE 256

typedef struct timezone_object_st {
    char tid[ZWSYSTEM_IPC_STRING_SIZE];
    char displayName[ZWSYSTEM_IPC_STRING_SIZE];
    char baseUtcOffset[ZWSYSTEM_IPC_STRING_SIZE];
    char TZ[ZWSYSTEM_IPC_STRING_SIZE];
} stTimezoneObject;

typedef struct timezone_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    char tId[ZWSYSTEM_IPC_STRING_SIZE];
    uint32_t timezoneObjSize;
    stTimezoneObject timezoneAll[ZWSYSTEM_IPC_TIMEONE_ARRAY_SIZE];

    stDateTimeInfo dateTimeInfo;
} stTimezoneRep;


typedef stTimezoneReq stSetTimezoneReq;
typedef stTimezoneReq stGetTimezoneReq;
typedef stTimezoneRep stSetTimezoneRep;
typedef stTimezoneRep stGetTimezoneRep;


/* Extend */
typedef stTimezoneRep stDateTimeInfoReq;
typedef stTimezoneRep stDateTimeInfoRep;

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

typedef struct set_image_quality_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char requestId[ZWSYSTEM_IPC_STRING_SIZE]; // <UDP/Relay>_live_<userId>_<UUID>
    eImageQualityMode imageQuality;
} stSetImageQualityReq;

typedef struct set_image_quality_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    char requestId[ZWSYSTEM_IPC_STRING_SIZE]; // <UDP/Relay>_live_<userId>_<UUID>
    eImageQualityMode imageQuality;
} stSetImageQualityRep;

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

typedef struct snapshot_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char eventId[ZWSYSTEM_IPC_STRING_SIZE];
    char snapshotTime[ZWSYSTEM_IPC_STRING_SIZE];
    char filePath[ZWSYSTEM_IPC_STRING_SIZE];
} stSnapshotReq;

typedef struct snapshot_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char eventId[ZWSYSTEM_IPC_STRING_SIZE];
    char snapshotTime[ZWSYSTEM_IPC_STRING_SIZE];
    char filePath[ZWSYSTEM_IPC_STRING_SIZE];
} stSnapshotRep;

typedef struct reboot_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
} stRebootReq;

typedef struct reboot_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
} stRebootRep;

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

typedef struct format_sd_card_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
} stFormatSdCardReq;

typedef struct format_sd_card_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
} stFormatSdCardRep;

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

typedef struct ptz_tour_go_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char indexSequence[ZWSYSTEM_IPC_STRING_SIZE];
} stPtzTourGoReq;

typedef struct ptz_tour_go_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
} stPtzTourGoRep;

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

typedef struct default_st {
    int result;
} stDefault;

typedef stDefault stSetPtzHomeReq;

typedef struct set_ptz_home_rep_st {
    int result;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
} stSetPtzHomeRep;

typedef struct get_camera_bind_list_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
} stGetCameraBindListReq;

typedef struct get_camera_bind_list_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    char wifiSsid[ZWSYSTEM_IPC_STRING_SIZE];
    char password[ZWSYSTEM_IPC_STRING_SIZE]; // base64
} stGetCameraBindListRep;

typedef enum ota_upgrade_mode {
    eUpgradeMode_Immediately,
    eUpgradeMode_Later
} eOtaUpgradeMode;

typedef struct upgrade_camera_OTA_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    eOtaUpgradeMode upgradeMode;
    char filePath[ZWSYSTEM_IPC_STRING_SIZE];
} stUpgradeCameraOtaReq;

typedef struct upgrade_camera_OTA_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
} stUpgradeCameraOtaRep;

typedef struct camera_ai_setting_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    stHamiAiSetting aiSetting;
} stCameraAiSettingReq;

typedef struct camera_ai_setting_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    stHamiAiSetting aiSetting;
} stCameraAiSettingRep;

typedef struct record_event_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char eventId[ZWSYSTEM_IPC_STRING_SIZE];
    char fromTime[ZWSYSTEM_IPC_STRING_SIZE];
    char toTime[ZWSYSTEM_IPC_STRING_SIZE];
    char filePath[ZWSYSTEM_IPC_STRING_SIZE];
    char thumbnailfilePath[ZWSYSTEM_IPC_STRING_SIZE];
} stRecordEventReq;

typedef struct record_event_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
} stRecordEventRep;

typedef enum recognition_event_type {
    eRecognitionType_EED = 0,
    eRecognitionType_FR,
    eRecognitionType_HSED_Loud,
    eRecognitionType_HSED_BabyCry,
    eRecognitionType_HSED_Alarm,
    eRecognitionType_HSED_Speech,
    eRecognitionType_HSED_Dog,
    eRecognitionType_HSED_Cat,
    eRecognitionType_FED,
    eRecognitionType_BD,
    eRecognitionType_Unknown
} eRecognitionType;

static const EnumStrEntry kRecognitionEventTypeMap[] = {
    { eRecognitionType_EED,             "EED"           },
    { eRecognitionType_FR,              "FR"            },
    { eRecognitionType_HSED_Loud,       "HSED_Loud"     },
    { eRecognitionType_HSED_BabyCry,    "HSED_BabyCry"  },
    { eRecognitionType_HSED_Alarm,      "HSED_Alarm"    },
    { eRecognitionType_HSED_Speech,     "HSED_Speech"   },
    { eRecognitionType_HSED_Dog,        "HSED_Dog"      },
    { eRecognitionType_HSED_Cat,        "HSED_Cat"      },
    { eRecognitionType_FED,             "FED"           },
    { eRecognitionType_BD,              "BD"            },
    { eRecognitionType_Unknown,         "Unknown"       }
};

static inline const char* zwsystem_ipc_recognitionType_int2str(eRecognitionType v)
{
    return enum_to_str((int)v,
        kRecognitionEventTypeMap, ARRAY_SIZE(kRecognitionEventTypeMap),
        "Unknown");
}

static inline eRecognitionType zwsystem_ipc_recognitionType_str2int(const char* s)
{
    return (eRecognitionType)str_to_enum(s,
        kRecognitionEventTypeMap, ARRAY_SIZE(kRecognitionEventTypeMap),
        (int)eRecognitionType_Unknown);
}

typedef enum recognition_event_class_type {
    eRecognitionEventClass_Person = 0,
    eRecognitionEventClass_Pet,
    eRecognitionEventClass_Montion,
    eRecognitionEventClass_Face,
    eRecognitionEventClass_Unknown
} eRecognitionEventClassType;

static const EnumStrEntry kRecogntionEventClassTypeMap[] = {
    { eRecognitionEventClass_Person,   "Person"    },
    { eRecognitionEventClass_Pet,      "Pet"       },
    { eRecognitionEventClass_Montion,  "Montion"   },
    { eRecognitionEventClass_Face,     "Face"      },
    { eRecognitionEventClass_Unknown,  "Unknown"   }
};

static inline const char* zwsystem_ipc_eventClass_int2str(eRecognitionEventClassType v)
{
    return enum_to_str((int)v,
        kRecogntionEventClassTypeMap, ARRAY_SIZE(kRecogntionEventClassTypeMap),
        "Unknown");
}

static inline eRecognitionEventClassType zwsystem_ipc_eventClass_str2int(const char* s)
{
    return (eRecognitionEventClassType)str_to_enum(s,
        kRecogntionEventClassTypeMap, ARRAY_SIZE(kRecogntionEventClassTypeMap),
        (int)eRecognitionEventClass_Unknown);
}

typedef struct recognition_event_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char eventId[ZWSYSTEM_IPC_STRING_SIZE];
    char eventTime[ZWSYSTEM_IPC_STRING_SIZE];
    eRecognitionType eventType;
    eRecognitionEventClassType eventClass;
    char videoFilePath[ZWSYSTEM_IPC_STRING_SIZE];
    char snapshotFilePath[ZWSYSTEM_IPC_STRING_SIZE];
    char audioFilePath[ZWSYSTEM_IPC_STRING_SIZE];
    char coordinate[ZWSYSTEM_IPC_STRING_SIZE];
    char fidResult[ZWSYSTEM_IPC_STRING_SIZE];
} stRecognitionEventReq;

typedef struct recognition_event_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
} stRecognitionEventRep;

typedef enum camera_status_event_type {
    eStatusEventType_Malfunction = 2,
    eStatuseventType_Normal = 4,
    eStatusEventType_Unknown
} eCameraStatusEventType;

static const EnumStrEntry kStatusEventTypeMap[] = {
    { eStatusEventType_Malfunction, "Malfunction"   },
    { eStatuseventType_Normal,      "Normal"        },
    { eStatusEventType_Unknown,     "Unknown"       }
};

static inline const char* zwsystem_ipc_statusEventType_int2str(eCameraStatusEventType v)
{
    return enum_to_str((int)v,
        kStatusEventTypeMap, ARRAY_SIZE(kStatusEventTypeMap),
        "Unknown");
}

static inline eCameraStatusEventType zwsystem_ipc_statusEventType_str2int(const char* s)
{
    return (eCameraStatusEventType)str_to_enum(s,
        kStatusEventTypeMap, ARRAY_SIZE(kStatusEventTypeMap),
        (int)eStatusEventType_Unknown);
}

typedef struct camera_status_event_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char eventId[ZWSYSTEM_IPC_STRING_SIZE];
    eCameraStatusEventType statusType;
    eCameraStatus status;
    eExternalStorageHealth externalStorageHealth;
} stCameraStatusEventReq;

typedef struct camera_status_event_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
} stCameraStatusEventRep;

typedef enum stream_frame_type {
    eStreamFrameType_RTP = 0,
    eStreamFrameType_RAW
} eStreamFrameType;

typedef struct start_video_stream_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char requestId[ZWSYSTEM_IPC_STRING_SIZE]; // <UDP/Relay>_live_<userId>_<UUID>
    eStreamFrameType frameType;
    eImageQualityMode imageQuality;
    char startTime[ZWSYSTEM_IPC_STRING_SIZE]; // epoch time?
} stStartVideoStreamReq;

typedef enum video_codec {
    eVideoCodec_MJPG = 0,
    eVideoCodec_MPEG4,
    eVideoCodec_H264,
    eVideoCodec_H263,
    eVideoCodec_H265
} eVideoCodec;

typedef enum audio_codec {
    eAudioodec_G711 = 11,
    eAudioCodec_G729,
    eAudioCodec_AAC,
} eAudioCodec;

typedef struct start_video_stream_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    char requestId[ZWSYSTEM_IPC_STRING_SIZE]; // <UDP/Relay>_live_<userId>_<UUID>
    bool video_enabled;
    eVideoCodec video_codec;
    uint32_t video_width;
    uint32_t video_height;
    uint32_t video_fps; // 1~30
    bool audio_enabled;
    eAudioCodec audio_codec;
    uint32_t audio_bitrate; // kbps
    uint32_t audio_sampleRate; // kHz
    char audio_sdp[ZWSYSTEM_IPC_STRING_SIZE];
} stStartVideoStreamRep;

typedef struct stop_video_live_stream_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char requestId[ZWSYSTEM_IPC_STRING_SIZE]; // <UDP/Relay>_live_<userId>_<UUID>
} stStopVideoLiveStreamReq;

typedef struct stop_video_live_stream_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    char requestId[ZWSYSTEM_IPC_STRING_SIZE]; // <UDP/Relay>_live_<userId>_<UUID>
} stStopVideoLiveStreamRep;

typedef struct start_audio_stream_req_st {
    char camId[ZWSYSTEM_IPC_STRING_SIZE];
    char requestId[ZWSYSTEM_IPC_STRING_SIZE]; // <UDP/Relay>_live_<userId>_<UUID>
    eAudioCodec audio_codec;
    uint32_t audio_bitrate; // kbps
    uint32_t audio_sampleRate; // kHz
    char audio_sdp[ZWSYSTEM_IPC_STRING_SIZE];
} stStartAudioStreamReq;

typedef struct start_audio_stream_rep_st {
    int code;
    char description[ZWSYSTEM_IPC_STRING_SIZE];
    int result;
    char requestId[ZWSYSTEM_IPC_STRING_SIZE]; // <UDP/Relay>_live_<userId>_<UUID>
} stStartAudioStreamRep;

typedef stStartAudioStreamReq stStopAudioStreamReq;
typedef stStartAudioStreamRep stStopAudioStreamRep;

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


#ifdef __cplusplus
}
#endif

#endif /* ZWSYSTEM_IPC_COMMON_H */
