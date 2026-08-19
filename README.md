# Thermal Sensor vcomponent - Build Skeleton

This directory is a **build-only skeleton** for the thermal sensor HAL
vcomponent. It is a copy of the build setup from `TEVSensorThermal/` with **all
implementation content removed**.

It contains the build system, dependency bootstrap and packaging rules - and
nothing else. No AIDL service implementations, no controllers, no utilities, no
service `main()`.

## AIDL specification reference

- https://github.com/rdkcentral/rdk-halif-aidl/tree/develop/sensor/current/com/rdk/hal/sensor/thermal

> The thermal AIDL package lives inside the HALIF **`sensor`** module, so
> `build.sh` builds `sensor` (producing `libsensor-vcurrent-cpp.so`).

## What is included

| Path | Purpose |
| --- | --- |
| `build.sh` | Clones/builds `rdk-halif-aidl` (binder tools + `common` + `sensor` modules) and `ut-core`, then configures/builds this component |
| `CMakeLists.txt` | Toolchain/standard setup, sysroot vs `build.sh` env detection, HALIF/Binder/ut-control discovery, config staging, install rules |
| `vcomponent_configurations/hfp-sensor-thermal.yaml` | HFP feature profile consumed at runtime and installed with the component |
| `vcomponent_configurations/hfp-sensor-thermal.yaml` | Verbatim copy of the upstream `rdk-halif-aidl` thermal HFP profile (no line edits) |
| `include/common/logger.h` | Printf-style logging helper macros (`LOGF_*`) |
| `include/utility/vcomponent_ThermalHfpConfigUtils.h` | Thermal HFP configuration model mirroring the YAML hierarchy |
| `include/utility/vcomponent_ThermalHelper.h` | Small helper utility declarations |
| `include/utility/vcomponent_ThermalParseConfig.h` | Thermal-only HFP YAML parsing entrypoints |
| `include/controller/vcomponent_ThermalUtController.h` | UT-controller facade contract |
| `src/utility/vcomponent_ThermalParseConfig.cpp` | Implemented parser, scoped strictly to the `sensor.thermal` keys required by the thermal YAML |
| `src/utility/vcomponent_ThermalHelper.cpp` | Skeleton stubs for the helper utilities |
| `src/controller/vcomponent_ThermalUtController.cpp` | Skeleton stubs for the UT controller facade |
| `src/service/vcomponent_ThermalSensorService.cpp` | Skeleton `main()` (argument handling + controller wiring only) |

## What is deliberately NOT included

- `src/aidl/` sources (`vcomponent_ThermalSensor.cpp`, `vcomponent_ThermalEventListener.cpp`)
- `include/aidl/` headers (`vcomponent_ThermalSensor.h`, `vcomponent_ThermalEventListener.h`)
- Any binder publishing / AIDL API implementation
- Parsing of non-thermal HFP sections (only `sensor.thermal` keys are handled)

## Skeleton build behaviour

`CMakeLists.txt` discovers sources with `file(GLOB ...)` instead of hard-coding
them:

- `src/aidl/*.cpp`, `src/utility/*.cpp`, `src/controller/*.cpp` -> `thermal_core` shared library
- `src/service/*.cpp` -> `RDKThermalSensorService` executable

While those directories are empty, the corresponding targets are **skipped**
(CMake prints a status message), so the skeleton configures and builds cleanly.
Add sources and re-run the build - the targets appear automatically with no
CMake edits required.

### Clone the Repository

```bash
git clone https://git@github.com:rdkcentral/rdk-halif-aidl-vcomponent-thermal-sensor.git

cd rdk-halif-aidl-vcomponent-thermal-sensor
```

### Environment variables

The build is driven by `./build.sh` in this repository. It uses (or defaults) the following environment variables:

- `UT_CORE_VERSION`: Specific version of UT-Core to build. If not set, the script checks out the latest tag.
- `RDK_HALIF_AIDL_VERSION`: Git ref used if the script must clone `rdk-halif-aidl`. The script defaults to `main`.

Example:

```bash
export UT_CORE_VERSION=5.1.0
export RDK_HALIF_AIDL_VERSION=0.22.0
```


## Build

```sh
./build.sh Target=linux
```

Optional debug logging:

```sh
./build.sh Target=linux HAL_DBG_LEVEL=DEBUG
```

Other entry points:

```sh
./build.sh clean        # remove build folder
./build.sh dist_clean   # remove build + checked-out dependencies
./build.sh help
```

Artifacts are produced in `./build/` and installed into `./build/out/`.

## Adding the implementation

1. Add headers under `include/{aidl,controller,utility,common}/`.
2. Add sources under `src/{aidl,controller,utility}/` (library) and
   `src/service/` (service `main()`).
3. Re-run `./build.sh Target=linux`.
