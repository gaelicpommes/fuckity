#include "RunAction.hh"

#include "Analysis.hh"

#include "G4Run.hh"
#include "G4ios.hh"

#include <cstdint>
#include <cstdlib>
#include <string>

namespace {
  std::string EnvironmentString(const char* name,
                                const std::string& fallback) {
    const char* value = std::getenv(name);
    return value && *value ? std::string(value) : fallback;
  }

  double EnvironmentDouble(const char* name, double fallback) {
    const char* value = std::getenv(name);
    if (!value || !*value) return fallback;

    char* end = nullptr;
    const double parsed = std::strtod(value, &end);
    return end != value && *end == '\0' ? parsed : fallback;
  }

  bool EnvironmentFlag(const char* name) {
    const auto value = EnvironmentString(name, "0");
    return value == "1" || value == "true" || value == "TRUE";
  }
}

RunAction::RunAction()
    : fPrefix(EnvironmentString("FLASH_OUTPUT_PREFIX", "transport")),
      fRunLabel(EnvironmentString("FLASH_RUN_LABEL", "manual")),
      fTransportScoringEnabled(
          EnvironmentFlag("FLASH_ENABLE_TRANSPORT_SCORING")),
      fNominalEnergyMeV(
          EnvironmentDouble("FLASH_NOMINAL_ENERGY_MEV", 0.0)),
      fSourceMeanMeV(
          EnvironmentDouble("FLASH_SOURCE_MEAN_MEV", 0.0)),
      fSourceSigmaMeV(
          EnvironmentDouble("FLASH_SOURCE_SIGMA_MEV", 0.0)),
      fApplicatorCm(
          EnvironmentDouble("FLASH_APPLICATOR_CM", 0.0)) {}

void RunAction::BeginOfRunAction(const G4Run*) {
  if (!fTransportScoringEnabled) return;

  auto* analysis = Analysis::Instance();

  if (IsMaster()) {
    analysis->Configure(
        /*rMax_mm*/ 120.0, /*nR*/ 240,
        /*eMax_MeV*/ 20.0, /*nE*/ 400,
        /*thMax_deg*/ 90.0, /*nTh*/ 360);
    analysis->ResetMerged();
  }

  // In an MT run this initializes one array per worker. In a sequential run it
  // initializes the only array. The MT master array remains empty.
  analysis->ResetThreadLocal();
}

void RunAction::EndOfRunAction(const G4Run* run) {
  if (!fTransportScoringEnabled) return;

  auto* analysis = Analysis::Instance();
  analysis->MergeThreadLocal();

  if (!IsMaster()) return;

  const int runID = run ? run->GetRunID() : 0;
  const auto primaryHistories = run
      ? static_cast<std::int64_t>(run->GetNumberOfEvent())
      : std::int64_t{0};
  const std::string out = fPrefix + "_run" + std::to_string(runID);

  analysis->WriteCSV(
      out,
      primaryHistories,
      runID,
      fRunLabel,
      fNominalEnergyMeV,
      fSourceMeanMeV,
      fSourceSigmaMeV,
      fApplicatorCm);

  G4cout << "[Analysis] Wrote transport summary and histograms with prefix: "
         << out << G4endl;
}
