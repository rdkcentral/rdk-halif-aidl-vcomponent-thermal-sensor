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

#include <chrono>
#include <cerrno>
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
    if (errno != 0 || endPtr == value.c_str() || (endPtr != nullptr && *endPtr != '\0'))
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
    if (m_controlPlaneInstance != nullptr)
    {
        LOGF_WARN(
            "%s control plane already initialized; ignoring duplicate init request (port=%u)",
            logPrefix,
            static_cast<unsigned>(port));
        return true;
    }

    if (port == 0U)
    {
        LOGF_ERROR("%s invalid control-plane port 0", logPrefix);
        return false;
    }

    m_userData = userData;
    m_controlPlaneInstance = UT_ControlPlane_Init(static_cast<int>(port));
    if (m_controlPlaneInstance == nullptr)
    {
        LOGF_ERROR(
            "%s failed to create UT control-plane instance on port %u",
            logPrefix,
            static_cast<unsigned>(port));
        m_userData = nullptr;
        return false;
    }

    UT_ControlPlane_RegisterCallbackOnMessage(
        m_controlPlaneInstance,
        const_cast<char*>(kCommandKeySlash),
        &ThermalUtController::messageCallback,
        this);

    UT_ControlPlane_Start(m_controlPlaneInstance);

    LOGF_INFO(
        "%s IThermalSensor control plane started on port %u",
        logPrefix,
        static_cast<unsigned>(port));
    return true;
}

void ThermalUtController::shutdown()
{
    if (m_controlPlaneInstance == nullptr)
    {
        return;
    }

    LOGF_INFO("%s stopping IThermalSensor control plane", logPrefix);
    UT_ControlPlane_Exit(m_controlPlaneInstance);
    m_controlPlaneInstance = nullptr;
    m_userData = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        std::queue<ThermalTemperatureUpdate> emptyQueue;
        m_temperatureUpdateQueue.swap(emptyQueue);
    }

    LOGF_INFO("%s IThermalSensor control plane stopped and pending messages drained", logPrefix);
}

std::optional<ThermalTemperatureUpdate> ThermalUtController::getTemperatureUpdate()
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    if (m_temperatureUpdateQueue.empty())
    {
        return std::nullopt;
    }

    ThermalTemperatureUpdate update = m_temperatureUpdateQueue.front();
    m_temperatureUpdateQueue.pop();

    return update;
}

bool ThermalUtController::isRunning() const
{
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

    controller->pushMessage(key, instance);
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
    if (update.timestampMonotonicMs == 0)
    {
        update.timestampMonotonicMs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_temperatureUpdateQueue.push(update);
    }
}

} // namespace controller
} // namespace thermal
} // namespace vcomponent
