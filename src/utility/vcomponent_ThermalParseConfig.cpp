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
 * @brief Thermal HFP YAML parser implementation.
 *
 * Implementation of thermal HFP YAML parsing using the same KVP instance
 * lifecycle and function naming conventions used by other vcomponent parsers.
 */

#include "utility/vcomponent_ThermalParseConfig.h"
#include "common/logger.h"

#include <ut_kvp_profile.h>

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_set>

namespace vcomponent
{
namespace utility
{
namespace
{

constexpr const char* THERMAL_ROOT = "sensor";
constexpr const char* THERMAL_LIST = "sensor.thermal";
constexpr const char* logPrefix = "[VDEVICE_THERMAL]<ThermalParseConfig>";
constexpr size_t KVP_BUFFER_SIZE = UT_KVP_MAX_ELEMENT_SIZE;

void setError(std::string* outError, const std::string& message)
{
    if (outError != nullptr)
    {
        *outError = message;
    }
}

bool fieldPresent(ut_kvp_instance_t* instance, const std::string& fieldKey)
{
    return instance != nullptr && ut_kvp_fieldPresent(instance, fieldKey.c_str());
}

bool readStringField(
    ut_kvp_instance_t* instance,
    const std::string& fieldKey,
    std::string* outValue,
    std::string* outError,
    bool required)
{
    if (outValue == nullptr)
    {
        setError(outError, "internal parser error: null string output for key " + fieldKey);
        return false;
    }

    outValue->clear();

    if (!fieldPresent(instance, fieldKey))
    {
        if (required)
        {
            setError(outError, "required thermal HFP field is missing: " + fieldKey);
            return false;
        }
        return true;
    }

    char resultKVP[KVP_BUFFER_SIZE] = {0};
    const ut_kvp_status_t status =
        ut_kvp_getStringField(instance, fieldKey.c_str(), resultKVP, sizeof(resultKVP));

    if (status != UT_KVP_STATUS_SUCCESS)
    {
        setError(outError, "failed to read thermal HFP string field: " + fieldKey);
        return false;
    }

    *outValue = resultKVP;
    return true;
}

bool parseInt32(const std::string& value, int32_t* outValue)
{
    if (outValue == nullptr || value.empty())
    {
        return false;
    }

    errno = 0;
    char* endPtr = nullptr;
    const long parsed = std::strtol(value.c_str(), &endPtr, 0);
    if (errno != 0 || endPtr == value.c_str() || (endPtr != nullptr && *endPtr != '\0') ||
        parsed < std::numeric_limits<int32_t>::min() ||
        parsed > std::numeric_limits<int32_t>::max())
    {
        return false;
    }

    *outValue = static_cast<int32_t>(parsed);
    return true;
}

bool parseDouble(const std::string& value, double* outValue)
{
    if (outValue == nullptr || value.empty())
    {
        return false;
    }

    errno = 0;
    char* endPtr = nullptr;
    const double parsed = std::strtod(value.c_str(), &endPtr);
    if (errno != 0 || endPtr == value.c_str() || (endPtr != nullptr && *endPtr != '\0') ||
        !std::isfinite(parsed))
    {
        return false;
    }

    *outValue = parsed;
    return true;
}

bool readInt32Field(
    ut_kvp_instance_t* instance,
    const std::string& fieldKey,
    int32_t* outValue,
    std::string* outError,
    bool required)
{
    std::string value;
    if (!readStringField(instance, fieldKey, &value, outError, required))
    {
        return false;
    }

    if (value.empty() && !required)
    {
        return true;
    }

    if (!parseInt32(value, outValue))
    {
        setError(outError, "failed to parse thermal HFP integer field: " + fieldKey + " value=" + value);
        return false;
    }

    return true;
}

bool readDoubleField(
    ut_kvp_instance_t* instance,
    const std::string& fieldKey,
    double* outValue,
    std::string* outError,
    bool required)
{
    std::string value;
    if (!readStringField(instance, fieldKey, &value, outError, required))
    {
        return false;
    }

    if (value.empty() && !required)
    {
        return true;
    }

    if (!parseDouble(value, outValue))
    {
        setError(
            outError,
            "failed to parse thermal HFP floating-point field: " + fieldKey + " value=" + value);
        return false;
    }

    return true;
}

bool readRange(
    ut_kvp_instance_t* instance,
    const std::string& prefix,
    ThermalRange* outRange,
    std::string* outError)
{
    if (outRange == nullptr)
    {
        setError(outError, "internal parser error: null range output for key " + prefix);
        return false;
    }

    if (!readDoubleField(instance, prefix + ".min", &outRange->min, outError, false))
    {
        return false;
    }

    return readDoubleField(instance, prefix + ".max", &outRange->max, outError, false);
}

bool validateSensorConfiguration(
    const ThermalSensorConfig& sensorConfig,
    uint32_t index,
    std::unordered_set<std::string>* sensorIds,
    std::unordered_set<std::string>* sensorNames,
    std::string* outError)
{
    if (sensorIds == nullptr || sensorNames == nullptr)
    {
        setError(outError, "internal parser error: null uniqueness tracker");
        return false;
    }

    const std::string sensorLabel = "thermal sensor index " + std::to_string(index);
    if (sensorConfig.id.empty())
    {
        setError(outError, sensorLabel + " has an empty id");
        return false;
    }

    if (!sensorIds->insert(sensorConfig.id).second)
    {
        setError(outError, sensorLabel + " duplicates id '" + sensorConfig.id + "'");
        return false;
    }

    if (!sensorConfig.sensorName.empty() && !sensorNames->insert(sensorConfig.sensorName).second)
    {
        setError(outError, sensorLabel + " duplicates sensorName '" + sensorConfig.sensorName + "'");
        return false;
    }

    const ThermalRange& measurable = sensorConfig.sensorReadingRangeCelsius;
    const ThermalRange& operational = sensorConfig.operationalTemperatureCelsius;
    if (measurable.min > measurable.max)
    {
        setError(outError, sensorLabel + " has an inverted sensor_reading_range_celsius");
        return false;
    }

    if (operational.min > operational.max)
    {
        setError(outError, sensorLabel + " has an inverted operational_temperature_celsius");
        return false;
    }

    if (operational.min < measurable.min || operational.max > measurable.max)
    {
        setError(
            outError,
            sensorLabel + " operational_temperature_celsius must be contained by sensor_reading_range_celsius");
        return false;
    }

    const ThermalTriggers& triggers = sensorConfig.triggers;
    if (!(triggers.criticalTemperatureRecoveredCelsius <
          triggers.criticalTemperatureExceededCelsius &&
          triggers.criticalTemperatureExceededCelsius <
          triggers.enteringCriticalShutdownCelsius))
    {
        setError(
            outError,
            sensorLabel + " triggers must satisfy recovered < exceeded < shutdown");
        return false;
    }

    if (triggers.criticalTemperatureRecoveredCelsius < measurable.min ||
        triggers.enteringCriticalShutdownCelsius > measurable.max)
    {
        setError(outError, sensorLabel + " triggers must be within sensor_reading_range_celsius");
        return false;
    }

    if (sensorConfig.policy.shutdownMinDowntimeSeconds < 0 ||
        sensorConfig.policy.recovery.minCooldownSeconds < 0)
    {
        setError(outError, sensorLabel + " policy durations must be non-negative");
        return false;
    }

    return true;
}

bool vcomponent_Thermal_get_sensor_info(
    ut_kvp_instance_t* kvpTestInstance,
    uint32_t index,
    ThermalSensorConfig& sensorConfig,
    std::string* outError)
{
    sensorConfig = ThermalSensorConfig{};

    const std::string prefix = std::string(THERMAL_LIST) + "." + std::to_string(index) + ".";

    if (!readStringField(kvpTestInstance, prefix + "id", &sensorConfig.id, outError, true))
    {
        return false;
    }

    if (!readStringField(kvpTestInstance, prefix + "sensorName", &sensorConfig.sensorName, outError, false))
    {
        return false;
    }

    if (!readStringField(kvpTestInstance, prefix + "location", &sensorConfig.location, outError, false))
    {
        return false;
    }

    if (!readRange(
            kvpTestInstance,
            prefix + "sensor_reading_range_celsius",
            &sensorConfig.sensorReadingRangeCelsius,
            outError))
    {
        return false;
    }

    if (!readRange(
            kvpTestInstance,
            prefix + "operational_temperature_celsius",
            &sensorConfig.operationalTemperatureCelsius,
            outError))
    {
        return false;
    }

    const std::string triggerPrefix = prefix + "triggers.";
    if (!readDoubleField(
            kvpTestInstance,
            triggerPrefix + "critical_temperature_recovered_celsius",
            &sensorConfig.triggers.criticalTemperatureRecoveredCelsius,
            outError,
            false))
    {
        return false;
    }

    if (!readDoubleField(
            kvpTestInstance,
            triggerPrefix + "critical_temperature_exceeded_celsius",
            &sensorConfig.triggers.criticalTemperatureExceededCelsius,
            outError,
            false))
    {
        return false;
    }

    if (!readDoubleField(
            kvpTestInstance,
            triggerPrefix + "entering_critical_shutdown_celsius",
            &sensorConfig.triggers.enteringCriticalShutdownCelsius,
            outError,
            false))
    {
        return false;
    }

    const std::string policyPrefix = prefix + "policy.";
    if (!readInt32Field(
            kvpTestInstance,
            policyPrefix + "shutdown_min_downtime_s",
            &sensorConfig.policy.shutdownMinDowntimeSeconds,
            outError,
            false))
    {
        return false;
    }

    if (!readStringField(
            kvpTestInstance,
            policyPrefix + "recovery.strategy",
            &sensorConfig.policy.recovery.strategy,
            outError,
            false))
    {
        return false;
    }

    if (!readInt32Field(
            kvpTestInstance,
            policyPrefix + "recovery.min_cooldown_seconds",
            &sensorConfig.policy.recovery.minCooldownSeconds,
            outError,
            false))
    {
        return false;
    }

    const std::string vendorPrefix = prefix + "vendor.";
    if (!readInt32Field(
            kvpTestInstance,
            vendorPrefix + "vendorCode",
            &sensorConfig.vendor.vendorCode,
            outError,
            false))
    {
        return false;
    }

    return readStringField(
        kvpTestInstance,
        vendorPrefix + "vendorInfo",
        &sensorConfig.vendor.vendorInfo,
        outError,
        false);
}

} // namespace

void* vcomponentkvp_create_instance(char* fileName)
{
    if (fileName == nullptr || std::strlen(fileName) == 0)
    {
        return nullptr;
    }

    ut_kvp_instance_t* instance = ut_kvp_createInstance();

    if (instance == nullptr)
    {
        return nullptr;
    }

    const ut_kvp_status_t result = ut_kvp_open(instance, fileName);

    if (result != UT_KVP_STATUS_SUCCESS)
    {
        ut_kvp_destroyInstance(instance);
        return nullptr;
    }

    return static_cast<void*>(instance);
}

void vcomponentkvp_destroy_instance(void* instance)
{
    if (instance != nullptr)
    {
        ut_kvp_destroyInstance(static_cast<ut_kvp_instance_t*>(instance));
    }
}

bool vcomponent_Thermal_parse_config(
    char* configurationFile,
    ThermalHfpConfig& thermalConfiguration,
    std::string* outError)
{
    if (outError != nullptr)
    {
        outError->clear();
    }

    if (configurationFile == nullptr || std::strlen(configurationFile) == 0)
    {
        thermalConfiguration = ThermalHfpConfig{};
        setError(outError, "thermal HFP YAML path is empty");
        return false;
    }

    void* testconfig = vcomponentkvp_create_instance(configurationFile);
    ut_kvp_instance_t* kvpTestInstance = static_cast<ut_kvp_instance_t*>(testconfig);

    if (kvpTestInstance == nullptr)
    {
        thermalConfiguration = ThermalHfpConfig{};
        setError(outError, std::string("failed to open thermal HFP YAML: ") + configurationFile);
        return false;
    }

    thermalConfiguration = ThermalHfpConfig{};

    if (!fieldPresent(kvpTestInstance, THERMAL_ROOT) || !fieldPresent(kvpTestInstance, THERMAL_LIST))
    {
        vcomponentkvp_destroy_instance(kvpTestInstance);
        thermalConfiguration = ThermalHfpConfig{};
        setError(outError, "missing top-level thermal HFP profile: sensor.thermal");
        return false;
    }

    bool success = true;
    const uint32_t sensorCount = ut_kvp_getListCount(kvpTestInstance, THERMAL_LIST);
    thermalConfiguration.sensors.reserve(sensorCount);
    std::unordered_set<std::string> sensorIds;
    std::unordered_set<std::string> sensorNames;

    for (uint32_t i = 0; i < sensorCount; ++i)
    {
        ThermalSensorConfig sensorConfig;
        if (!vcomponent_Thermal_get_sensor_info(kvpTestInstance, i, sensorConfig, outError))
        {
            success = false;
            break;
        }

        if (!validateSensorConfiguration(sensorConfig, i, &sensorIds, &sensorNames, outError))
        {
            success = false;
            break;
        }

        thermalConfiguration.sensors.push_back(sensorConfig);
    }

    vcomponentkvp_destroy_instance(kvpTestInstance);

    if (!success)
    {
        thermalConfiguration = ThermalHfpConfig{};
        return false;
    }

    LOGF_INFO(
        "%s Successfully parsed thermal HFP YAML '%s' with %zu sensor configuration(s).",
        logPrefix,
        configurationFile,
        thermalConfiguration.sensors.size());
    return true;
}

bool loadThermalHfpConfigFromYaml(
    const std::string& path,
    ThermalHfpConfig* outConfig,
    std::string* outError)
{
    if (outConfig == nullptr)
    {
        setError(outError, "outConfig is null");
        return false;
    }

    std::string mutablePath = path;
    return vcomponent_Thermal_parse_config(mutablePath.data(), *outConfig, outError);
}

} // namespace utility
} // namespace vcomponent
