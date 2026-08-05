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
 * @file vcomponent_ThermalHfpConfigUtils.h
 * @brief Thermal HFP configuration data model.
 *
 * These structures mirror the
 * vcomponent_configurations/hfp-sensor-thermal.yaml layout. Parsing is
 * implemented in vcomponent_ThermalParseConfig.cpp; this header defines only
 * the in-memory representation.
 *
 * The model intentionally follows the hfp-sensor-thermal.yaml hierarchy:
 *
 *   sensor
 *     thermal[]
 *       id / sensorName / location
 *       sensor_reading_range_celsius { min, max }
 *       operational_temperature_celsius { min, max }
 *       triggers { recovered, exceeded, shutdown }
 *       policy { shutdown_min_downtime_s, recovery { strategy, min_cooldown_seconds } }
 *       vendor { vendorCode, vendorInfo }
 *
 * Keeping the in-memory structure close to the YAML makes the parser
 * reviewable and supports future runtime use when new AIDL/HFP fields are
 * added.
 */

#include <cstdint>
#include <string>
#include <vector>

namespace vcomponent
{
namespace utility
{

/**
 * @brief Inclusive temperature range in degrees Celsius.
 */
struct ThermalRange
{
    double min{0.0};
    double max{0.0};
};

/**
 * @brief Vendor thermal policy trigger points in degrees Celsius.
 *
 * The HFP policy convention is recovered < exceeded < shutdown. The current
 * parser stores supplied values but does not enforce this ordering.
 */
struct ThermalTriggers
{
    double criticalTemperatureRecoveredCelsius{0.0};
    double criticalTemperatureExceededCelsius{0.0};
    double enteringCriticalShutdownCelsius{0.0};
};

/**
 * @brief Vendor recovery strategy configuration.
 */
struct ThermalRecoveryPolicy
{
    std::string strategy{"TIME_BASED"};
    int32_t minCooldownSeconds{0};
};

/**
 * @brief Vendor thermal policy configuration.
 */
struct ThermalPolicy
{
    int32_t shutdownMinDowntimeSeconds{0};
    ThermalRecoveryPolicy recovery;
};

/**
 * @brief Vendor diagnostic metadata mapped into TemperatureReading.
 */
struct ThermalVendorInfo
{
    int32_t vendorCode{0};
    std::string vendorInfo;
};

/**
 * @brief Per-sensor thermal HFP configuration entry.
 */
struct ThermalSensorConfig
{
    std::string id;
    std::string sensorName;
    std::string location;

    ThermalRange sensorReadingRangeCelsius;
    ThermalRange operationalTemperatureCelsius;
    ThermalTriggers triggers;
    ThermalPolicy policy;
    ThermalVendorInfo vendor;
};

/**
 * @brief Top-level thermal HFP configuration model.
 */
struct ThermalHfpConfig
{
    std::vector<ThermalSensorConfig> sensors;
};

} // namespace utility
} // namespace vcomponent
