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
 * @brief SKELETON implementation of the thermal event listener AIDL callback.
 *
 * The callback body is a placeholder so the skeleton links; no event handling
 * behaviour is implemented.
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
constexpr const char* componentName = "ThermalEventListener";
} // namespace

android::binder::Status ThermalEventListener::onThermalStateChange(const ActionEvent& event)
{
    // TODO(skeleton): log/forward the received thermal state change event.
    (void)event;
    LOGF_INFO("%s: onThermalStateChange (skeleton, no behaviour implemented)", componentName);
    return android::binder::Status::ok();
}

} // namespace thermal
} // namespace sensor
} // namespace hal
} // namespace rdk
} // namespace com
