# Wiring

Target: ESP32-WROOM-32 DevKitC, TMC2209 module in standalone STEP/DIR mode, 12 V supply.
Pin numbers are defined in `firmware/star_tracker/config.h`; change them there, not here.

```
                    +--------------------- 12 V (3S Li-ion / PD trigger) ---------------------+
                    |                                                                          |
                 [switch]                                                                      |
                    |                                                                          |
        +-----------+------------+                                                             |
        |                        |                                                             |
   MP1584 IN               TMC2209 VM --+-- 100 uF --+                                      100 k
   MP1584 OUT 5.0 V             GND ----+------------+                                         |
        |                                                                                  GPIO34 (VBAT)
   ESP32 5V/VIN                                                                                |
                                                                                             22 k
   ESP32                    TMC2209                    Motor (NEMA17)                          |
   GPIO26  -------------->  STEP                       A1 A2 = coil A                         GND
   GPIO27  -------------->  DIR                        B1 B2 = coil B
   GPIO25  -------------->  EN (low = on)
   3V3     -------------->  VIO, MS1, MS2   (MS1 = MS2 = high -> 1/16, interpolated to 1/256)
   GND     -------------->  GND             SPREAD, PDN/UART: module defaults (StealthChop)

   GPIO33  ----[NC home switch]---- GND     (internal pull-up; pressed or broken wire = HIGH = "home")

   GPIO18 --[470 R]--> PC817 #1 anode, cathode -> GND      collector -> 2.5 mm TIP  (shutter)
                                                          emitter   -> 2.5 mm SLEEVE (camera ground)
   GPIO19 --[470 R]--> PC817 #2 anode, cathode -> GND      collector -> 2.5 mm RING (focus)
                                                          emitter   -> 2.5 mm SLEEVE
   GPIO2   on-board LED (dim blinks only)      GPIO0  BOOT button: short = track/stop, hold 2 s = home
```

## Notes

- **Canon E3 remote port** (EOS 2000D/250D/800D/80D/90D and most APS-C bodies): 2.5 mm
  stereo plug, tip = shutter, ring = focus, sleeve = ground. Shorting tip (and ring) to sleeve
  fires the shutter; in BULB mode it stays open while shorted. The optocouplers keep the
  camera electrically separate from the tracker: no ground loop, no 12 V near the camera. Bodies
  with the N3 connector (5D/6D/7D series) need an N3 cable; the logic is identical.
- **Optocoupler drive:** (3.3 V - 1.2 V) / 470 R = 4.5 mA through the PC817 LED; with a
  current-transfer ratio >= 50 % that sinks > 2 mA, far more than the camera's pull-up needs.
- **Motor current:** set the TMC2209 Vref for about 0.5 A RMS. On modules with 0.11 R sense
  resistors (BIGTREETECH V1.3) I_rms ~= 0.71 x Vref, so Vref ~= 0.7 V. The tracker needs about
  12 mN.m at the screw (predicted, 1.5 kg payload at 150 mm, 30 % screw efficiency) against
  ~200 mN.m available at that current, so it runs cool and quiet in StealthChop.
- **Motor coils:** find the pairs with a multimeter (1-3 R across a coil). If "track" closes
  the boards, swap one coil's two wires or set `INVERT_DIR = true`.
- **Home switch:** wired normally-closed so a broken wire reads as "at home" and stops
  closing moves instead of driving into the base.
- **Battery sense:** 12.6 V full -> 2.27 V at GPIO34 (ADC1, 11 dB). Status JSON reports it.
- **Decoupling:** keep the 100 uF across VM at the driver; twist the motor wires.
- Do not power the motor from USB. Flash over USB with the 12 V off or on - the buck feeds
  5V/VIN and the DevKit's diode keeps USB and VIN apart.
