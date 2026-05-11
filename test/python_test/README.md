# BMU Kvaser GUI Test

Install dependencies:

```powershell
pip install PyQt5 python-can
```

Run with a Kvaser Leaf Light v2 on channel 0:

```powershell
python .\bmu_kvaser_gui.py --channel 0 --bitrate 500000
```

The GUI sends `M001_StartLTCTransmission` and `M001_BalanceCells`, then displays `C001..C020_CellMessage` plus the implemented temperature, dew sensor, module status, max/min voltage, and IC summary frames.
