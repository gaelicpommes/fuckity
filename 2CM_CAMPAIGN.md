# 2 cm, 6/9 MeV campaign

## Quick start

Run these commands on the VSC cluster login node:

```bash
cd /scratch/brussel/113/vsc11383/projects/third_sim/FlashElectronSim

module purge
module load CMake
module load Geant4/11.3.0-GCC-13.3.0
module load Geant4-data/11.3

command -v cmake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 32

sbatch run_2cm_campaign.slurm
```

The `cmake -S . -B build` form must be run from the project root—the directory
that contains `CMakeLists.txt`. If the prompt currently ends in
`FlashElectronSim/build`, either return to the project root:

```bash
cd /scratch/brussel/113/vsc11383/projects/third_sim/FlashElectronSim
module load CMake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 32
```

or configure the existing build directory in place:

```bash
cd /scratch/brussel/113/vsc11383/projects/third_sim/FlashElectronSim/build
module load CMake
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release
cmake --build . -j 32
```

Do not use `cmake -S . -B build` while already inside `FlashElectronSim/build`:
there is no `CMakeLists.txt` there, and it incorrectly asks CMake to use a
nested `build/build` output directory.

### If `cmake: command not found` appears

`module load Geant4` does not necessarily put the CMake executable in `PATH`.
Load the cluster's CMake module separately and verify it before configuring:

```bash
module purge
module spider CMake
module load CMake
command -v cmake
cmake --version
```

`command -v cmake` must print a path. If `module load CMake` asks for a specific
version or prerequisite, use the exact load command displayed by
`module spider CMake`. Do not continue to `make`: no Makefile exists until CMake
configuration succeeds.

After CMake is available, choose the command according to the current prompt:

```bash
# Prompt ends in FlashElectronSim (project root):
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 32

# Prompt ends in FlashElectronSim/build:
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release
cmake --build . -j 32
```

Only one `sbatch` command is needed. It submits four array tasks:

| Array task | Calculation | Histories |
| ---: | --- | ---: |
| 1 | 2 cm applicator, 9 MeV PDD and profiles | 200,000,000 |
| 2 | 2 cm applicator, 6 MeV PDD and profiles | 200,000,000 |
| 3 | 2 cm applicator, 9 MeV transport | 50,000,000 |
| 4 | 2 cm applicator, 6 MeV transport | 50,000,000 |

Slurm can run all four tasks concurrently on four available nodes, or start
them individually as nodes become available. After submission, note the job ID
printed by `sbatch`, then monitor and inspect it with:

```bash
squeue -u "$USER"
sacct -j JOB_ID --format=JobID,JobName,State,Elapsed,AllocCPUS,NodeList
tail -f flash_2cm_JOB_ID_1.out
```

### Check your job queue

The shortest command to show only your jobs is:

```bash
squeue --me
```

If the cluster's Slurm version does not support `--me`, use:

```bash
squeue -u "$USER"
```

For this campaign, show the four array tasks and why any task is waiting with:

```bash
squeue -j JOB_ID -r -o "%.18i %.9P %.24j %.2t %.10M %.10l %.6D %R"
```

Replace `JOB_ID` with the number printed by `sbatch`. In the `ST` column, `R`
means running and `PD` means pending. For pending jobs, the final `NODELIST(REASON)`
column reports the reason, such as `Resources` or `Priority`. A completed job no
longer appears in `squeue`; check it with:

```bash
sacct -j JOB_ID --format=JobID,JobName%24,State,Elapsed,AllocCPUS,NodeList
```

Replace `JOB_ID` with the number returned by `sbatch`. Standard output/error
logs are written in the directory from which `sbatch` was invoked. Simulation
results are written to:

```text
/scratch/brussel/113/vsc11383/projects/third_sim/FlashElectronSim/2cm_campaign_results/JOB_ID/
├── pdd_9MeV_task1/
├── pdd_6MeV_task2/
├── transport_9MeV_task3/
└── transport_6MeV_task4/
```

Do not run `FlashElectronSim` manually for the production campaign; the Slurm
script generates the four source/run macros, sets 32 Geant4 worker threads, and
starts each simulation with `srun`.

## Build and submit the four jobs

The array requests one node and 32 CPU cores per task, permits all four tasks
to run concurrently, and also allows Slurm to start each task separately as a
node becomes available. From the project directory on the cluster:

```bash
cd /scratch/brussel/113/vsc11383/projects/third_sim/FlashElectronSim
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 32
sbatch run_2cm_campaign.slurm
```

Array tasks 1 and 2 produce PDD and lateral-profile files using 200 million
histories for 9 and 6 MeV. Tasks 3 and 4 produce transport CSV files using 50
million histories for the same energies. All outputs are grouped below
`2cm_campaign_results/<array-job-id>/`. Use `squeue -u "$USER"` to follow them.

## Inspect the geometry without running particles

The geometry macro selects the 2 cm option before initialization and contains
no `beamOn`, so it only opens the model. Use a graphical interactive node or an
X11-forwarded session (not a batch compute job):

```bash
cd /scratch/brussel/113/vsc11383/projects/third_sim/FlashElectronSim
./build/FlashElectronSim macros/2cm_geometry_vis.mac --interactive
```

## Export publication PNGs for the 10 cm applicator

Use an X11-forwarded session or graphical interactive node; the Qt OpenGL
viewer requires `DISPLAY`. From the project root, run:

```bash
module purge
module load Geant4/11.3.0-GCC-13.3.0
module load Geant4-data/11.3
cd /scratch/brussel/113/vsc11383/projects/third_sim/FlashElectronSim
./render_10cm_figures.sh
```

The script creates `figures/beamline_10cm_geometry_300dpi.png`, a geometry-only
view, and `figures/beamline_10cm_tracks50_200dpi.png`, a view after exactly 50
9 MeV-class primary histories. Both show the complete beamline, blue 10 cm STL
applicator, and cyan water phantom. The trajectory image draws electrons red,
positrons magenta, and photons green.

The OpenGL exporter creates 3000x2000-pixel images. PNG pixels do not by
themselves define print DPI, so the script also writes the requested 300 DPI
and 200 DPI `pHYs` metadata using `tools/set_png_dpi.py`. The first image prints
at 10 x 6.67 inches at 300 DPI; the second prints at 15 x 10 inches at 200 DPI.
No production scoring output is generated by either figure macro.

The blue cone is loaded from `2cmapplicator-Cone.stl`. The native mesh is 422 mm
long, which would leave a 6 mm gap if its entrance were aligned with the 428 mm
long 10 cm applicator. The geometry scales only the 2 cm STL's beam-axis length
by `428/422`; its transverse aperture dimensions are not scaled. Consequently,
both applicators start at z=2 mm, both exit at z=430 mm, and the cyan water
phantom starts at z=430 mm. Their exit-to-phantom gaps are therefore:

- 10 cm STL: 0 mm
- 2 cm STL: 0 mm (6 mm before axial scaling)

Touching boundary faces are valid in Geant4 because the volumes do not overlap.
Placement overlap checks remain enabled, and the terminal will report an error
if the STL actually intersects another volume.

### Diameter and axial scaling

The `428/422` correction is applied to Z only. Every X and Y coordinate is
copied unchanged, so the STL keeps exactly the diameters contained in the
supplied file. Inspection of the mesh gives these face dimensions:

| Face | Inner diameter | Outer diameter |
| --- | ---: | ---: |
| Upstream (`CAD Z=136 mm`) | 100 mm | 130 mm |
| Downstream (`CAD Z=-286 mm`) | 40 mm | 70 mm |

Therefore scaling the length does **not** change the applicator diameter.
However, the supplied file's downstream opening is 40 mm in diameter, not
20 mm. If “2 cm applicator” is intended to mean a 20 mm **diameter** rather
than a 20 mm radius, the supplied STL itself does not have that aperture and a
different/corrected STL is required; the simulation does not silently shrink
its transverse geometry.
