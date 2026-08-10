// PrimaryGeneratorAction.cc
#include "PrimaryGeneratorAction.hh"
#include "G4GeneralParticleSource.hh"
#include "G4Event.hh"

PrimaryGeneratorAction::PrimaryGeneratorAction()
{
  fGPS = new G4GeneralParticleSource();
}

PrimaryGeneratorAction::~PrimaryGeneratorAction()
{
  delete fGPS;
}

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* evt)
{
  fGPS->GeneratePrimaryVertex(evt);
}
