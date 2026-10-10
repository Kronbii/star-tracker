#!/usr/bin/env bash
# Exports every printable part (STL, in print orientation), the 1:1 drill
# templates (SVG) and the renders. Needs OpenSCAD >= 2021 (2025+ with the
# Manifold backend is much faster). Renders need an OpenGL context: on a
# headless machine run `xvfb-run ./build.sh`.
set -euo pipefail
cd "$(dirname "$0")"
OPENSCAD=${OPENSCAD:-openscad}
HELP=$("$OPENSCAD" --help 2>&1 || true)
BACKEND=()
[[ "$HELP" == *"--backend"* ]] && BACKEND=(--backend=manifold)
STLFMT=()
[[ "$HELP" == *"binstl"* ]] && STLFMT=(--export-format=binstl)

PARTS=(hinge_block hinge_clamp drive_base drive_cap carriage strike_bracket
       sight_cradle_rear sight_cradle_front sight_cap_eye sight_cap_reticle
       electronics_box electronics_lid bahtinov_mask)
mkdir -p stl templates build
for p in "${PARTS[@]}"; do
  echo "part: $p"
  "$OPENSCAD" "${BACKEND[@]}" "${STLFMT[@]}" -q -D "part=\"$p\"" -o "stl/$p.stl" star_tracker.scad
done
for t in template_bottom template_top; do
  "$OPENSCAD" -q -D "part=\"$t\"" -o "templates/$t.svg" star_tracker.scad
done

if [[ "${SKIP_RENDERS:-0}" != "1" ]]; then
  R=(--render --colorscheme="Tomorrow Night")
  "$OPENSCAD" "${BACKEND[@]}" -q "${R[@]}" -D 'part="assembly"' -D theta=18 -D show_camera=false \
    --imgsize=1800,1200 --camera=105,35,75,60,0,226,1000 --projection=p -o build/hero_raw.png star_tracker.scad
  "$OPENSCAD" "${BACKEND[@]}" -q "${R[@]}" -D 'part="assembly"' -D theta=18 -D show_camera=true \
    --imgsize=1600,1200 --camera=95,40,60,62,0,210,1050 --projection=p -o build/in_use_raw.png star_tracker.scad
  "$OPENSCAD" "${BACKEND[@]}" -q "${R[@]}" -D 'part="contact_max"' \
    --imgsize=1200,900 --camera=0,0,0,90,0,0,10 --viewall --autocenter --projection=o -o build/contact_raw.png star_tracker.scad
  PY=${PYTHON:-../.venv/bin/python}
  [[ -x "$PY" ]] || PY=python3
  "$PY" finish_renders.py
fi
