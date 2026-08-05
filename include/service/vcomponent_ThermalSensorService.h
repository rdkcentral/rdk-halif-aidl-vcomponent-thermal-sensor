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
 * @file vcomponent_ThermalSensorService.h
 * @brief Public entrypoint definitions for the Thermal Sensor Service executable.
 *
 * The service publishes the Binder/AIDL service (sensor.thermal) and then joins
 * the Binder thread pool via Binder APIs.
 */

namespace vcomponent::thermal::service
{

/**
 * @brief Default relative path to the Thermal Sensor HFP YAML profile.
 *
 * The service accepts:
 *  - CLI: --hfp <path>
 *
 * If not provided, this default is used.
 */
inline constexpr const char* kDefaultHfpPath =
    "vcomponent_configurations/hfp-sensor-thermal.yaml";

// PUBLIC_INTERFACE
/**
 * @brief Print CLI usage information for the Thermal Sensor service.
 *
 * @param argv0 argv[0] (program name) used for formatting usage output.
 *
 * Note: The implementation logs usage via the project's logger utility.
 */
void printUsage(const char* argv0);

} // namespace vcomponent::thermal::service