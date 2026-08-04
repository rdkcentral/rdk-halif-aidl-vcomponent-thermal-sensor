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
 * @file vcomponent_ThermalUtController.cpp
 * @brief SKELETON implementation of the Thermal UT controller facade.
 *
 * Every method declared in vcomponent_ThermalUtController.h is defined here so
 * the skeleton links, but the bodies are placeholders. The YAML parsing
 * entrypoint (vcomponent::utility::loadThermalHfpConfigFromYaml) is already
 * implemented and can be wired in directly.
 */

#include "controller/vcomponent_ThermalUtController.h"

#include "utility/vcomponent_ThermalHelper.h"
#include "utility/vcomponent_Thermal_ParseConfig.h"

namespace vcomponent
{
namespace thermal
{
namespace controller
{

ThermalUtController::ThermalUtController()
    : m_controlPlaneInstance(nullptr)
    , m_userData(nullptr)
{
    // TODO(skeleton): add any additional controller state initialization.
}

ThermalUtController::~ThermalUtController()
{
    shutdown();
}

bool ThermalUtController::loadConfiguration(const std::string& hfpYamlPath, std::string* outError)
{
    // TODO(skeleton): call vcomponent::utility::loadThermalHfpConfigFromYaml()
    // and cache the parsed model in m_hfpConfig / m_hfpYamlPath.
    (void)hfpYamlPath;
    if (outError != nullptr)
    {
        *outError = "ThermalUtController::loadConfiguration not implemented (skeleton)";
    }
    return false;
}

bool ThermalUtController::buildInventory(std::string* outInventory, std::string* outError) const
{
    // TODO(skeleton): render m_hfpConfig into a text inventory for UT runners.
    (void)outInventory;
    if (outError != nullptr)
    {
        *outError = "ThermalUtController::buildInventory not implemented (skeleton)";
    }
    return false;
}

bool ThermalUtController::init(std::uint16_t port, void* userData)
{
    // TODO(skeleton): UT_ControlPlane_Init(port),
    // UT_ControlPlane_RegisterCallbackOnMessage(..., "thermal", messageCallback, this),
    // then UT_ControlPlane_Start().
    (void)port;
    (void)userData;
    return false;
}

void ThermalUtController::shutdown()
{
    // TODO(skeleton): UT_ControlPlane_Exit() and drain m_messageQueue.
}

std::optional<std::tuple<std::string, std::string, void*>> ThermalUtController::getMessage()
{
    // TODO(skeleton): pop the next queued UT control-plane message.
    return std::nullopt;
}

bool ThermalUtController::isRunning() const
{
    return m_controlPlaneInstance != nullptr;
}

void ThermalUtController::messageCallback(char* key, ut_kvp_instance_t* instance, void* userData)
{
    // TODO(skeleton): forward to pushMessage() on the owning instance.
    (void)key;
    (void)instance;
    (void)userData;
}

void ThermalUtController::pushMessage(char* key, ut_kvp_instance_t* instance)
{
    // TODO(skeleton): copy key/payload into m_messageQueue under m_queueMutex.
    (void)key;
    (void)instance;
}

} // namespace controller
} // namespace thermal
} // namespace vcomponent
