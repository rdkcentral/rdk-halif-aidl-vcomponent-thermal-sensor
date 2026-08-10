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
 * @brief Implementation of the thermal sensor AIDL Binder service.
 *
 * The service is published as `sensor.thermal` and implements the AIDL APIs for
 * listener registration, aggregate state lookup, and temperature telemetry.
 * Sensor records are initialized from the selected HFP YAML profile. Runtime
 * control-plane orchestration remains intentionally excluded from this pass.
 */

#include "aidl/vcomponent_ThermalSensor.h"

#include "common/logger.h"
#include "utility/vcomponent_ThermalHelper.h"

#include <binder/IInterface.h>
#include <binder/Status.h>
#include <utils/String16.h>

#include <algorithm>
#include <cstdint>
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
constexpr const char* logPrefix = "[VDEVICE_THERMAL]<ThermalSensor>";

int stateSeverity(State state)
{
    switch (state)
    {
        case State::CRITICAL_SHUTDOWN_IMMINENT:
            return 3;
        case State::CRITICAL_TEMPERATURE_EXCEEDED:
            return 2;
        case State::CRITICAL_TEMPERATURE_RECOVERED:
            return 1;
        case State::NORMAL:
        default:
            return 0;
    }
}

bool isActiveCriticalTemperatureState(State state)
{
    return state == State::CRITICAL_TEMPERATURE_EXCEEDED ||
           state == State::CRITICAL_SHUTDOWN_IMMINENT;
}

State moreSevere(State left, State right)
{
    return (stateSeverity(right) > stateSeverity(left)) ? right : left;
}

TemperatureReading makeReadingFromConfig(
    const vcomponent::utility::ThermalSensorConfig& config,
    double temperatureCelsius,
    std::int64_t timestampMs)
{
    TemperatureReading reading{};
    const std::string sensorName = config.sensorName.empty() ? config.id : config.sensorName;

    reading.sensorName = android::String16(sensorName.c_str());
    reading.location = android::String16(config.location.c_str());
    reading.temperatureCelsius = temperatureCelsius;
    reading.timestampMonotonicMs = timestampMs;
    reading.vendorCode = config.vendor.vendorCode;
    reading.vendorInfo = android::String16(config.vendor.vendorInfo.c_str());

    return reading;
}

double initialTemperatureForSensor(const vcomponent::utility::ThermalSensorConfig& config)
{
    const double minTemperature = config.operationalTemperatureCelsius.min;
    const double maxTemperature = config.operationalTemperatureCelsius.max;

    if (maxTemperature > minTemperature)
    {
        return minTemperature + ((maxTemperature - minTemperature) / 2.0);
    }

    if (config.triggers.criticalTemperatureRecoveredCelsius > 0.0)
    {
        return config.triggers.criticalTemperatureRecoveredCelsius;
    }

    return 0.0;
}
} // namespace

ThermalSensor::ThermalSensor(vcomponent::utility::ThermalHfpConfig configuration)
{
    LOGF_INFO("%s: constructing AIDL service instance", logPrefix);

    initializeSensors(std::move(configuration));

    LOGF_INFO(
        "%s: constructed (serviceName=%s)",
        logPrefix,
        getServiceName());
}

ThermalSensor::~ThermalSensor()
{
    LOGF_INFO("%s: destroying AIDL service instance", logPrefix);

    m_stopUtWorker = true;
    if (m_utWorkerThread.joinable())
    {
        m_utWorkerThread.join();
    }

    // Control-plane shutdown is intentionally not invoked here because the
    // control-plane path is not initialized in this implementation pass.
}

void ThermalSensor::initializeSensors(vcomponent::utility::ThermalHfpConfig configuration)
{
    std::vector<SensorRuntime> sensors;
    sensors.reserve(configuration.sensors.size());

    const std::int64_t timestampMs = vcomponent::utility::monotonicTimestampMs();
    for (const auto& sensorConfig : configuration.sensors)
    {
        SensorRuntime runtime{};
        runtime.config = sensorConfig;
        runtime.state = State::NORMAL;
        runtime.reading = makeReadingFromConfig(
            sensorConfig,
            initialTemperatureForSensor(sensorConfig),
            timestampMs);

        sensors.push_back(runtime);

        LOGF_DEBUG(
            "%s: configured sensor id=%s name=%s location=%s vendorCode=%d",
            logPrefix,
            sensorConfig.id.c_str(),
            sensorConfig.sensorName.c_str(),
            sensorConfig.location.c_str(),
            static_cast<int>(sensorConfig.vendor.vendorCode));
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sensors = std::move(sensors);
        m_currentState = State::NORMAL;
    }

    LOGF_INFO(
        "%s: loaded %zu configured thermal sensor(s); aggregate state=%s",
        logPrefix,
        configuration.sensors.size(),
        toString(State::NORMAL).c_str());
}

void ThermalSensor::utWorkerLoop()
{
    // Control-plane processing is intentionally excluded from this pass.
    LOGF_DEBUG("%s: UT worker loop is disabled; control-plane work is excluded", logPrefix);
}

void ThermalSensor::handleQueuedUtMessage(const std::tuple<std::string, std::string, void*>& msg)
{
    // Control-plane message parsing is intentionally excluded from this pass.
    (void)msg;
    LOGF_DEBUG("%s: queued UT message ignored; control-plane work is excluded", logPrefix);
}

State ThermalSensor::evaluateSensorState(const SensorRuntime& runtime, double temperatureCelsius) const
{
    const auto& triggers = runtime.config.triggers;

    if (temperatureCelsius >= triggers.enteringCriticalShutdownCelsius)
    {
        return State::CRITICAL_SHUTDOWN_IMMINENT;
    }

    if (temperatureCelsius >= triggers.criticalTemperatureExceededCelsius)
    {
        return State::CRITICAL_TEMPERATURE_EXCEEDED;
    }

    if (isActiveCriticalTemperatureState(runtime.state))
    {
        if (temperatureCelsius <= triggers.criticalTemperatureRecoveredCelsius)
        {
            return State::CRITICAL_TEMPERATURE_RECOVERED;
        }

        // Keep reporting a critical state while cooling through the hysteresis
        // band between the recovery and exceeded thresholds. This prevents a
        // premature NORMAL transition before the configured recovery point is
        // reached.
        return State::CRITICAL_TEMPERATURE_EXCEEDED;
    }

    return State::NORMAL;
}

void ThermalSensor::applyTemperatureSample(const std::string& sensorId, double temperatureCelsius)
{
    LOGF_INFO(
        "%s: applying temperature sample sensorId=%s temperatureCelsius=%.3f",
        logPrefix,
        sensorId.c_str(),
        temperatureCelsius);

    ActionEvent event{};
    bool shouldNotify = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = std::find_if(
            m_sensors.begin(),
            m_sensors.end(),
            [&](const SensorRuntime& runtime) {
                return runtime.config.id == sensorId;
            });

        if (it == m_sensors.end())
        {
            LOGF_WARN(
                "%s: ignoring temperature sample for unknown sensorId=%s",
                logPrefix,
                sensorId.c_str());
            return;
        }

        const std::int64_t timestampMs = vcomponent::utility::monotonicTimestampMs();
        it->reading = makeReadingFromConfig(it->config, temperatureCelsius, timestampMs);
        it->state = evaluateSensorState(*it, temperatureCelsius);

        State aggregateState = State::NORMAL;
        for (const auto& runtime : m_sensors)
        {
            aggregateState = moreSevere(aggregateState, runtime.state);
        }

        LOGF_DEBUG(
            "%s: sample evaluated sensorId=%s sensorState=%s aggregateState=%s previousAggregateState=%s",
            logPrefix,
            sensorId.c_str(),
            toString(it->state).c_str(),
            toString(aggregateState).c_str(),
            toString(m_currentState).c_str());

        if (aggregateState != m_currentState)
        {
            m_currentState = aggregateState;

            event.state = aggregateState;
            event.timestampMonotonicMs = timestampMs;
            event.temperatureReading = it->reading;
            shouldNotify = true;
        }
    }

    if (shouldNotify)
    {
        LOGF_INFO(
            "%s: aggregate state changed to %s; notifying listeners",
            logPrefix,
            toString(event.state).c_str());
        notifyListeners(event);
    }
}

void ThermalSensor::notifyListeners(const ActionEvent& event)
{
    std::vector<android::sp<IThermalEventListener>> listeners;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        listeners = m_listeners;
    }

    LOGF_INFO(
        "%s: dispatching onThermalStateChange state=%s listenerCount=%zu",
        logPrefix,
        toString(event.state).c_str(),
        listeners.size());

    for (const auto& listener : listeners)
    {
        if (!listener.get())
        {
            LOGF_WARN("%s: skipped null listener during notification", logPrefix);
            continue;
        }

        const android::binder::Status status = listener->onThermalStateChange(event);
        if (!status.isOk())
        {
            LOGF_WARN(
                "%s: listener notification failed for state=%s",
                logPrefix,
                toString(event.state).c_str());
        }
    }
}

android::binder::Status ThermalSensor::registerEventListener(
    const android::sp<IThermalEventListener>& listener,
    bool* _aidl_return)
{
    LOGF_INFO("%s: registerEventListener called", logPrefix);

    if (_aidl_return)
        *_aidl_return = false;

    if (!listener.get())
    {
        LOGF_WARN("%s: registerEventListener rejected null listener", logPrefix);
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

            LOGF_DEBUG(
                "%s: registerEventListener duplicate ignored listenerCount=%zu",
                logPrefix,
                m_listeners.size());
            return android::binder::Status::ok();
        }
    }

    m_listeners.push_back(listener);

    if (_aidl_return)
        *_aidl_return = true; // newly registered

    LOGF_INFO(
        "%s: registerEventListener registered listenerCount=%zu",
        logPrefix,
        m_listeners.size());

    return android::binder::Status::ok();
}

android::binder::Status ThermalSensor::unregisterEventListener(
    const android::sp<IThermalEventListener>& listener,
    bool* _aidl_return)
{
    LOGF_INFO("%s: unregisterEventListener called", logPrefix);

    if (_aidl_return)
        *_aidl_return = false;

    if (!listener.get())
    {
        LOGF_WARN("%s: unregisterEventListener rejected null listener", logPrefix);
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

    LOGF_INFO(
        "%s: unregisterEventListener removed=%s listenerCount=%zu",
        logPrefix,
        removed ? "true" : "false",
        m_listeners.size());

    return android::binder::Status::ok();
}

android::binder::Status ThermalSensor::getCurrentThermalState(State* _aidl_return)
{
    LOGF_INFO("%s: getCurrentThermalState called", logPrefix);

    if (_aidl_return == nullptr)
    {
        LOGF_WARN("%s: getCurrentThermalState rejected null return pointer", logPrefix);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    *_aidl_return = m_currentState;

    LOGF_DEBUG(
        "%s: getCurrentThermalState returning %s",
        logPrefix,
        toString(m_currentState).c_str());

    return android::binder::Status::ok();
}

android::binder::Status ThermalSensor::getCurrentTemperatures(
    std::vector<TemperatureReading>* _aidl_return)
{
    LOGF_INFO("%s: getCurrentTemperatures called", logPrefix);

    if (_aidl_return == nullptr)
    {
        LOGF_WARN("%s: getCurrentTemperatures rejected null return pointer", logPrefix);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    _aidl_return->clear();
    _aidl_return->reserve(m_sensors.size());

    for (const auto& runtime : m_sensors)
    {
        _aidl_return->push_back(runtime.reading);
    }

    LOGF_DEBUG(
        "%s: getCurrentTemperatures returning %zu reading(s)",
        logPrefix,
        _aidl_return->size());

    return android::binder::Status::ok();
}

} // namespace thermal
} // namespace sensor
} // namespace hal
} // namespace rdk
} // namespace com
