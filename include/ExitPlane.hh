// include/ExitPlane.hh
#pragma once

#include "globals.hh"

class G4LogicalVolume;

struct ExitPlaneHandles {
  G4double zCenter = 0.0;
  G4LogicalVolume* planeLV = nullptr;
};

class ExitPlaneGeometry {
public:
  // Defaults in mm (plain numbers), converted in ExitPlane.cc using *mm
  static ExitPlaneHandles BuildExitPlane(
      G4LogicalVolume* worldLV,
      G4double zCenter,
      G4double radius_mm = 60.0,
      G4double thickness_mm = 0.1
  );
};