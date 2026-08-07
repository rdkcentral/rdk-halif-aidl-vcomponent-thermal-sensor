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
 * @file vcomponent_ThermalEventListener.cpp
 * @brief Implementation of the thermal event listener AIDL callback.
 *
 * The listener acknowledges asynchronous onThermalStateChange callbacks and
 * logs ActionEvent details for debugging and integration verification.
 */

#include "aidl/vcomponent_ThermalEventListener.h"

#include "common/logger.h"

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
constexpr const char* logPrefix = "[VDEVICE_THERMAL]<ThermalEventListener>";
} // namespace

android::binder::Status ThermalEventListener::onThermalStateChange(const ActionEvent& event)
{
    LOGF_INFO(
        "%s: onThermalStateChange received state=%s timestampMonotonicMs=%lld hasTemperatureReading=%s",
        logPrefix,
        toString(event.state).c_str(),
        static_cast<long long>(event.timestampMonotonicMs),
        event.temperatureReading.has_value() ? "true" : "false");

    if (event.temperatureReading.has_value())
    {
        const TemperatureReading& reading = event.temperatureReading.value();
        LOGF_DEBUG(
            "%s: event reading temperatureCelsius=%.3f timestampMonotonicMs=%lld vendorCode=%d",
            logPrefix,
            reading.temperatureCelsius,
            static_cast<long long>(reading.timestampMonotonicMs),
            static_cast<int>(reading.vendorCode));
    }

    return android::binder::Status::ok();
}

} // namespace thermal
} // namespace sensor
} // namespace hal
} // namespace rdk
} // namespace com
