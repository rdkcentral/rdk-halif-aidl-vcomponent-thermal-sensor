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
 * YAML profile before publishing the binder service, then passes the parsed
 * configuration to the service implementation. The Binder runtime initializes
 * its sensor model from that configuration and exposes it through the thermal
 * AIDL state and telemetry APIs. UT/control-plane work remains reserved.
 */


#include "aidl/vcomponent_ThermalSensor.h"
#include "service/vcomponent_ThermalSensorService.h"

#include "common/logger.h"
#include "utility/vcomponent_ThermalHelper.h" 
#include "utility/vcomponent_ThermalParseConfig.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
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
    std::optional<std::uint16_t>* outPort)
{
    if (!outHfpPath || !outPort)
        return false;

    *outHfpPath = vcomponent::thermal::service::kDefaultHfpPath;
    *outPort = std::nullopt;

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
            *outPort = port;
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

static void publishAndJoinThreadPool(vcomponent::utility::ThermalHfpConfig thermalConfig)
{
    using com::rdk::hal::sensor::thermal::ThermalSensor;

    const char* serviceName = ThermalSensor::getServiceName();
    if (!serviceName)
        serviceName = "(null)";

    android::sp<android::IServiceManager> sm = android::defaultServiceManager();
    android::sp<ThermalSensor> svc = new ThermalSensor(std::move(thermalConfig));

    LOGF_INFO("%s Publishing Thermal binder service (serviceName=%s)", logPrefix, serviceName);

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
    LOGF_INFO("%s   --port <port>      Optional UT Control Plane port (reserved for future use).",
              logPrefix);
}

int main(int argc, char** argv)
{
    LOGF_INFO("%s ===============================", logPrefix);
    LOGF_INFO("%s Thermal Sensor Service 0.1.0.", logPrefix);
    LOGF_INFO("%s ===============================", logPrefix);

    std::string configPath;
    std::optional<std::uint16_t> port;

    if (!parseArgs(argc, argv, &configPath, &port))
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

    LOGF_INFO("%s Starting Thermal binder service (serviceName=%s, configPath=%s)",
              logPrefix,
              serviceName,
              configPath.c_str());

    if (port.has_value())
    {
        // The optional port is accepted and logged; it is not yet connected to
        // the service's UT controller.
        LOGF_INFO("%s --port provided (reserved): %u",
                  logPrefix,
                  static_cast<unsigned>(*port));
    }

    // Publish the service with the startup-validated configuration and join
    // the Binder thread pool. The YAML profile is intentionally parsed once.
    publishAndJoinThreadPool(std::move(thermalConfig));

    return 0;
}