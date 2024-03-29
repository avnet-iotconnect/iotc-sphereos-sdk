#  Copyright (c) Microsoft Corporation. All rights reserved.
#  Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

project(basic-sample C)

azsphere_configure_tools(TOOLS_REVISION "21.04")
azsphere_configure_api(TARGET_API_SET "9")
 
add_executable(${PROJECT_NAME}
main.c
eventloop_timer_utilities.c
../../iotc-azsphere-sdk/cJSON/cJSON.c
../../iotc-azsphere-sdk/iotc-c-lib/src/iotconnect_common.c
../../iotc-azsphere-sdk/iotc-c-lib/src/iotconnect_event.c
../../iotc-azsphere-sdk/iotc-c-lib/src/iotconnect_lib.c
../../iotc-azsphere-sdk/iotc-c-lib/src/iotconnect_request.c
../../iotc-azsphere-sdk/iotc-c-lib/src/iotconnect_telemetry.c
../../iotc-azsphere-sdk/azsphere-layer/src/azsphere_iothub_client.c
../../iotc-azsphere-sdk/src/iotConnect.c)

target_include_directories(${PROJECT_NAME} PUBLIC
${AZURE_SPHERE_API_SET_DIR}/usr/include/azureiot 
${AZURE_SPHERE_API_SET_DIR}/usr/include/azure_prov_client 
${AZURE_SPHERE_API_SET_DIR}/usr/include/azure_c_shared_utility
../../iotc-azsphere-sdk/cJSON 
../../iotc-azsphere-sdk/iotc-c-lib/include 
../../iotc-azsphere-sdk/azsphere-layer/include 
../../iotc-azsphere-sdk/include 
.
)
target_compile_definitions(${PROJECT_NAME} PUBLIC AZURE_IOT_HUB_CONFIGURED
                                                  IOTCONNECT_DM_V2_0
                                                  ENABLE_IOTC_MESSAGE_DEBUG)  

target_link_libraries(${PROJECT_NAME} m azureiot applibs gcc_s c)

azsphere_target_hardware_definition(${PROJECT_NAME} TARGET_DIRECTORY "../../hardware-definitions/mt3620_rdb" TARGET_DEFINITION "sample_appliance.json")

find_program(POWERSHELL powershell.exe)

if (POWERSHELL)
    # Run validate_manifest script during build
    add_custom_target(ValidateManifest ALL
        COMMAND ${POWERSHELL} -ExecutionPolicy Bypass -NoProfile -NonInteractive -File ${CMAKE_SOURCE_DIR}/script/validate_manifest.ps1
        DEPENDS ${CMAKE_SOURCE_DIR}/app_manifest.json)
else()
    # Warn users without PowerShell to update their manifest
    add_custom_target(ValidateManifest ALL
        COMMAND echo "Please ensure that you have updated app_manifest.json as described in IoTCentral.md or IoTHub.md, as appropriate."
        DEPENDS ${CMAKE_SOURCE_DIR}/app_manifest.json)
endif()

add_dependencies(ValidateManifest ${PROJECT_NAME}) 

azsphere_target_add_image_package(${PROJECT_NAME}) 
