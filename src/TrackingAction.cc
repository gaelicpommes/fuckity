#include "TrackingAction.hh"

#include "TrackInformation.hh"

#include "G4Track.hh"
#include "G4TrackingManager.hh"
#include "G4VPhysicalVolume.hh"

#include <cstdlib>
#include <string>

namespace {
  bool IsApplicatorVolume(const G4VPhysicalVolume* volume) {
    if (!volume) return false;
    const auto& name = volume->GetName();
    return name == "Applicator10cmPV" ||
           name == "Applicator5cmPV" ||
           name == "Applicator2cmPV";
  }

  bool TransportScoringEnabled() {
    const char* value = std::getenv("FLASH_ENABLE_TRANSPORT_SCORING");
    return value && (std::string(value) == "1" ||
                     std::string(value) == "true" ||
                     std::string(value) == "TRUE");
  }
}

TrackingAction::TrackingAction()
    : fEnabled(TransportScoringEnabled()) {}

void TrackingAction::PreUserTrackingAction(const G4Track* track) {
  if (!fEnabled) return;
  if (!track) return;

  auto* info = dynamic_cast<TrackInformation*>(track->GetUserInformation());
  if (!info) {
    info = new TrackInformation();
    fpTrackingManager->SetUserTrackInformation(info);
  }

  // This also tags electrons created as secondaries inside PMMA.
  if (IsApplicatorVolume(track->GetVolume())) {
    info->MarkApplicatorInteraction();
  }
}

void TrackingAction::PostUserTrackingAction(const G4Track* track) {
  if (!fEnabled) return;
  if (!track) return;

  const auto* parentInfo =
      dynamic_cast<const TrackInformation*>(track->GetUserInformation());
  const bool inheritedInteraction =
      parentInfo && parentInfo->InteractedWithApplicator();

  const auto* secondaries = fpTrackingManager->GimmeSecondaries();
  if (!secondaries) return;

  for (auto* secondary : *secondaries) {
    if (!secondary) continue;

    auto* childInfo =
        dynamic_cast<TrackInformation*>(secondary->GetUserInformation());
    if (!childInfo) {
      childInfo = new TrackInformation(inheritedInteraction);
      secondary->SetUserInformation(childInfo);
    } else if (inheritedInteraction) {
      childInfo->MarkApplicatorInteraction();
    }
  }
}
