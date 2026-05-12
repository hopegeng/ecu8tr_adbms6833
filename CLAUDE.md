# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Embedded firmware for the **ECU8TR Battery Management Unit (BMU)** from NeutronControls Inc., targeting the Infineon **AURIX TC399 B-step** microcontroller. The BMU monitors BorgWarner battery module CSC boards (Gen3.0 LTC6812, Gen3.3 ADBMS6833, and ADBMS6830 for development validation) via isoSPI using the ADBMS6822 bridge.

Key capabilities: real-time cell voltage/temperature monitoring, passive cell balancing, CAN message publishing, EEPROM configuration storage on CSC board, and SOTA (Software-Over-The-Air) field upgrades.

## Build System

**Toolchain:** TASKING VX-toolset v6.3r1 for TriCore (Eclipse-based managed build — there is no Makefile-based CLI build). Build is done via Eclipse CDT with the TASKING plugin targeting `TC39xB`.

- Build configuration files: `.cproject`, `.project`, `Lcf_Tasking_Tricore_Tc.lsl` (linker script), `Tricore_IncludePathList.opt`
- Output artifacts: ELF, HEX, BIN, MAP files
- Debug launch: `Ecu8TR_freeRTOS.launch` (JTAG via UDAS/Lauterbach to TriBoard TC39xB)

**Compile-time AFE chip selection** — set `ADBMS_DEVICE_FAMILY` preprocessor symbol in `.cproject`:
- `6830` → ADBMS6830
- `6833` → ADBMS6833
- `6812` → LTC6812
- `0` (default) → Auto-detect at runtime (`ADBMS_DEVICE_FAMILY_AUTO`)

## Testing

Tests are Python-based over CAN bus (Kvaser Leaf Light v2 interface required):

```powershell
pip install PyQt5 python-can

# GUI tester
cd test\py_gui
python bmu_kvaser_gui.py --channel 0 --bitrate 500000

# Automated tests
cd test\python_test
python ecu8tr_autoTest.py
```

DBC files and BorgWarner-specific test data are in `test/borgWarner/`. Firmware upgrade testing: `test/python_test/fw_upgrade.py`.

## Architecture

### Core Assignment
| Core | Role |
|------|------|
| Core 0 | FreeRTOS scheduler, lwIP TCP/IP, CAN stack, BMU application logic |
| Core 1 | Debug console / UART output |
| Core 2 | isoSPI driver — dedicated to AFE chip communication |

Entry points: [`src/Cpu0_Main.c`](src/Cpu0_Main.c), [`src/Cpu1_Main.c`](src/Cpu1_Main.c), [`src/Cpu2_Main.c`](src/Cpu2_Main.c).

### FreeRTOS Task Cadence (Core 0)
Defined in [`src/app/core/bmu_main.c`](src/app/core/bmu_main.c):
- **10 ms** — CAN cell message publishing
- **20 ms** — medium-priority tasks
- **100 ms** — slow monitoring
- **1000 ms** — diagnostics / logging

### Key Source Layout

```
src/
├── app/
│   ├── core/          # BMU init, task scheduler, bmu_cfg.h (topology), bmu_types.h
│   ├── bmu/           # Cell DB, CAN handling, CSC acquisition, scheduler
│   ├── dbc/           # DBC message encode/decode (CellMessage publisher)
│   ├── eeprom/        # I2C EEPROM on CSC board
│   ├── led/           # LED status indication
│   └── shell/         # CLI debug interface
├── drivers/
│   ├── adbms/
│   │   ├── adbms_family_select.h      # Chip selection / polymorphic dispatch
│   │   ├── adbms6830_on_core2/        # ADBMS6830 driver (10 cells/AFE)
│   │   ├── adbms6833_on_core2/        # ADBMS6833 driver (20 cells/AFE)
│   │   ├── ltc6812_on_core2/          # LTC6812 driver (Gen3.0 legacy)
│   │   └── adbms_runtime_detect.h     # Auto-detection logic (AUTO mode)
│   ├── qspi/          # QSPI0/CS2 interface to ADBMS6822 isoSPI bridge
│   ├── can/           # CAN hardware interface
│   ├── gpio/          # Temperature sensors, LED, relay control
│   └── eth/           # Ethernet / lwIP glue
├── middleware/
│   ├── FreeRTOS/      # RTOS kernel (multi-core)
│   ├── lwip-2.0.3/    # lwIP TCP/IP stack
│   └── can/           # CAN stack abstraction
├── Infineon/
│   ├── Libraries/     # iLLD (TC39B peripheral drivers from Infineon)
│   └── Configurations/ # MCU pin and peripheral configuration
└── bootloader/        # TC3xx SOTA bootloader
```

### System Topology (configured in `bmu_cfg.h`)

Current build target: **1 CSC × 2 AFEs × 10 cells/AFE = 20 total cells**

```c
#define BMU_CSC_COUNT          1
#define BMU_AFE_COUNT_PER_CSC  2
#define BMU_CELL_COUNT_PER_AFE 10   // cells 7..16 used on ADBMS6830
#define BMU_TOTAL_CELLS        20   // compile-time enforced
```

### AFE Polymorphism

[`src/drivers/adbms/adbms_family_select.h`](src/drivers/adbms/adbms_family_select.h) provides a uniform interface regardless of which chip is compiled in. The macro `ADBMS_CORE2_MAIN_FN` is the Core 2 entry point and `AdbmsSharedRead` / `AdbmsSharedSnapshot_t` are the shared-memory types used by Core 0 to consume measurement data produced by Core 2.

### Inter-Core Data Sharing

Core 2 writes measurement snapshots to a shared memory region; Core 0 calls `AdbmsSharedRead()` (dispatched by `adbms_family_select.h`) to atomically copy the latest snapshot. The stale-data policy is controlled by `BMU_CELL_STALE_TIMEOUT_MS` in `bmu_cfg.h`.

## Documentation

- TC399 reference docs: `docs/Aurix_TC399_docs/`
- ECU8TR board schematics: `docs/Ecu8tr_docs/`
- LTC6812 / Gen3.0 docs: `src/drivers/adbms/ltc6812_on_core2/docs/`
