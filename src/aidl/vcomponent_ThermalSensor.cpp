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
 * @file vcomponent_ThermalSensor.cpp
 * @brief Skeleton implementation of the thermal AIDL binder service.
 *
 * This implementation intentionally does not implement real thermal policy,
 * UT control-plane behavior, or HFP parsing yet. It provides minimal, deterministic
 * stub behavior so that the AIDL contract is honored and automated tests can run.
 */

#include "aidl/vcomponent_ThermalSensor.h"

#include "common/logger.h"
#include "utility/vcomponent_ThermalHelper.h"
#include "utility/vcomponent_ThermalParseConfig.h"

#include <binder/IInterface.h>
#include <binder/Status.h>
#include <utils/String16.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
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

namespace
{
constexpr const char* componentName = "ThermalSensor";
constexpr const char* defaultConfigPath = "vcomponent_configurations/hfp-sensor-thermal.yaml";

// UT control-plane port reserved for thermal orchestration.
constexpr std::uint16_t kUtControlPlanePort = 8085;

std::mutex g_configurationPathMutex;
std::string g_configurationPath = defaultConfigPath;
} // namespace

ThermalSensor::ThermalSensor()
{
    // TODO(skeleton): initialize UT control plane on kUtControlPlanePort,
    // load HFP configuration and start the UT worker thread.
    (void)kUtControlPlanePort;

    LOGF_INFO("%s: constructed (skeleton)", componentName);

    // Default state per HAL definition.
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentState = State::NORMAL;
}

ThermalSensor::~ThermalSensor()
{
    // TODO(skeleton): stop UT worker thread and shut down control plane.
}

void ThermalSensor::setConfigurationPath(const std::string& configurationPath)
{
    std::lock_guard<std::mutex> lock(g_configurationPathMutex);
    g_configurationPath = configurationPath.empty() ? defaultConfigPath : configurationPath;
}

bool ThermalSensor::loadConfiguration(const std::string& configurationPath)
{
    // TODO(skeleton): call vcomponent::utility::loadThermalHfpConfigFromYaml()
    // and seed m_sensors / m_currentState from the parsed model.
    (void)configurationPath;
    return false;
}

void ThermalSensor::utWorkerLoop()
{
    // TODO(skeleton): drain m_ut.getMessage() and dispatch to handleQueuedUtMessage().
}

void ThermalSensor::handleQueuedUtMessage(const std::tuple<std::string, std::string, void*>& msg)
{
    // TODO(skeleton): parse the KVP payload and handle the `set_temperature` command.
    (void)msg;
}

State ThermalSensor::evaluateSensorState(const SensorRuntime& runtime, double temperatureCelsius) const
{
    // TODO(skeleton): compare temperatureCelsius against runtime.config.triggers.
    (void)runtime;
    (void)temperatureCelsius;
    return State::NORMAL;
}

void ThermalSensor::applyTemperatureSample(const std::string& sensorId, double temperatureCelsius)
{
    // TODO(skeleton): update the sensor reading, recompute aggregated state and notify listeners.
    (void)sensorId;
    (void)temperatureCelsius;
}

void ThermalSensor::notifyListeners(const ActionEvent& event)
{
    // TODO(skeleton): dispatch onThermalStateChange() to all registered listeners.
    (void)event;
}

android::binder::Status ThermalSensor::registerEventListener(
    const android::sp<IThermalEventListener>& listener,
    bool* _aidl_return)
{
    if (_aidl_return)
        *_aidl_return = false;

    if (!listener.get())
    {
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    const auto b = android::IInterface::asBinder(listener);
    for (const auto& l : m_listeners)
    {
        if (android::IInterface::asBinder(l) == b)
        {
            if (_aidl_return)
                *_aidl_return = false; // already registered
            return android::binder::Status::ok();
        }
    }

    m_listeners.push_back(listener);

    if (_aidl_return)
        *_aidl_return = true; // newly registered

    return android::binder::Status::ok();
}

android::binder::Status ThermalSensor::unregisterEventListener(
    const android::sp<IThermalEventListener>& listener,
    bool* _aidl_return)
{
    if (_aidl_return)
        *_aidl_return = false;

    if (!listener.get())
    {
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    const auto b = android::IInterface::asBinder(listener);
    const auto oldSize = m_listeners.size();

    m_listeners.erase(
        std::remove_if(m_listeners.begin(),
                       m_listeners.end(),
                       [&](const android::sp<IThermalEventListener>& l) {
                           return android::IInterface::asBinder(l) == b;
                       }),
        m_listeners.end());

    const bool removed = (m_listeners.size() != oldSize);

    if (_aidl_return)
        *_aidl_return = removed;

    return android::binder::Status::ok();
}

android::binder::Status ThermalSensor::getCurrentThermalState(State* _aidl_return)
{
    if (_aidl_return == nullptr)
    {
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    *_aidl_return = m_currentState;
    return android::binder::Status::ok();
}

android::binder::Status ThermalSensor::getCurrentTemperatures(
    std::vector<TemperatureReading>* _aidl_return)
{
    if (_aidl_return == nullptr)
    {
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }

    _aidl_return->clear();

    // Minimal deterministic telemetry record.
    TemperatureReading r{};
    r.sensorName = android::String16("Stub Sensor");
    r.location   = android::String16("SoC");
    r.temperatureCelsius = 42.0;

    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    r.timestampMonotonicMs = (ms < 0) ? 0 : ms;

    r.vendorCode = 0;
    r.vendorInfo = android::String16("skeleton");

    _aidl_return->push_back(r);

    return android::binder::Status::ok();
}

} // namespace thermal
} // namespace sensor
} // namespace hal
} // namespace rdk
} // namespace com