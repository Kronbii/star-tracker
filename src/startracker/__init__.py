"""Modelling and planning tools for the ESP32 barn-door star tracker.

Modules
-------
constants   tracking rates and unit conversions (mirror firmware/src/core/st_math.h)
geometry    barn-door geometry (mirrors firmware/src/core/geometry.cpp)
optics      camera/lens description and Canon APS-C defaults
errors      tracking-error model -> longest sharp exposure
exposure    NPF / 500 / 300 rules, sky-limited sub length, SNR
astro       low-precision Sun/Moon/star positions (offline fallback)
ephemeris   skyfield + JPL DE421 backend (preferred when the file is present)
planner     M42 visibility windows for Beirut and the first-session plan
autoguide   synthetic star fields, centroiding, guiding simulation
pec         periodic-error fitting -> firmware PEC table
figures     regenerates every figure in figures/

Everything these tools output about the tracker's performance is a
prediction from a model, not a measurement.
"""

__version__ = "0.1.0"
