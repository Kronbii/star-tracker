# Portfolio entry: Star tracker

## Summary

A 3D-printed barn-door star tracker for photographing the Orion Nebula with a Canon DSLR from
Lebanon, designed as one build that joins control, vision and the sky. Instead of hiding the barn
door's tangent error with a curved rod or a second arm, the ESP32 firmware schedules every
microstep from the closed-form inverse of the mechanism's geometry, so a cheap lead screw drives
the camera at the sidereal rate to within half a microstep (0.32 arcsec) in simulation of the
same C++ code. A Python error model turns polar alignment, drive-rate error and the screw's
periodic error into the longest sharp exposure per lens; a planner finds the moonless windows for
M42 over Beirut and writes the first-session plan. The design is modelled, compiled and tested
in software; it has not been built or tested under the sky yet, so every performance figure
here is a prediction.

## Facts and predictions

1. **Design:** a 2 mm-lead screw at 200 mm gives a 629:1 effective reduction, 0.64 arcsec per
   microstep, and 2.35 h of tracking per rewind.
2. **Verified in host tests:** the firmware's tracking schedule stays within +-0.5 microstep
   (0.32 arcsec) of sidereal over a 2 h simulated run; an uncorrected constant-speed drive would
   be 29 arcmin off after one hour.
3. **Predicted:** at 200 mm f/5.6 on an APS-C body the longest sharp sub is 21 s (expected;
   11 s worst case) versus 0.38 s untracked, with 1.5 px allowed trail.
4. **Predicted:** the lead screw's periodic error (assumed +-4 um) dominates the budget up to
   ~1 deg of polar error; loading the firmware's PEC table (75 % removed) would raise the 200 mm
   limit to ~77 s.
5. **Planned:** first session Saturday 14 November 2026, 22:30-04:45, 180 x 20 s at ISO 1600
   (from the skyfield/DE421 planner).

## CV bullets

- Designed an ESP32 star tracker whose firmware removes the barn-door tangent error exactly by
  scheduling each microstep from the mechanism's inverse kinematics; 60 host unit tests (ASan/
  UBSan) on a hardware-independent C++ core shared with the firmware.
- Built a Python tracking-error model (exact polar-misalignment rotation, periodic and rate
  errors, refraction) and an exposure/target planner using skyfield and JPL ephemerides, which
  identified screw periodic error, not polar alignment, as the limit to design around.
- Modelled the mechanism parametrically in OpenSCAD (15 printed parts, binary STLs, drill
  templates) with CI covering Python tests, host C++ tests, firmware compilation and CAD export.

## Portfolio fields

- **What:** an ESP32 barn-door star tracker for DSLR astrophotography: parametric CAD,
  firmware, an error model, an observing planner and an autoguiding prototype.
- **Problem:** the sky turns 15 arcsec per second; at 200 mm a fixed camera smears stars in
  under half a second, and cheap barn doors trade accuracy for simplicity through a geometric
  tangent error.
- **How:** commodity printer parts (NEMA17, Tr8x2 lead screw, rods, bearings) for a high
  reduction; firmware that times every microstep from the closed-form inverse of the geometry;
  a Python model that predicts each error source's trail and the longest sharp exposure; a
  planner for M42's moonless windows from Beirut.
- **Role:** sole designer and developer: mechanics and CAD, electronics, firmware, modelling,
  documentation.

### Stages

| Step | Title | Detail |
|---|---|---|
| 1 | Choose the mechanism | Compared worm, belt, geared-stepper and lead-screw drives by how much they divide motor-side error; chose a tangent-arm barn door with 629:1 effective reduction. |
| 2 | Make the firmware do the geometry | Derived h(theta) and theta(h) for a roller on a strike plane; position-based microstep scheduling, trapezoidal slews, homing, guide pulses, PEC; 20 kHz integer-only ISR. |
| 3 | Predict before building | Error budget per source, longest sharp sub vs focal length, PA vs PE sensitivity; identified screw PE as the limit. |
| 4 | Plan the night | skyfield + DE421 scan of every night to March for darkness, M42 altitude and Moon; first-session plan with exposure settings and calibration frames. |
| 5 | Prepare the vision step | Autoguiding prototype on synthetic star fields: detection, centroiding, matching, hysteresis control over the tracker's HTTP guide command. |

### Measurements

These are design values, simulations and predictions, not hardware measurements.

| Label | Value | Context |
|---|---|---|
| Tracking residual | +-0.32 arcsec | host simulation of the firmware scheduler, 2 h sidereal run |
| Angle per microstep | 0.64 arcsec | design (R = 200 mm, Tr8x2, 1/16 microstepping) |
| Sharp sub at 200 mm | 21 s (worst 11 s) | predicted, APS-C 3.72 um, 0.25 deg polar error, 1.5 px |
| Same with PEC | ~77 s | predicted, 75 % of screw PE removed |
| Guided RA error | 3.0 -> 0.9 arcsec RMS | simulation on synthetic frames |
| Tests | 60 C++ + 50 Python | all passing; firmware compiles with 0 warnings |
| Parts cost | ~140 USD | estimate, excluding tripod and camera |

### Limits

Not built and not tested under the sky; the lead-screw periodic error is an assumption and the
largest uncertainty; single axis, so declination drift can only be measured, not guided; the
sight tube's polar alignment (predicted 0.15-0.4 deg) is coarse; SNR photometry is rough.

### Hero image

`figures/hero_render.png`: the OpenSCAD render of the assembly. It must be captioned as a CAD
render of an unbuilt design, and it carries that label inside the image. Alternatives:
`figures/tangent_correction.png` (the firmware idea in one picture) or
`figures/exposure_vs_focal_length.png` (the predicted payoff). Once the tracker is built and
M42 is photographed, the photograph replaces the render.
