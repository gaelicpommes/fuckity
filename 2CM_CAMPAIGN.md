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

The blue cone is loaded from `2cmapplicator-Cone.stl`. Its downstream 20 mm-ID
face is at z=424 mm, while the cyan water phantom starts at z=430 mm. The 6 mm
air gap prevents coincident surfaces. The placement overlap checks are enabled;
the terminal will report a Geant4 overlap error if the STL intersects another
volume.
