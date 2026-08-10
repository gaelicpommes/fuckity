// include/DetectorConstruction.hh
#pragma once

#include "G4VUserDetectorConstruction.hh"
#include "globals.hh"

#include <memory>

class G4VPhysicalVolume;
class G4GenericMessenger;

class DetectorConstruction : public G4VUserDetectorConstruction {
public:
  DetectorConstruction();
  ~DetectorConstruction() override = default;

  G4VPhysicalVolume* Construct() override;



private:
  void SetApplicatorIDcm(G4double idCm);
  void SetPlateStudy(G4bool enabled);
  void SetWaterOnlyStudy(G4bool enabled);

  G4double fApplicatorIDmm = 100.0;
  G4bool fPlateStudy = false;
  G4bool fWaterOnlyStudy = false;

  std::unique_ptr<G4GenericMessenger> fMessenger;
};
