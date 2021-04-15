cmake_minimum_required(VERSION 3.11.4)
project(gateway)

include("${CMAKE_CURRENT_LIST_DIR}/configs/azure_iot_sdksFunctions.cmake")

getIoTSDKVersion()
message(STATUS "IoT Client SDK Version = ${IOT_SDK_VERSION}")

if (POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(WINDOWS TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LINUX TRUE)
    # on Linux, enable valgrind
    # these commands (MEMORYCHECK...) need to apear BEFORE include(CTest) or they will not have any effect
    find_program(MEMORYCHECK_COMMAND valgrind)
    set(MEMORYCHECK_COMMAND_OPTIONS "--leak-check=full --error-exitcode=1")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(MACOSX TRUE)
    add_definitions(-DMACOSX)
endif()


if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /wd4232")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3 /wd4232")
    # Make warning as error ${CMAKE_C_FLAGS} /W3 /wd4232
    add_definitions(/WX)
else()
    # Make warning as error
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
endif()

#SET( CMAKE_BUILD_TYPE Release ... FORCE )

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
  message(STATUS "Build type not specified: Use Release by default")
endif(NOT CMAKE_BUILD_TYPE)

set(hsm_type_x509 OFF CACHE BOOL "x509 type of hsm used with the Provisioning client")
set(hsm_type_sastoken OFF CACHE BOOL "tpm type of hsm used with the Provisioning client")
set(hsm_type_symm_key OFF CACHE BOOL "Symmetric key type of hsm used with the Provisioning client")
set(hsm_type_custom OFF CACHE BOOL "hsm type of custom used with the Provisioning client")
set(hsm_custom_lib "" CACHE STRING "Full path to custom HSM repo library")
set(run_sfc_tests OFF CACHE BOOL "setup the Service Fault tests")

# the following variables are project-wide and can be used with cmake-gui
option(use_amqp "set use_amqp to ON if amqp is to be used, set to OFF to not use amqp" ON)
option(use_http "set use_http to ON if http is to be used, set to OFF to not use http" ON)
option(use_mqtt "set use_mqtt to ON if mqtt is to be used, set to OFF to not use mqtt" ON)
option(run_e2e_tests "set run_e2e_tests to ON to run e2e tests (default is OFF)" OFF)
option(run_unittests "set run_unittests to ON to run unittests (default is OFF)" OFF)
option(run_longhaul_tests "set run_longhaul_tests to ON to run longhaul tests (default is OFF)[if possible, they are always build]" OFF)
option(skip_samples "set skip_samples to ON to skip building samples (default is OFF)[if possible, they are always build]" OFF)
option(build_service_client "controls whether the iothub_service_client is built or not" ON)
option(build_provisioning_service_client "controls whether the provisioning_service_client is built or not" ON)
option(build_python "builds the Python native iothub_client module" OFF)
option(dont_use_uploadtoblob "set dont_use_uploadtoblob to ON if the functionality of upload to blob is to be excluded, OFF otherwise. It requires HTTP" OFF)
option(no_logging "disable logging" OFF)
option(use_installed_dependencies "set use_installed_dependencies to ON to use installed packages instead of building dependencies from submodules" OFF)
option(build_as_dynamic "build the IoT SDK libaries as dynamic"  OFF)
option(use_prov_client "Enable provisioning client" OFF)
option(use_tpm_simulator "tpm simulator type of hsm used with the provisioning client" OFF)
option(use_edge_modules "Enable support for running modules against Azure IoT Edge" OFF)
option(use_custom_heap "use externally defined heap functions instead of the malloc family" OFF)
option(use_baltimore_cert "set use_baltimore_cert to ON if the Baltimore cert is to be used, set to OFF to not use it" OFF)
option(use_microsoftazure_de_cert "set use_microsoftazure_de_cert to ON if the MicrosoftAzure DE cert is to be used, set to OFF to not use it" OFF)
option(use_portal_azure_cn_cert "set use_portal_azure_cn_cert to ON if the Portal Azure CN cert is to be used, set to OFF to not use it" OFF)

set(compileOption_C "" CACHE STRING "passes a string to the command line of the C compiler")
set(compileOption_CXX "" CACHE STRING "passes a string to the command line of the C++ compiler")
set(linkerOption "" CACHE STRING "passes a string to the shared and exe linker options of the C compiler")

set(use_prov_client_core OFF)

if(${use_custom_heap})
    add_definitions(-DGB_USE_CUSTOM_HEAP)
endif()

if (NOT ${use_amqp} AND NOT ${use_http} AND NOT ${use_mqtt})
    message(FATAL_ERROR "CMAKE Failure: AMQP, HTTP & MQTT are all disable, iothub client must have one protocol enabled")
endif()

if (XCODE AND ${use_prov_client})
    # The TPM module is not available on Mac, and Mac's <string.h> and <unistd.h> files collide as well
    message(FATAL_ERROR "Provisioning client is not supported on Mac")
endif()

if (WIN32 OR MACOSX)
    option(use_openssl "set use_openssl to ON to use OpenSSL." OFF)
else()
    option(use_openssl "set use_openssl to ON to use OpenSSL." ON)
endif()
option(use_mbedtls "set use_mbedtls to ON to use mbedtls." OFF)
option(use_bearssl "set use_bearssl to ON to use bearssl." OFF)
option(use_wolfssl "set use_bearssl to ON to use bearssl." OFF)


set(SOURCES
${CMAKE_SOURCE_DIR}/src/iothub_communicator_x509.c
)

IF(WIN32)
    #windows needs this define
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_WIN32)
	add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
ENDIF(WIN32)

include_directories(D:/IOTHub/vcpkg_new/azure-iot-sdk-c/deps/azure-macro-utils-c/inc)
include_directories(D:/IOTHub/vcpkg_new/azure-iot-sdk-c/deps/umock-c/inc)
include_directories(D:/IOTHub/vcpkg_new/azure-iot-sdk-c/c-utility/inc)
include_directories(D:/IOTHub/vcpkg_new/azure-iot-sdk-c/c-utility/pal/windows)
include_directories(D:/IOTHub/vcpkg_new/azure-iot-sdk-c/iothub_client/inc)
include_directories(D:/IOTHub/vcpkg_new/azure-iot-sdk-c/c-utility/inc)
include_directories(D:/IOTHub/vcpkg_new/azure-iot-sdk-c/uamqp/inc)
include_directories(D:/IOTHub/vcpkg_new/azure-iot-sdk-c/umqtt/inc)
include_directories(D:/IOTHub/vcpkg_new/azure-iot-sdk-c/deps/parson)

link_directories(gateway D:/Gateway/debug_32_libs)
#link_directories(gateway D:/IOTHub/vcpkg_new/installed/x86-windows/lib)

add_executable(gateway ${SOURCES})

if(${use_amqp})
    target_link_libraries(gateway
        iothub_client_amqp_transport
    )
    linkUAMQP(gateway)
    add_definitions(-DUSE_AMQP)
endif()

if(${use_mqtt})
    target_link_libraries(gateway iothub_client_mqtt_transport)
    linkMqttLibrary(gateway)
    add_definitions(-DUSE_MQTT)
endif()

target_link_libraries(gateway iothub_client)
target_link_libraries(gateway parson)
target_link_libraries(gateway iothub_client_amqp_transport)
target_link_libraries(gateway iothub_service_client)
target_link_libraries(gateway aziotsharedutil)
target_link_libraries(gateway umock_c)

TARGET_LINK_LIBRARIES(gateway crypt32)
TARGET_LINK_LIBRARIES(gateway ncrypt)
TARGET_LINK_LIBRARIES(gateway rpcrt4)
TARGET_LINK_LIBRARIES(gateway Winhttp)

linkSharedUtil(gateway)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)