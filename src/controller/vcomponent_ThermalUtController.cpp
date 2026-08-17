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
 * @file vcomponent_ThermalUtController.cpp
 * @brief Implementation of the Thermal UT control-plane facade.
 *
 * The controller owns the UT control-plane endpoint, listens for
 * IThermalSensor.temperature_update commands, parses KVP fields, and queues
 * validated temperature samples for the Binder service. It does not perform
 * reboot or power-management side effects.
 */

#include "controller/vcomponent_ThermalUtController.h"

#include "common/logger.h"
#include "utility/vcomponent_ThermalHelper.h"
#include "utility/vcomponent_ThermalParseConfig.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <utility>

namespace vcomponent
{
namespace thermal
{
namespace controller
{
namespace
{
constexpr const char* logPrefix = "[VDEVICE_THERMAL]<ThermalUtController>";

constexpr const char* kCommandKeySlash = "IThermalSensor/command";
constexpr const char* kSensorNameKeySlash = "IThermalSensor/sensorName";
constexpr const char* kTemperatureKeySlash = "IThermalSensor/temperatureCelsius";
constexpr const char* kTimestampKeySlash = "IThermalSensor/timestampMonotonicMs";
constexpr const char* kCommandTemperatureUpdate = "temperature_update";
constexpr std::uint32_t kKvpReadBufferSize = 256U;

void setError(std::string* outError, const std::string& message)
{
    if (outError != nullptr)
    {
        *outError = message;
    }
}

bool parseDouble(const std::string& value, double* outValue)
{
    if (outValue == nullptr || value.empty())
    {
        return false;
    }

    errno = 0;
    char* endPtr = nullptr;
    const double parsed = std::strtod(value.c_str(), &endPtr);
    if (errno != 0 || endPtr == value.c_str() || (endPtr != nullptr && *endPtr != '\0') ||
        !std::isfinite(parsed))
    {
        return false;
    }

    *outValue = parsed;
    return true;
}

bool parseInt64(const std::string& value, std::int64_t* outValue)
{
    if (outValue == nullptr || value.empty())
    {
        return false;
    }

    errno = 0;
    char* endPtr = nullptr;
    const long long parsed = std::strtoll(value.c_str(), &endPtr, 10);
    if (errno != 0 || endPtr == value.c_str() || (endPtr != nullptr && *endPtr != '\0') || parsed < 0)
    {
        return false;
    }

    *outValue = static_cast<std::int64_t>(parsed);
    return true;
}

bool readKvpString(ut_kvp_instance_t* instance, const char* key, std::string* outValue)
{
    if (instance == nullptr || key == nullptr || outValue == nullptr)
    {
        return false;
    }

    char result[kKvpReadBufferSize] = {0};
    const ut_kvp_status_t status =
        ut_kvp_getStringField(instance, key, result, static_cast<std::uint32_t>(sizeof(result)));
    if (status != UT_KVP_STATUS_SUCCESS || result[0] == '\0')
    {
        return false;
    }

    *outValue = result;
    return true;
}
} // namespace

ThermalUtController::ThermalUtController()
    : m_controlPlaneInstance(nullptr)
    , m_userData(nullptr)
{
}

ThermalUtController::~ThermalUtController()
{
    shutdown();
}

bool ThermalUtController::loadConfiguration(const std::string& hfpYamlPath, std::string* outError)
{
    LOGF_INFO("%s loading HFP configuration from '%s'", logPrefix, hfpYamlPath.c_str());

    vcomponent::utility::ThermalHfpConfig parsedConfig;
    std::string parseError;
    if (!vcomponent::utility::loadThermalHfpConfigFromYaml(hfpYamlPath, &parsedConfig, &parseError))
    {
        setError(outError, parseError);
        LOGF_ERROR(
            "%s failed to load HFP configuration '%s': %s",
            logPrefix,
            hfpYamlPath.c_str(),
            parseError.c_str());
        return false;
    }

    m_hfpYamlPath = hfpYamlPath;
    m_hfpConfig = std::move(parsedConfig);

    LOGF_INFO(
        "%s loaded HFP configuration '%s' with %zu thermal sensor(s)",
        logPrefix,
        m_hfpYamlPath.c_str(),
        m_hfpConfig.sensors.size());
    return true;
}

bool ThermalUtController::buildInventory(std::string* outInventory, std::string* outError) const
{
    if (outInventory == nullptr)
    {
        setError(outError, "outInventory is null");
        LOGF_ERROR("%s cannot build inventory: outInventory is null", logPrefix);
        return false;
    }

    std::ostringstream inventory;
    inventory << "Thermal Sensor Inventory\n";
    inventory << "hfpYamlPath=" << m_hfpYamlPath << "\n";
    inventory << "sensorCount=" << m_hfpConfig.sensors.size() << "\n";

    for (const auto& sensor : m_hfpConfig.sensors)
    {
        inventory << "sensor id=" << sensor.id
                  << " sensorName=" << sensor.sensorName
                  << " location=" << sensor.location
                  << " operationalMinCelsius=" << sensor.operationalTemperatureCelsius.min
                  << " operationalMaxCelsius=" << sensor.operationalTemperatureCelsius.max
                  << " criticalExceededCelsius=" << sensor.triggers.criticalTemperatureExceededCelsius
                  << " criticalShutdownCelsius=" << sensor.triggers.enteringCriticalShutdownCelsius
                  << "\n";
    }

    *outInventory = inventory.str();
    return true;
}

bool ThermalUtController::init(std::uint16_t port, void* userData)
{
    {
        std::lock_guard<std::mutex> lock(m_lifecycleMutex);
        if (m_controlPlaneInstance != nullptr)
        {
            LOGF_WARN(
                "%s control plane already initialized; ignoring duplicate init request (port=%u)",
                logPrefix,
                static_cast<unsigned>(port));
            return true;
        }
    }

    if (port == 0U)
    {
        LOGF_ERROR("%s invalid control-plane port 0", logPrefix);
        return false;
    }

    ut_controlPlane_instance_t* controlPlaneInstance = UT_ControlPlane_Init(static_cast<int>(port));
    if (controlPlaneInstance == nullptr)
    {
        LOGF_ERROR(
            "%s failed to create UT control-plane instance on port %u",
            logPrefix,
            static_cast<unsigned>(port));
        return false;
    }

    UT_ControlPlane_RegisterCallbackOnMessage(
        controlPlaneInstance,
        const_cast<char*>(kCommandKeySlash),
        &ThermalUtController::messageCallback,
        this);

    {
        std::lock_guard<std::mutex> lock(m_lifecycleMutex);
        if (m_controlPlaneInstance != nullptr)
        {
            LOGF_WARN(
                "%s control plane initialized concurrently; releasing duplicate instance",
                logPrefix);
            UT_ControlPlane_Exit(controlPlaneInstance);
            return true;
        }

        m_controlPlaneInstance = controlPlaneInstance;
        m_userData = userData;
        m_acceptingCallbacks = true;
    }

    UT_ControlPlane_Start(controlPlaneInstance);

    LOGF_INFO(
        "%s IThermalSensor control plane started on port %u",
        logPrefix,
        static_cast<unsigned>(port));
    return true;
}

void ThermalUtController::shutdown()
{
    ut_controlPlane_instance_t* controlPlaneInstance = nullptr;
    {
        std::unique_lock<std::mutex> lock(m_lifecycleMutex);
        if (m_controlPlaneInstance == nullptr)
        {
            return;
        }

        m_acceptingCallbacks = false;
        m_callbacksDrained.wait(lock, [this] { return m_activeCallbacks == 0U; });
        controlPlaneInstance = m_controlPlaneInstance;
        m_controlPlaneInstance = nullptr;
        m_userData = nullptr;
    }

    if (controlPlaneInstance == nullptr)
    {
        return;
    }

    LOGF_INFO("%s stopping IThermalSensor control plane", logPrefix);
    UT_ControlPlane_Exit(controlPlaneInstance);

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_pendingTemperatureUpdates.clear();
    }

    LOGF_INFO("%s IThermalSensor control plane stopped and pending messages drained", logPrefix);
}

std::optional<ThermalTemperatureUpdate> ThermalUtController::getTemperatureUpdate()
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    if (m_pendingTemperatureUpdates.empty())
    {
        return std::nullopt;
    }

    auto updateIt = m_pendingTemperatureUpdates.begin();
    ThermalTemperatureUpdate update = std::move(updateIt->second);
    m_pendingTemperatureUpdates.erase(updateIt);
    return update;
}

bool ThermalUtController::isRunning() const
{
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);
    return m_controlPlaneInstance != nullptr;
}

void ThermalUtController::messageCallback(char* key, ut_kvp_instance_t* instance, void* userData)
{
    auto* controller = static_cast<ThermalUtController*>(userData);
    if (controller == nullptr)
    {
        LOGF_ERROR("%s callback invoked without controller context", logPrefix);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(controller->m_lifecycleMutex);
        if (!controller->m_acceptingCallbacks)
        {
            return;
        }

        ++controller->m_activeCallbacks;
    }

    controller->pushMessage(key, instance);

    {
        std::lock_guard<std::mutex> lock(controller->m_lifecycleMutex);
        --controller->m_activeCallbacks;
        if (controller->m_activeCallbacks == 0U)
        {
            controller->m_callbacksDrained.notify_all();
        }
    }
}

void ThermalUtController::pushMessage(char* key, ut_kvp_instance_t* instance)
{
    if (instance == nullptr)
    {
        LOGF_ERROR("%s received null KVP instance for key=%s", logPrefix, key != nullptr ? key : "(null)");
        return;
    }

    std::string command;
    if (!readKvpString(instance, kCommandKeySlash, &command))
    {
        LOGF_ERROR(
            "%s control-plane message missing command key '%s'",
            logPrefix,
            kCommandKeySlash);
        return;
    }

    command = vcomponent::utility::trim(command);
    if (command != kCommandTemperatureUpdate)
    {
        return;
    }

    ThermalTemperatureUpdate update;
    if (!readKvpString(instance, kSensorNameKeySlash, &update.sensorName))
    {
        LOGF_ERROR(
            "%s temperature_update missing sensorName key '%s'",
            logPrefix,
            kSensorNameKeySlash);
        return;
    }

    update.sensorName = vcomponent::utility::trim(update.sensorName);
    if (update.sensorName.empty())
    {
        LOGF_ERROR("%s temperature_update rejected empty sensorName", logPrefix);
        return;
    }

    std::string temperatureString;
    if (!readKvpString(instance, kTemperatureKeySlash, &temperatureString) ||
        !parseDouble(vcomponent::utility::trim(temperatureString), &update.temperatureCelsius))
    {
        LOGF_ERROR(
            "%s temperature_update has invalid temperatureCelsius; expected key '%s'",
            logPrefix,
            kTemperatureKeySlash);
        return;
    }

    std::string timestampString;
    if (!readKvpString(instance, kTimestampKeySlash, &timestampString) ||
        !parseInt64(vcomponent::utility::trim(timestampString), &update.timestampMonotonicMs))
    {
        LOGF_ERROR(
            "%s temperature_update has invalid timestampMonotonicMs; expected key '%s'",
            logPrefix,
            kTimestampKeySlash);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        const auto result = m_pendingTemperatureUpdates.insert_or_assign(update.sensorName, std::move(update));
        if (!result.second)
        {
            LOGF_WARN(
                "%s temperature_update replaced an older pending sample for sensorName=%s",
                logPrefix,
                result.first->first.c_str());
        }
    }
}

} // namespace controller
} // namespace thermal
} // namespace vcomponent
