#ifndef ZWSYSTEM_IPC_CLIENT_H
#define ZWSYSTEM_IPC_CLIENT_H

#include <stddef.h>
#include <string.h>

#include "zwsystem_ipc_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum zwsystem_sub_systemEventType {
    eSystemEventType_Unknown = 0,
    eSystemEventType_Snapshot,
    eSystemEventType_Record,
    eSystemEventType_Recognition,
    eSystemEventType_StatusEvent
} eZwsystemSubSystemEventType;

typedef void (*zwsystem_sub_callback) (void *userParam, eZwsystemSubSystemEventType, const uint8_t *, size_t);

int zwsystem_sub_subscribeSystemEvent(zwsystem_sub_callback callback, void *userParam);
int zwsystem_sub_unsubscribeSystemEvent(void);

extern int zwsystem_ipc_bindCameraReport(stBindCameraReportReq stReq, stBindCameraReportRep *pRep);
extern int zwsystem_ipc_cameraRegister(stCamerRegisterReq stReq, stCamerRegisterRep *pRep);
extern int zwsystem_ipc_checkHiOssStatus(stCheckHiOssStatusReq stReq, stCheckHiOssStatusRep *pRep);
extern int zwsystem_ipc_getHamiCamInitialInfo(stGetHamiCamInitialInfoReq stReq, stGetHamiCamInitialInfoRep *pRep);
extern int zwsystem_ipc_setHamiCamInitialInfo(stSetHamiCamInitialInfoReq stReq, stSetHamiCamInitialInfoRep *pRep);
extern int zwsystem_ipc_getCamStatusById(stCamStatusByIdReq stReq, stCamStatusByIdRep *pRep);
extern int zwsystem_ipc_deleteCameraInfo(stDeleteCameraInfoReq stReq, stDeleteCameraInfoRep *pRep);
extern int zwsystem_ipc_setTimezone(stSetTimezoneReq stReq, stSetTimezoneRep *pRep);
extern int zwsystem_ipc_getTimezone(stGetTimezoneReq stReq, stGetTimezoneRep *pRep);
extern int zwsystem_ipc_updateCameraName(stUpdateCameraNameReq stReq, stUpdateCameraNameRep *pRep);
extern int zwsystem_ipc_setCameraOsd(stSetCameraOsdReq stReq, stSetCameraOsdRep *pRep);
extern int zwsystem_ipc_setCameraHd(stSetCameraHdReq stReq, stSetCameraHdRep *pRep);
extern int zwsystem_ipc_setFlicker(stSetFlickerReq stReq, stSetFlickerRep *pRep);
extern int zwsystem_ipc_setImageQuality(stSetImageQualityReq stReq, stSetImageQualityRep *pRep);
extern int zwsystem_ipc_setMicrophone(stSetMicrophoneReq stReq, stSetMicrophoneRep *pRep);
extern int zwsystem_ipc_setNightMode(stSetNightModeReq stReq, stSetNightModeRep *pRep);
extern int zwsystem_ipc_setAutoNightVision(stSetAutoNightVisionReq stReq, stSetAutoNightVisionRep *pRep);
extern int zwsystem_ipc_setSpeaker(stSetSpeakerReq stReq, stSetSpeakerRep *pRep);
extern int zwsystem_ipc_setFlipUpDown(stSetFlipUpDownReq stReq, stSetFlipUpDownRep *pRep);
extern int zwsystem_ipc_setLed(stSetLedReq stReq, stSetLedRep *pRep);
extern int zwsystem_ipc_setCameraPower(stSetCameraPowerReq stReq, stSetCameraPowerRep *pRep);
extern int zwsystem_ipc_quarySnapshot(stSnapshotReq stReq, stSnapshotRep *pRep);
extern int zwsystem_ipc_feedbackSnapshot(stSnapshotReq stReq, stSnapshotRep *pRep);
extern int zwsystem_ipc_reboot(stRebootReq stReq, stRebootRep *pRep);
extern int zwsystem_ipc_setStorageDay(stSetStorageDayReq stReq, stSetStorageDayRep *pRep);
extern int zwsystem_ipc_setEventStorageDay(stSetStorageDayReq stReq, stSetStorageDayRep *pRep);
extern int zwsystem_ipc_formatSdCard(stFormatSdCardReq stReq, stFormatSdCardRep *pRep);
extern int zwsystem_ipc_setPtzControlMove(stPtzControlMoveReq stReq, stPtzControlMoveRep *pRep);
extern int zwsystem_ipc_setPtzAbsoluteMove(stPtzMoveReq stReq, stPtzMoveRep *pRep);
extern int zwsystem_ipc_setPtzRelativeMove(stPtzMoveReq stReq, stPtzMoveRep *pRep);
extern int zwsystem_ipc_setPtzContinuousMove(stPtzMoveReq stReq, stPtzMoveRep *pRep);
extern int zwsystem_ipc_gotoPtzHome(stPtzMoveReq stReq, stPtzMoveRep *pRep);
extern int zwsystem_ipc_setPtzSpeed(stSetPtzSpeedReq stReq, stSetPtzSpeedRep *pRep);
extern int zwsystem_ipc_getPtzStatus(stGetPtzStatusReq stReq, stGetPtzStatusRep *pRep);
extern int zwsystem_ipc_setPtzTourGo(stPtzTourGoReq stReq, stPtzTourGoRep *pRep);
extern int zwsystem_ipc_setPtzGoPreset(stPtzGoPresetReq stReq, stPtzGoPresetRep *pRep);
extern int zwsystem_ipc_setPtzPresetPoint(stPtzSetPresetReq stReq, stPtzSetPresetRep *pRep);
extern int zwsystem_ipc_setPtzHumanTracking(stPtzSetTrackingReq stReq, stPtzSetTrackingRep *pRep);
extern int zwsystem_ipc_setPtzPetTracking(stPtzSetTrackingReq stReq, stPtzSetTrackingRep *pRep);
extern int zwsystem_ipc_setPtzHome(stSetPtzHomeReq stReq, stSetPtzHomeRep *pRep);
extern int zwsystem_ipc_getCameraBindWifiInfo(stGetCameraBindWifiInfoReq stReq, stGetCameraBindWifiInfoRep *pRep);
extern int zwsystem_ipc_upgradeCameraOta(stUpgradeCameraOtaReq stReq, stUpgradeCameraOtaRep *pRep);
extern int zwsystem_ipc_setCameraAiSetting(stCameraAiSettingReq stReq, stCameraAiSettingRep *pRep);
extern int zwsystem_ipc_getCameraAiSetting(stCameraAiSettingReq stReq, stCameraAiSettingRep *pRep);
extern int zwsystem_ipc_feedbackRecordEvent(stRecordEventReq stReq, stRecordEventRep *pRep);
extern int zwsystem_ipc_feedbackRecognitionEvent(stRecognitionEventReq stReq, stRecognitionEventRep *pRep);
extern int zwsystem_ipc_feedbackCameraStatusEvent(stCameraStatusEventReq stReq, stCameraStatusEventRep *pRep);
extern int zwsystem_ipc_startVideoStream(stStartVideoStreamReq stReq, stStartVideoStreamRep *pRep);
extern int zwsystem_ipc_stopVideoStream(stStopVideoStreamReq stReq, stStopVideoStreamRep *pRep);
extern int zwsystem_ipc_startAudioStream(stStartAudioStreamReq stReq, stStartAudioStreamRep *pRep);
extern int zwsystem_ipc_stopAudioStream(stStopAudioStreamReq stReq, stStopAudioStreamRep *pRep);

extern int zwsystem_ipc_changeWifi(stChangeWifiReq stReq, stChangeWifiRep *pRep);

#ifdef __cplusplus
}
#endif

#endif /* ZWSYSTEM_IPC_CLIENT_H */
