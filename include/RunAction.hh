
// include/RunAction.hh
#pragma once
#include "G4UserRunAction.hh"
#include <string>

class RunAction : public G4UserRunAction {
public:
  RunAction();
  ~RunAction() override = default;

  void BeginOfRunAction(const G4Run*) override;
  void EndOfRunAction(const G4Run*) override;

  void SetOutPrefix(const std::string& p) { fPrefix = p; }
  const std::string& GetOutPrefix() const { return fPrefix; }

private:
  std::string fPrefix = "transport";
  std::string fRunLabel = "manual";
  bool fTransportScoringEnabled = false;
  double fNominalEnergyMeV = 0.0;
  double fSourceMeanMeV = 0.0;
  double fSourceSigmaMeV = 0.0;
  double fApplicatorCm = 0.0;
};
