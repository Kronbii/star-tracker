# Build guide

From parts to a tracker that has passed its bench tests. Plan on two or three evenings plus
print time. Parts list: [`hardware/bom.csv`](../hardware/bom.csv). Wiring:
[`hardware/wiring.md`](../hardware/wiring.md). CAD source: [`cad/star_tracker.scad`](../cad/star_tracker.scad).

> Status: this design has been modelled, compiled and unit-tested but **not built yet**. Expect
> to adjust a tolerance or two on the first prints; the parameters are at the top of the SCAD file.

## 1. Print the parts

Every part fits a 180 x 180 mm bed (A1 mini included). STLs in [`cad/stl/`](../cad/stl/) are
already in print orientation; regenerate them with `cad/build.sh` after changing parameters.

Bambu Studio, 0.4 mm nozzle, textured PEI plate, "Generic PETG" (or Bambu PETG HF) unless noted.
PETG is preferred over PLA: it does not creep under the constant load and survives a car in the
sun. Use ASA if the tracker will live outdoors.

| Part | Qty | Material | Layer | Walls | Infill | Notes |
|---|---|---|---|---|---|---|
| `hinge_block` | 2 | PETG | 0.20 | 4 | 40 % gyroid | lies on its side: bearing bore vertical |
| `hinge_clamp` | 2 | PETG | 0.20 | 4 | 40 % gyroid | board face down |
| `drive_base` | 1 | PETG | 0.20 | 4 | 40 % gyroid | |
| `drive_cap` | 1 | PETG | 0.20 | 4 | 40 % gyroid | |
| `carriage` | 1 | PETG | 0.16 | 5 | 50 % gyroid | top face down; axle hole bridges cleanly |
| `strike_bracket` | 1 | PETG | 0.20 | 4 | 40 % gyroid | flange down |
| `sight_cradle_rear`, `sight_cradle_front` | 1 + 1 | PETG | 0.20 | 3 | 30 % | |
| `sight_cap_eye`, `sight_cap_reticle` | 1 + 1 | black PLA/PETG | 0.12 | 3 | 100 % | enable "precise wall"; check the reticle aperture is 4.33 mm |
| `electronics_box`, `electronics_lid` | 1 + 1 | PETG | 0.24 | 3 | 15 % | |
| `bahtinov_mask` | 1 | black PLA/PETG | 0.20 | 2 | 100 % | set `lens_od` to your lens barrel + 0.4 mm |

No supports are needed. About 350 g of PETG in total. Press-fit holes (608 pockets, LM8UU,
rod sockets) are drawn with 0.1-0.25 mm clearance; if your printer runs tight, raise `tol`
in the SCAD file rather than forcing parts. Melt M3 heat-set inserts into the set-screw holes of
the drive base (2), top cap (2) and front sight cradle (3), plus one spare.

## 2. Boards

Cut 12 mm birch plywood: **bottom 275 x 195 mm, top 290 x 110 mm**. Print the drill templates
`cad/templates/template_bottom.svg` and `template_top.svg` at **100 % scale** (check the
outline against the board size with a ruler), tape them on, centre-punch the marks.

Bottom board holes: hinge-block M5 x4 (T-nuts from below), Ø25 mm for the motor boss and
coupler, M3 x4 for the motor bolts, M4 x2 for the drive base, the 3/8"-16 T-nut for the tripod
head, M4 x2 for the electronics box. Top board: M4 x4 for the hinge clamps, Ø6.5 for the 1/4"-20
ball-head bolt, M4 x3 for the strike bracket, M4 x4 for the sight-tube cradles.

The template's two long lines are the hinge axis (x = 0) and the screw axis (x = R = 200 mm).
Drill those holes square: the motor hole position sets R.

## 3. Hinge

1. Press a 608ZZ into each `hinge_block` (bearing pockets facing each other).
2. Screw the blocks to the bottom board (M5 into T-nuts). Slide the 130 mm, 8 mm shaft through
   both bearings and check it turns freely with no binding: if it binds, loosen one block, let the
   shaft align it, retighten.
3. Screw the two `hinge_clamp`s to the underside of the top board, slide them onto the shaft
   between the blocks, and tighten the M3 pinch screws. Lay the top board flat and check the
   board edge is square to the shaft.
4. The top board should now swing smoothly with no felt play. Play here is play in the polar axis.

## 4. Drive tower

1. Mount the NEMA17 **under** the bottom board: M3 x 35 bolts from the top of the `drive_base`,
   through the base and the board, into the motor face.
2. Fit the 5-8 mm coupler on the motor shaft (it sits inside the base's Ø30 bore).
3. Push the two 250 mm rods into the base sockets; lock with M3 set screws.
4. Press the two LM8UU into the `carriage`. Fit the Tr8x2 brass nut on the carriage top with
   M3 screws and nylon washers, **snug but not tight**: the oversize holes let the nut float so
   screw wobble does not push the carriage (the largest predicted error source is screw PE).
5. Slide the carriage onto the rods, thread the screw through the nut into the coupler (12 mm
   engagement), fit the `drive_cap` with a 608ZZ over the rod tops and the screw end, lock the
   rods with the cap's set screws.
6. Turn the coupler by hand over the full travel: the carriage must move with no tight spots.
   A tight spot is a misaligned rod or a bent screw; fix it now.
7. Fit the KW12 micro switch in the base pocket, wired **normally closed**. Its height (slotted
   holes) sets where home is; the carriage flag should close it ~1 mm above the coupler.

## 5. Roller and strike strip

1. Mount a 608ZZ as the roller on an M8 x 25 socket-head bolt with a washer each side; the nut
   drops into the trap at the top of the carriage arm.
2. Epoxy the 10 x 2 mm steel strip into the recess under the `strike_bracket` (clamp it flat
   while it cures), then fit the two end screws.
3. Screw the bracket to the +y edge of the top board.
4. With the top board resting on the roller, the strip should sit centred on the roller's width.

## 6. Preload, tripod and camera mounts

- Two tension springs (or strong rubber bands) between screw eyes near the drive end of each
  board keep the strip on the roller whatever way the tracker is turned. Aim for 10-20 N.
- 3/8"-16 T-nut in the bottom board for the sturdy ball head on the tripod.
- 1/4"-20 bolt through the top board for the small ball head carrying the camera.

## 7. Sight tube

Cut the 16 mm tube to `sight_len - 2` = 198 mm. Push the eyepiece cap (pinhole) on the
**south (-y)** end and the reticle cap on the **north (+y, tower side)** end. Clamp it in the rear
cradle; the three M3 screws of the front cradle centre it.

Collimate it against the hinge axis, by day or at dusk:

1. Sight a distant point (> 500 m: a mast light, a hilltop) so it sits in the reticle aperture.
2. From the web page, `goto 35` (rotate the top board about the hinge by ~30 deg).
3. A tube parallel to the hinge keeps the point where it was. If it moves, turn the front screws
   to bring it **halfway** back, re-centre on the point, `goto 5`, and repeat until it stays put.

## 8. Electronics and firmware

Build the perfboard per [`hardware/wiring.md`](../hardware/wiring.md). Set the buck converter to
5.0 V **before** connecting the ESP32, set the TMC2209 Vref to ~0.7 V (about 0.5 A RMS).

Flash the firmware (user-space arduino-cli is enough):

```bash
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.11
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/star_tracker
arduino-cli upload  --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 firmware/star_tracker
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

Join the `StarTracker-XXXX` Wi-Fi (password in `config.h`: change it), open
`http://192.168.4.1`. The serial console accepts the same commands (`help`).

## 9. Calibrate the geometry

The firmware removes the tangent error exactly **for the geometry it is told**. Errors in these
numbers become drive-rate errors (budget in README section 5), so measure them:

| Setting | What it is | How to measure | Command |
|---|---|---|---|
| `radius` | R, hinge axis to screw axis | calipers across the hinge shaft and the screw at the same height, minus both radii (8/2 + 8/2) | `set radius 200.14` |
| `offset` | c = roller radius - e, where e is the strip face's height above the hinge axis | prop the top board parallel to the bottom board; measure the strip face and the shaft centre above the bottom board; e = difference | `set offset 11.05` |
| `home` | roller-centre height above the hinge axis at the switch | after `home` the carriage parks 0.5 mm above the switch: measure the roller axle centre above the shaft centre, subtract 0.5 | `set home 3.20` |
| `lon` | longitude for the Polaris clock | 35.50 for Beirut | `set lon 35.50` |

Then `save`. A 0.1 mm error in R is ~400 ppm of rate error (predicted); in c or home, ~160 ppm.

## 10. Bench tests before the first night

1. **Direction and limits.** `home`, then `track`: the boards must open. If they close, swap one
   motor coil pair. `rewind` must return to the park position; `jog -1` from park stops at the home position
   (soft limit); an unexpected switch hit while closing stops the move and reports `limit_hit`.
2. **Rate.** Mark the carriage height, `track` for 60 min, measure the rise. The firmware's
   status shows `height_mm`; the measured rise should agree to ~0.05 mm (0.1 %).
3. **Intervalometer.** Camera in BULB, manual focus: `test 5` must give a 5 s exposure;
   `shoot 3 10 3 2` three 10 s frames.
4. **Periodic error (optional, recommended).** Tape a < 1 mW laser pointer to the top board
   indoors, aim it at a wall 5-10 m away, film the dot with a webcam on a tripod while
   tracking for 15 min. Centroid the dot per frame (`startracker.autoguide.detect_stars` works
   on the frames), convert to arcsec (dot motion / distance), pair each frame with `pos_steps`
   from `/api/status`, and fit error against step position (CSV columns `steps,err_arcsec`): `startracker pec measured.csv --period 3200` prints the `pec` commands that load the
   correction. Never point the laser at people, aircraft or the sky.
