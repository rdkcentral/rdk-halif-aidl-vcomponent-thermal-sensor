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
 * @brief SKELETON AIDL binder service declaration for the thermal policy service.
 *
 * Implemented AIDL interface (declaration only in this skeleton):
 *   com.rdk.hal.sensor.thermal.IThermalSensor  (singleton, serviceName "sensor.thermal")
 *
 * This header declares the full surface expected by the build skeleton. The
 * corresponding source file (src/aidl/vcomponent_ThermalSensor.cpp) contains
 * placeholder bodies only; no thermal policy behaviour is implemented.
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
 * @brief Skeleton thermal policy service driven by the HFP profile and UT control plane.
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
     * @brief Construct the service. Skeleton: no thermal state is initialized.
     */
    ThermalSensor();

    /**
     * @brief Destroy the service and release skeleton resources.
     */
    ~ThermalSensor() override;

    ThermalSensor(const ThermalSensor&) = delete;
    ThermalSensor& operator=(const ThermalSensor&) = delete;

    // PUBLIC_INTERFACE
    /**
     * @brief Configure the HFP YAML path used when the service is constructed.
     *
     * The service entry point calls this before publishing the Binder service.
     *
     * @param[in] configurationPath Path to hfp-sensor-thermal.yaml.
     */
    static void setConfigurationPath(const std::string& configurationPath);

    // PUBLIC_INTERFACE
    /**
     * @brief Register a thermal event listener.
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

    bool loadConfiguration(const std::string& configurationPath);

    void utWorkerLoop();
    void handleQueuedUtMessage(const std::tuple<std::string, std::string, void*>& msg);

    /**
     * @brief Apply a simulated temperature sample and evaluate the thermal policy.
     *
     * @param[in] sensorId           Configured sensor id (e.g. "soc_die").
     * @param[in] temperatureCelsius Sampled temperature in degrees Celsius.
     */
    void applyTemperatureSample(const std::string& sensorId, double temperatureCelsius);

    State evaluateSensorState(const SensorRuntime& runtime, double temperatureCelsius) const;
    void notifyListeners(const ActionEvent& event);

    mutable std::mutex m_mutex;

    std::vector<SensorRuntime> m_sensors;
    State m_currentState{State::NORMAL};
    std::vector<android::sp<IThermalEventListener>> m_listeners;

    std::atomic<bool> m_stopUtWorker{false};
    std::thread m_utWorkerThread;

    // UT control plane owned by the service (single instance, service lifetime).
    vcomponent::thermal::controller::ThermalUtController m_ut;
};

} // namespace thermal
} // namespace sensor
} // namespace hal
} // namespace rdk
} // namespace com
