#!/bin/bash
#SBATCH --job-name=flash_transport
#SBATCH --output=transport_%A_%a.out
#SBATCH --error=transport_%A_%a.err
#SBATCH --time=119:00:00
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --mem=8G
#SBATCH --array=1-6%4
#SBATCH --requeue

set -euo pipefail

module purge
module load Geant4/11.3.0-GCC-13.3.0
module load Geant4-data/11.3

# Submit from the repository root. PROJECT_DIR, BUILD_DIR, OUTPUT_ROOT, and
# TRANSPORT_GRID can be overridden with sbatch --export when needed.
PROJECT_DIR="${PROJECT_DIR:-${SLURM_SUBMIT_DIR:-$(pwd)}}"
BUILD_DIR="${BUILD_DIR:-${PROJECT_DIR}/build}"
OUTPUT_ROOT="${OUTPUT_ROOT:-${PROJECT_DIR}/transport_results}"
CSV="${TRANSPORT_GRID:-${PROJECT_DIR}/macros/transport_grid.csv}"
TASK_ID="${SLURM_ARRAY_TASK_ID:-1}"
JOB_ID="${SLURM_JOB_ID:-1}"

if [[ ! -f "${PROJECT_DIR}/CMakeLists.txt" ]]; then
  echo "PROJECT_DIR is not the electroseeps repository: ${PROJECT_DIR}" >&2
  echo "Submit from the repository root or export PROJECT_DIR=/path/to/electroseeps." >&2
  exit 1
fi

EXECUTABLE="${BUILD_DIR}/FlashElectronSim"
if [[ ! -x "${EXECUTABLE}" ]]; then
  echo "Executable not found: ${EXECUTABLE}" >&2
  echo "Build first with: cmake -S ${PROJECT_DIR} -B ${BUILD_DIR} -DCMAKE_BUILD_TYPE=Release" >&2
  exit 1
fi

if [[ ! -f "${CSV}" ]]; then
  echo "Transport grid not found: ${CSV}" >&2
  exit 1
fi

LINE=$(awk -F',' -v wanted="${TASK_ID}" '
  BEGIN {count=0}
  /^[[:space:]]*#/ {next}
  /^[[:space:]]*$/ {next}
  $1 == "run_id" {next}
  {count++}
  count == wanted {print; exit}
' "${CSV}")

if [[ -z "${LINE}" ]]; then
  echo "No transport-grid row for array task ${TASK_ID}." >&2
  exit 1
fi

# CSV columns:
# run_id,beamOn,nominal_energy_MeV,source_mean_MeV,source_sigma_MeV,
# sigmaTheta_deg,sigma_x_mm,sigma_y_mm,applicator_cm,config
IFS=',' read -r RUN_ID BEAMON NOMINAL_ENERGY SOURCE_MEAN SOURCE_SIGMA \
  SIGMA_THETA SIGMA_X SIGMA_Y APPLICATOR_CM CONFIG <<< "${LINE}"

trim() {
  local value="$1"
  value="${value#"${value%%[![:space:]]*}"}"
  value="${value%"${value##*[![:space:]]}"}"
  printf '%s' "${value}"
}

RUN_ID=$(trim "${RUN_ID}")
BEAMON=$(trim "${BEAMON}")
NOMINAL_ENERGY=$(trim "${NOMINAL_ENERGY}")
SOURCE_MEAN=$(trim "${SOURCE_MEAN}")
SOURCE_SIGMA=$(trim "${SOURCE_SIGMA}")
SIGMA_THETA=$(trim "${SIGMA_THETA}")
SIGMA_X=$(trim "${SIGMA_X}")
SIGMA_Y=$(trim "${SIGMA_Y}")
APPLICATOR_CM=$(trim "${APPLICATOR_CM}")
CONFIG=$(trim "${CONFIG}")

if [[ -n "${BEAMON_OVERRIDE:-}" ]]; then
  BEAMON="${BEAMON_OVERRIDE}"
fi

if [[ ! "${BEAMON}" =~ ^[1-9][0-9]*$ ]]; then
  echo "beamOn must be a positive integer; got '${BEAMON}'." >&2
  exit 1
fi

case "${APPLICATOR_CM}" in
  10|5|2) ;;
  *)
    echo "Invalid applicator_cm=${APPLICATOR_CM}; expected 10, 5, or 2." >&2
    exit 1
    ;;
esac

TAG="${RUN_ID}_${CONFIG}_N${BEAMON}"
RUN_DIR="${OUTPUT_ROOT}/${JOB_ID}/${TAG}_task${TASK_ID}"
mkdir -p "${RUN_DIR}"

SOURCE_MAC="${RUN_DIR}/source.mac"
RUN_MAC="${RUN_DIR}/run.mac"

sed \
  -e "s/@SOURCE_MEAN_MEV@/${SOURCE_MEAN}/g" \
  -e "s/@SOURCE_SIGMA_MEV@/${SOURCE_SIGMA}/g" \
  -e "s/@SIGMA_THETA_DEG@/${SIGMA_THETA}/g" \
  -e "s/@SIGMA_X_MM@/${SIGMA_X}/g" \
  -e "s/@SIGMA_Y_MM@/${SIGMA_Y}/g" \
  "${PROJECT_DIR}/macros/transport_source_template.mac" > "${SOURCE_MAC}"

# Different deterministic seeds for every job/task pair.
SEED1=$(( (JOB_ID * 104729 + TASK_ID * 1009) % 900000000 + 1 ))
SEED2=$(( (JOB_ID * 130363 + TASK_ID * 9176 + 37) % 900000000 + 1 ))

sed \
  -e "s/@SEED1@/${SEED1}/g" \
  -e "s/@SEED2@/${SEED2}/g" \
  -e "s/@APPLICATOR_CM@/${APPLICATOR_CM}/g" \
  -e "s|@SOURCE_MAC@|${SOURCE_MAC}|g" \
  -e "s/@BEAMON@/${BEAMON}/g" \
  "${PROJECT_DIR}/macros/transport_run_template.mac" > "${RUN_MAC}"

export G4FORCENUMBEROFTHREADS="${SLURM_CPUS_PER_TASK:-1}"
export FLASH_ENABLE_TRANSPORT_SCORING=1
export FLASH_APPLICATOR_CM="${APPLICATOR_CM}"
export FLASH_NOMINAL_ENERGY_MEV="${NOMINAL_ENERGY}"
export FLASH_SOURCE_MEAN_MEV="${SOURCE_MEAN}"
export FLASH_SOURCE_SIGMA_MEV="${SOURCE_SIGMA}"
export FLASH_RUN_LABEL="${RUN_ID}_${CONFIG}"
export FLASH_OUTPUT_PREFIX="${RUN_DIR}/${RUN_ID}_${CONFIG}_J${JOB_ID}_T${TASK_ID}"

echo "Run label: ${FLASH_RUN_LABEL}"
echo "Histories: ${BEAMON}"
echo "Source: ${SOURCE_MEAN} +/- ${SOURCE_SIGMA} MeV, sigma theta ${SIGMA_THETA} deg"
echo "Applicator: ${APPLICATOR_CM} cm"
echo "Threads: ${G4FORCENUMBEROFTHREADS}"
echo "Output directory: ${RUN_DIR}"

cd "${BUILD_DIR}"
srun --ntasks=1 --cpus-per-task="${SLURM_CPUS_PER_TASK:-1}" \
  "${EXECUTABLE}" "${RUN_MAC}" "${SLURM_CPUS_PER_TASK:-1}"

SUMMARY="${FLASH_OUTPUT_PREFIX}_run0_transport_summary.csv"
HISTOGRAMS="${FLASH_OUTPUT_PREFIX}_run0_transport_histograms.csv"
if [[ ! -s "${SUMMARY}" || ! -s "${HISTOGRAMS}" ]]; then
  echo "Transport outputs were not created or are empty." >&2
  exit 1
fi

echo "Wrote: ${SUMMARY}"
echo "Wrote: ${HISTOGRAMS}"
