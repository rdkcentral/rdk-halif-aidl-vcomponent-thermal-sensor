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
 * @file vcomponent_ThermalSensor.h
 * @brief AIDL Binder service declaration for the thermal policy service.
 *
 * Implemented AIDL interface:
 *   com.rdk.hal.sensor.thermal.IThermalSensor  (singleton, serviceName "sensor.thermal")
 *
 * The service exposes listener registration, current aggregate thermal state,
 * and current temperature telemetry as defined by the thermal sensor AIDL
 * contract. Runtime sensor records are initialized from the selected HFP YAML
 * profile. The UT control plane listens for IThermalSensor temperature_update
 * messages, updates in-memory telemetry, notifies listeners on aggregate state
 * transitions, persists BootCause::THERMAL_RESET through Boot Reason, and
 * autonomously executes systemctl poweroff when the aggregate policy enters
 * CRITICAL_SHUTDOWN_IMMINENT.
 */

#include <com/rdk/hal/sensor/thermal/ActionEvent.h>
#include <com/rdk/hal/sensor/thermal/BnThermalSensor.h>
#include <com/rdk/hal/sensor/thermal/IThermalEventListener.h>
#include <com/rdk/hal/sensor/thermal/IThermalSensor.h>
#include <com/rdk/hal/sensor/thermal/State.h>
#include <com/rdk/hal/sensor/thermal/TemperatureReading.h>

#include <binder/BinderService.h>
#include <utils/StrongPointer.h>

#include "controller/vcomponent_ThermalUtController.h"
#include "utility/vcomponent_ThermalHfpConfigUtils.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace com
{
namespace rdk
{
namespace hal
{
namespace sensor
{
namespace thermal
{

/**
 * @brief Thermal policy service driven by configured HFP sensor metadata.
 */
class ThermalSensor final : public android::BinderService<ThermalSensor>, public BnThermalSensor
{
public:
    // PUBLIC_INTERFACE
    /**
     * @brief Get the well-known service name for publication/lookup.
     *
     * @return Service name string ("sensor.thermal").
     */
    static char const* getServiceName()
    {
        // IThermalSensor::serviceName() may return std::string by value; cache it
        // in static storage so the returned C-string remains valid.
        static const std::string kServiceName = IThermalSensor::serviceName();
        return kServiceName.c_str();
    }

    // PUBLIC_INTERFACE
    /**
     * @brief Construct the service from a validated HFP thermal profile.
     *
     * @param[in] configuration Parsed thermal sensor configuration, supplied
     *                           by the service entrypoint at startup.
     * @param[in] controlPort   UT control-plane TCP port for IThermalSensor
     *                           temperature_update messages.
     */
    ThermalSensor(vcomponent::utility::ThermalHfpConfig configuration, std::uint16_t controlPort);

    // PUBLIC_INTERFACE
    /**
     * @brief Destroy the service and release runtime resources.
     */
    ~ThermalSensor() override;

    ThermalSensor(const ThermalSensor&) = delete;
    ThermalSensor& operator=(const ThermalSensor&) = delete;

    // PUBLIC_INTERFACE
    /**
     * @brief Register a thermal event listener.
     *
     * A null listener fails with Binder EX_NULL_POINTER. A duplicate listener is
     * not added and returns false. A newly registered listener returns true.
     *
     * @param[in]  listener      Listener to register.
     * @param[out] _aidl_return  Set to true when newly registered.
     *
     * @return Binder status.
     */
    android::binder::Status registerEventListener(
        const android::sp<IThermalEventListener>& listener,
        bool* _aidl_return) override;

    // PUBLIC_INTERFACE
    /**
     * @brief Unregister a previously registered thermal event listener.
     *
     * A null listener fails with Binder EX_NULL_POINTER. An unknown listener
     * returns false. A removed listener returns true.
     *
     * @param[in]  listener      Listener to unregister.
     * @param[out] _aidl_return  Set to true when a listener was removed.
     *
     * @return Binder status.
     */
    android::binder::Status unregisterEventListener(
        const android::sp<IThermalEventListener>& listener,
        bool* _aidl_return) override;

    // PUBLIC_INTERFACE
    /**
     * @brief Get the current aggregated thermal state.
     *
     * @param[out] _aidl_return  Receives the current thermal state.
     *
     * @return Binder status.
     */
    android::binder::Status getCurrentThermalState(State* _aidl_return) override;

    // PUBLIC_INTERFACE
    /**
     * @brief Get the latest temperature reading for each configured sensor.
     *
     * Returns an empty vector when no sensor telemetry is configured or
     * available, matching the AIDL contract.
     *
     * @param[out] _aidl_return  Receives the temperature readings.
     *
     * @return Binder status.
     */
    android::binder::Status getCurrentTemperatures(std::vector<TemperatureReading>* _aidl_return) override;

private:
    /**
     * @brief Runtime pairing of a configured sensor and its latest reading.
     */
    struct SensorRuntime
    {
        vcomponent::utility::ThermalSensorConfig config;
        TemperatureReading reading;
        State state{State::NORMAL};
    };

    /**
     * @brief Initialize configured sensors into runtime records.
     *
     * @param[in] configuration Validated HFP profile supplied at startup.
     */
    void initializeSensors(vcomponent::utility::ThermalHfpConfig configuration);

    /**
     * @brief Poll the UT control-plane queue and apply thermal updates.
     *
     * The worker consumes temperature_update commands and delegates all policy
     * evaluation to applyTemperatureSample(). The state-transition path
     * notifies listeners, persists the thermal cause, and requests power-off.
     */
    void utWorkerLoop();

    /**
     * @brief Handle a parsed control-plane temperature_update command.
     *
     * @param[in] update Parsed IThermalSensor temperature_update payload.
     */
    void handleControlPlaneUpdate(const vcomponent::thermal::controller::ThermalTemperatureUpdate& update);

    /**
     * @brief Apply a temperature sample and evaluate the aggregate policy state.
     *
     * @param[in] sensorNameOrId       Configured sensorName or id.
     * @param[in] temperatureCelsius   Sampled temperature in degrees Celsius.
     * @param[in] timestampMonotonicMs Monotonic timestamp supplied by control plane.
     */
    void applyTemperatureSample(
        const std::string& sensorNameOrId,
        double temperatureCelsius,
        std::int64_t timestampMonotonicMs);

    /**
     * @brief Evaluate the AIDL State represented by one sensor sample.
     *
     * @param[in] runtime            Runtime record for the configured sensor.
     * @param[in] temperatureCelsius Sampled temperature in degrees Celsius.
     *
     * @return Evaluated high-level thermal state.
     */
    State evaluateSensorState(const SensorRuntime& runtime, double temperatureCelsius) const;

    /**
     * @brief Notify registered listeners of a state transition.
     *
     * Listener callbacks are invoked outside the service mutex to avoid holding
     * shared service state while making Binder calls.
     *
     * @param[in] event ActionEvent payload for IThermalEventListener callbacks.
     */
    void notifyListeners(const ActionEvent& event);

    /**
     * @brief Initiate a thermal shutdown once for the current shutdown event.
     *
     * Duplicate CRITICAL_SHUTDOWN_IMMINENT transitions are suppressed so the
     * shutdown sequence is initiated at most once for this service lifetime.
     *
     * @param[in] reasonString Stable BootReason string persisted for diagnosis.
     */
    void scheduleThermalShutdown(const std::string& reasonString);

    /**
     * @brief Persist the thermal shutdown cause through the BootReason service.
     *
     * The helper obtains the published IBootReason Binder service, records
     * BootCause::THERMAL_RESET using the bounded non-empty reason string. It
     * never manages the device power transition and must be called outside
     * m_mutex because Binder calls can block.
     *
     * @param[in] reasonString Stable BootReason string persisted for diagnosis.
     */
    void recordThermalShutdownReason(const std::string& reasonString);

    /**
     * @brief Request device power-off through systemd.
     *
     * This executes `systemctl poweroff` after the Boot Reason persistence
     * attempt. It is deliberately independent from the persistence result so a
     * transient Boot Reason failure cannot prevent thermal protection.
     */
    void requestSystemPoweroff();

    mutable std::mutex m_mutex;

    std::vector<SensorRuntime> m_sensors;
    State m_currentState{State::NORMAL};
    std::vector<android::sp<IThermalEventListener>> m_listeners;

    std::uint16_t m_controlPort{0};
    std::atomic<bool> m_stopUtWorker{false};
    std::thread m_utWorkerThread;

    std::atomic<bool> m_shutdownScheduled{false};

    // UT control plane owned by the service for IThermalSensor temperature_update
    // messages. Samples entering CRITICAL_SHUTDOWN_IMMINENT flow through the
    // normal policy path, notify listeners for time-critical cleanup, persist
    // THERMAL_RESET, then autonomously invoke systemctl poweroff.
    vcomponent::thermal::controller::ThermalUtController m_ut;
};

} // namespace thermal
} // namespace sensor
} // namespace hal
} // namespace rdk
} // namespace com
