#AGENTS.md


## Project 
Project name: `ecu8tr_freertos_illd_adbms6830`

Purpose:

- This project is to implement a BMU based on Ecu8tr from NeutronControls Inc for BorgWarner battery module(CSC) test systeming. 
- The BorgWarner battery module upgraded from Gen3.0( based on LTC6812 ) to Gen3.3(Based on ADBMS6833 )


- The ADBMS6822 bridge is used to communicate with these two different battery module boards(CSC) via isoSPI bus

- BorgWarner BMS Key Testing & Monitoring Features:Cell Monitoring: Real-time monitoring of voltage, current, and temperature for individual cells to ensure safety and performance.Passive Cell Balancing: Optimizes battery durability and capacity by balancing cell voltages.State Estimation: Advanced algorithms determine the State of Charge (SoC), State of Health (SoH), and State of Energy (SoE).Fast Charging Support: Designed for quick DC charging, allowing 10% to 80% SoC in 30 minutes for specific LFP blade battery packs.Diagnostics: Includes integrated, replaceable contactor box diagnostics to manage high-voltage connections.Production Testing: In manufacturing, BorgWarner uses a 2,000 m² lab for thermal, vibration, corrosion, and water resistance testing



- In additon to the BMU functionalities, this project also needs to support field upgrading by using Aurix TC399 SOTA feature
	
- The BMU module is Ecu8tr from NeutronControls: which is based on Aurix tc399 B step, the ADBMS6822 bridge is integrated in this module. The qspi0 and cs2 is used to talk to adbms6833/ltc6812.

- The documentations for the tc399 is here: D:\workspace\Aurix\Ecu8Tr\ADI\adbms6830_ws\ecu8tr_freertos_illd_adbms6830\docs\Aurix_TC399_docs

- The schematics for the Ecu8tr BMU board: D:\workspace\Aurix\Ecu8Tr\ADI\adbms6830_ws\ecu8tr_freertos_illd_adbms6830\docs\Ecu8tr_docs

- The docments for LTC6812 and Gen3.0 are in the folder: D:\workspace\Aurix\Ecu8Tr\ADI\adbms6830_ws\ecu8tr_freertos_illd_adbms6830\src\drivers\adbms\ltc6812_on_core2\docs

          
- The firmware implemented on Ecu8tr will also support ADBMS6830, because currently BorgWarner Gen3.3 ADBM6833 CSC board not available. And ADBM6830 is used to verify the basical features, such as cell voltage measurements, ambinent temperature via GPIO, and Balancing.

- The Gen3.0 CSC(LTC6812) is also not available, so DC3036A LTC6812 eval board from ADI is used to implement the basical features.

- An imporant feature for this BMU firmware is to store the necessary configurations to the EEPROM on the CSC board. This EEPROM is a slave I2C device of the ADBMS6833 or LTC6812.

- Main hardware and software:

- Infineon AURIX TC399
- ADI LTC6812
- ADI ADBMS6822
- ADI ADBMS6830
- ADI ADBMS6833
- FreeRTOS
- lwIP
- CAN bus stack
- TC3xx SOTA bootloader

- TC399 Core usage:

 Core0: FreeRTOS, lwIP, and CAN bus stack
 Core1: debug console output
 Core2: isoSPI driver for CSC communication

- CSC support:

 LTC6812
 ADBMS6833
 ADBMS6830

- The firmware uses conditional compilation depending on which CSC chip is selected.
