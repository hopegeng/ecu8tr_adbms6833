# ADBMS6830 / ADBMS6833 driver starter set

This package collects the code fragments developed in chat into importable C files.

## Files

- `adbms6830_driver.h`
- `adbms6830_driver.c`
- `adbms6830_balance.h`
- `adbms6830_balance.c`
- `adbms6830_pec.h`
- `adbms6830_pec.c`

## What is included

- basic driver context and service state machine
- wake-up helper
- command frame builder
- CFGA / CFGB write helpers
- RDCVALL parsing helper
- raw-to-mV conversion
- simple top-balancing algorithm using DCC masks
- PEC helper functions

## What you still need to fill in

1. Exact command opcodes from your approved ADI documentation.
2. Exact data PEC behavior for your exact device/document revision.
3. Exact CFGB bit layout for DCC bits, DCTO, DTMEN, and related controls.
4. Real TC399 HAL hooks:
   - SPI transfer
   - delay functions
5. Any project-specific diagnostics and fault policy.

## Important cautions

- Some earlier chat examples referenced both ADBMS6830 and ADBMS6833 family behavior.
- For ADBMS6833 specifically, the public documentation is limited and NDA material may be required.
- Treat placeholder opcodes and placeholder CFGB packing as TODO items before production use.

## Suggested integration order

1. Add the PEC module.
2. Add your QSPI HAL wrapper.
3. Verify wake-up and command transfers on one device.
4. Verify RDCVALL parsing with a resistor ladder.
5. Add balancing only after voltage measurement is stable.
