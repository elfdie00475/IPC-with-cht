#ifndef ZWSYSTEM_IPC_CLIENT_H
#define ZWSYSTEM_IPC_CLIENT_H

#include <stddef.h>
#include <string.h>

#include "zwsystem_ipc_common.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int zwsystem_ipc_bindCameraReport(const stBindCameraReportReq *pReq, stBindCameraReportRep *pRep);
extern int zwsystem_ipc_cameraRegister(const stCamerRegisterReq *pReq, stCamerRegisterRep *pRep);
extern int zwsystem_ipc_checkHiOssStatus(const stCheckHiOssStatusReq *pReq, stCheckHiOssStatusRep *pRep);
extern int zwsystem_ipc_getHamiCamInitialInfo(const stGetHamiCamInitialInfoReq *pReq, stGetHamiCamInitialInfoRep *pRep);
extern int zwsystem_ipc_getCamStatusById(const stCamStatusByIdReq *pReq, stCamStatusByIdRep *pRep);
extern int zwsystem_ipc_deleteCameraInfo(const stDeleteCameraInfoReq *pReq, stDeleteCameraInfoRep *pRep);
extern int zwsystem_ipc_setTimezone(const stSetTimezoneReq *pReq, stSetTimezoneRep *pRep);
extern int zwsystem_ipc_getTimezone(const stGetTimezoneReq *pReq, stGetTimezoneRep *pRep);
extern int zwsystem_ipc_getDateTimeInfo(const stDateTimeInfoReq *pReq, stDateTimeInfoRep *pRep);
extern int zwsystem_ipc_setDateTimeInfo(const stDateTimeInfoReq *pReq, stDateTimeInfoRep *pRep);
extern int zwsystem_ipc_updateCameraName(const stUpdateCameraNameReq *pReq, stUpdateCameraNameRep *pRep);
extern int zwsystem_ipc_setCameraOsd(const stSetCameraOsdReq *pReq, stSetCameraOsdRep *pRep);
extern int zwsystem_ipc_setCameraHd(const stSetCameraHdReq *pReq, stSetCameraHdRep *pRep);
extern int zwsystem_ipc_setFlicker(const stSetFlickerReq *pReq, stSetFlickerRep *pRep);
extern int zwsystem_ipc_setImageQuality(const stSetImageQualityReq *pReq, stSetImageQualityRep *pRep);
extern int zwsystem_ipc_setMicrophone(const stSetMicrophoneReq *pReq, stSetMicrophoneRep *pRep);
extern int zwsystem_ipc_setNightMode(const stSetNightModeReq *pReq, stSetNightModeRep *pRep);
extern int zwsystem_ipc_setAutoNightVision(const stSetAutoNightVisionReq *pReq, stSetAutoNightVisionRep *pRep);
extern int zwsystem_ipc_setSpeaker(const stSetSpeakerReq *pReq, stSetSpeakerRep *pRep);
extern int zwsystem_ipc_setFlipUpDown(const stSetFlipUpDownReq *pReq, stSetFlipUpDownRep *pRep);
extern int zwsystem_ipc_setLed(const stSetLedReq *pReq, stSetLedRep *pRep);
extern int zwsystem_ipc_setCameraPower(const stSetCameraPowerReq *pReq, stSetCameraPowerRep *pRep);
extern int zwsystem_ipc_quarySnapshot(const stSnapshotReq *pReq, stSnapshotRep *pRep);
extern int zwsystem_ipc_feedbackSnapshot(const stSnapshotReq *pReq, stSnapshotRep *pRep);
extern int zwsystem_ipc_reboot(const stRebootReq *pReq, stRebootRep *pRep);
extern int zwsystem_ipc_setStorageDay(const stSetStorageDayReq *pReq, stSetStorageDayRep *pRep);
extern int zwsystem_ipc_setEventStorageDay(const stSetEventStorageDayReq *pReq, stSetEventStorageDayRep *pRep);
extern int zwsystem_ipc_formatSdCard(const stFormatSdCardReq *pReq, stFormatSdCardRep *pRep);
extern int zwsystem_ipc_setPtzControlMove(const stPtzMoveReq *pReq, stPtzMoveRep *pRep);
extern int zwsystem_ipc_setPtzAbsoluteMove(const stPtzMoveReq *pReq, stPtzMoveRep *pRep);
extern int zwsystem_ipc_setPtzRelativeMove(const stPtzMoveReq *pReq, stPtzMoveRep *pRep);
extern int zwsystem_ipc_setPtzContinuousMove(const stPtzMoveReq *pReq, stPtzMoveRep *pRep);
extern int zwsystem_ipc_gotoPtzHome(const stPtzMoveReq *pReq, stPtzMoveRep *pRep);
extern int zwsystem_ipc_setPtzSpeed(const stSetPtzSpeedReq *pReq, stSetPtzSpeedRep *pRep);
extern int zwsystem_ipc_getPtzStatus(const stGetPtzStatusReq *pReq, stGetPtzStatusRep *pRep);
extern int zwsystem_ipc_setPtzTourGo(const stPtzTourGoReq *pReq, stPtzTourGoRep *pRep);
extern int zwsystem_ipc_setPtzGoPreset(const stPtzGoPresetReq *pReq, stPtzGoPresetRep *pRep);
extern int zwsystem_ipc_setPtzPresetPoint(const stPtzSetPresetReq *pReq, stPtzSetPresetRep *pRep);
extern int zwsystem_ipc_setPtzHumanTracking(const stPtzSetTrackingReq *pReq, stPtzSetTrackingRep *pRep);
extern int zwsystem_ipc_setPtzPetTracking(const stPtzSetTrackingReq *pReq, stPtzSetTrackingRep *pRep);
extern int zwsystem_ipc_setPtzHome(const stSetPtzHomeReq *pReq, stSetPtzHomeRep *pRep);
extern int zwsystem_ipc_getCameraBindList(const stGetCameraBindListReq *pReq, stGetCameraBindListRep *pRep);
extern int zwsystem_ipc_upgradeCameraOta(const stUpgradeCameraOtaReq *pReq, stUpgradeCameraOtaRep *pRep);
extern int zwsystem_ipc_setCameraAiSetting(const stCameraAiSettingReq *pReq, stCameraAiSettingRep *pRep);
extern int zwsystem_ipc_getCameraAiSetting(const stCameraAiSettingReq *pReq, stCameraAiSettingRep *pRep);
extern int zwsystem_ipc_feedbackRecordEvent(const stRecordEventReq *pReq, stRecordEventRep *pRep);
extern int zwsystem_ipc_feedbackRecognitionEvent(const stRecognitionEventReq *pReq, stRecognitionEventRep *pRep);
extern int zwsystem_ipc_feedbackCameraStatusEvent(const stCameraStatusEventReq *pReq, stCameraStatusEventRep *pRep);
extern int zwsystem_ipc_startVideoLiveStream(const stStartVideoStreamReq *pReq, stStartVideoStreamRep *pRep);
extern int zwsystem_ipc_startVideoHistoryStream(const stStartVideoStreamReq *pReq, stStartVideoStreamRep *pRep);
extern int zwsystem_ipc_stopVideoLiveStream(const stStopVideoLiveStreamReq *pReq, stStopVideoLiveStreamRep *pRep);
extern int zwsystem_ipc_stopVideoHistoryStream(const stStopVideoLiveStreamReq *pReq, stStopVideoLiveStreamRep *pRep);
extern int zwsystem_ipc_startAudioStream(const stStartAudioStreamReq *pReq, stStartAudioStreamRep *pRep);
extern int zwsystem_ipc_stopAudioStream(const stStopAudioStreamReq *pReq, stStopAudioStreamRep *pRep);

#ifdef __cplusplus
}
#endif

#endif /* ZWSYSTEM_IPC_CLIENT_H */
