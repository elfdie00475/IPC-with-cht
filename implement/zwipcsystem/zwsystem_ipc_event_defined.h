#ifndef ZWSYSTEM_IPC_EVENT_DEFINED_H
#define ZWSYSTEM_IPC_EVENT_DEFINED_H

#ifdef __cplusplus
extern "C" {
#endif

#define ZS_IPC_EVENT_TAG_LEN 32
#define ZS_IPC_EVNET_RECORED_PREFIX         "rec."
#define ZS_IPC_EVENT_RECORED_STARTED        "rec.started"
#define ZS_IPC_EVENT_RECORED_ENDED          "rec.ended"
#define ZS_IPC_EVENT_RECORED_CONFIG_CHANGED "rec.config_changed"
#define ZS_IPC_EVENT_RECORED_ERROR          "rec.error"
#define ZS_IPC_EVNET_STORAGE_PREFIX         "stor."
#define ZS_IPC_EVENT_STORAGE_STATUS         "stor.status"
#define ZS_IPC_EVENT_STORAGE_ERROR          "stor.error"
// maybe into status event
//#define ZS_IPC_EVENT_STORAGE_LOW            "stor.low"
//#define ZS_IPC_EVENT_STORAGE_FULL           "stor.full"
//#define ZS_IPC_EVENT_STORAGE_FORMAT_STARTED "stor.format_started"
//#define ZS_IPC_EVENT_STORAGE_FORMAT_ENDED   "stor.format_ended"
//#define ZS_IPC_EVENT_STORAGE_AUTO_REMOVED   "stor.auto_removed"

// struct
// event header
// -- event tag
// -- event seq id
// -- event UTC (local time) string, include offset, e.g. 2026/02/03 15:34:04 +08:00
// -- event monotonic time（ns/us), epoch time
// -- event payload size (msg header)
// msg header
// -- msg fourCC
// -- msg header size
// -- msg header array
// -- msg payload size (data payload)
// data payload (custom struct)

#ifdef __cplusplus
}
#endif
#endif /* ZWSYSTEM_IPC_EVENT_DEFINED_H */
