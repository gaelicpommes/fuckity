//ActionInitialization.cc
#include "ActionInitialization.hh"

#include "PrimaryGeneratorAction.hh"
#include "G4Threading.hh"
#include "SteppingAction.hh"
#include "RunAction.hh"

void ActionInitialization::Build() const
{
  // Primary generator must be set here for MT
  SetUserAction(new PrimaryGeneratorAction());
  SetUserAction(new SteppingAction());
  SetUserAction(new RunAction());

  // Later you'll add:
  // SetUserAction(new RunAction());
  // SetUserAction(new EventAction());
  // SetUserAction(new SteppingAction());
}

void ActionInitialization::BuildForMaster() const
{
  SetUserAction(new RunAction());
}
