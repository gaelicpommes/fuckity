#!/bin/bash
#SBATCH --job-name=topas_pdd4
#SBATCH --output=logs/pdd4_%A_%a.out
#SBATCH --error=logs/pdd4_%A_%a.err
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

cd /scratch/brussel/113/vsc11383/projects/third_sim/FlashElectronSim/build
mkdir -p logs
export G4FORCENUMBEROFTHREADS=${SLURM_CPUS_PER_TASK}

CSV="../macros/four_node_grid.csv"

format_beamon_tag() {
    local n="$1"
    if (( n >= 1000000 )); then
        if (( n % 1000000 == 0 )); then
            echo "$((n/1000000))M"
        else
            awk -v x="$n" 'BEGIN{printf "%.1fM", x/1000000.0}'
        fi
    elif (( n >= 1000 )); then
        if (( n % 1000 == 0 )); then
            echo "$((n/1000))k"
        else
            awk -v x="$n" 'BEGIN{printf "%.1fk", x/1000.0}'
        fi
    else
        echo "${n}"
    fi
}

LINE=$(awk -F',' '
    BEGIN {count=0}
    /^[[:space:]]*#/ {next}
    /^[[:space:]]*$/ {next}
    $1 == "run_id" {next}
    {count++}
    count == ENVIRON["SLURM_ARRAY_TASK_ID"] {print; exit}
' "${CSV}")

if [[ -z "${LINE}" ]]; then
  echo "No valid row for task ${SLURM_ARRAY_TASK_ID} in ${CSV}"
  exit 1
fi

# CSV columns:
# run_id,beamOn,Emean_MeV,sigmaE_MeV,sigmaTheta_deg,sigma_x_mm,sigma_y_mm,applicator_cm,latDmax_z_mm,latR50_z_mm,latMid_z_mm,config
IFS=',' read -r RUNID BEAMON EMEAN SIGE SIGTH SX SY APPLICATOR_CM LAT_DMAX_Z LAT_R50_Z LAT_MID_Z CONFIG <<< "${LINE}"

RUNID=$(echo "${RUNID}" | xargs)
BEAMON=$(echo "${BEAMON}" | xargs)
EMEAN=$(echo "${EMEAN}" | xargs)
SIGE=$(echo "${SIGE}" | xargs)
SIGTH=$(echo "${SIGTH}" | xargs)
SX=$(echo "${SX}" | xargs)
SY=$(echo "${SY}" | xargs)
APPLICATOR_CM=$(echo "${APPLICATOR_CM}" | xargs)
LAT_DMAX_Z=$(echo "${LAT_DMAX_Z}" | xargs)
LAT_R50_Z=$(echo "${LAT_R50_Z}" | xargs)
LAT_MID_Z=$(echo "${LAT_MID_Z}" | xargs)
CONFIG=$(echo "${CONFIG}" | xargs)

case "${APPLICATOR_CM}" in
  10|5|2) ;;
  *) echo "Invalid applicator_cm=${APPLICATOR_CM}; allowed values are 10, 5, or 2" >&2; exit 1 ;;
esac

BEAMTAG=$(format_beamon_tag "${BEAMON}")
echo "Task ${SLURM_ARRAY_TASK_ID}: config=${CONFIG} run_id=${RUNID} beamOn=${BEAMON} (${BEAMTAG}) E=${EMEAN} sigE=${SIGE} sigTh=${SIGTH} SX=${SX} SY=${SY} app=${APPLICATOR_CM}cm latZ=${LAT_DMAX_Z}/${LAT_R50_Z}/${LAT_MID_Z} mm"

TAG="${CONFIG}_r${RUNID}_E${EMEAN}_sE${SIGE}_sTh${SIGTH}_${APPLICATOR_CM}cm"
TAG_CLEAN=$(echo "${TAG}" | sed 's/[^A-Za-z0-9_+-]/_/g')

SRCMAC="src_${TAG_CLEAN}_J${SLURM_JOB_ID}_T${SLURM_ARRAY_TASK_ID}.mac"
sed -e "s/EMEAN/${EMEAN}/g" \
    -e "s/SIGE/${SIGE}/g" \
    -e "s/SIGTH/${SIGTH}/g" \
    -e "s/SX/${SX}/g" \
    -e "s/SY/${SY}/g" \
    ../macros/sourcepossib.mac > "${SRCMAC}"

PDDTXT="pdd_${TAG_CLEAN}_N${BEAMTAG}_J${SLURM_JOB_ID}_T${SLURM_ARRAY_TASK_ID}.txt"
LAT_DMAX="latDmax_${TAG_CLEAN}_N${BEAMTAG}_J${SLURM_JOB_ID}_T${SLURM_ARRAY_TASK_ID}.txt"
LAT_R50="latR50_${TAG_CLEAN}_N${BEAMTAG}_J${SLURM_JOB_ID}_T${SLURM_ARRAY_TASK_ID}.txt"
LAT_MID="latMid_${TAG_CLEAN}_N${BEAMTAG}_J${SLURM_JOB_ID}_T${SLURM_ARRAY_TASK_ID}.txt"

RUNMAC=$(mktemp "run_${TAG_CLEAN}_XXXX.mac")
trap 'rm -f "$RUNMAC" "$SRCMAC"' EXIT

sed -e "s|SOURCE_MAC|${SRCMAC}|g" \
    -e "s|BEAMON|${BEAMON}|g" \
    -e "s|APPLICATOR_CM|${APPLICATOR_CM}|g" \
    -e "s|LAT_DMAX_Z|${LAT_DMAX_Z}|g" \
    -e "s|LAT_R50_Z|${LAT_R50_Z}|g" \
    -e "s|LAT_MID_Z|${LAT_MID_Z}|g" \
    -e "s|PDD_TXT|${PDDTXT}|g" \
    -e "s|LAT_DMAX_DOSE_TXT|${LAT_DMAX}|g" \
    -e "s|LAT_R50_DOSE_TXT|${LAT_R50}|g" \
    -e "s|LAT_MID_DOSE_TXT|${LAT_MID}|g" \
    ../macros/pdd_four_node.mac > "${RUNMAC}"

./FlashElectronSim "${RUNMAC}"

echo "Wrote: ${PDDTXT}"
echo "Wrote: ${LAT_DMAX}"
echo "Wrote: ${LAT_R50}"
echo "Wrote: ${LAT_MID}"

