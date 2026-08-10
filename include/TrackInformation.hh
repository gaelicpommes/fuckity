#pragma once

#include "G4VUserTrackInformation.hh"

class TrackInformation : public G4VUserTrackInformation {
public:
  explicit TrackInformation(bool interactedWithApplicator = false)
      : fInteractedWithApplicator(interactedWithApplicator) {}

  ~TrackInformation() override = default;

  void Print() const override {}

  void MarkApplicatorInteraction() { fInteractedWithApplicator = true; }
  bool InteractedWithApplicator() const {
    return fInteractedWithApplicator;
  }

  void MarkApplicatorEntranceScored() { fApplicatorEntranceScored = true; }
  bool ApplicatorEntranceScored() const { return fApplicatorEntranceScored; }

  void MarkWaterEntranceScored() { fWaterEntranceScored = true; }
  bool WaterEntranceScored() const { return fWaterEntranceScored; }

private:
  bool fInteractedWithApplicator = false;
  bool fApplicatorEntranceScored = false;
  bool fWaterEntranceScored = false;
};
