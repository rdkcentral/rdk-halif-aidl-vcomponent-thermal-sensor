/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file vcomponent_ThermalSensorService.cpp
 * @brief Entrypoint for the Thermal Sensor vcomponent service.
 *
 * The entrypoint parses command-line arguments, loads and validates the HFP
 * YAML profile before publishing the Binder service, starts the IThermalSensor
 * UT control plane on the requested port, then joins the Binder thread pool.
 * Control-plane temperature updates flow through the thermal policy engine; when
 * the aggregate state reaches CRITICAL_SHUTDOWN_IMMINENT, listeners are notified
 * first and the thermal service then calls the separate BootReason Binder API.
 */

#include "aidl/vcomponent_ThermalSensor.h"
#include "service/vcomponent_ThermalSensorService.h"

#include "common/logger.h"
#include "utility/vcomponent_ThermalHelper.h" 
#include "utility/vcomponent_ThermalParseConfig.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

// Binder publish/join
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <utils/String16.h>

namespace
{
constexpr const char* logPrefix = "[VDEVICE_THERMAL]<ThermalSensorService>";
constexpr int kDecimalBase = 10;
constexpr long kMinimumPortNumber = 0;
constexpr long kMaximumPortNumber = 65535;
constexpr int kFirstCommandLineArgumentIndex = 1;
constexpr int kArgumentValueOffset = 1;
constexpr int kServiceStartupFailureExitCode = 1;
constexpr int kInvalidArgumentsExitCode = 2;

static bool parsePort(const char* s, std::uint16_t* out)
{
    if (!s || s[0] == '\0' || !out)
        return false;

    char* end = nullptr;
    const long v = std::strtol(s, &end, kDecimalBase);

    if (!end || *end != '\0')
        return false;

    if (v <= kMinimumPortNumber || v > kMaximumPortNumber)
        return false;

    *out = static_cast<std::uint16_t>(v);
    return true;
}

static bool parseArgs(
    int argc,
    char** argv,
    std::string* outHfpPath,
    std::uint16_t* outControlPort)
{
    if (!outHfpPath || !outControlPort)
        return false;

    *outHfpPath = vcomponent::thermal::service::kDefaultHfpPath;
    *outControlPort = vcomponent::thermal::service::kDefaultControlPlanePort;

    if (argc < 1 || !argv || !argv[0])
        return false;

    for (int i = kFirstCommandLineArgumentIndex; i < argc; ++i)
    {
        const char* arg = argv[i];
        if (!arg)
            continue;

        if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0)
        {
            return false;
        }
        else if (std::strcmp(arg, "--hfp") == 0)
        {
            if (i + kArgumentValueOffset >= argc ||
                !argv[i + kArgumentValueOffset] ||
                argv[i + kArgumentValueOffset][0] == '\0')
            {
                LOGF_ERROR("%s Missing value for --hfp", logPrefix);
                return false;
            }
            *outHfpPath = argv[i + kArgumentValueOffset];
            i += kArgumentValueOffset;
        }
        else if (std::strcmp(arg, "--port") == 0)
        {
            if (i + kArgumentValueOffset >= argc ||
                !argv[i + kArgumentValueOffset] ||
                argv[i + kArgumentValueOffset][0] == '\0')
            {
                LOGF_ERROR("%s Missing value for --port", logPrefix);
                return false;
            }
            std::uint16_t port = 0;
            if (!parsePort(argv[i + kArgumentValueOffset], &port))
            {
                LOGF_ERROR(
                    "%s Invalid --port value: %s",
                    logPrefix,
                    argv[i + kArgumentValueOffset]);
                return false;
            }
            *outControlPort = port;
            i += kArgumentValueOffset;
        }
        else
        {
            LOGF_ERROR("%s Unknown argument: %s", logPrefix, arg);
            return false;
        }
    }

    return true;
}

static void publishAndJoinThreadPool(
    vcomponent::utility::ThermalHfpConfig thermalConfig,
    std::uint16_t controlPort)
{
    using com::rdk::hal::sensor::thermal::ThermalSensor;

    const char* serviceName = ThermalSensor::getServiceName();
    if (!serviceName)
        serviceName = "(null)";

    android::sp<android::IServiceManager> sm = android::defaultServiceManager();
    android::sp<ThermalSensor> svc = new ThermalSensor(std::move(thermalConfig), controlPort);

    LOGF_INFO(
        "%s Publishing Thermal binder service (serviceName=%s, controlPort=%u)",
        logPrefix,
        serviceName,
        static_cast<unsigned>(controlPort));

    const android::status_t st = sm->addService(android::String16(serviceName), svc);
    if (st != android::OK)
    {
        LOGF_ERROR("%s addService(%s) failed: %d", logPrefix, serviceName, static_cast<int>(st));
        std::exit(kServiceStartupFailureExitCode);
    }

    android::ProcessState::self()->startThreadPool();
    android::IPCThreadState::self()->joinThreadPool();
}
} // namespace

void vcomponent::thermal::service::printUsage(const char* progName)
{
    const char* p = (progName && progName[0] != '\0') ? progName : "RDKThermalSensorService";
    LOGF_INFO("%s Usage:", logPrefix);
    LOGF_INFO("%s   %s [--hfp <path>] [--port <port>]", logPrefix, p);
    LOGF_INFO("%s", logPrefix);
    LOGF_INFO("%s Args:", logPrefix);
    LOGF_INFO("%s   --hfp <path>       Optional HFP YAML path (default: %s).",
              logPrefix,
              vcomponent::thermal::service::kDefaultHfpPath);
    LOGF_INFO("%s   --port <port>      Optional IThermalSensor UT Control Plane port (default: %u).",
              logPrefix,
              static_cast<unsigned>(vcomponent::thermal::service::kDefaultControlPlanePort));
    LOGF_INFO("%s", logPrefix);
    LOGF_INFO("%s Control-plane protocol:", logPrefix);
    LOGF_INFO("%s   IThermalSensor.command=temperature_update", logPrefix);
    LOGF_INFO("%s   IThermalSensor.sensorName=<configured sensorName or id>", logPrefix);
    LOGF_INFO("%s   IThermalSensor.temperatureCelsius=<double>", logPrefix);
    LOGF_INFO("%s   IThermalSensor.timestampMonotonicMs=<int64>", logPrefix);
}

int main(int argc, char** argv)
{
    LOGF_INFO("%s ===============================", logPrefix);
    LOGF_INFO("%s Thermal Sensor Service 0.1.0.", logPrefix);
    LOGF_INFO("%s ===============================", logPrefix);

    std::string configPath;
    std::uint16_t controlPort = vcomponent::thermal::service::kDefaultControlPlanePort;

    if (!parseArgs(argc, argv, &configPath, &controlPort))
    {
        vcomponent::thermal::service::printUsage((argc > 0) ? argv[0] : nullptr);
        return kInvalidArgumentsExitCode;
    }

    // Normalize surrounding whitespace before opening the requested profile.
    configPath = vcomponent::utility::trim(configPath);

    if (configPath.empty())
    {
        LOGF_ERROR("%s Empty config path after parsing input", logPrefix);
        return kInvalidArgumentsExitCode;
    }

    vcomponent::utility::ThermalHfpConfig thermalConfig;
    std::string configurationError;
    if (!vcomponent::utility::loadThermalHfpConfigFromYaml(
            configPath, &thermalConfig, &configurationError))
    {
        LOGF_ERROR(
            "%s Failed to load HFP configuration '%s': %s",
            logPrefix,
            configPath.c_str(),
            configurationError.c_str());
        return kInvalidArgumentsExitCode;
    }

    LOGF_INFO(
        "%s Loaded HFP configuration '%s' with %zu sensor configuration(s)",
        logPrefix,
        configPath.c_str(),
        thermalConfig.sensors.size());

    const char* serviceName = com::rdk::hal::sensor::thermal::ThermalSensor::getServiceName();
    if (!serviceName)
        serviceName = "(null)";

    LOGF_INFO(
        "%s Starting Thermal binder service (serviceName=%s, configPath=%s, controlPort=%u)",
        logPrefix,
        serviceName,
        configPath.c_str(),
        static_cast<unsigned>(controlPort));

    // Publish the service with the startup-validated configuration and start the
    // IThermalSensor control plane on the selected port. The YAML profile is
    // intentionally parsed once; subsequent reboot escalation is handled by the
    // ThermalSensor state-transition path through the BootReason Binder service.
    publishAndJoinThreadPool(std::move(thermalConfig), controlPort);

    return 0;
}
