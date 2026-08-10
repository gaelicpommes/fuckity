//ActionInitialization.cc
#include "ActionInitialization.hh"

#include "PrimaryGeneratorAction.hh"
#include "G4Threading.hh"
#include "SteppingAction.hh"
#include "TrackingAction.hh"
#include "RunAction.hh"

void ActionInitialization::Build() const
{
  // Primary generator must be set here for MT
  SetUserAction(new PrimaryGeneratorAction());
  SetUserAction(new TrackingAction());
  SetUserAction(new SteppingAction());
  SetUserAction(new RunAction());
}

void ActionInitialization::BuildForMaster() const
{
  SetUserAction(new RunAction());
}
