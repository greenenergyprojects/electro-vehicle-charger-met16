# Manufacturing report

Device is tested on electric vehicle (Hyundai Ioniqu). The remaining loading time, shown in the vehicles dashboard is calculated from the start loading current (depends on the control pilot pwm duty cycle). The control pilot duty cycle can be changed during loading phase. The vehicle is changing the loading current immediatly, but the remaining loading time in the dashboard stays unchanged. The loading principle may be used for dynamic load adjustment, for example to adjust loading current to photovoltaic power generation level.

Also measurement for voltage and current is working as desired. 

## Phase switching relais needed

The electric vehicle will not start loading, if there is voltage on phase (J6-3).
A relais is needed to separate phase J6-3 from J5-3.

**Bugfix:**  
Insert solid state relais between J6-3 and the EV cable phase wire.
This relais is switched by Port PC4 (A4 = LED D2).

See: [device_assembled.png](device_assembled.png)

## Bugfixes / Improvements on pcb


| Reference | Description |
| --------- | ---------------------------------------------------- |
| U5, U6    | Pads: space between rows can be reduced (0,1..0,3mm) |
| C8        | Reference not good visible
| C5        | swap location to R4
| R21       | -> 27K
| R22       | -> 10K
| R31       | -> 15K
| R16, R18, R29, R23, R24 | -> 47K
| R10       | -> 100M (or remove resistor)
| R33       | -> 120K
| R34       | -> 33K
| R35       | -> 3K3
| R32       | -> 100M
| R27       | -> 560R
| R28       | -> 560R
| RV1       | -> 100K
| R13,R26   | -> 1K
| R3            | PCB-Error, U5B-5 to +5V, T1-3 to U5B-5 without resistor
| Via: PE, N, L | drill size to diameter 1.5mm ?
| luster-drill  | increase ring size, better placement of connection vias, move distance holes for cable
