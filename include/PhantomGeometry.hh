#pragma once

#include "globals.hh"

class G4LogicalVolume;

struct PhantomHandles {
  G4double zPhantomFront = 0.0;
  G4double zPhantomCenter = 0.0;
  G4double thicknessZ = 0.0;
  G4LogicalVolume* phantomLV = nullptr;
};

class PhantomGeometry {
public:
  // NOTE: Default sizes are given in mm to avoid unit symbols in headers.
  static PhantomHandles BuildWaterBox(
      G4LogicalVolume* worldLV,
      G4double zFrontFace,
      G4double sizeXY = 300.0,  // mm (30 cm)
      G4double sizeZ  = 300.0   // mm (30 cm)
  );
};