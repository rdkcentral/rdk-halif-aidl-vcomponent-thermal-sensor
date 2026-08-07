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
 * profile; external UT/control-plane orchestration is intentionally not wired
 * in this implementation pass.
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
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
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
     */
    explicit ThermalSensor(vcomponent::utility::ThermalHfpConfig configuration);

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
     * @brief Reserved UT worker hook.
     *
     * Control-plane behavior is intentionally excluded from this pass.
     */
    void utWorkerLoop();

    /**
     * @brief Reserved UT message handler hook.
     *
     * Control-plane behavior is intentionally excluded from this pass.
     *
     * @param[in] msg Queued UT message tuple.
     */
    void handleQueuedUtMessage(const std::tuple<std::string, std::string, void*>& msg);

    /**
     * @brief Apply a temperature sample and evaluate the aggregate policy state.
     *
     * @param[in] sensorId           Configured sensor id (e.g. "soc_die").
     * @param[in] temperatureCelsius Sampled temperature in degrees Celsius.
     */
    void applyTemperatureSample(const std::string& sensorId, double temperatureCelsius);

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

    mutable std::mutex m_mutex;

    std::vector<SensorRuntime> m_sensors;
    State m_currentState{State::NORMAL};
    std::vector<android::sp<IThermalEventListener>> m_listeners;

    std::atomic<bool> m_stopUtWorker{false};
    std::thread m_utWorkerThread;

    // UT control plane owned by the service (single instance, service lifetime).
    // It is intentionally not initialized until control-plane work is requested.
    vcomponent::thermal::controller::ThermalUtController m_ut;
};

} // namespace thermal
} // namespace sensor
} // namespace hal
} // namespace rdk
} // namespace com
