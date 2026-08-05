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
 * @file vcomponent_ThermalParseConfig.cpp
 * @brief SKELETON implementation of the Thermal HFP YAML parser.
 *
 * Every function declared in vcomponent_ThermalParseConfig.h is defined here
 * with a placeholder body only, so the skeleton compiles and links without
 * pulling in any parsing back-end (ut-core / ut-control KVP).
 *
 * Implementation notes for whoever fills this in:
 *  - Create the KVP instance with ut_kvp_createInstance() + ut_kvp_open() in
 *    vcomponentkvp_create_instance() and release it with
 *    ut_kvp_destroyInstance() in vcomponentkvp_destroy_instance().
 *  - vcomponent_Thermal_parse_config() should walk the `sensor.thermal` list
 *    (ut_kvp_getListCount) and fill one ThermalSensorConfig per entry using the
 *    YAML key names from vcomponent_configurations/hfp-sensor-thermal.yaml:
 *      id, sensorName, location,
 *      sensor_reading_range_celsius.{min,max},
 *      operational_temperature_celsius.{min,max},
 *      triggers.{critical_temperature_recovered_celsius,
 *                critical_temperature_exceeded_celsius,
 *                entering_critical_shutdown_celsius},
 *      policy.shutdown_min_downtime_s,
 *      policy.recovery.{strategy,min_cooldown_seconds},
 *      vendor.{vendorCode,vendorInfo}
 *  - Validation rule to enforce once implemented:
 *      recovered < exceeded < shutdown
 */

#include "utility/vcomponent_ThermalParseConfig.h"

namespace vcomponent
{
namespace utility
{

void* vcomponentkvp_create_instance(char* fileName)
{
    // TODO(skeleton): create a KVP instance and open `fileName`; return the
    // opaque instance pointer, or nullptr when creation/open fails.
    (void)fileName;
    return nullptr;
}

void vcomponentkvp_destroy_instance(void* instance)
{
    // TODO(skeleton): destroy the KVP instance created above (ignore nullptr).
    (void)instance;
}

bool vcomponent_Thermal_parse_config(
    char* configurationFile,
    ThermalHfpConfig& thermalConfiguration,
    std::string* outError)
{
    // TODO(skeleton): parse the `sensor.thermal` profile list from
    // `configurationFile` into `thermalConfiguration`, and populate `outError`
    // with a human readable reason on failure.
    (void)configurationFile;
    thermalConfiguration = ThermalHfpConfig{};
    if (outError != nullptr)
    {
        outError->clear();
    }
    return false;
}

bool loadThermalHfpConfigFromYaml(
    const std::string& path,
    ThermalHfpConfig* outConfig,
    std::string* outError)
{
    // TODO(skeleton): forward to vcomponent_Thermal_parse_config() once the
    // parser above is implemented.
    (void)path;
    if (outConfig != nullptr)
    {
        *outConfig = ThermalHfpConfig{};
    }
    if (outError != nullptr)
    {
        outError->clear();
    }
    return false;
}

} // namespace utility
} // namespace vcomponent
