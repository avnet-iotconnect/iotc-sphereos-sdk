//
// Copyright: Avnet, Softweb Inc. 2020
// Modified by Nik Markovic <nikola.markovic@avnet.com> on 6/15/20.
// Modified by Alan Low <alan.low@avnet.com> on 6/23/21.
//

#ifndef IOTCONNECT_H
#define IOTCONNECT_H

#include "iotconnect_common.h"
#include "iotconnect_event.h"
#include "iotconnect_telemetry.h"
#include "iotconnect_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IOTC_SDK_SUCCESS                          0
#define IOTC_SDK_IOTHUB_INIT_FAIL                 1
#define IOTC_SDK_IOTCONNECT_INIT_FAIL             2
#define IOTC_SDK_CONNECT_INIT_FAIL                3
#define IOTC_SDK_RUN_FAIL                         4
#define IOTC_SDK_INVALID_STATE                    5

typedef enum {
    UNDEFINED,
    IOTCONNECT_CONNECTED,
    IOTCONNECT_DISCONNECTED,
    // MQTT_FAILED, this status is not applicable to Azure Sphere implementation
    // TODO: Sync statuses etc.
} IotConnectConnectionStatus;

typedef void (*IotConnectStatusCallback)(IotConnectConnectionStatus data);

typedef struct {
    const char *p_netif;
    const char* p_scope_id;
} IotConnectAzsphereConfig;

typedef struct {
    char *env;    // Environment name. Contact your representative for details.
    char *cpid;   // Settings -> Company Profile.
    char *duid;   // Name of the device.
    IotclOtaCallback ota_cb; // callback for OTA events.
    IotclCommandCallback cmd_cb; // callback for command events.
    IotclMessageCallback msg_cb; // callback for ALL messages, including the specific ones like cmd or ota callback.
    IotConnectStatusCallback status_cb; // callback for connection status
} IotConnectClientConfig;

IotConnectClientConfig *iotconnect_sdk_init_and_get_config(void);

unsigned int iotconnect_sdk_init(IotConnectAzsphereConfig *p_cfg);

bool iotconnect_sdk_is_connected(void);

IotclConfig *iotconnect_sdk_get_lib_config(void);

void iotconnect_sdk_send_packet(const char *data);

unsigned int iotconnect_sdk_poll(int wait_time_ms);

void iotconnect_sdk_disconnect(void);

#ifdef __cplusplus
}
#endif

#endif
