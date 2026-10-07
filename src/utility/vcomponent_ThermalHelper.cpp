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
 * @file vcomponent_ThermalHelper.cpp
 * @brief Implementation of dependency-light helper utilities for Thermal.
 */

#include "utility/vcomponent_ThermalHelper.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>

namespace vcomponent
{
namespace utility
{

std::optional<std::string> readFileToString(const std::string& path)
{
    if (path.empty())
    {
        return std::nullopt;
    }

    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open())
    {
        return std::nullopt;
    }

    std::string contents;
    file.seekg(0, std::ios::end);
    const std::ifstream::pos_type fileSize = file.tellg();
    if (fileSize > 0)
    {
        contents.reserve(static_cast<std::size_t>(fileSize));
    }
    file.seekg(0, std::ios::beg);

    contents.assign(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());

    if (file.bad())
    {
        return std::nullopt;
    }

    return contents;
}

std::string trim(const std::string& input)
{
    auto isNotWhitespace = [](unsigned char c) {
        return std::isspace(c) == 0;
    };

    auto begin = std::find_if(input.begin(), input.end(), isNotWhitespace);
    if (begin == input.end())
    {
        return std::string{};
    }

    auto end = std::find_if(input.rbegin(), input.rend(), isNotWhitespace).base();
    return std::string(begin, end);
}

std::int64_t monotonicTimestampMs()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    return (milliseconds < 0) ? 0 : static_cast<std::int64_t>(milliseconds);
}

} // namespace utility
} // namespace vcomponent
