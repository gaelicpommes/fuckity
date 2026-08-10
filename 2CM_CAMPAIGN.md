# 2 cm, 6/9 MeV campaign

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
