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

#pragma once

/**
 * @file vcomponent_ThermalUtController.h
 * @brief UT-control-plane facade for the Thermal sensor vcomponent.
 *
 * The control plane accepts the following KVP/YAML message shape on the
 * IThermalSensor protocol root:
 *
 *   IThermalSensor:
 *     command: temperature_update
 *     sensorName: <configured sensorName or id>
 *     temperatureCelsius: <floating-point Celsius value>
 *     timestampMonotonicMs: <monotonic timestamp in milliseconds>
 *
 * Slash-delimited flattened keys are accepted by the implementation:
 *   IThermalSensor/command
 */

#include "utility/vcomponent_ThermalHfpConfigUtils.h"

#include <ut.h>
#include <ut_control_plane.h>
#include <ut_kvp_profile.h>
#include <ut_log.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <string>

namespace vcomponent
{
namespace thermal
{
namespace controller
{

/**
 * @brief Parsed thermal control-plane temperature update.
 */
struct ThermalTemperatureUpdate
{
    std::string sensorName;              ///< Configured sensorName or sensor id.
    double temperatureCelsius{0.0};      ///< Temperature sample in degrees Celsius.
    std::int64_t timestampMonotonicMs{0};///< Caller-supplied monotonic timestamp.
};

/**
 * @brief UT-controller facade for the Thermal sensor vcomponent.
 *
 * This facade loads the HFP YAML into the parsed thermal configuration model,
 * exposes a lightweight inventory string for higher-level runners, and manages
 * the UT control-plane endpoint used by vcomponent orchestration clients.
 */
class ThermalUtController
{
public:
    // PUBLIC_INTERFACE
    /**
     * @brief Construct the controller.
     */
    ThermalUtController();

    // PUBLIC_INTERFACE
    /**
     * @brief Destroy the controller and stop any running UT control-plane instance.
     */
    ~ThermalUtController();

    ThermalUtController(const ThermalUtController&) = delete;
    ThermalUtController& operator=(const ThermalUtController&) = delete;

    // PUBLIC_INTERFACE
    /**
     * @brief Load thermal configuration from the HFP YAML.
     *
     * @param[in]  hfpYamlPath  Path to hfp-sensor-thermal.yaml.
     * @param[out] outError     Optional output error string.
     *
     * @return true on success, false on parse or file access error.
     */
    bool loadConfiguration(const std::string& hfpYamlPath, std::string* outError);

    // PUBLIC_INTERFACE
    /**
     * @brief Build a text inventory for the thermal component.
     *
     * @param[out] outInventory  Output inventory string. Must not be nullptr.
     * @param[out] outError      Optional output error string.
     *
     * @return true on success.
     */
    bool buildInventory(std::string* outInventory, std::string* outError) const;

    // PUBLIC_INTERFACE
    /**
     * @brief Initialize and start the UT control-plane endpoint.
     *
     * The controller registers for the IThermalSensor temperature_update command
     * keys and queues parsed temperature samples for the service worker thread.
     *
     * @param[in] port      TCP port used by the UT control plane.
     * @param[in] userData  Optional caller context retained for diagnostics.
     *
     * @return true when the control plane is initialized and started.
     */
    bool init(std::uint16_t port, void* userData = nullptr);

    // PUBLIC_INTERFACE
    /**
     * @brief Stop the UT control-plane endpoint and release its resources.
     */
    void shutdown();

    // PUBLIC_INTERFACE
    /**
     * @brief Pop the next pending temperature_update message, if available.
     *
     * @return Optional parsed temperature update.
     */
    std::optional<ThermalTemperatureUpdate> getTemperatureUpdate();

    // PUBLIC_INTERFACE
    /**
     * @brief Return whether the UT control-plane endpoint has been initialized.
     *
     * @return true when a UT control-plane instance is active.
     */
    bool isRunning() const;

private:
    static void messageCallback(char* key, ut_kvp_instance_t* instance, void* userData);
    void pushMessage(char* key, ut_kvp_instance_t* instance);

    std::string m_hfpYamlPath;
    vcomponent::utility::ThermalHfpConfig m_hfpConfig;

    ut_controlPlane_instance_t* m_controlPlaneInstance;
    void* m_userData;

    mutable std::mutex m_queueMutex;
    std::queue<ThermalTemperatureUpdate> m_temperatureUpdateQueue;
};

} // namespace controller
} // namespace thermal
} // namespace vcomponent
