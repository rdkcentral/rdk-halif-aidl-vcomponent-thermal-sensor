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
 * @brief SKELETON client-side Binder stub for the thermal event listener AIDL interface.
 *
 * Upstream AIDL interface (sensor/current):
 *   com.rdk.hal.sensor.thermal.IThermalEventListener
 *
 * Usage:
 *   A client can instantiate this object and pass it into:
 *     IThermalSensor::registerEventListener(IThermalEventListener)
 *
 * The skeleton declares the callback surface only; the source file provides a
 * placeholder body.
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
 * @brief Minimal IThermalEventListener skeleton.
 *
 * This listener is intentionally a thin AIDL callback stub: it exposes no extra
 * non-AIDL APIs and implements no event handling behaviour.
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
     * @param[in] event Thermal action event carrying state and reading data.
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
