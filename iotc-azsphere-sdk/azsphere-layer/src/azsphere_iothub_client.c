//
// Copyright: Avnet 2021
// Modified by Alan Low <alan.low@avnet.com> on 6/23/21.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <azure_sphere_provisioning.h>
#include <applibs/eventloop.h>
#include <applibs/networking.h>
#include <applibs/log.h>
#include "azsphere_iothub_client.h"

/******************************************************/
/* Data type definition                               */
/******************************************************/
typedef struct {
    bool used;
    int timer_fd;
    int interval_s;
    EventRegistration* evt_reg;
    void* p_ctx;
    IotHubTimerCallback cb;
} TimerContext;

/******************************************************/
/* Forward declarations                               */
/******************************************************/
static void iothub_poll_handler(void* p_ctx);

/******************************************************/
/* Macros definition                                  */
/******************************************************/
#define M_ARRAY_SIZE(a)                     (sizeof(a)/sizeof(a[0]))

#define M_ADD_TIMER(t)                      do{ \
                                                add_timer(t, iothub_poll_handler, NULL, true); \
                                            }while(0)

#define M_GET_TIMER_INTERVAL()              get_timer_interval(0, true)

#define M_SET_TIMER_INTERVAL(t)             do{ \
                                                set_timer_interval(0, t, true); \
                                            }while (0)

#define M_DEL_TIMER()                       do{ \
                                                delete_timer(0, true); \
                                            }while (0)

/******************************************************/
/* Static definition                                  */
/******************************************************/
#define MAX_TIMERS                          (3 + 1)
#define IOTHUB_POLL_INTERVAL_S              5
#define MAX_DEVICE_TWIN_PAYLOAD_SIZE        (8 * 1024)

/******************************************************/
/* Member variables declaration                       */
/******************************************************/
static IOTHUB_DEVICE_CLIENT_LL_HANDLE m_client_handle = NULL;
static IotHubClientInit m_init = { 0 };
static EventLoop* m_evt_loop = NULL;
static IotHubAuthenticateStatus m_auth_status = StatusNotAuthenticated;
static TimerContext m_timer_ctx[MAX_TIMERS];
static bool m_initialized = false;
static bool m_disconnect_pending = false;

/******************************************************/
/* Helper functions definition                        */
/******************************************************/
static char *print_provisioning_result_string(AZURE_SPHERE_PROV_RETURN_VALUE res) {
    char* res_str = "unknown";
    switch (res.result) {
        case AZURE_SPHERE_PROV_RESULT_OK:
            res_str = "OK";
            break;
        case AZURE_SPHERE_PROV_RESULT_INVALID_PARAM:
            res_str = "Invalid parameter";
            break;
        case AZURE_SPHERE_PROV_RESULT_NETWORK_NOT_READY:
            res_str = "Network is not ready";
            break;
        case AZURE_SPHERE_PROV_RESULT_DEVICEAUTH_NOT_READY:
            res_str = "Device authentication is not ready";
            break;
        case AZURE_SPHERE_PROV_RESULT_PROV_DEVICE_ERROR:
            res_str = "Provisioning device error";
            break;
        case AZURE_SPHERE_PROV_RESULT_IOTHUB_CLIENT_ERROR:
            res_str = "Iothub client error";
            break;
        case AZURE_SPHERE_PROV_RESULT_GENERIC_ERROR:
            res_str = "Generic error";
            break;
    }
    return res_str;
}

static char* print_connection_status_string(IOTHUB_CLIENT_CONNECTION_STATUS_REASON res) {
    char* res_str = "unknown";
    switch (res) {
    case IOTHUB_CLIENT_CONNECTION_EXPIRED_SAS_TOKEN:
        res_str = "SAS token expired";
        break;
    case IOTHUB_CLIENT_CONNECTION_DEVICE_DISABLED:
        res_str = "Device disabled";
        break;
    case IOTHUB_CLIENT_CONNECTION_BAD_CREDENTIAL:
        res_str = "Bad credential";
        break;
    case IOTHUB_CLIENT_CONNECTION_RETRY_EXPIRED:
        res_str = "Retry expired";
        break;
    case IOTHUB_CLIENT_CONNECTION_NO_NETWORK:
        res_str = "No network";
        break;
    case IOTHUB_CLIENT_CONNECTION_COMMUNICATION_ERROR:
        res_str = "Communication error";
        break;
    case IOTHUB_CLIENT_CONNECTION_OK:
        res_str = "OK";
        break;
    case IOTHUB_CLIENT_CONNECTION_NO_PING_RESPONSE:
        res_str = "No ping response";
        break;
    }
    return res_str;
}

static void user_timer_cb(EventLoop* el, int fd, EventLoop_IoEvents events, void* context) {
    int idx = (int)context;
    uint64_t timer_data = 0;
    if (read(fd, &timer_data, sizeof(timer_data)) == -1) {
        Log_Debug("ERROR: Could not read timerfd %s (%d).\n", strerror(errno), errno);
        return;
    }
    if (m_timer_ctx[idx].cb) {
        m_timer_ctx[idx].cb(m_timer_ctx[idx].p_ctx);
    }
}

static int add_timer(int interval_s, IotHubTimerCallback timer_cb, void* p_ctx,
    bool internal_use) {
    if (m_evt_loop == NULL) {
        return 0;
    }
    for (int i = (internal_use ? 0:1); i < M_ARRAY_SIZE(m_timer_ctx); i++) {
        if (m_timer_ctx[i].used == false) {
            m_timer_ctx[i].timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
            if (m_timer_ctx[i].timer_fd == -1) {
                Log_Debug("ERROR: Unable to create timer: %s (%d).\n",
                    strerror(errno), errno);
                return 0;
            }
            struct timespec ts = { .tv_sec = interval_s, .tv_nsec = 0 };
            struct itimerspec new_value = { .it_value = ts,
                                          .it_interval = ts };
            if (timerfd_settime(m_timer_ctx[i].timer_fd, 0, &new_value, NULL) == -1) {
                Log_Debug("ERROR: Could not set timer period: %s (%d).\n",
                    strerror(errno), errno);
                return 0;
            }
            m_timer_ctx[i].evt_reg =
                EventLoop_RegisterIo(m_evt_loop, m_timer_ctx[i].timer_fd,
                    EventLoop_Input, user_timer_cb, (void*)i);
            if (m_timer_ctx[i].evt_reg == NULL) {
                Log_Debug("ERROR: Unable to register timer event: %s (%d).\n",
                    strerror(errno), errno);
                close(m_timer_ctx[i].timer_fd);
                return 0;
            }
            m_timer_ctx[i].interval_s = interval_s;
            m_timer_ctx[i].cb = timer_cb;
            m_timer_ctx[i].p_ctx = p_ctx;
            m_timer_ctx[i].used = true;
            return i + 1;
        } else {
            if (i == 0 && internal_use) {
                return i + 1;
            }
        }
    }
    Log_Debug("ERROR: No timer resource available!\n");
    return 0;
}

static int get_timer_interval(int timer_handle, bool internal_use) {
    int idx = timer_handle - 1;
    if (internal_use) {
        idx = 0;
    } else if ((idx <= 0) || (idx >= M_ARRAY_SIZE(m_timer_ctx))) {
        Log_Debug("Invalid timer_handle!\n");
        return -1;
    }
    if (m_timer_ctx[idx].used) {
        return m_timer_ctx[idx].interval_s;
    }
    return -1;
}

static bool set_timer_interval(int timer_handle, int interval_s, bool internal_use) {
    int idx = timer_handle - 1;
    if (internal_use) {
        idx = 0;
    } else if ((idx <= 0) || (idx >= M_ARRAY_SIZE(m_timer_ctx))) {
        Log_Debug("Invalid timer_handle!\n");
        return false;
    }
    if (m_timer_ctx[idx].used) {
        struct timespec ts = { .tv_sec = interval_s, .tv_nsec = 0 };
        struct itimerspec new_value = { .it_value = ts,
                                      .it_interval = ts };
        if (timerfd_settime(m_timer_ctx[idx].timer_fd, 0, &new_value, NULL) == -1) {
            Log_Debug("ERROR: Could not set timer period: %s (%d).\n",
                strerror(errno), errno);
            return false;
        }
        m_timer_ctx[idx].interval_s = interval_s;
        return true;
    }
    return false;
}

static void delete_timer(int timer_handle, bool internal_use) {
    int idx = timer_handle - 1;
    if (internal_use) {
        idx = 0;
    } else if ((idx <= 0) || (idx >= M_ARRAY_SIZE(m_timer_ctx))) {
        Log_Debug("Invalid timer_handle!\n");
        return;
    }
    if (m_timer_ctx[idx].used) {
        EventLoop_UnregisterIo(m_evt_loop, m_timer_ctx[idx].evt_reg);
        close(m_timer_ctx[idx].timer_fd);
        m_timer_ctx[idx].used = false;
    }
}

static void on_device_twin_cb(DEVICE_TWIN_UPDATE_STATE update_state,
                              const unsigned char* payload,
                              size_t payload_len,
                              void* user_context_cb) {
    if (payload_len > MAX_DEVICE_TWIN_PAYLOAD_SIZE) {
        Log_Debug("ERROR: Device twin payload size (%u bytes) exceeds maximum (%u bytes).\n",
            payload_len, MAX_DEVICE_TWIN_PAYLOAD_SIZE);
        return;
    }
    if (m_init.twin_msg_cb)    {
        m_init.twin_msg_cb((unsigned char *)payload, payload_len);
    }
}

static IOTHUBMESSAGE_DISPOSITION_RESULT on_recv_msg_cb(IOTHUB_MESSAGE_HANDLE message,
    void* context) {
    const unsigned char* buffer = NULL;
    size_t size = 0;
    if (IoTHubMessage_GetByteArray(message, &buffer, &size) != IOTHUB_MESSAGE_OK) {
        Log_Debug("WARNING: failure performing IoTHubMessage_GetByteArray\n");
        return IOTHUBMESSAGE_REJECTED;
    }
    if (m_init.recv_msg_cb) {
        m_init.recv_msg_cb((unsigned char *)buffer, size);
    }
    return IOTHUBMESSAGE_ACCEPTED;
}

static void on_connect_status_cb(IOTHUB_CLIENT_CONNECTION_STATUS result,
                                 IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason,
                                 void* user_context_cb) {

    Log_Debug("IoTHub connection status: %s\n", print_connection_status_string(reason));
    if (result != IOTHUB_CLIENT_CONNECTION_AUTHENTICATED) {
        if (m_auth_status == StatusAuthenticated) {
            Log_Debug("IoTHub auth status: Not authenticated\n");
            m_auth_status = StatusNotAuthenticated;
            if (m_timer_ctx[0].used) {
                M_SET_TIMER_INTERVAL(1);
            } else {
                M_ADD_TIMER(1);
            }
        }
    } else {
        if (m_auth_status != StatusAuthenticated) {
            Log_Debug("IoTHub auth status: Authenticated\n");
            m_auth_status = StatusAuthenticated;
        }
    }
    if (m_init.auth_status_cb) {
        m_init.auth_status_cb(m_auth_status);
    }
}

static void on_send_evt_cb(IOTHUB_CLIENT_CONFIRMATION_RESULT result,
    void* context) {
    Log_Debug("INFO: Send status code %d.\n", result);
}

static void setup_azure_iot_client(void) {
    if (m_client_handle != NULL) {
        IoTHubDeviceClient_LL_Destroy(m_client_handle);
    }
    AZURE_SPHERE_PROV_RETURN_VALUE prov_res =
        IoTHubDeviceClient_LL_CreateWithAzureSphereDeviceAuthProvisioning(m_init.scope_id,
            10000, &m_client_handle);
    Log_Debug("IoTHub provisioning result: %s\n",
        print_provisioning_result_string(prov_res));
    if (prov_res.result == AZURE_SPHERE_PROV_RESULT_OK) {
        m_auth_status = StatusInitiated;
        IoTHubDeviceClient_LL_SetMessageCallback(m_client_handle, on_recv_msg_cb, NULL);
        IoTHubDeviceClient_LL_SetDeviceTwinCallback(m_client_handle, on_device_twin_cb, NULL);
        IoTHubDeviceClient_LL_SetConnectionStatusCallback(m_client_handle, on_connect_status_cb,
            NULL);
    }
}

static void iothub_poll_handler(void *p_ctx) {
    bool is_networking_ready = false;
    bool need_report = false;
    if (M_GET_TIMER_INTERVAL() == 1) {
        M_SET_TIMER_INTERVAL(IOTHUB_POLL_INTERVAL_S);
    }
    if ((Networking_IsNetworkingReady(&is_networking_ready) == -1) || !is_networking_ready) {
        if (m_auth_status == StatusAuthenticated) {
            m_auth_status = StatusNotAuthenticated;
            need_report = true;
            Log_Debug("WARNING: Network down. Device need to re-authenticate when network is up.\n");
        } else {
            Log_Debug("WARNING: Network is not ready. Device cannot connect until network is ready.\n");
        }
        goto handler_end;
    }
    Networking_InterfaceConnectionStatus status;
    if (Networking_GetInterfaceConnectionStatus(m_init.netif, &status) == 0) {
        if (status & Networking_InterfaceConnectionStatus_ConnectedToInternet) {
            if (m_auth_status == StatusNotAuthenticated) {
                setup_azure_iot_client();
            }
        } else {
            if (m_auth_status == StatusAuthenticated) {
                m_auth_status = StatusNotAuthenticated;
                need_report = true;
            }
        }
    } else {
        if (m_auth_status == StatusAuthenticated) {
            m_auth_status = StatusNotAuthenticated;
            need_report = true;
        }
        if (errno != EAGAIN) {
            m_auth_status = StatusInitiateError;
            need_report = true;
            Log_Debug("ERROR: Networking_GetInterfaceConnectionStatus: %d (%s)\n", errno,
                strerror(errno));
        }
    }
handler_end:
    if (need_report && m_init.auth_status_cb) {
        m_init.auth_status_cb(m_auth_status);
    }
}

/********************************************************************************************/
/* IotHub client functions definition                                                       */
/********************************************************************************************/

IotHubClientReturnCode iothub_client_init(IotHubClientInit *p_init) {

    if (m_initialized) {
        Log_Debug("ERROR: IoTHub client already initialized!\n");
        return CodeInvalidState;
    }
    if (!p_init) {
        Log_Debug("ERROR: p_init is NULL!\n");
        return CodeInvalidParam;
    }
    memcpy(&m_init, p_init, sizeof(IotHubClientInit));
    if (m_evt_loop) {
        EventLoop_Close(m_evt_loop);
    }
    m_evt_loop = EventLoop_Create();
    if (m_evt_loop == NULL) {
        Log_Debug("ERROR: Unable to create event loop!\n");
        return CodeResourceNotAvailable;
    }
    for (int i = 0; i < M_ARRAY_SIZE(m_timer_ctx); i++)    {
        m_timer_ctx[i].used = false;
    }
    m_initialized = true;
    return CodeSuccess;
}

IotHubClientReturnCode iothub_client_connect(void) {
    if (!m_initialized) {
        Log_Debug("ERROR: IoTHub client not initialize!\n");
        return CodeInvalidState;
    }
    if (m_auth_status == StatusAuthenticated) {
    Log_Debug("ERROR: IoTHub client already connected!\n");
    return CodeInvalidState;
    }
    if (m_timer_ctx[0].used) {
        M_SET_TIMER_INTERVAL(1);
    } else {
        M_ADD_TIMER(1);
    }
    return CodeSuccess;
}

IotHubClientReturnCode iothub_client_send_message(const char* p_msg,
    const char* p_content_type, const char* p_content_encoding) {
    IOTHUB_MESSAGE_HANDLE msg_handle;
    if (!m_initialized) {
        Log_Debug("ERROR: IoTHub client not initialize!\n");
        return CodeInvalidState;
    }
    if (m_auth_status != StatusAuthenticated) {
        Log_Debug("ERROR: IoTHub client not connected!\n");
        return CodeInvalidState;
    }
    //Log_Debug("msg => %s\n", p_msg);
    msg_handle = IoTHubMessage_CreateFromString(p_msg);
    if (msg_handle == 0) {
        Log_Debug("ERROR: unable to create a new IoTHubMessage.\n");
        return CodeResourceNotAvailable;
    }
    IoTHubMessage_SetContentTypeSystemProperty(msg_handle, p_content_type);
    IoTHubMessage_SetContentEncodingSystemProperty(msg_handle, p_content_encoding);
    // Attempt to send the message we created
    if (IoTHubDeviceClient_LL_SendEventAsync(m_client_handle, msg_handle, on_send_evt_cb,
        /*&callback_param*/ NULL) != IOTHUB_CLIENT_OK) {
        Log_Debug("ERROR: failure sending message to Iothub.\n");
    }
    // Cleanup
    IoTHubMessage_Destroy(msg_handle);
    return CodeSuccess;
}

IotHubClientReturnCode iothub_client_run(int timeout_ms) {
    if (!m_initialized) {
        Log_Debug("ERROR: IoTHub client not initialize!\n");
        return CodeInvalidState;
    }
    EventLoop_Run_Result result = EventLoop_Run(m_evt_loop, timeout_ms, true);
    // Continue if interrupted by signal, e.g. due to breakpoint being set.
    if (result == EventLoop_Run_Failed && errno != EINTR) {
        return CodeRunFailed;
    }
    if (m_client_handle != NULL) {
        IoTHubDeviceClient_LL_DoWork(m_client_handle);
        if (m_disconnect_pending) {
            m_disconnect_pending = false;
            IoTHubDeviceClient_LL_Destroy(m_client_handle);
            m_client_handle = NULL;
            m_auth_status = StatusNotAuthenticated;
            if (m_init.auth_status_cb) {
                m_init.auth_status_cb(m_auth_status);
            }
        }
    }
    return CodeSuccess;
}

IotHubClientReturnCode iothub_client_disconnect(void) {
    if (!m_initialized) {
        Log_Debug("ERROR: IoTHub client not initialize!\n");
        return CodeInvalidState;
    }
    if (m_client_handle) {
        m_disconnect_pending = true;
    }
    return CodeSuccess;
}

IotHubClientReturnCode iothub_client_add_timer(int interval_s, IotHubTimerCallback timer_cb,
    void *p_ctx, int *p_timer_handle) {
    if (!m_initialized) {
        Log_Debug("ERROR: IoTHub client not initialize!\n");
        return CodeInvalidState;
    }
    int hndl = add_timer(interval_s, timer_cb, p_ctx, false);
    if (hndl == 0) {
        Log_Debug("ERROR: Unable to add timer!\n");
        return CodeResourceNotAvailable;
    }
    *p_timer_handle = hndl;
    return CodeSuccess;
}

IotHubClientReturnCode iothub_client_get_timer_interval(int timer_handle, int *p_interval) {
    if (!m_initialized) {
        Log_Debug("ERROR: IoTHub client not initialize!\n");
        return CodeInvalidState;
    }
    int interval = get_timer_interval(timer_handle, false);
    if (interval == -1) {
        Log_Debug("ERROR: Unable to get timer interval for handle %d!\n", timer_handle);
        return CodeResourceNotAvailable;
    }
    *p_interval = interval;
    return CodeSuccess;
}

IotHubClientReturnCode iothub_client_set_timer_interval(int timer_handle, int interval_s) {
    if (!m_initialized) {
        Log_Debug("ERROR: IoTHub client not initialize!\n");
        return CodeInvalidState;
    }
    if (set_timer_interval(timer_handle, interval_s, false) == false) {
        Log_Debug("ERROR: Unable to set timer interval for handle %d!\n", timer_handle);
        return CodeResourceNotAvailable;
    }
    return CodeSuccess;
}

IotHubClientReturnCode iothub_client_delete_timer(int timer_handle) {
    if (!m_initialized) {
        Log_Debug("ERROR: IoTHub client not initialize!\n");
        return CodeInvalidState;
    }
    delete_timer(timer_handle, false);
    return CodeSuccess;
}

IotHubClientReturnCode iothub_client_uninit(void) {
    if (!m_initialized) {
        Log_Debug("ERROR: IoTHub client not initialize!\n");
        return CodeInvalidState;
    }
    if (m_client_handle) {
        Log_Debug("ERROR: IoTHub client handle is not NULL!\n");
        return CodeInvalidState;
    }
    if (m_evt_loop) {
        for (int i = 0; i < M_ARRAY_SIZE(m_timer_ctx); i++) {
            if (m_timer_ctx[i].used) {
                iothub_client_delete_timer(i + 1);
            }
        }
        EventLoop_Close(m_evt_loop);
        m_evt_loop = NULL;
    }
    m_initialized = false;
    return CodeSuccess;
}
