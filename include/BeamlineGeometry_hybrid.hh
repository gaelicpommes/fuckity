// include/BeamlineGeometry.hh
#pragma once

#include "globals.hh"

class G4LogicalVolume;

struct BeamlineHandles {
  // Global z landmarks. This Geant4 model uses +Z as the downstream beam axis.
  G4double zWindow = 0.0;
  G4double zFoil1  = 0.0;
  G4double zFoil4  = 0.0;

  // Kept for compatibility with existing DetectorConstruction logic.
  G4double zCollCenter  = 0.0;
  G4double zCollExit    = 0.0;

  // STL PMMA applicator landmarks.
  G4double zAppEntrance = 0.0;
  G4double zAppExit     = 0.0;
  G4double appInnerR    = 0.0;
  G4double appOuterR    = 0.0;
  G4double appLength    = 0.0;

  G4LogicalVolume* applicatorLV = nullptr;
};

class BeamlineGeometry {
public:
  // Builds the hybrid beamline:
  //   source/eWindow reference plane
  //   -> 4 aluminium foils
  //   -> imported TOPAS STL components:
  //      UnionAl, CAD4_bis, CAD6, CAD7, CAD11, CAD12,
  //      Applicateaur100mmx428mm
  //
  // applicatorIDmm is kept to preserve the old interface, but the imported STL
  // applicator is the 100 mm TOPAS applicator.
  static BeamlineHandles BuildBeamline(G4LogicalVolume* worldLV, G4double applicatorIDmm);
};
