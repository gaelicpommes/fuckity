// src/main.cc
#include "G4RunManagerFactory.hh"
#include "G4UIExecutive.hh"
#include "G4UImanager.hh"
#include "G4VisExecutive.hh"

#include "FTFP_BERT.hh"
#include "G4EmStandardPhysics_option4.hh"

#include "ActionInitialization.hh"
#include "DetectorConstruction.hh"
#include "G4ScoringManager.hh"
#include "G4String.hh"
#include "G4ios.hh"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>

#if defined(G4MULTITHREADED)
#include "G4MTRunManager.hh"
#endif

int main(int argc, char** argv)
{
  // Keep the UI open when:
  //
  // 1. No macro was supplied.
  // 2. The macro filename ends in "_vis.mac".
  // 3. The user supplies "--interactive".
  //
  bool keepUiOpen = (argc == 1);
  bool noSession = false;

  if (argc > 1) {
    const std::string macroPath = argv[1];
    const std::string visSuffix = "_vis.mac";

    keepUiOpen =
        macroPath.size() >= visSuffix.size() &&
        macroPath.compare(
            macroPath.size() - visSuffix.size(),
            visSuffix.size(),
            visSuffix) == 0;
  }

  for (int i = 2; i < argc; ++i) {
    if (std::string(argv[i]) == "--interactive") {
      keepUiOpen = true;
    } else if (std::string(argv[i]) == "--no-session") {
      // Construct the Qt application so OGLSQt can export a PNG, but return
      // immediately after the supplied macro instead of opening a UI session.
      noSession = true;
    }
  }

  // Use the Geant4 default run-manager type.
  // This selects a multithreaded run manager when supported by the
  // installed Geant4 build.
  auto* runManager =
      G4RunManagerFactory::CreateRunManager(
          G4RunManagerType::Default);

#if defined(G4MULTITHREADED)
  if (auto* mtRM =
          dynamic_cast<G4MTRunManager*>(runManager)) {
    // Zero means that the Geant4 default thread configuration is retained.
    G4int nThreads = 0;

    // Optional thread count supplied through the standard Geant4
    // environment variable.
    if (const char* envThreads =
            std::getenv("G4FORCENUMBEROFTHREADS")) {
      nThreads = std::max(
          1,
          std::atoi(envThreads));
    }

    // Optional command-line thread override:
    //
    // ./FlashElectronSim <macro.mac> <threads>
    //
    // or:
    //
    // ./FlashElectronSim <macro.mac> <threads> --interactive
    //
    if (argc > 2 &&
        std::string(argv[2]) != "--interactive" &&
        std::string(argv[2]) != "--no-session") {
      nThreads = std::max(
          1,
          std::atoi(argv[2]));
    }

    if (nThreads > 0) {
      mtRM->SetNumberOfThreads(nThreads);
    }
  }
#endif

  // Enable Geant4 command-based scoring.
  G4ScoringManager::GetScoringManager();

  // Geometry.
  runManager->SetUserInitialization(
      new DetectorConstruction());

  // Physics.
  auto* physics = new FTFP_BERT;
  physics->ReplacePhysics(
      new G4EmStandardPhysics_option4());

  runManager->SetUserInitialization(physics);

  // Primary generator and run/event actions.
  runManager->SetUserInitialization(
      new ActionInitialization());

  // A live G4UIExecutive prevents the visualization window from being
  // destroyed when the macro reaches the end.
  //
  // For a visualization macro, pass only argv[0] to G4UIExecutive.
  // This prevents the macro filename, thread count, and --interactive flag
  // from being interpreted as Qt or shell arguments.
  std::unique_ptr<G4UIExecutive> uiExecutive;

  if (keepUiOpen || noSession) {
    G4int uiArgc = (argc == 1) ? argc : 1;

    uiExecutive =
        std::make_unique<G4UIExecutive>(
            uiArgc,
            argv);
  }

  // Initialize the Geant4 visualization manager.
  auto* visManager = new G4VisExecutive();
  visManager->Initialize();

  auto* uiManager =
      G4UImanager::GetUIpointer();

  if (argc > 1) {
    // Execute the macro supplied on the command line.
    uiManager->ApplyCommand(
        "/control/execute " + G4String(argv[1]));
  } else {
    // No macro was supplied, so start the standard visualization macro.
    G4cout
        << "[FlashElectronSim] No macro supplied; "
        << "starting interactive visualization."
        << G4endl;

    G4cout
        << "[FlashElectronSim] Batch usage: "
        << "./FlashElectronSim <macro.mac>"
        << G4endl;

    uiManager->ApplyCommand(
        "/control/execute macros/vis.mac");
  }

  // Start the interactive session only after all commands in the macro
  // have completed. The geometry and trajectories remain visible while
  // the user rotates, pans, zooms, and saves images.
  if (uiExecutive && !noSession) {
    G4cout
        << "[FlashElectronSim] UI remains open. "
        << "Use the viewer controls, or type 'exit' to close."
        << G4endl;

    uiExecutive->SessionStart();
  }

  // These objects must not be deleted until after SessionStart() returns.
  delete visManager;
  delete runManager;

  return 0;
}
