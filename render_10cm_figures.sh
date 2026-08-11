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
ln -s "${PROJECT_DIR}/macros" "${WORK_DIR}/macros"
cd "${WORK_DIR}"
"${EXECUTABLE}" "${PROJECT_DIR}/macros/10cm_geometry_png.mac" --no-session
"${EXECUTABLE}" "${PROJECT_DIR}/macros/10cm_tracks50_png.mac" --no-session

python3 "${PROJECT_DIR}/tools/set_png_dpi.py" \
  beamline_10cm_geometry_300dpi.png 300
python3 "${PROJECT_DIR}/tools/set_png_dpi.py" \
  beamline_10cm_tracks50_200dpi.png 200

mv beamline_10cm_geometry_300dpi.png beamline_10cm_tracks50_200dpi.png \
  "${OUTPUT_DIR}/"

echo "Created ${OUTPUT_DIR}/beamline_10cm_geometry_300dpi.png"
echo "Created ${OUTPUT_DIR}/beamline_10cm_tracks50_200dpi.png"
