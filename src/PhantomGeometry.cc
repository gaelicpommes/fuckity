#include "PhantomGeometry.hh"

#include "G4NistManager.hh"
#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"

namespace {
  void SetWireVis(G4LogicalVolume* lv, const G4Colour& c) {
    auto vis = new G4VisAttributes(c);
    vis->SetForceWireframe(true);
    lv->SetVisAttributes(vis);
  }
}

PhantomHandles PhantomGeometry::BuildWaterBox(G4LogicalVolume* worldLV,
                                             G4double zFrontFace,
                                             G4double sizeXY,
                                             G4double sizeZ)
{
  PhantomHandles h;

  const auto sizeXY_mm = sizeXY * mm;
  const auto sizeZ_mm  = sizeZ  * mm;

  h.zPhantomFront  = zFrontFace;
  h.thicknessZ     = sizeZ_mm;
  h.zPhantomCenter = zFrontFace + sizeZ_mm/2;

  auto nist = G4NistManager::Instance();
  auto water = nist->FindOrBuildMaterial("G4_WATER");

  auto solid = new G4Box("WaterPhantom", sizeXY_mm/2, sizeXY_mm/2, sizeZ_mm/2);
  auto logic = new G4LogicalVolume(solid, water, "WaterPhantomLV");
  new G4PVPlacement(nullptr, G4ThreeVector(0,0,h.zPhantomCenter), logic,
                    "WaterPhantomPV", worldLV, false, 0, true);

  SetWireVis(logic, G4Colour::Cyan());

  h.phantomLV = logic;
  return h;
}