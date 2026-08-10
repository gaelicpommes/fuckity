#include "Analysis.hh"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>

namespace {
  struct GroupDescriptor {
    const char* plane;
    const char* particle;
    const char* category;
  };

  constexpr std::array<GroupDescriptor, 6> kGroupDescriptors{{
      {"applicator_entrance", "electron", "all"},
      {"applicator_entrance", "photon", "all"},
      {"water_entrance", "electron", "all"},
      {"water_entrance", "electron", "direct"},
      {"water_entrance", "electron", "applicator_interacting"},
      {"water_entrance", "photon", "all"}
  }};

  std::string CsvEscape(const std::string& value) {
    std::string escaped = "\"";
    for (const char c : value) {
      if (c == '\"') escaped += '\"';
      escaped += c;
    }
    escaped += "\"";
    return escaped;
  }

  double WeightedMean(double sum, double sumWeight) {
    return sumWeight > 0.0
        ? sum / sumWeight
        : std::numeric_limits<double>::quiet_NaN();
  }

  double WeightedSD(double sum,
                    double sum2,
                    double sumWeight) {
    if (sumWeight <= 0.0) {
      return std::numeric_limits<double>::quiet_NaN();
    }

    const double mean = sum / sumWeight;
    const double variance = std::max(0.0, sum2 / sumWeight - mean * mean);
    return std::sqrt(variance);
  }
}

thread_local Analysis::GroupArray Analysis::fThreadGroups{};
thread_local bool Analysis::fThreadReady = false;

Analysis* Analysis::Instance() {
  static Analysis instance;
  return &instance;
}

void Analysis::Configure(double rMax_mm, int nR,
                         double eMax_MeV, int nE,
                         double thMax_deg, int nTh) {
  std::lock_guard<std::mutex> lock(fMutex);
  fRmax = rMax_mm;
  fNR = nR;
  fEmax = eMax_MeV;
  fNE = nE;
  fThmax = thMax_deg;
  fNTheta = nTh;
}

void Analysis::ResetGroups(GroupArray& groups) const {
  for (auto& group : groups) {
    group = GroupData{};
    group.hR.assign(fNR, 0.0);
    group.hE.assign(fNE, 0.0);
    group.hTheta.assign(fNTheta, 0.0);
  }
}

void Analysis::ResetMerged() {
  std::lock_guard<std::mutex> lock(fMutex);
  ResetGroups(fMergedGroups);
}

void Analysis::ResetThreadLocal() {
  std::lock_guard<std::mutex> lock(fMutex);
  ResetGroups(fThreadGroups);
  fThreadReady = true;
}

int Analysis::BinIndex(double x, double xmax, int n) const {
  if (x < 0.0 || x >= xmax) return -1;
  const int index = static_cast<int>(std::floor((x / xmax) * n));
  return (index >= 0 && index < n) ? index : -1;
}

std::size_t Analysis::GroupIndex(Plane plane,
                                 Particle particle,
                                 ElectronClass electronClass) const {
  if (plane == Plane::ApplicatorEntrance) {
    return particle == Particle::Electron ? 0 : 1;
  }

  if (particle == Particle::Photon) return 5;

  switch (electronClass) {
    case ElectronClass::All:
      return 2;
    case ElectronClass::Direct:
      return 3;
    case ElectronClass::ApplicatorInteracting:
      return 4;
  }

  return kGroupCount;
}

void Analysis::FillCrossing(Plane plane,
                            Particle particle,
                            ElectronClass electronClass,
                            double r_mm,
                            double e_MeV,
                            double theta_deg,
                            double weight) {
  if (!fThreadReady) ResetThreadLocal();
  if (weight <= 0.0) return;

  const auto groupIndex = GroupIndex(plane, particle, electronClass);
  if (groupIndex >= kGroupCount) return;

  auto& group = fThreadGroups[groupIndex];
  ++group.unweightedCrossings;
  group.weightedCrossings += weight;
  group.sumEnergy += weight * e_MeV;
  group.sumEnergy2 += weight * e_MeV * e_MeV;
  group.sumTheta += weight * theta_deg;
  group.sumTheta2 += weight * theta_deg * theta_deg;
  group.sumRadius += weight * r_mm;
  group.sumRadius2 += weight * r_mm * r_mm;

  const int rBin = BinIndex(r_mm, fRmax, fNR);
  const int eBin = BinIndex(e_MeV, fEmax, fNE);
  const int thetaBin = BinIndex(theta_deg, fThmax, fNTheta);

  if (rBin >= 0) group.hR[rBin] += weight;
  else group.radiusOverflow += weight;

  if (eBin >= 0) group.hE[eBin] += weight;
  else group.energyOverflow += weight;

  if (thetaBin >= 0) group.hTheta[thetaBin] += weight;
  else group.thetaOverflow += weight;
}

void Analysis::MergeThreadLocal() {
  if (!fThreadReady) return;

  std::lock_guard<std::mutex> lock(fMutex);
  for (std::size_t i = 0; i < kGroupCount; ++i) {
    auto& merged = fMergedGroups[i];
    const auto& local = fThreadGroups[i];

    merged.unweightedCrossings += local.unweightedCrossings;
    merged.weightedCrossings += local.weightedCrossings;
    merged.sumEnergy += local.sumEnergy;
    merged.sumEnergy2 += local.sumEnergy2;
    merged.sumTheta += local.sumTheta;
    merged.sumTheta2 += local.sumTheta2;
    merged.sumRadius += local.sumRadius;
    merged.sumRadius2 += local.sumRadius2;
    merged.radiusOverflow += local.radiusOverflow;
    merged.energyOverflow += local.energyOverflow;
    merged.thetaOverflow += local.thetaOverflow;

    for (int j = 0; j < fNR; ++j) merged.hR[j] += local.hR[j];
    for (int j = 0; j < fNE; ++j) merged.hE[j] += local.hE[j];
    for (int j = 0; j < fNTheta; ++j) {
      merged.hTheta[j] += local.hTheta[j];
    }
  }

  fThreadReady = false;
}

void Analysis::WriteCSV(const std::string& outPrefix,
                        std::int64_t primaryHistories,
                        int runID,
                        const std::string& runLabel,
                        double nominalEnergy_MeV,
                        double sourceMean_MeV,
                        double sourceSigma_MeV,
                        double applicator_cm) const {
  std::lock_guard<std::mutex> lock(fMutex);

  const std::filesystem::path prefixPath(outPrefix);
  if (!prefixPath.parent_path().empty()) {
    std::filesystem::create_directories(prefixPath.parent_path());
  }

  const auto summaryPath = outPrefix + "_transport_summary.csv";
  std::ofstream summary(summaryPath, std::ios::trunc);
  if (!summary) {
    throw std::runtime_error("Could not open transport summary: " + summaryPath);
  }

  summary << std::setprecision(12);
  summary
      << "run_label,run_id,nominal_energy_MeV,source_mean_MeV,"
      << "source_sigma_MeV,applicator_cm,primary_histories,plane,particle,"
      << "category,unweighted_crossings,weighted_crossings,"
      << "crossings_per_primary,kinetic_energy_sum_MeV,"
      << "energy_per_primary_MeV,mean_energy_MeV,sd_energy_MeV,"
      << "mean_theta_deg,sd_theta_deg,mean_radius_mm,sd_radius_mm,"
      << "radius_hist_overflow_weight,energy_hist_overflow_weight,"
      << "theta_hist_overflow_weight\n";

  const double nPrimary = static_cast<double>(primaryHistories);
  for (std::size_t i = 0; i < kGroupCount; ++i) {
    const auto& descriptor = kGroupDescriptors[i];
    const auto& group = fMergedGroups[i];
    const double crossingsPerPrimary = nPrimary > 0.0
        ? group.weightedCrossings / nPrimary
        : std::numeric_limits<double>::quiet_NaN();
    const double energyPerPrimary = nPrimary > 0.0
        ? group.sumEnergy / nPrimary
        : std::numeric_limits<double>::quiet_NaN();

    summary
        << CsvEscape(runLabel) << ','
        << runID << ','
        << nominalEnergy_MeV << ','
        << sourceMean_MeV << ','
        << sourceSigma_MeV << ','
        << applicator_cm << ','
        << primaryHistories << ','
        << descriptor.plane << ','
        << descriptor.particle << ','
        << descriptor.category << ','
        << group.unweightedCrossings << ','
        << group.weightedCrossings << ','
        << crossingsPerPrimary << ','
        << group.sumEnergy << ','
        << energyPerPrimary << ','
        << WeightedMean(group.sumEnergy, group.weightedCrossings) << ','
        << WeightedSD(group.sumEnergy, group.sumEnergy2,
                      group.weightedCrossings) << ','
        << WeightedMean(group.sumTheta, group.weightedCrossings) << ','
        << WeightedSD(group.sumTheta, group.sumTheta2,
                      group.weightedCrossings) << ','
        << WeightedMean(group.sumRadius, group.weightedCrossings) << ','
        << WeightedSD(group.sumRadius, group.sumRadius2,
                      group.weightedCrossings) << ','
        << group.radiusOverflow << ','
        << group.energyOverflow << ','
        << group.thetaOverflow << '\n';
  }

  const auto histogramPath = outPrefix + "_transport_histograms.csv";
  std::ofstream histograms(histogramPath, std::ios::trunc);
  if (!histograms) {
    throw std::runtime_error("Could not open transport histograms: " +
                             histogramPath);
  }

  histograms << std::setprecision(12);
  histograms
      << "run_label,plane,particle,category,quantity,bin_low,bin_high,"
      << "bin_center,weighted_count,count_per_primary\n";

  const auto writeDistribution = [&](const GroupDescriptor& descriptor,
                                     const char* quantity,
                                     const std::vector<double>& values,
                                     double maximum) {
    const double width = maximum / static_cast<double>(values.size());
    for (std::size_t bin = 0; bin < values.size(); ++bin) {
      const double low = static_cast<double>(bin) * width;
      const double high = low + width;
      const double perPrimary = nPrimary > 0.0
          ? values[bin] / nPrimary
          : std::numeric_limits<double>::quiet_NaN();

      histograms
          << CsvEscape(runLabel) << ','
          << descriptor.plane << ','
          << descriptor.particle << ','
          << descriptor.category << ','
          << quantity << ','
          << low << ','
          << high << ','
          << 0.5 * (low + high) << ','
          << values[bin] << ','
          << perPrimary << '\n';
    }
  };

  for (std::size_t i = 0; i < kGroupCount; ++i) {
    const auto& descriptor = kGroupDescriptors[i];
    const auto& group = fMergedGroups[i];
    writeDistribution(descriptor, "radius_mm", group.hR, fRmax);
    writeDistribution(descriptor, "kinetic_energy_MeV", group.hE, fEmax);
    writeDistribution(descriptor, "theta_deg", group.hTheta, fThmax);
  }
}
