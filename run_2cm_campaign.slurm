#!/bin/bash
# Run the complete 2 cm campaign as four independent one-node array tasks.
# Tasks 1-2: PDD/profile scoring, 200 million histories at 9 and 6 MeV.
# Tasks 3-4: transport diagnostics, 50 million histories at 9 and 6 MeV.
#SBATCH --job-name=flash_2cm
#SBATCH --output=flash_2cm_%A_%a.out
#SBATCH --error=flash_2cm_%A_%a.err
#SBATCH --time=119:00:00
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --mem=8G
#SBATCH --array=1-4%4
#SBATCH --requeue

set -euo pipefail

module purge
module load Geant4/11.3.0-GCC-13.3.0
module load Geant4-data/11.3

PROJECT_DIR="${PROJECT_DIR:-/scratch/brussel/113/vsc11383/projects/third_sim/FlashElectronSim}"
BUILD_DIR="${BUILD_DIR:-${PROJECT_DIR}/build}"
OUTPUT_ROOT="${OUTPUT_ROOT:-${PROJECT_DIR}/2cm_campaign_results}"
EXECUTABLE="${BUILD_DIR}/FlashElectronSim"
TASK_ID="${SLURM_ARRAY_TASK_ID}"
JOB_ID="${SLURM_ARRAY_JOB_ID:-${SLURM_JOB_ID}}"

[[ -x "${EXECUTABLE}" ]] || { echo "Missing executable: ${EXECUTABLE}" >&2; exit 1; }
[[ -f "${PROJECT_DIR}/2cmapplicator-Cone.stl" ]] || {
  echo "Missing 2 cm applicator: ${PROJECT_DIR}/2cmapplicator-Cone.stl" >&2
  exit 1
}

# task,mode,nominal energy,source mean,sigma,beamOn,dmax,R50,mid
case "${TASK_ID}" in
  1) MODE=pdd;       ENERGY=9; EMEAN=9.70; SIGE=1.30; BEAMON=200000000; DMAX=438.0; R50=444.3; MID=441.1 ;;
  2) MODE=pdd;       ENERGY=6; EMEAN=6.70; SIGE=1.00; BEAMON=200000000; DMAX=438.1; R50=453.1; MID=445.6 ;;
  3) MODE=transport; ENERGY=9; EMEAN=9.70; SIGE=1.30; BEAMON=50000000  ;;
  4) MODE=transport; ENERGY=6; EMEAN=6.70; SIGE=1.00; BEAMON=50000000  ;;
  *) echo "Unexpected array task ${TASK_ID}" >&2; exit 1 ;;
esac

RUN_DIR="${OUTPUT_ROOT}/${JOB_ID}/${MODE}_${ENERGY}MeV_task${TASK_ID}"
mkdir -p "${RUN_DIR}"
SOURCE_MAC="${RUN_DIR}/source.mac"
RUN_MAC="${RUN_DIR}/run.mac"

sed -e "s/EMEAN/${EMEAN}/g" -e "s/SIGE/${SIGE}/g" \
    -e 's/SIGTH/0.2/g' -e 's/SX/0.73/g' -e 's/SY/0.73/g' \
    "${PROJECT_DIR}/macros/sourcepossib.mac" > "${SOURCE_MAC}"

if [[ "${MODE}" == pdd ]]; then
  sed -e "s|SOURCE_MAC|${SOURCE_MAC}|g" -e "s/BEAMON/${BEAMON}/g" \
      -e 's/APPLICATOR_CM/2/g' -e "s/LAT_DMAX_Z/${DMAX}/g" \
      -e "s/LAT_R50_Z/${R50}/g" -e "s/LAT_MID_Z/${MID}/g" \
      -e "s|PDD_TXT|${RUN_DIR}/pdd_2cm_${ENERGY}MeV_200M.txt|g" \
      -e "s|LAT_DMAX_DOSE_TXT|${RUN_DIR}/latDmax_2cm_${ENERGY}MeV_200M.txt|g" \
      -e "s|LAT_R50_DOSE_TXT|${RUN_DIR}/latR50_2cm_${ENERGY}MeV_200M.txt|g" \
      -e "s|LAT_MID_DOSE_TXT|${RUN_DIR}/latMid_2cm_${ENERGY}MeV_200M.txt|g" \
      "${PROJECT_DIR}/macros/pdd_four_node.mac" > "${RUN_MAC}"
else
  SEED1=$(( (SLURM_JOB_ID * 104729 + TASK_ID * 1009) % 900000000 + 1 ))
  SEED2=$(( (SLURM_JOB_ID * 130363 + TASK_ID * 9176 + 37) % 900000000 + 1 ))
  sed -e "s/@SEED1@/${SEED1}/g" -e "s/@SEED2@/${SEED2}/g" \
      -e 's/@APPLICATOR_CM@/2/g' -e "s|@SOURCE_MAC@|${SOURCE_MAC}|g" \
      -e "s/@BEAMON@/${BEAMON}/g" \
      "${PROJECT_DIR}/macros/transport_run_template.mac" > "${RUN_MAC}"
  export FLASH_ENABLE_TRANSPORT_SCORING=1 FLASH_APPLICATOR_CM=2
  export FLASH_NOMINAL_ENERGY_MEV="${ENERGY}" FLASH_SOURCE_MEAN_MEV="${EMEAN}"
  export FLASH_SOURCE_SIGMA_MEV="${SIGE}" FLASH_RUN_LABEL="2cm_${ENERGY}MeV"
  export FLASH_OUTPUT_PREFIX="${RUN_DIR}/transport_2cm_${ENERGY}MeV_50M"
fi

export G4FORCENUMBEROFTHREADS="${SLURM_CPUS_PER_TASK}"
echo "Starting ${MODE}, 2 cm applicator, ${ENERGY} MeV, ${BEAMON} histories on $(hostname)"
cd "${RUN_DIR}"
srun --ntasks=1 --cpus-per-task="${SLURM_CPUS_PER_TASK}" \
  "${EXECUTABLE}" "${RUN_MAC}" "${SLURM_CPUS_PER_TASK}"
echo "Completed task ${TASK_ID}; results are in ${RUN_DIR}"
