#pragma once

#include "G4UserTrackingAction.hh"

class TrackingAction : public G4UserTrackingAction {
public:
  TrackingAction();
  ~TrackingAction() override = default;

  void PreUserTrackingAction(const G4Track* track) override;
  void PostUserTrackingAction(const G4Track* track) override;

private:
  bool fEnabled = false;
};
