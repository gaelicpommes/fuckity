#include "SteppingAction.hh"

#include "Analysis.hh"
#include "TrackInformation.hh"

#include "G4Electron.hh"
#include "G4Gamma.hh"
#include "G4Step.hh"
#include "G4StepPoint.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4Track.hh"
#include "G4VPhysicalVolume.hh"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace {
  const G4double kApplicatorEntranceScoringZ = 1.9 * mm;

  bool TransportScoringEnabled() {
    const char* value = std::getenv("FLASH_ENABLE_TRANSPORT_SCORING");
    return value && (std::string(value) == "1" ||
                     std::string(value) == "true" ||
                     std::string(value) == "TRUE");
  }

  double Clamp(double value, double low, double high) {
    return std::max(low, std::min(high, value));
  }

  bool IsApplicatorVolume(const G4VPhysicalVolume* volume) {
    if (!volume) return false;
    const auto& name = volume->GetName();
    return name == "Applicator10cmPV" ||
           name == "Applicator5cmPV" ||
           name == "Applicator2cmPV";
  }

  TrackInformation* GetOrCreateTrackInformation(G4Track* track) {
    auto* info = dynamic_cast<TrackInformation*>(track->GetUserInformation());
    if (!info) {
      info = new TrackInformation();
      track->SetUserInformation(info);
    }
    return info;
  }

  double PolarAngleDegrees(const G4ThreeVector& direction) {
    const double cosz = Clamp(direction.z(), -1.0, 1.0);
    return std::acos(cosz) * 180.0 / CLHEP::pi;
  }

  void ScoreCrossing(Analysis::Plane plane,
                     Analysis::Particle particle,
                     Analysis::ElectronClass electronClass,
                     const G4ThreeVector& position,
                     G4double kineticEnergy,
                     const G4ThreeVector& direction,
                     G4double weight) {
    const double radius =
        std::hypot(position.x(), position.y()) / mm;
    Analysis::Instance()->FillCrossing(
        plane,
        particle,
        electronClass,
        radius,
        kineticEnergy / MeV,
        PolarAngleDegrees(direction),
        weight);
  }
}

SteppingAction::SteppingAction()
    : fEnabled(TransportScoringEnabled()) {}

void SteppingAction::UserSteppingAction(const G4Step* step) {
  if (!fEnabled) return;
  if (!step) return;

  auto* track = step->GetTrack();
  if (!track) return;

  const auto* prePoint = step->GetPreStepPoint();
  const auto* postPoint = step->GetPostStepPoint();
  const auto* preVolume = prePoint->GetPhysicalVolume();
  const auto* postVolume = postPoint->GetPhysicalVolume();
  if (!preVolume || !postVolume) return;

  auto* info = GetOrCreateTrackInformation(track);
  if (IsApplicatorVolume(preVolume) || IsApplicatorVolume(postVolume)) {
    info->MarkApplicatorInteraction();
  }

  const auto* definition = track->GetDefinition();
  const bool isElectron = definition == G4Electron::ElectronDefinition();
  const bool isPhoton = definition == G4Gamma::GammaDefinition();
  if (!isElectron && !isPhoton) return;

  const auto particle = isElectron
      ? Analysis::Particle::Electron
      : Analysis::Particle::Photon;

  // Virtual plane 0.1 mm upstream of the applicator entrance at z=2 mm.
  // No physical scoring volume is inserted into the geometry.
  const auto& prePosition = prePoint->GetPosition();
  const auto& postPosition = postPoint->GetPosition();
  const double deltaZ = postPosition.z() - prePosition.z();
  const bool crossesApplicatorEntrance =
      !info->ApplicatorEntranceScored() &&
      deltaZ > 0.0 &&
      prePosition.z() < kApplicatorEntranceScoringZ &&
      postPosition.z() >= kApplicatorEntranceScoringZ;

  if (crossesApplicatorEntrance) {
    const double fraction =
        (kApplicatorEntranceScoringZ - prePosition.z()) / deltaZ;
    const auto position =
        prePosition + fraction * (postPosition - prePosition);
    const auto energy =
        prePoint->GetKineticEnergy() +
        fraction * (postPoint->GetKineticEnergy() -
                    prePoint->GetKineticEnergy());

    // The interaction that terminates a step occurs at the post-step point.
    // The pre-step direction therefore represents the direction at an
    // interior virtual-plane crossing.
    ScoreCrossing(
        Analysis::Plane::ApplicatorEntrance,
        particle,
        Analysis::ElectronClass::All,
        position,
        energy,
        prePoint->GetMomentumDirection(),
        track->GetWeight());
    info->MarkApplicatorEntranceScored();
  }

  // Score the first downstream boundary crossing into the water phantom.
  const bool entersWater =
      !info->WaterEntranceScored() &&
      preVolume->GetName() != "WaterPhantomPV" &&
      postVolume->GetName() == "WaterPhantomPV" &&
      postPoint->GetStepStatus() == fGeomBoundary &&
      postPoint->GetMomentumDirection().z() > 0.0;

  if (!entersWater) return;

  if (isElectron) {
    ScoreCrossing(
        Analysis::Plane::WaterEntrance,
        particle,
        Analysis::ElectronClass::All,
        postPoint->GetPosition(),
        postPoint->GetKineticEnergy(),
        postPoint->GetMomentumDirection(),
        track->GetWeight());

    const auto electronClass = info->InteractedWithApplicator()
        ? Analysis::ElectronClass::ApplicatorInteracting
        : Analysis::ElectronClass::Direct;
    ScoreCrossing(
        Analysis::Plane::WaterEntrance,
        particle,
        electronClass,
        postPoint->GetPosition(),
        postPoint->GetKineticEnergy(),
        postPoint->GetMomentumDirection(),
        track->GetWeight());
  } else {
    ScoreCrossing(
        Analysis::Plane::WaterEntrance,
        particle,
        Analysis::ElectronClass::All,
        postPoint->GetPosition(),
        postPoint->GetKineticEnergy(),
        postPoint->GetMomentumDirection(),
        track->GetWeight());
  }

  info->MarkWaterEntranceScored();
}
