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
 * control-plane orchestration accepts IThermalSensor temperature_update messages
 * on the configured UT port, updates telemetry/state in memory, notifies
 * listeners on aggregate state transitions, and requests the separate
 * BootReason Binder service when thermal shutdown is imminent.
 */

#include "aidl/vcomponent_ThermalSensor.h"

#include "common/logger.h"
#include "utility/vcomponent_ThermalHelper.h"

#include <binder/IInterface.h>
#include <binder/IServiceManager.h>
#include <binder/Status.h>
#include <com/rdk/hal/bootreason/BootCause.h>
#include <com/rdk/hal/bootreason/IBootReason.h>
#include <utils/String16.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cerrno>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
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

namespace
{
constexpr const char* logPrefix = "[VDEVICE_THERMAL]<ThermalSensor>";
constexpr int kUtWorkerIdleSleepMs = 50;
constexpr const char* kThermalShutdownReason = "thermal_shutdown";
constexpr std::size_t kBootReasonMaxBytes = 64U;
constexpr const char* kSystemctlPath = "/bin/systemctl";
constexpr const char* kSystemctlPoweroffArgument = "poweroff";
constexpr const char* kSystemctlNoBlockArgument = "--no-block";

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

ThermalSensor::ThermalSensor(
    vcomponent::utility::ThermalHfpConfig configuration,
    std::uint16_t controlPort)
    : m_controlPort(controlPort)
{
    LOGF_INFO(
        "%s: constructing AIDL service instance with controlPort=%u",
        logPrefix,
        static_cast<unsigned>(m_controlPort));

    initializeSensors(std::move(configuration));

    if (m_ut.init(m_controlPort, this))
    {
        m_stopUtWorker = false;
        m_utWorkerThread = std::thread(&ThermalSensor::utWorkerLoop, this);
        LOGF_INFO(
            "%s: IThermalSensor control plane enabled on port %u",
            logPrefix,
            static_cast<unsigned>(m_controlPort));
    }
    else
    {
        LOGF_WARN(
            "%s: IThermalSensor control plane failed to start on port %u; Binder service remains available",
            logPrefix,
            static_cast<unsigned>(m_controlPort));
    }

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
        LOGF_DEBUG("%s: joining control-plane worker thread", logPrefix);
        m_utWorkerThread.join();
    }

    m_ut.shutdown();

    LOGF_INFO("%s: destroyed AIDL service instance", logPrefix);
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
    LOGF_INFO(
        "%s: control-plane worker started (port=%u)",
        logPrefix,
        static_cast<unsigned>(m_controlPort));

    while (!m_stopUtWorker.load())
    {
        std::optional<vcomponent::thermal::controller::ThermalTemperatureUpdate> update =
            m_ut.getTemperatureUpdate();

        if (update.has_value())
        {
            handleControlPlaneUpdate(*update);
            continue;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(kUtWorkerIdleSleepMs));
    }

    LOGF_INFO("%s: control-plane worker stopped", logPrefix);
}

void ThermalSensor::handleControlPlaneUpdate(
    const vcomponent::thermal::controller::ThermalTemperatureUpdate& update)
{
    LOGF_INFO(
        "%s: handling temperature_update sensorName=%s temperatureCelsius=%.3f timestampMonotonicMs=%lld",
        logPrefix,
        update.sensorName.c_str(),
        update.temperatureCelsius,
        static_cast<long long>(update.timestampMonotonicMs));

    applyTemperatureSample(
        update.sensorName,
        update.temperatureCelsius,
        update.timestampMonotonicMs);
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

void ThermalSensor::applyTemperatureSample(
    const std::string& sensorNameOrId,
    double temperatureCelsius,
    std::int64_t timestampMonotonicMs)
{
    LOGF_INFO(
        "%s: applying temperature sample sensorNameOrId=%s temperatureCelsius=%.3f timestampMonotonicMs=%lld",
        logPrefix,
        sensorNameOrId.c_str(),
        temperatureCelsius,
        static_cast<long long>(timestampMonotonicMs));

    ActionEvent event{};
    bool shouldNotify = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = std::find_if(
            m_sensors.begin(),
            m_sensors.end(),
            [&](const SensorRuntime& runtime) {
                return runtime.config.sensorName == sensorNameOrId || runtime.config.id == sensorNameOrId;
            });

        if (it == m_sensors.end())
        {
            LOGF_WARN(
                "%s: ignoring temperature_update for unknown sensorNameOrId=%s",
                logPrefix,
                sensorNameOrId.c_str());
            return;
        }

        const auto& measurableRange = it->config.sensorReadingRangeCelsius;
        if (!std::isfinite(temperatureCelsius) ||
            temperatureCelsius < measurableRange.min ||
            temperatureCelsius > measurableRange.max)
        {
            LOGF_WARN(
                "%s: rejecting out-of-range temperature_update sensorNameOrId=%s "
                "temperatureCelsius=%.3f measurableRange=[%.3f, %.3f]",
                logPrefix,
                sensorNameOrId.c_str(),
                temperatureCelsius,
                measurableRange.min,
                measurableRange.max);
            return;
        }

        const std::int64_t effectiveTimestampMs =
            (timestampMonotonicMs >= 0) ? timestampMonotonicMs : vcomponent::utility::monotonicTimestampMs();

        it->reading = makeReadingFromConfig(it->config, temperatureCelsius, effectiveTimestampMs);
        it->state = evaluateSensorState(*it, temperatureCelsius);

        State aggregateState = State::NORMAL;
        for (const auto& runtime : m_sensors)
        {
            aggregateState = moreSevere(aggregateState, runtime.state);
        }

        if (m_currentState == State::CRITICAL_SHUTDOWN_IMMINENT &&
            aggregateState != State::CRITICAL_SHUTDOWN_IMMINENT)
        {
            LOGF_INFO(
                "%s: suppressing aggregate transition from CRITICAL_SHUTDOWN_IMMINENT to %s for sensorNameOrId=%s",
                logPrefix,
                toString(aggregateState).c_str(),
                sensorNameOrId.c_str());
            aggregateState = State::CRITICAL_SHUTDOWN_IMMINENT;
        }

        LOGF_DEBUG(
            "%s: sample evaluated sensorNameOrId=%s sensorState=%s aggregateState=%s previousAggregateState=%s",
            logPrefix,
            sensorNameOrId.c_str(),
            toString(it->state).c_str(),
            toString(aggregateState).c_str(),
            toString(m_currentState).c_str());

        if (aggregateState != m_currentState)
        {
            m_currentState = aggregateState;

            event.state = aggregateState;
            event.timestampMonotonicMs = effectiveTimestampMs;
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

        if (event.state == State::CRITICAL_SHUTDOWN_IMMINENT)
        {
            LOGF_INFO(
                "%s: aggregate state entered CRITICAL_SHUTDOWN_IMMINENT; initiating autonomous power-off",
                logPrefix);
            scheduleThermalShutdown(kThermalShutdownReason);
        }
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

void ThermalSensor::scheduleThermalShutdown(const std::string& reasonString)
{
    bool expected = false;
    if (!m_shutdownScheduled.compare_exchange_strong(expected, true))
    {
        LOGF_INFO(
            "%s: thermal shutdown already scheduled; ignoring duplicate request",
            logPrefix);
        return;
    }

    LOGF_INFO(
        "%s: recording thermal shutdown reason before autonomous power-off",
        logPrefix);
    recordThermalShutdownReason(reasonString);

    // Do not let Boot Reason availability prevent hardware protection. The
    // thermal HAL owns the shutdown action and always requests power-off after
    // attempting to persist the diagnostic cause.
    requestSystemPoweroff();
}

void ThermalSensor::recordThermalShutdownReason(const std::string& reasonString)
{
    using com::rdk::hal::bootreason::BootCause;
    using com::rdk::hal::bootreason::IBootReason;

    if (reasonString.empty() || reasonString.size() > kBootReasonMaxBytes)
    {
        LOGF_ERROR(
            "%s: cannot record thermal shutdown reason because length=%zu is outside the allowed range 1..%zu",
            logPrefix,
            reasonString.size(),
            kBootReasonMaxBytes);
        return;
    }

    android::sp<android::IServiceManager> serviceManager = android::defaultServiceManager();
    if (!serviceManager.get())
    {
        LOGF_ERROR("%s: cannot record thermal shutdown reason because Binder service manager is unavailable", logPrefix);
        return;
    }

    const std::string bootServiceName = IBootReason::serviceName();
    const android::sp<android::IBinder> binder =
        serviceManager->checkService(android::String16(bootServiceName.c_str()));
    if (!binder.get())
    {
        LOGF_ERROR(
            "%s: BootReason service '%s' is unavailable; thermal shutdown cause cannot be persisted",
            logPrefix,
            bootServiceName.c_str());
        return;
    }

    const android::sp<IBootReason> boot = android::interface_cast<IBootReason>(binder);
    if (!boot.get())
    {
        LOGF_ERROR(
            "%s: failed to create IBootReason client for service '%s'",
            logPrefix,
            bootServiceName.c_str());
        return;
    }

    const android::String16 binderReason(reasonString.c_str());

    // Preserve the next-boot diagnostic cause before the HAL initiates its
    // independent system power-off sequence.
    const android::binder::Status causeStatus =
        boot->setBootCause(BootCause::THERMAL_RESET, binderReason);
    if (!causeStatus.isOk())
    {
        LOGF_ERROR(
            "%s: BootReason setBootCause(THERMAL_RESET) failed; power-off will still be requested",
            logPrefix);
        return;
    }

    LOGF_INFO(
        "%s: persisted BootCause::THERMAL_RESET through BootReason with reason=%s",
        logPrefix,
        reasonString.c_str());
}

void ThermalSensor::requestSystemPoweroff()
{
    // Use an absolute executable path and explicit argv so this critical path
    // neither invokes a shell nor relies on PATH. A double-fork detaches
    // systemctl from the HAL, while the parent reaps the short-lived launcher
    // child so no zombie can remain if power-off is delayed or fails.
    if (access(kSystemctlPath, X_OK) != 0)
     {
         LOGF_ERROR(
             "%s: autonomous power-off command is unavailable at %s: errno=%d",
             logPrefix,
             kSystemctlPath,
             errno);
         return;
     }
    LOGF_INFO(
        "%s: launching autonomous power-off command: %s %s %s",
        logPrefix,
        kSystemctlPath,
        kSystemctlNoBlockArgument,
        kSystemctlPoweroffArgument);

    const pid_t childPid = fork();
    if (childPid < 0)
    {
        LOGF_ERROR(
            "%s: could not fork autonomous power-off command: errno=%d",
            logPrefix,
            errno);
        m_shutdownScheduled = false;
        return;
    }

    if (childPid == 0)
    {
        const pid_t systemctlPid = fork();
        if (systemctlPid < 0)
        {
            _exit(127);
        }

        if (systemctlPid > 0)
        {
            // The parent below reaps this launcher process. The systemctl
            // grandchild is re-parented to init (or a configured subreaper).
            _exit(0);
        }

        execl(
            kSystemctlPath,
            kSystemctlPath,
            kSystemctlNoBlockArgument,
            kSystemctlPoweroffArgument,
            static_cast<char*>(nullptr));

        // exec only returns when launch fails. _exit prevents child cleanup
        // handlers from running in the forked copy of this Binder service.
        _exit(127);
    }

    // This child only performs the second fork and immediately exits. Reap it
    // before returning; retry when an unrelated signal interrupts waitpid.
    int launcherStatus = 0;
    pid_t reapedPid;
    do
    {
        reapedPid = waitpid(childPid, &launcherStatus, 0);
    } while (reapedPid == -1 && errno == EINTR);

    if (reapedPid == -1)
    {
        LOGF_ERROR(
            "%s: could not reap autonomous power-off launcher pid=%ld: errno=%d",
            logPrefix,
            static_cast<long>(childPid),
            errno);
        return;
    }

    if (WIFEXITED(launcherStatus) && WEXITSTATUS(launcherStatus) == 0)
    {
        LOGF_INFO(
            "%s: autonomous power-off command launcher completed successfully pid=%ld",
            logPrefix,
            static_cast<long>(childPid));
        return;
    }

    if (WIFEXITED(launcherStatus))
    {
        LOGF_ERROR(
            "%s: autonomous power-off command launcher failed pid=%ld exitStatus=%d",
            logPrefix,
            static_cast<long>(childPid),
            WEXITSTATUS(launcherStatus));
        m_shutdownScheduled = false;
        return;
    }

    if (WIFSIGNALED(launcherStatus))
    {
        LOGF_ERROR(
            "%s: autonomous power-off command launcher was terminated pid=%ld signal=%d",
            logPrefix,
            static_cast<long>(childPid),
            WTERMSIG(launcherStatus));
        return;
    }

    LOGF_ERROR(
        "%s: autonomous power-off command launcher ended unexpectedly pid=%ld status=%d",
        logPrefix,
        static_cast<long>(childPid),
        launcherStatus);
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
