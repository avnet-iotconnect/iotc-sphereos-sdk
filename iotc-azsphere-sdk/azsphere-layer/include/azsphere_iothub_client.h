//
// Copyright: Avnet 2021
// Modified by Alan Low <alan.low@avnet.com> on 6/23/21.
//

#ifndef AZSPHERE_IOTHUB_CLIENT_H
#define AZSPHERE_IOTHUB_CLIENT_H

typedef enum {
    CodeSuccess = 0,
    CodeInvalidParam,
    CodeInvalidState,
    CodeResourceNotAvailable,
    CodeRunFailed,
    CodeInternalError
} IotHubClientReturnCode;

typedef enum {
    StatusNotAuthenticated = 0,
    StatusInitiateError,
    StatusInitiated,
    StatusAuthenticated
} IotHubAuthenticateStatus;

typedef void (*IotHubAuthenticateStatusCallback)(IotHubAuthenticateStatus status);
typedef void (*IotHubReceiveMessageCallback)(unsigned char* p_msg, size_t msg_len);
typedef void (*IotHubTwinMessageCallback)(unsigned char* p_twin_msg, size_t msg_len);
typedef void (*IotHubTimerCallback)(void* p_context);

typedef struct {
    char netif[10];
    char scope_id[30];
    IotHubAuthenticateStatusCallback auth_status_cb;
    IotHubReceiveMessageCallback recv_msg_cb;
    IotHubTwinMessageCallback twin_msg_cb;
} IotHubClientInit;

// Functions declarations
IotHubClientReturnCode iothub_client_init(IotHubClientInit* p_init);
IotHubClientReturnCode iothub_client_connect(void);
IotHubClientReturnCode iothub_client_send_message(const char* p_msg, const char* p_content_type,
    const char* p_content_encoding);
IotHubClientReturnCode iothub_client_run(int timeout_ms);
IotHubClientReturnCode iothub_client_disconnect(void);
IotHubClientReturnCode iothub_client_add_timer(int interval_s,
IotHubTimerCallback timer_cb, void* p_ctx, int *p_timer_handle);
IotHubClientReturnCode iothub_client_get_timer_interval(int timer_handle, int *p_interval);
IotHubClientReturnCode iothub_client_set_timer_interval(int timer_handle, int interval_s);
IotHubClientReturnCode iothub_client_delete_timer(int timer_handle);

#endif //AZSPHERE_IOTHUB_CLIENT_H