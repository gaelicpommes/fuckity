#!/bin/bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXECUTABLE="${PROJECT_DIR}/build/FlashElectronSim"
MACRO="${PROJECT_DIR}/macros/10cm_geometry_vis.mac"

[[ -x "${EXECUTABLE}" ]] || {
  echo "Build FlashElectronSim first; executable not found: ${EXECUTABLE}" >&2
  exit 1
}
[[ -f "${MACRO}" ]] || {
  echo "Visualization macro not found: ${MACRO}" >&2
  exit 1
}
[[ -n "${DISPLAY:-}" ]] || {
  echo "DISPLAY is unset. Connect with X11 forwarding or use a graphical node." >&2
  exit 1
}

export G4FORCENUMBEROFTHREADS=1
cd "${PROJECT_DIR}"
exec "${EXECUTABLE}" "${MACRO}" --interactive
