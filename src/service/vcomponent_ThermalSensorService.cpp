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
 * @brief SKELETON entrypoint for the Thermal Sensor vcomponent service.
 *
 * This skeleton intentionally contains NO thermal AIDL implementation. It only
 * performs argument handling and hands the HFP YAML path to the UT controller
 * facade. Once the AIDL layer is added, publish the binder service here
 * (ThermalSensor::setConfigurationPath() / publishAndJoinThreadPool()).
 */


#include "aidl/vcomponent_ThermalSensor.h"

#include "common/logger.h"
#include "utility/vcomponent_ThermalHelper.h" // if you have trim/readFileToString equivalents

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
constexpr const char* defaultConfigPath = "vcomponent_configurations/hfp-sensor-thermal.yaml";

static void printUsage(const char* progName)
{
    const char* p = (progName && progName[0] != '\0') ? progName : "RDKThermalSensorService";
    LOGF_INFO("Usage:");
    LOGF_INFO("  %s [--hfp <path>] [--port <port>]", p);
    LOGF_INFO("");
    LOGF_INFO("Args:");
    LOGF_INFO("  --hfp <path>       Optional HFP YAML path (default: %s).", defaultConfigPath);
    LOGF_INFO("  --port <port>      Optional UT Control Plane port (reserved for future use).");
}

static bool parsePort(const char* s, std::uint16_t* out)
{
    if (!s || s[0] == '\0' || !out)
        return false;

    char* end = nullptr;
    const long v = std::strtol(s, &end, 10);

    if (!end || *end != '\0')
        return false;

    if (v <= 0 || v > 65535)
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

    *outHfpPath = defaultConfigPath;
    *outPort = std::nullopt;

    if (argc < 1 || !argv || !argv[0])
        return false;

    for (int i = 1; i < argc; ++i)
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
            if (i + 1 >= argc || !argv[i + 1] || argv[i + 1][0] == '\0')
            {
                LOGF_ERROR("%s Missing value for --hfp", logPrefix);
                return false;
            }
            *outHfpPath = argv[i + 1];
            ++i;
        }
        else if (std::strcmp(arg, "--port") == 0)
        {
            if (i + 1 >= argc || !argv[i + 1] || argv[i + 1][0] == '\0')
            {
                LOGF_ERROR("%s Missing value for --port", logPrefix);
                return false;
            }
            std::uint16_t port = 0;
            if (!parsePort(argv[i + 1], &port))
            {
                LOGF_ERROR("%s Invalid --port value: %s", logPrefix, argv[i + 1]);
                return false;
            }
            *outPort = port;
            ++i;
        }
        else
        {
            LOGF_ERROR("%s Unknown argument: %s", logPrefix, arg);
            return false;
        }
    }

    return true;
}

static void publishAndJoinThreadPool()
{
    using com::rdk::hal::sensor::thermal::ThermalSensor;

    const char* serviceName = ThermalSensor::getServiceName();
    if (!serviceName)
        serviceName = "(null)";

    android::sp<android::IServiceManager> sm = android::defaultServiceManager();
    android::sp<ThermalSensor> svc = new ThermalSensor();

    LOGF_INFO("%s Publishing Thermal binder service (serviceName=%s)", logPrefix, serviceName);

    const android::status_t st = sm->addService(android::String16(serviceName), svc);
    if (st != android::OK)
    {
        LOGF_ERR("%s addService(%s) failed: %d", logPrefix, serviceName, static_cast<int>(st));
        std::exit(1);
    }

    android::ProcessState::self()->startThreadPool();
    android::IPCThreadState::self()->joinThreadPool();
}
} // namespace

int main(int argc, char** argv)
{
    LOGF_INFO("%s ===============================", logPrefix);
    LOGF_INFO("%s Thermal Sensor Service 0.1.0.", logPrefix);
    LOGF_INFO("%s ===============================", logPrefix);

    std::string configPath;
    std::optional<std::uint16_t> port;

    if (!parseArgs(argc, argv, &configPath, &port))
    {
        printUsage((argc > 0) ? argv[0] : nullptr);
        return 2;
    }

    // If you have a trim helper like HDMI, use it; otherwise remove this.
    configPath = vcomponent::utility::trim(configPath);

    if (configPath.empty())
    {
        LOGF_ERR("%s Empty config path after parsing input", logPrefix);
        return 2;
    }

    const char* serviceName = com::rdk::hal::sensor::thermal::ThermalSensor::getServiceName();
    if (!serviceName)
        serviceName = "(null)";

    LOGF_INFO("%s Starting Thermal binder service (serviceName=%s, configPath=%s)",
              logPrefix,
              serviceName,
              configPath.c_str());

    if (port.has_value())
    {
        // Skeleton: you can wire this later to your UT controller if desired.
        LOGF_INFO("%s --port provided (reserved): %u",
                  logPrefix,
                  static_cast<unsigned>(*port));
    }

    // Hand off configuration to the service implementation (even if skeleton).
    com::rdk::hal::sensor::thermal::ThermalSensor::setConfigurationPath(configPath);

    // Publish and block forever.
    publishAndJoinThreadPool();

    return 0;
}