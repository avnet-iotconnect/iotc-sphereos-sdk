/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

// This sample C application demonstrates how to interface Azure Sphere devices with Azure IoT
// services. Using the Azure IoT SDK C APIs, it shows how to:
// 1. Use Device Provisioning Service (DPS) to connect to Azure IoT Hub/Central with
// certificate-based authentication
// 2. Use X.509 Certificate Authority (CA) certificates to authenticate devices connecting directly
// to Azure IoT Hub
// 3. Use X.509 Certificate Authority (CA) certificates to authenticate devices connecting to an
// IoT Edge device.
// 4. Use Azure IoT Hub messaging to upload simulated temperature measurements and to signal button
// press events
// 5. Use Device Twin to receive desired LED state from the Azure IoT Hub
// 6. Use Direct Methods to receive a "Trigger Alarm" command from Azure IoT Hub/Central
//
// It uses the following Azure Sphere libraries:
// - eventloop (system invokes handlers for timer events)
// - gpio (digital input for button, digital output for LED)
// - log (displays messages in the Device Output window during debugging)
// - networking (network interface connection status)
// - storage (device storage interaction)
//
// You will need to provide information in the 'CmdArgs' section of the application manifest to
// use this application. Please see README.md for full details.

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// applibs_versions.h defines the API struct versions to use for applibs APIs.
#include "applibs_versions.h"
#include <applibs/eventloop.h>
#include <applibs/log.h>
#include <applibs/networking.h>
#include <arpa/inet.h>

// The following #include imports a "sample appliance" definition. This app comes with multiple
// implementations of the sample appliance, each in a separate directory, which allow the code to
// run on different hardware.
//
// By default, this app targets hardware that follows the MT3620 Reference Development Board (RDB)
// specification, such as the MT3620 Dev Kit from Seeed Studio.
//
// To target different hardware, you'll need to update CMakeLists.txt. For example, to target the
// Avnet MT3620 Starter Kit, change the TARGET_DIRECTORY argument in the call to
// azsphere_target_hardware_definition to "HardwareDefinitions/avnet_mt3620_sk".
//
// See https://aka.ms/AzureSphereHardwareDefinitions for more details.
#include <hw/sample_appliance.h>
#include "eventloop_timer_utilities.h"
#include "iotConnect.h"
#include "exit_codes.h"

#define TELEMETRY_BUFFER_SIZE       256

typedef enum {

    connection_type_dps,
    connection_type_direct,
    connection_type_iotedge

} connection_type_t;

volatile sig_atomic_t exit_code = ExitCode_Success;
static connection_type_t connection_type;
static char *scope_id = NULL;  // ScopeId for DPS.
static char *hostname = NULL;
static char* iotedge_root_ca_path = NULL; // Path (including filename) of the IotEdge cert.
static const char network_interface[] = "wlan0";
EventLoop *app_evt_loop = NULL;
static EventLoopTimer *app_timer = NULL;
static const int send_telemetry_interval_s = 5;         // only send telemetry 1/5 of polls
static const int stay_connected_duration_s = (5 * 60);
static int app_poll_period_s = 1;

/// <summary>
///     Signal handler for termination requests. This handler must be async-signal-safe.
/// </summary>
static void TerminationHandler(int signalNumber) {
    // Don't use Log_Debug here, as it is not guaranteed to be async-signal-safe.
    exit_code = ExitCode_TermHandler_SigTerm;
}

/// <summary>
///     Parse the command line arguments given in the application manifest.
/// </summary>
static void parse_cmdline_args(int argc, char* argv[]) {
    int option = 0;
    static const struct option cmdLineOptions[] = {
        {.name = "ConnectionType", .has_arg = required_argument, .flag = NULL, .val = 'c'},
        {.name = "ScopeID", .has_arg = required_argument, .flag = NULL, .val = 's'},
        {.name = "Hostname", .has_arg = required_argument, .flag = NULL, .val = 'h'},
        {.name = "IoTEdgeRootCAPath", .has_arg = required_argument, .flag = NULL, .val = 'i'},
        {.name = NULL, .has_arg = 0, .flag = NULL, .val = 0} };

    // Loop over all of the options.
    while ((option = getopt_long(argc, argv, "c:s:h:i:", cmdLineOptions, NULL)) != -1) {
        // Check if arguments are missing. Every option requires an argument.
        if (optarg != NULL && optarg[0] == '-') {
            Log_Debug("WARNING: Option %c requires an argument\n", option);
            continue;
        }
        switch (option) {
        case 'c':
            Log_Debug("ConnectionType: %s\n", optarg);
            if (strcmp(optarg, "DPS") == 0) {
                connection_type = connection_type_dps;
            }
            else if (strcmp(optarg, "Direct") == 0) {
                connection_type = connection_type_direct;
            }
            else if (strcmp(optarg, "IoTEdge") == 0) {
                connection_type = connection_type_iotedge;
            }
            break;
        case 's':
            Log_Debug("ScopeID: %s\n", optarg);
            scope_id = optarg;
            break;
        case 'h':
            Log_Debug("Hostname: %s\n", optarg);
            hostname = optarg;
            break;
        case 'i':
            Log_Debug("IoTEdgeRootCAPath: %s\n", optarg);
            iotedge_root_ca_path = optarg;
            break;
        default:
            // Unknown options are ignored.
            break;
        }
    }
}
/// <summary>
///     Validates if the values of the Connection type, Scope ID, IoT Hub or IoT Edge Hostname were set.
/// </summary>
/// <returns>
///     ExitCode_Success if the parameters were provided; otherwise another
///     ExitCode value which indicates the specific failure.
/// </returns>
static ExitCode ValidateUserConfiguration(void) {
    ExitCode validation_exit_code = ExitCode_Success;

    if (connection_type != connection_type_dps) {
        Log_Debug("The app only support DPS connection!\r\n");
        validation_exit_code = ExitCode_Validate_ConnectionType;
    }

    if (scope_id == NULL) {
        validation_exit_code = ExitCode_Validate_ScopeId;
        Log_Debug("scope id is NULL!\r\n");
    }
    else {
        Log_Debug("Using DPS Connection: Azure IoT DPS Scope ID %s\n", scope_id);
    }

    return validation_exit_code;
}

/// <summary>
///     Generate simulated telemetry and send to IoTConnect.
/// </summary>
void SendSimulatedTelemetry(void) {
    // Generate a simulated temperature.
    static float temperature = 50.0f;
    // Generate a simulated humidity.
    static float humidity = 120.0f;
    float delta = ((float)(rand() % 41)) / 20.0f - 1.0f; // between -1.0 and +1.0
    temperature += delta;
    delta = ((float)(rand() % 41)) / 20.0f - 1.0f; // between -1.0 and +1.0
    humidity += delta;

    IotclMessageHandle msg_hndl = iotcl_telemetry_v2_create();

    if (msg_hndl == NULL) {
        Log_Debug("Unable to create V2 telemetry message!\r\n");
        return;
    }

    char tmp_str[10];

    // Abjust to 1 decimal point.
    snprintf(tmp_str, sizeof(tmp_str), "%0.1f", temperature);
    // Add temperature to telemetry
    iotcl_telemetry_set_number(msg_hndl, "temperature", strtod(tmp_str, NULL));
    // Abjust to 2 decimal point.
    snprintf(tmp_str, sizeof(tmp_str), "%0.2f", humidity);
    // Add humidity to telemetry
    iotcl_telemetry_set_number(msg_hndl, "humidity", strtod(tmp_str, NULL));

    const char* p_msg = iotcl_create_serialized_string(msg_hndl, false);

    if (p_msg == NULL) {
        Log_Debug("Unable to get serialized string!\r\n");
    } else {
        Log_Debug("Send telemetry: Temperature %0.1f, Humidity %0.2f\n",
            temperature, humidity);
        iotconnect_sdk_send_packet(p_msg);
        iotcl_destroy_serialized(p_msg);
    }

    iotcl_telemetry_destroy(msg_hndl);
}

/// <summary>
///     App timer event: send simulated telemetry once connected to IoTConnect.
/// </summary>
static void app_timer_handler(EventLoopTimer* timer) {
    static bool first_msg = true;
    static int telemetry_count = 0;
    static int duration_count = 0;

    if (ConsumeEventLoopTimerEvent(timer) != 0) {
        exit_code = ExitCode_AppTimer_Consume;
        return;
    }
    // Check whether the device is connected to the internet.
    if (iotconnect_sdk_is_connected()) {

		telemetry_count += app_poll_period_s;
		duration_count += app_poll_period_s;
		
        if (first_msg || (telemetry_count >= send_telemetry_interval_s)) {
            if (first_msg) {
                first_msg = false;
            }
            SendSimulatedTelemetry();
            telemetry_count = 0;
        }
        if (duration_count >= stay_connected_duration_s) {
            //Times up. Stop sending.
            exit_code = ExitCode_TermHandler_SigTerm;
        }
    }
}

/// <summary>
///     Set up SIGTERM termination handler, initialize peripherals, and set up event handlers.
/// </summary>
/// <returns>
///     ExitCode_Success if all resources were allocated successfully; otherwise another
///     ExitCode value which indicates the specific failure.
/// </returns>
static ExitCode InitPeripheralsAndHandlers(void) {
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = TerminationHandler;
    sigaction(SIGTERM, &action, NULL);

    app_evt_loop = EventLoop_Create();
    if (app_evt_loop == NULL) {
        Log_Debug("Could not create event loop.\n");
        return ExitCode_Init_EventLoop;
    }
    struct timespec ts = { .tv_sec = app_poll_period_s, .tv_nsec = 0 };
    app_timer = CreateEventLoopPeriodicTimer(app_evt_loop, &app_timer_handler, &ts);
    if (app_timer == NULL) {
        return ExitCode_Init_AppTimer;
    }
    IotConnectAzsphereConfig iotconnect_cfg = {
        .p_netif = network_interface,
        .p_scope_id = scope_id
    };
    if (iotconnect_sdk_init(&iotconnect_cfg) != 0) {
        return ExitCode_Init_IotConnect_Sdk;
    }
    return ExitCode_Success;
}

/// <summary>
/// ....Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void) {
    iotconnect_sdk_disconnect();
    DisposeEventLoopTimer(app_timer);
    EventLoop_Close(app_evt_loop);
}

/// <summary>
///     Main entry point for this sample.
/// </summary>
int main(int argc, char *argv[]) {
    Log_Debug("Azure Sphere basic sample starting.\n");
    parse_cmdline_args(argc, argv);
    exit_code = ValidateUserConfiguration();
    if (exit_code != ExitCode_Success) {
        return exit_code;
    }
    exit_code = InitPeripheralsAndHandlers();

    // Main loop
    while (exit_code == ExitCode_Success) {
        EventLoop_Run_Result result = EventLoop_Run(app_evt_loop, 50, true);
        // Continue if interrupted by signal, e.g. due to breakpoint being set.
        if (result == EventLoop_Run_Failed && errno != EINTR) {
            exit_code = ExitCode_Main_EventLoopFail;
        }
        if (iotconnect_sdk_poll(50) != IOTC_SDK_SUCCESS) {
            exit_code = ExitCode_IotConnect_Sdk_EventLoopFail;
        }
    }
    ClosePeripheralsAndHandlers();
    Log_Debug("Application exiting.\n");
    return exit_code;
}

