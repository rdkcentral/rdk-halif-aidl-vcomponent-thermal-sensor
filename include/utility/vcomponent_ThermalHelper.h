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
 * @file vcomponent_ThermalHelper.h
 * @brief Small dependency-free helper utilities for the Thermal component.
 */

#include <cstdint>
#include <optional>
#include <string>

namespace vcomponent
{
namespace utility
{

// PUBLIC_INTERFACE
/**
 * @brief Read an entire file into a string.
 *
 * @param[in] path  Path to the file.
 *
 * @return File contents, or std::nullopt on error.
 */
std::optional<std::string> readFileToString(const std::string& path);

// PUBLIC_INTERFACE
/**
 * @brief Trim leading/trailing whitespace from a string.
 *
 * @param[in] input  Input string.
 *
 * @return Trimmed copy.
 */
std::string trim(const std::string& input);

// PUBLIC_INTERFACE
/**
 * @brief Return a monotonic timestamp in milliseconds.
 *
 * Used to populate `timestampMonotonicMs` fields defined by
 * com.rdk.hal.sensor.thermal.TemperatureReading and ActionEvent.
 *
 * @return Monotonic milliseconds since an unspecified epoch.
 */
std::int64_t monotonicTimestampMs();

} // namespace utility
} // namespace vcomponent
