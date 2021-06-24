//
// Copyright: Avnet, Softweb Inc. 2020
// Modified by Nik Markovic <nikola.markovic@avnet.com> on 6/15/20.
// Modified by Alan Low <alan.low@avnet.com> on 6/22/21.
//
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <applibs/log.h>
#include "azsphere_iothub_client.h"
#include "iotconnect.h"

/******************************************************/
/* Static definition                                  */
/******************************************************/
#define SEND_HELLO_INTERVAL_S               15 //secs

/******************************************************/
/* Member variables declaration                       */
/******************************************************/
static IotConnectClientConfig config = { 0 };
static IotclConfig lib_config = { 0 };
static bool iothub_authenticated = false;
static bool iotconnect_connected = false;
static char sid_str[80] = "";
static char dtg_str[80] = "";
static int timer_hndl = 0;

/******************************************************/
/* Helper functions definition                        */
/******************************************************/
static void send_hello_msg(void) {
    Log_Debug("send hello message to iotconnect...\n");
    strcpy(sid_str, "");
    strcpy(dtg_str, "");
    char* hello_request = iotcl_request_create_hello();
    iotconnect_sdk_send_packet(hello_request);
    free(hello_request);
}

/******************************************************/
/* Callback functions definition                        */
/******************************************************/
static void on_timer_cb(void* p_ctx) {
    if (!iotconnect_connected) {
        send_hello_msg();
    } else {
        iothub_client_delete_timer(timer_hndl);
    }
}

// this function will Give you Device CallBack payload
static void on_iothub_data(unsigned char *data, size_t len) {
    char *str = malloc(len + 1);
    memcpy(str, data, len);
    str[len] = 0;
    Log_Debug("event>>> %s\n", str);
    if (!iotcl_process_event(str)) {
        Log_Debug("Error encountered while processing %s\n", str);
    }
    free(str);
}

static void on_iotconnect_status(IotHubAuthenticateStatus status) {
    if (status == StatusAuthenticated) {
        iothub_authenticated = true;
        if (!iotconnect_connected) {
            send_hello_msg();
            if (iothub_client_add_timer(SEND_HELLO_INTERVAL_S,
                on_timer_cb, NULL, &timer_hndl) != CodeSuccess) {
                printf("Unable to add a iothub client timer!\n");
            }
        }
    } else {
        iothub_authenticated = false;
        iotconnect_connected = false;
        if (config.status_cb) {
            config.status_cb(IOTCONNECT_DISCONNECTED);
        }
    }
}

static void on_message_intercept(IotclEventData data, IotclEventType type) {
    switch (type) {
    case ON_CLOSE:
        Log_Debug("Got a disconnect request. Closing connection.\n");
        iotconnect_sdk_disconnect();
    default:
        break; // not handling nay other messages
    }
    if (NULL != config.msg_cb) {
        config.msg_cb(data, type);
    }
}

static void on_hello_response(IotclEventData data, IotclEventType type) {
    switch (type) {
    case REQ_HELLO: {
        char* p_str = iotcl_clone_response_sid(data);
        if (NULL != p_str) {
            strcpy((char *)lib_config.request.sid, p_str);
            free(p_str);
            Log_Debug("Hello reponse SID is %s\n", lib_config.request.sid);
            p_str = iotcl_clone_response_dtg(data);
            if (NULL != p_str) {
                strcpy((char *)lib_config.telemetry.dtg, p_str);
                free(p_str);
                Log_Debug("Hello reponse DTG is %s\n", lib_config.telemetry.dtg);
            } else {
                Log_Debug("Error from hello response. SID is null.\n");
            }
        } else {
            Log_Debug("Error from hello response. SID is null.\n");
        }
        if (strlen(lib_config.request.sid) > 0 && strlen(lib_config.telemetry.dtg) > 0) {
            Log_Debug("connected to iotconnect.\n");
            iotconnect_connected = true;
            if (config.status_cb) {
                config.status_cb(IOTCONNECT_CONNECTED);
            }
        }
    }
    break;
    default:
        Log_Debug("Warning: Received an unknown hello response type %d\n", type);
    }
}

/******************************************************/
/* IoTConnect SDK functions definition                */
/******************************************************/
void iotconnect_sdk_disconnect() {
    Log_Debug("Disconnecting...\n");
    iothub_client_disconnect();
    iothub_authenticated = false;
    iotconnect_connected = false;
    if (config.status_cb) {
        config.status_cb(IOTCONNECT_DISCONNECTED);
    }
}

void iotconnect_sdk_send_packet(const char *data) {
    if (iothub_client_send_message(data, "application%2fjson", "utf-8") != 
        CodeSuccess) {
        Log_Debug("Failed to send message: %s\n", data);
    }
}

IotclConfig* iotconnect_sdk_get_lib_config() {
    return iotcl_get_config();
}

IotConnectClientConfig* iotconnect_sdk_init_and_get_config() {
    memset(&config, 0, sizeof(config));
    return &config;
}

bool iotconnect_sdk_is_connected() {
    return iotconnect_connected;
}

unsigned int iotconnect_sdk_poll(int timeout_ms) {
    if (iothub_client_run(timeout_ms) != CodeSuccess) {
        return IOTC_SDK_RUN_FAIL;
    }
    return IOTC_SDK_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////
// this the Initialization on IoTConnect SDK
unsigned int iotconnect_sdk_init(IotConnectAzsphereConfig *p_cfg) {
    IotHubClientInit iothub_cli_init;
    strcpy(iothub_cli_init.netif, p_cfg->p_netif);
    strcpy(iothub_cli_init.scope_id, p_cfg->p_scope_id);
    iothub_cli_init.recv_msg_cb = on_iothub_data;
    iothub_cli_init.auth_status_cb = on_iotconnect_status;
    iothub_cli_init.twin_msg_cb = NULL;
    if (iothub_client_init(&iothub_cli_init) != CodeSuccess) {
        Log_Debug("Failed to initialize Azure Sphere IoTHub client\n");
        return IOTC_SDK_IOTHUB_INIT_FAIL;
    }
    lib_config.device.cpid = "unused";
    lib_config.device.env = "unused";
    lib_config.device.duid = "unused";
    lib_config.event_functions.ota_cb = NULL; //azsphere have its own OTA machanisim.
    lib_config.event_functions.cmd_cb = config.cmd_cb;
    lib_config.event_functions.msg_cb = on_message_intercept;
    lib_config.event_functions.response_cb = on_hello_response;
    // TODO: deal with workaround for telemetry config
    lib_config.telemetry.dtg = dtg_str;
    lib_config.request.sid = sid_str;
    if (!iotcl_init(&lib_config)) {
        Log_Debug("Failed to initialize the IoTConnect Lib\n");
        return IOTC_SDK_IOTCONNECT_INT_FAIL;
    }
    if (iothub_client_connect() != CodeSuccess) {
        Log_Debug("Failed to connect!\n");
        return IOTC_SDK_CONNECT_INIT_FAIL;
    }
    return IOTC_SDK_SUCCESS;
}
