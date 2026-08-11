// src/DetectorConstruction.cc
#include "DetectorConstruction.hh"

#include "BeamlineGeometry.hh"
#include "PhantomGeometry.hh"

#include "G4Box.hh"
#include "G4Colour.hh"
#include "G4GenericMessenger.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4VisAttributes.hh"
#include "G4ios.hh"

#include <cmath>
#include <cstdlib>

DetectorConstruction::DetectorConstruction()
{
  fMessenger = std::make_unique<G4GenericMessenger>(
      this,
      "/flash/",
      "Flash electron beam setup");

  auto& appCmd = fMessenger->DeclareMethod(
      "setApplicatorIDcm",
      &DetectorConstruction::SetApplicatorIDcm,
      "Select applicator ID in cm (allowed: 10, 5, 2).");

  appCmd.SetParameterName("idCm", false);
  appCmd.SetStates(G4State_PreInit);

  auto& plateCmd = fMessenger->DeclareMethod(
      "enablePlateStudy",
      &DetectorConstruction::SetPlateStudy,
      "Build the 16 mm water + 6 mm titanium plate depth-dose phantom.");

  plateCmd.SetParameterName("enabled", false);
  plateCmd.SetStates(G4State_PreInit);
  

  auto& waterOnlyCmd = fMessenger->DeclareMethod(
      "enableWaterOnlyStudy", &DetectorConstruction::SetWaterOnlyStudy,
      "Build the matched 80 mm homogeneous-water control phantom.");
  waterOnlyCmd.SetParameterName("enabled", false);
  waterOnlyCmd.SetStates(G4State_PreInit);

  // Optional environment-based selector for normal beamline batch jobs:
  //
  //   export FLASH_APPLICATOR_CM=10
  //   export FLASH_APPLICATOR_CM=5
  //   export FLASH_APPLICATOR_CM=2
  //
  if (const char* appEnv = std::getenv("FLASH_APPLICATOR_CM")) {
    const auto idCm = std::atof(appEnv);
    SetApplicatorIDcm(idCm);

    G4cout << "[DetectorConstruction] FLASH_APPLICATOR_CM="
           << appEnv << G4endl;
  }
}

void DetectorConstruction::SetPlateStudy(G4bool enabled)
{
  fPlateStudy = enabled;
  if (enabled) fWaterOnlyStudy = false;
}

void DetectorConstruction::SetWaterOnlyStudy(G4bool enabled)
{
  fWaterOnlyStudy = enabled;
  if (enabled) fPlateStudy = false;
}

void DetectorConstruction::SetApplicatorIDcm(G4double idCm)
{
  const auto idMm = idCm * cm;

  if (std::abs(idMm - 100.0 * mm) < 1e-6 * mm ||
      std::abs(idMm - 50.0 * mm)  < 1e-6 * mm ||
      std::abs(idMm - 20.0 * mm)  < 1e-6 * mm) {
    fApplicatorIDmm = idMm;
    return;
  }

  G4cout << "[DetectorConstruction] Unsupported applicator ID="
         << idCm << " cm. Using default 10 cm." << G4endl;

  fApplicatorIDmm = 100.0 * mm;
}

G4VPhysicalVolume* DetectorConstruction::Construct()
{
  auto* nist = G4NistManager::Instance();
  auto* air = nist->FindOrBuildMaterial("G4_AIR");

  // ---------------------------------------------------------------------------
  // World
  // ---------------------------------------------------------------------------

  const auto worldSize = 2.0 * m;

  auto* solidWorld = new G4Box(
      "World",
      worldSize / 2,
      worldSize / 2,
      worldSize / 2);

  auto* logicWorld = new G4LogicalVolume(
      solidWorld,
      air,
      "WorldLV");

  auto* physWorld = new G4PVPlacement(
      nullptr,
      G4ThreeVector(),
      logicWorld,
      "WorldPV",
      nullptr,
      false,
      0,
      true);

  logicWorld->SetVisAttributes(G4VisAttributes::GetInvisible());

  // ---------------------------------------------------------------------------
  // Simplified water / metal / water plate study
  // ---------------------------------------------------------------------------
  //
  // This branch intentionally does not build the normal accelerator beamline.
  // It is intended for the idealized GPS source in macros/plate_study.mac.
  //
  // Geometry along z:
  //
  //   0 mm  to 16 mm : water
  //   16 mm to 22 mm : titanium
  //   22 mm to 80 mm : water
  //
  // Total phantom depth: 80 mm.
  //
 if (fPlateStudy || fWaterOnlyStudy) {
    auto* water = nist->FindOrBuildMaterial("G4_WATER");

    // The colleague specified only approximately 4.5 g/cm3.
    // Density alone does not completely define a material in Geant4.
    // G4_Ti is used here because the density of titanium is close to the
    // specified value. Replace this with the real plate material or alloy
    // when its composition is known.
    auto* titanium = nist->FindOrBuildMaterial("G4_Ti");

    const auto transverseSize = 160.0 * mm;
    const auto upstreamWaterThickness = 16.0 * mm;
    const auto plateThickness = 6.0 * mm;
    const auto downstreamWaterThickness = 58.0 * mm;

    auto placeLayer =
        [&](const G4String& name,
            G4Material* material,
            G4double frontZ,
            G4double thickness,
            const G4Colour& colour) {
          auto* solid = new G4Box(
              name + "Solid",
              transverseSize / 2,
              transverseSize / 2,
              thickness / 2);

          auto* logical = new G4LogicalVolume(
              solid,
              material,
              name + "LV");

          new G4PVPlacement(
              nullptr,
              G4ThreeVector(0.0, 0.0, frontZ + thickness / 2),
              logical,
              name + "PV",
              logicWorld,
              false,
              0,
              true);

          auto* vis = new G4VisAttributes(colour);
          vis->SetForceSolid(true);
          logical->SetVisAttributes(vis);
        };

    if (fWaterOnlyStudy) {
      placeLayer("ControlWater", water, 0.0*mm, 80.0*mm,
                 G4Colour(0.2, 0.6, 1.0, 0.35));
      G4cout << "[WaterOnlyStudy] homogeneous water 0-80 mm" << G4endl;
    } else {
      placeLayer("UpstreamWater", water, 0.0*mm, upstreamWaterThickness,
                 G4Colour(0.2, 0.6, 1.0, 0.35));
      placeLayer("TitaniumPlate", titanium, upstreamWaterThickness, plateThickness,
                 G4Colour(0.55, 0.55, 0.55));
      placeLayer("DownstreamWater", water,
                 upstreamWaterThickness + plateThickness,
                 downstreamWaterThickness, G4Colour(0.2, 0.6, 1.0, 0.35));
      G4cout << "[PlateStudy] water 0-16 mm; G4_Ti 16-22 mm; water 22-80 mm"
             << G4endl;
    }
    return physWorld;
  }

  // ---------------------------------------------------------------------------
  // Normal beamline geometry
  // ---------------------------------------------------------------------------

  const auto beam =
      BeamlineGeometry::BuildBeamline(logicWorld, fApplicatorIDmm);

  // Both STL applicators share this exit plane, so the water surface is flush
  // with the selected applicator and there is no air gap or volume overlap.
  const auto phantomFrontZ = beam.zAppExit;

  // Clinical water phantom: 300 x 300 x 300 mm3. Its upstream face remains
  // flush with the applicator exit/reference plane.
  const auto phantom = PhantomGeometry::BuildWaterBox(
      logicWorld,
      phantomFrontZ,
      300.0,
      300.0);

  G4cout << "========================================" << G4endl;
  G4cout << "Applicator exit Z = "
         << beam.zAppExit / mm << " mm" << G4endl;
  G4cout << "Phantom front Z   = "
         << phantomFrontZ / mm << " mm" << G4endl;
  G4cout << "Phantom center Z  = "
         << phantom.zPhantomCenter / mm << " mm" << G4endl;
  G4cout << "Transport scoring = virtual planes (no scoring volume)"
         << G4endl;
  G4cout << "========================================" << G4endl;

  return physWorld;
}
