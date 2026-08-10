#!/usr/bin/env bash
set -euo pipefail

threads="${1:-${SLURM_CPUS_PER_TASK:-16}}"

for energy in 6 9 12; do
  echo "=== ${energy} MeV: titanium plate (${threads} threads) ==="
  ./FlashElectronSim "macros/plate_study_${energy}MeV.mac" "${threads}"
  echo "=== ${energy} MeV: homogeneous-water control (${threads} threads) ==="
  ./FlashElectronSim "macros/water_only_study_${energy}MeV.mac" "${threads}"
done

echo "=== Plate/control comparison simulations complete ==="
