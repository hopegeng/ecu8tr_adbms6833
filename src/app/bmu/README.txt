How this framework behaves

For your assumed mapping:

CSC1 cell1..20 → C001..C020
CSC2 cell1..20 → C021..C040
…
CSC18 cell1..20 → C341..C360

Each Cxxx_CellMessage is packed as:

bytes 0..1 = cell voltage raw, factor 0.1 mV
bytes 2..3 = cell temp raw, factor 0.01 °C
bytes 4..5 = GPIO voltage raw, factor 0.1 mV
byte 6 bit0 = balancing
byte 7 = 0

And the scheduler sends the whole C001..C360 set once per second, matching the DBC cycle time of 1000 ms.