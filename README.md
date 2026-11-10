# rdk-halif-aidl-vcomponent-thermal-sensor

The Thermal Sensor vComponent module provides an emulated Thermal Sensor HAL
implementation backed by a Binder/AIDL service. The service uses an HFP (HAL
Feature Profile) YAML configuration to define the thermal sensor interface
version, configured sensors, and thermal policy thresholds. During startup, the
service loads the HFP YAML, starts a UT ControlPlane endpoint for receiving
YAML/KVP control messages, publishes the Binder service under the AIDL-defined
Thermal Sensor service name, and then joins the Binder thread pool.

## Table of Contents

- [Thermal Sensor (vComponent) README](#rdk-halif-aidl-vcomponent-thermal-sensor)
  - [Acronyms, Terms and Abbreviations](#acronyms-terms-and-abbreviations)
  - [Build RDKThermalSensorService](#build-rdkthermalsensorservice)
  - [Run Thermal Sensor](#run-thermal-sensor)

## Acronyms, Terms and Abbreviations

| Acronym / Term | Description |
|----------------|-------------|
| **AIDL** | Android Interface Definition Language |
| **HAL** | Hardware Abstraction Layer |
| **HFP** | HAL Feature Profile (YAML profile used to configure the vComponent) |
| **RDK** | Reference Design Kit |
| **UT-Core** | RDK Unified Test Core Framework |
| **UT ControlPlane** | UT-Core / UT-Control control plane for receiving YAML/KVP control messages |
| **VTS** | Vendor Test Suite |
| **YAML** | YAML Ain’t Markup Language (configuration format) |

## Build RDKThermalSensorService

### Prerequisites for UT-Core

This module relies on UT-Core / UT-Control headers and libraries. Ensure all
required packages for UT-Core are installed. See:
[Packages for ut-core](https://github.com/rdkcentral/ut-core/wiki/UT-Core-Building-using-Docker-or-Vagrant#script-for-installing-basic-packages-for-ut-core)

### Clone the Repository

```bash
git clone https://github.com/rdkcentral/rdk-halif-aidl-vcomponent-thermal-sensor.git

cd rdk-halif-aidl-vcomponent-thermal-sensor
```

### Environment variables

The build is driven by `./build.sh` in this repository. It uses, or defaults
to, the following environment variables:

- `UT_CORE_VERSION`: Specific version of UT-Core to build. If not set, the
  script checks out the latest tag.
- `RDK_HALIF_AIDL_VERSION`: Git ref used if the script must clone
  `rdk-halif-aidl`. The script defaults to `0.23.0`. An existing checkout is
  reused without changing its Git ref; ensure it provides the matching snapshots.

Example:

```bash
export UT_CORE_VERSION=5.1.0
export RDK_HALIF_AIDL_VERSION=0.23.0
```

The default released module snapshots are `sensor/0.3.0.0`,
`common/0.3.0.0`, and `bootreason/0.2.0.0`. Their build overrides are
`VCOMPONENT_VERSION`, `VCOMPONENT_INCLUDE_VERSION` (common), and
`BOOTREASON_VERSION`, respectively; keep headers and libraries aligned.

Each configured sensor has an independent state machine. Critical temperature
recovers directly to `NORMAL` only below its recovered threshold; shutdown is
terminal. Events always identify the affected sensor, even when the worst
current state returned by `getCurrentThermalState()` does not change.
`policy.max_state_events_per_minute` must be positive and defaults to 6 when
omitted. Events above that rolling 60-second limit are coalesced to the current
sensor state and dispatched when the window permits, without requiring another
sample. Shutdown events bypass the limit and precede BootReason persistence and
the existing autonomous power-off request.

This is an emulated service: initial readings still come from the configured
operational range, and subsequent readings come from UT control-plane messages,
not hardware sampling. Startup state is evaluated from those initial readings.
`shutdown_min_downtime_s` remains metadata; enforcing device restart timing
requires platform support.

### Build command (Target Linux)

From the repository root:

```bash
./build.sh Target=linux
```

At a high level, the `build.sh` script:

1. Builds the required HALIF Binder tools and HALIF `common`, `sensor`, and
   `bootreason` modules.
2. Stages Linux Binder service-manager binaries, headers, libraries, and HALIF
   libraries into `build/usr`.
3. Clones and builds `ut-core` (checked out to `UT_CORE_VERSION`) and builds
   the UT Control library.
4. Builds the Thermal Sensor service using CMake. The service executable is
   named `RDKThermalSensorService`.

## Run Thermal Sensor

### Run the service on a target device

To run the Thermal Sensor Binder service:

1. Copy the repository's `build/` folder onto the target device, for example
   using `scp`, or otherwise ensure the built binary and
   `vcomponent_configurations/` directory are present on the target filesystem.
2. Run the Thermal Sensor service binary.

The service executable is named:

- `RDKThermalSensorService`

### Command line interface

The Thermal Sensor service supports the following command line flags:

- `--hfp <path>`: Optional HFP YAML path.  
  Default: `vcomponent_configurations/hfp-sensor-thermal.yaml`
- `--port <port>`: Optional UT ControlPlane port to listen on.  
  Default: `8085`

Example:

```bash
./RDKThermalSensorService \
  --hfp vcomponent_configurations/hfp-sensor-thermal.yaml \
  --port 8085
```

If `--help` (or `-h`) is provided, or if an unknown argument is provided, the
service prints usage and exits with failure.
