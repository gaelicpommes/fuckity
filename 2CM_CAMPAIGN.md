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

### Just open the 10 cm geometry window

For an interactive window without PNG export and without particle transport,
run this from an X11-forwarded or graphical session:

```bash
cd /scratch/brussel/113/vsc11383/projects/third_sim/FlashElectronSim
module purge
module load Geant4/11.3.0-GCC-13.3.0
module load Geant4-data/11.3
bash ./view_10cm_geometry.sh
```

The window shows the complete beamline, blue 10 cm STL applicator, and cyan
wireframe 300 x 300 x 300 mm3 water phantom on a dark navy background. Curved
components use additional line segments and auxiliary edges so their outlines
remain easy to distinguish. Use the mouse to rotate, pan, and zoom. Close the Qt window or
enter `exit` in its command box to stop the program. The wrapper forces one
Geant4 worker and the macro contains no `/run/beamOn`, so it does not run a
simulation.

Use an X11-forwarded session or graphical interactive node; the Qt OpenGL
viewer requires `DISPLAY`. From the project root, run:

```bash
module purge
module load Geant4/11.3.0-GCC-13.3.0
module load Geant4-data/11.3
cd /scratch/brussel/113/vsc11383/projects/third_sim/FlashElectronSim
bash ./render_10cm_figures.sh
```

Using `bash` explicitly works even if the script's executable permission was
lost while copying or uploading the repository. If direct execution reports
`Permission denied`, either continue with the command above or restore the bit:

```bash
chmod u+x render_10cm_figures.sh
./render_10cm_figures.sh
```

Confirm the permissions with `stat -c '%A %n' render_10cm_figures.sh`; an
executable copy normally starts with `-rwx`. If `chmod` is prohibited or the
filesystem is mounted `noexec`, use `bash render_10cm_figures.sh` rather than
executing the file directly.

The script creates `figures/beamline_10cm_geometry_300dpi.png`, a geometry-only
view, and `figures/beamline_10cm_tracks50_200dpi.png`, a view after exactly 50
9 MeV-class primary histories. Both show the complete beamline, blue 10 cm STL
applicator, and cyan water phantom. The trajectory image draws electrons red,
positrons magenta, and photons green.

The macros request 3000x2000-pixel viewer exports. PNG pixels do not by
themselves define print DPI, so the script also writes the requested 300 DPI
and 200 DPI `pHYs` metadata using its embedded Python helper. The first image prints
at 10 x 6.67 inches at 300 DPI; the second prints at 15 x 10 inches at 200 DPI.
No production scoring output is generated by either figure macro.

The rendering wrapper is self-contained: it creates temporary macro files and
embeds the DPI metadata writer itself. It therefore does not fail if the
checkout is missing `macros/10cm_*_png.mac` or `tools/set_png_dpi.py`. If output
still reports `Can not open a macro file`, update `render_10cm_figures.sh` to the
latest committed version and rebuild `FlashElectronSim` before retrying.
You can confirm that the self-contained version is present with
`grep -n 'Generate self-contained copies' render_10cm_figures.sh`.

Geant4 Qt commonly appends a sequence suffix, for example `_0000.png`, even
when the requested filename already ends in `.png`. The wrapper now detects
that filename and renames it to the documented canonical name before setting
DPI. It requests a 3000x2000 window in the supported `/vis/open` size hint; if
the X11/OpenGL framebuffer restricts the export to a smaller size such as
640x480, the script preserves the valid image but prints a resolution warning.
Geant4 11.3 does not provide `/vis/viewer/set/size`, so the wrapper deliberately
does not issue that command.

The wrapper forces `G4FORCENUMBEROFTHREADS=1` for both figure runs. Rendering 50
histories does not benefit from a full compute-node thread count, and a single
worker prevents the `/run/initialize` commands from being echoed once by every
`G4WT` worker.

`GeomNav1002` about a stuck track in `CAD11PV` is a Geant4 navigation warning
from a transported visualization history. It does not mean the PNG export
failed: success is determined by the `File ... has been saved` message and the
output-file check. `Polyhedron::SetReferences` messages are emitted while the
Qt viewer redraws tessellated CAD for export and are likewise not missing-file
errors. The geometry-only PNG transports no particles and therefore cannot
produce the stuck-track warning.

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
