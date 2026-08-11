#!/bin/bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXECUTABLE="${PROJECT_DIR}/build/FlashElectronSim"
OUTPUT_DIR="${1:-${PROJECT_DIR}/figures}"

[[ -x "${EXECUTABLE}" ]] || {
  echo "Build FlashElectronSim first; executable not found: ${EXECUTABLE}" >&2
  exit 1
}
[[ -n "${DISPLAY:-}" ]] || {
  echo "DISPLAY is unset. Run on a graphical/X11-forwarded interactive session." >&2
  exit 1
}

mkdir -p "${OUTPUT_DIR}"
OUTPUT_DIR="$(cd "${OUTPUT_DIR}" && pwd)"
WORK_DIR="$(mktemp -d "${OUTPUT_DIR}/render.XXXXXX")"
trap 'rm -rf "${WORK_DIR}"' EXIT
cd "${WORK_DIR}"

# Generate self-contained copies so rendering does not depend on whether new
# macro/tool files were copied into the cluster checkout or build directory.
cat > 10cm_geometry_png.mac <<'MACRO'
/control/verbose 1
/run/verbose 0
/flash/setApplicatorIDcm 10
/run/initialize
/vis/open OGLSQt 1800x1200-0+0
/vis/scene/create
/vis/scene/add/volume
/vis/drawVolume
/vis/viewer/set/style surface
/vis/viewer/set/background 1 1 1
/vis/viewer/set/viewpointThetaPhi 90 0 deg
/vis/viewer/set/targetPoint 0 0 215 mm
/vis/viewer/zoom 1.35
/vis/viewer/set/size 3000 2000
/vis/viewer/flush
/vis/ogl/export beamline_10cm_geometry_300dpi.png
MACRO

cat > 10cm_vis_source.mac <<'MACRO'
/gps/verbose 0
/gps/particle e-
/gps/ene/type Gauss
/gps/ene/mono 9.70 MeV
/gps/ene/sigma 1.30 MeV
/gps/direction 0 0 1
/gps/ang/type beam2d
/gps/ang/rot1 1 0 0
/gps/ang/rot2 0 -1 0
/gps/ang/sigma_x 0.2 deg
/gps/ang/sigma_y 0.2 deg
/gps/pos/type Beam
/gps/pos/centre 0 0 -121.65 mm
/gps/pos/sigma_x 0.73 mm
/gps/pos/sigma_y 0.73 mm
/gps/pos/confine SourceCutoffPV
MACRO

cat > 10cm_tracks50_png.mac <<'MACRO'
/control/verbose 1
/run/verbose 0
/event/verbose 0
/tracking/verbose 0
/random/setSeeds 1357911 2468021
/flash/setApplicatorIDcm 10
/run/initialize
/control/execute 10cm_vis_source.mac
/vis/open OGLSQt 1800x1200-0+0
/vis/scene/create
/vis/scene/add/volume
/vis/drawVolume
/vis/viewer/set/style surface
/vis/viewer/set/background 1 1 1
/vis/viewer/set/viewpointThetaPhi 90 0 deg
/vis/viewer/set/targetPoint 0 0 215 mm
/vis/viewer/zoom 1.35
/tracking/storeTrajectory 1
/vis/scene/add/trajectories smooth
/vis/modeling/trajectories/create/drawByParticleID
/vis/modeling/trajectories/drawByParticleID-0/set e- red
/vis/modeling/trajectories/drawByParticleID-0/set e+ magenta
/vis/modeling/trajectories/drawByParticleID-0/set gamma green
/vis/scene/endOfEventAction accumulate 50
/run/beamOn 50
/vis/viewer/set/size 3000 2000
/vis/viewer/flush
/vis/ogl/export beamline_10cm_tracks50_200dpi.png
MACRO

normalize_export() {
  local wanted="$1"
  local stem="${wanted%.png}"
  shopt -s nullglob
  local matches=("${stem}"*.png)
  shopt -u nullglob
  if (( ${#matches[@]} == 0 )); then
    echo "Geant4 did not create ${wanted} (including numbered export names)." >&2
    return 1
  fi
  if [[ "${matches[0]}" != "${wanted}" ]]; then
    mv -- "${matches[0]}" "${wanted}"
  fi
}

"${EXECUTABLE}" "${WORK_DIR}/10cm_geometry_png.mac" --no-session
normalize_export beamline_10cm_geometry_300dpi.png
"${EXECUTABLE}" "${WORK_DIR}/10cm_tracks50_png.mac" --no-session
normalize_export beamline_10cm_tracks50_200dpi.png

set_png_dpi() {
  python3 - "$1" "$2" <<'PY'
import binascii
import struct
import sys
from pathlib import Path

path = Path(sys.argv[1])
dpi = float(sys.argv[2])
data = path.read_bytes()
signature = b"\x89PNG\r\n\x1a\n"
if not data.startswith(signature):
    raise SystemExit(f"not a PNG: {path}")
ppm = round(dpi / 0.0254)
payload = struct.pack(">IIB", ppm, ppm, 1)
kind = b"pHYs"
chunk = (struct.pack(">I", len(payload)) + kind + payload
         + struct.pack(">I", binascii.crc32(kind + payload) & 0xffffffff))
out = bytearray(signature)
offset = len(signature)
inserted = False
while offset < len(data):
    length = struct.unpack(">I", data[offset:offset + 4])[0]
    end = offset + length + 12
    old_kind = data[offset + 4:offset + 8]
    if old_kind == b"pHYs":
        if not inserted:
            out.extend(chunk)
            inserted = True
    else:
        out.extend(data[offset:end])
        if old_kind == b"IHDR" and not inserted:
            out.extend(chunk)
            inserted = True
    offset = end
path.write_bytes(out)
width, height = struct.unpack(">II", out[16:24])
if (width, height) != (3000, 2000):
    print(
        f"WARNING: {path.name} is {width}x{height}; the OpenGL/X11 viewer "
        "did not honor the requested 3000x2000 window size.",
        file=sys.stderr,
    )
PY
}

set_png_dpi beamline_10cm_geometry_300dpi.png 300
set_png_dpi beamline_10cm_tracks50_200dpi.png 200

mv beamline_10cm_geometry_300dpi.png beamline_10cm_tracks50_200dpi.png \
  "${OUTPUT_DIR}/"

echo "Created ${OUTPUT_DIR}/beamline_10cm_geometry_300dpi.png"
echo "Created ${OUTPUT_DIR}/beamline_10cm_tracks50_200dpi.png"
