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
 * @file vcomponent_ThermalEventListener.h
 * @brief Client-side Binder stub for the thermal event listener AIDL interface.
 *
 * Upstream AIDL interface:
 *   com.rdk.hal.sensor.thermal.IThermalEventListener
 *
 * Usage:
 *   A client can instantiate this object and pass it into:
 *     IThermalSensor::registerEventListener(IThermalEventListener)
 *
 * The callback logs received ActionEvent payload details for observability.
 * When CRITICAL_SHUTDOWN_IMMINENT is received, clients may only perform
 * time-critical cleanup, such as flushing caches or persisting critical state;
 * the thermal policy HAL records the cause and manages device power-off.
 */

#include <com/rdk/hal/sensor/thermal/ActionEvent.h>
#include <com/rdk/hal/sensor/thermal/BnThermalEventListener.h>

#include <binder/Status.h>

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
 * @brief Minimal IThermalEventListener implementation for local clients/tests.
 *
 * This listener is intentionally thin: it acknowledges callback delivery and
 * logs the ActionEvent fields without adding non-AIDL behavior. Clients must
 * not attempt to reboot or power off the device in response to a thermal event.
 */
class ThermalEventListener final : public BnThermalEventListener
{
public:
    // PUBLIC_INTERFACE
    /**
     * @brief Construct an event listener instance.
     */
    ThermalEventListener() = default;

    /**
     * @brief Destroy the event listener instance.
     */
    ~ThermalEventListener() override = default;

    ThermalEventListener(const ThermalEventListener&) = delete;
    ThermalEventListener& operator=(const ThermalEventListener&) = delete;

    // PUBLIC_INTERFACE
    /**
     * @brief AIDL callback invoked when the thermal state changes.
     *
     * @param[in] event Thermal action event carrying state and optional reading data.
     *
     * @return Binder status.
     */
    android::binder::Status onThermalStateChange(const ActionEvent& event) override;
};

} // namespace thermal
} // namespace sensor
} // namespace hal
} // namespace rdk
} // namespace com
