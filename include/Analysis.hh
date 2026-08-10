#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class Analysis {
public:
  enum class Plane {
    ApplicatorEntrance,
    WaterEntrance
  };

  enum class Particle {
    Electron,
    Photon
  };

  enum class ElectronClass {
    All,
    Direct,
    ApplicatorInteracting
  };

  static Analysis* Instance();

  void Configure(double rMax_mm, int nR,
                 double eMax_MeV, int nE,
                 double thMax_deg, int nTh);

  // The merged data are shared by all Geant4 worker threads. Each worker fills
  // its own thread-local copy and merges once at the end of the run.
  void ResetMerged();
  void ResetThreadLocal();
  void MergeThreadLocal();

  void FillCrossing(Plane plane,
                    Particle particle,
                    ElectronClass electronClass,
                    double r_mm,
                    double e_MeV,
                    double theta_deg,
                    double weight);

  void WriteCSV(const std::string& outPrefix,
                std::int64_t primaryHistories,
                int runID,
                const std::string& runLabel,
                double nominalEnergy_MeV,
                double sourceMean_MeV,
                double sourceSigma_MeV,
                double applicator_cm) const;

private:
  Analysis() = default;

  struct GroupData {
    std::vector<double> hR;
    std::vector<double> hE;
    std::vector<double> hTheta;

    std::uint64_t unweightedCrossings = 0;
    double weightedCrossings = 0.0;
    double sumEnergy = 0.0;
    double sumEnergy2 = 0.0;
    double sumTheta = 0.0;
    double sumTheta2 = 0.0;
    double sumRadius = 0.0;
    double sumRadius2 = 0.0;
    double radiusOverflow = 0.0;
    double energyOverflow = 0.0;
    double thetaOverflow = 0.0;
  };

  static constexpr std::size_t kGroupCount = 6;
  using GroupArray = std::array<GroupData, kGroupCount>;

  static thread_local GroupArray fThreadGroups;
  static thread_local bool fThreadReady;

  mutable std::mutex fMutex;
  GroupArray fMergedGroups;

  double fRmax = 120.0;
  int fNR = 240;
  double fEmax = 20.0;
  int fNE = 400;
  double fThmax = 90.0;
  int fNTheta = 360;

  int BinIndex(double x, double xmax, int n) const;
  std::size_t GroupIndex(Plane plane,
                         Particle particle,
                         ElectronClass electronClass) const;
  void ResetGroups(GroupArray& groups) const;
};
