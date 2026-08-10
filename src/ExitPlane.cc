// src/ExitPlane.cc
#include "ExitPlane.hh"

#include "G4NistManager.hh"
#include "G4Tubs.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"

namespace {
  void SetSolidVis(G4LogicalVolume* lv, const G4Colour& c) {
    auto vis = new G4VisAttributes(c);
    vis->SetForceSolid(true);
    lv->SetVisAttributes(vis);
  }
}

ExitPlaneHandles ExitPlaneGeometry::BuildExitPlane(G4LogicalVolume* worldLV,
                                                   G4double zCenter,
                                                   G4double radius,
                                                   G4double thickness)
{
  ExitPlaneHandles h;
  h.zCenter = zCenter;

  auto nist = G4NistManager::Instance();
  auto air  = nist->FindOrBuildMaterial("G4_AIR");

  auto solid = new G4Tubs("ExitPlane",
                          0.0*mm,
                          radius,
                          thickness/2.0,
                          0.*deg, 360.*deg);

  auto logic = new G4LogicalVolume(solid, air, "ExitPlaneLV");
  new G4PVPlacement(nullptr, G4ThreeVector(0,0,zCenter), logic,
                    "ExitPlanePV", worldLV, false, 0, true);

  SetSolidVis(logic, G4Colour::Red());
  h.planeLV = logic;
  return h;
}