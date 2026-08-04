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
 * @file vcomponent_Thermal_ParseConfig.h
 * @brief SKELETON declarations for loading Thermal configuration from HFP YAML.
 *
 * This header declares the parser contract only; all bodies live as stubs in
 * src/utility/vcomponent_Thermal_ParseConfig.cpp and must be implemented.
 *
 * This module mirrors the structure used by other vcomponent parsers: create a
 * KVP instance with vcomponentkvp_create_instance(), walk profile keys, then
 * release the instance with vcomponentkvp_destroy_instance().
 */

#include "utility/vcomponent_ThermalHfpConfigUtils.h"

#include <string>

namespace vcomponent
{
namespace utility
{

// PUBLIC_INTERFACE
/**
 * @brief Create and open a ut-core/KVP instance for a YAML configuration file.
 *
 * @param[in] fileName YAML file path. Must not be nullptr.
 *
 * @return Opaque KVP instance on success, nullptr on failure.
 */
void* vcomponentkvp_create_instance(char* fileName);

// PUBLIC_INTERFACE
/**
 * @brief Destroy a KVP instance created by vcomponentkvp_create_instance().
 *
 * @param[in] instance Opaque KVP instance. A nullptr value is ignored.
 */
void vcomponentkvp_destroy_instance(void* instance);

// PUBLIC_INTERFACE
/**
 * @brief Parse thermal HFP YAML into ThermalHfpConfig.
 *
 * The parser reads the `sensor.thermal` profile list from the supplied YAML
 * file using ut-core/ut-control KVP APIs. Field names intentionally match the
 * YAML names used by the AIDL specification HFP file.
 *
 * @param[in]  configurationFile    YAML file path. Must not be nullptr.
 * @param[out] thermalConfiguration Output config populated on success.
 * @param[out] outError             Optional error string populated on failure.
 *
 * @return true on success, false on error.
 */
bool vcomponent_Thermal_parse_config(
    char* configurationFile,
    ThermalHfpConfig& thermalConfiguration,
    std::string* outError);

// PUBLIC_INTERFACE
/**
 * @brief Compatibility wrapper around vcomponent_Thermal_parse_config().
 *
 * @param[in]  path       YAML file path.
 * @param[out] outConfig  Output config. Must not be nullptr.
 * @param[out] outError   Optional error string populated on failure.
 *
 * @return true on success, false on error.
 */
bool loadThermalHfpConfigFromYaml(
    const std::string& path,
    ThermalHfpConfig* outConfig,
    std::string* outError);

} // namespace utility
} // namespace vcomponent
