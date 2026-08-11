// src/BeamlineGeometry.cc
#include "BeamlineGeometry.hh"

#include "G4Colour.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4RotationMatrix.hh"
#include "G4SystemOfUnits.hh"
#include "G4TessellatedSolid.hh"
#include "G4TriangularFacet.hh"
#include "G4Tubs.hh"
#include "G4VisAttributes.hh"
#include "G4ios.hh"
#include "G4Exception.hh"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
  struct Triangle {
    G4ThreeVector a;
    G4ThreeVector b;
    G4ThreeVector c;
  };

  struct Bounds {
    G4ThreeVector min{
      std::numeric_limits<G4double>::max(),
      std::numeric_limits<G4double>::max(),
      std::numeric_limits<G4double>::max()
    };
    G4ThreeVector max{
      -std::numeric_limits<G4double>::max(),
      -std::numeric_limits<G4double>::max(),
      -std::numeric_limits<G4double>::max()
    };

    void Include(const G4ThreeVector& p) {
      min.setX(std::min(min.x(), p.x()));
      min.setY(std::min(min.y(), p.y()));
      min.setZ(std::min(min.z(), p.z()));
      max.setX(std::max(max.x(), p.x()));
      max.setY(std::max(max.y(), p.y()));
      max.setZ(std::max(max.z(), p.z()));
    }
  };

  G4Colour TopasOrange() {
    return G4Colour(1.0, 0.5, 0.0);
  }

  G4Colour ApplicatorBlue() {
    // Bright enough to remain distinct against the dark visualization theme.
    return G4Colour(0.15, 0.45, 1.0);
  }

  void SetSolidVis(G4LogicalVolume* lv, const G4Colour& c) {
    auto vis = new G4VisAttributes(c);
    vis->SetForceSolid(true);
    lv->SetVisAttributes(vis);
  }

  std::filesystem::path ResolveSTLPath(const std::string& filename) {
    const std::array<std::filesystem::path, 5> candidates = {
      std::filesystem::path(filename),
      std::filesystem::path("../") / filename,
      std::filesystem::path("../../") / filename,
#ifdef PROJECT_SOURCE_DIR
      std::filesystem::path(PROJECT_SOURCE_DIR) / filename,
      std::filesystem::path(PROJECT_SOURCE_DIR) / "Input_Files" / filename
#else
      std::filesystem::path("Input_Files") / filename,
      std::filesystem::path("../Input_Files") / filename
#endif
    };

    for (const auto& p : candidates) {
      if (std::filesystem::exists(p)) {
        return p;
      }
    }

    G4ExceptionDescription msg;
    msg << "Could not find STL file '" << filename << "'. Searched current/build/source directories.";
    G4Exception("BeamlineGeometry::ResolveSTLPath", "FlashElectronSim001", FatalException, msg);
    return filename;
  }

  bool LooksLikeAsciiSTL(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::array<char, 512> buffer{};
    in.read(buffer.data(), buffer.size());
    const auto n = in.gcount();
    std::string header(buffer.data(), static_cast<std::size_t>(n));
    auto first = header.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || header.compare(first, 5, "solid") != 0) {
      return false;
    }
    return header.find("facet") != std::string::npos || header.find("vertex") != std::string::npos;
  }

  std::vector<Triangle> ReadBinarySTL(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      throw std::runtime_error("failed to open STL");
    }

    in.seekg(80);
    std::uint32_t nTriangles = 0;
    in.read(reinterpret_cast<char*>(&nTriangles), sizeof(nTriangles));

    std::vector<Triangle> triangles;
    triangles.reserve(nTriangles);

    for (std::uint32_t i = 0; i < nTriangles; ++i) {
      float values[12] = {};
      std::uint16_t attr = 0;
      in.read(reinterpret_cast<char*>(values), sizeof(values));
      in.read(reinterpret_cast<char*>(&attr), sizeof(attr));
      if (!in) {
        throw std::runtime_error("truncated binary STL");
      }

      triangles.push_back({
        G4ThreeVector(values[3]*mm, values[4]*mm, values[5]*mm),
        G4ThreeVector(values[6]*mm, values[7]*mm, values[8]*mm),
        G4ThreeVector(values[9]*mm, values[10]*mm, values[11]*mm)
      });
    }

    return triangles;
  }

  std::vector<Triangle> ReadAsciiSTL(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
      throw std::runtime_error("failed to open STL");
    }

    std::vector<G4ThreeVector> vertices;
    std::string word;
    while (in >> word) {
      std::transform(word.begin(), word.end(), word.begin(), [](unsigned char c) { return std::tolower(c); });
      if (word == "vertex") {
        double x = 0.0, y = 0.0, z = 0.0;
        in >> x >> y >> z;
        vertices.emplace_back(x*mm, y*mm, z*mm);
      }
    }

    if (vertices.size() % 3 != 0) {
      throw std::runtime_error("ASCII STL vertex count is not divisible by 3");
    }

    std::vector<Triangle> triangles;
    triangles.reserve(vertices.size()/3);
    for (std::size_t i = 0; i < vertices.size(); i += 3) {
      triangles.push_back({vertices[i], vertices[i + 1], vertices[i + 2]});
    }
    return triangles;
  }

  G4ThreeVector TopasToGeant4Point(const G4ThreeVector& p) {
    // TOPAS model beam axis is -Y. This Geant4 application uses +Z as beam axis.
    // Map: TOPAS X -> G4 X, TOPAS Z -> G4 Y, TOPAS -Y -> G4 Z.
    return G4ThreeVector(p.x(), p.z(), -p.y());
  }

  G4ThreeVector Applicator2cmToGeant4Point(const G4ThreeVector& p) {
    // The supplied 2 cm cone was exported with its beam axis along CAD Z
    // (unlike the TOPAS parts, whose beam axis is -Y).  Its wide upstream
    // face is at CAD Z=136 mm. The downstream face is at CAD Z=-286 mm;
    // its mesh radii are 20..35 mm (40 mm ID and 70 mm OD).
    // CAD Z=-286 mm. Its native axial length is therefore 422 mm, whereas
    // the reference 10 cm applicator spans 428 mm. Scale only the beam axis
    // by 428/422 so both supplied applicators share the exact same entrance
    // and exit planes; keep the STL's transverse dimensions unchanged.
    constexpr G4double nativeUpstreamZ = 136.0;
    constexpr G4double axialScale = 428.0 / 422.0;
    return G4ThreeVector(p.x(), p.y(), (nativeUpstreamZ - p.z()) * axialScale);
  }

  G4ThreeVector TopasToGeant4Translation(G4double xTopas, G4double yTopas, G4double zTopas) {
    return TopasToGeant4Point(G4ThreeVector(xTopas, yTopas, zTopas));
  }

  G4TessellatedSolid* LoadTopasSTLAsG4Solid(const G4String& solidName, const std::string& filename, Bounds& bounds) {
    const auto path = ResolveSTLPath(filename);
    std::vector<Triangle> triangles;
    try {
      triangles = LooksLikeAsciiSTL(path) ? ReadAsciiSTL(path) : ReadBinarySTL(path);
    } catch (const std::exception& e) {
      G4ExceptionDescription msg;
      msg << "Failed to parse STL file '" << path.string() << "': " << e.what();
      G4Exception("BeamlineGeometry::LoadTopasSTLAsG4Solid", "FlashElectronSim002", FatalException, msg);
    }

    auto solid = new G4TessellatedSolid(solidName);
    for (const auto& tri : triangles) {
      const auto a = TopasToGeant4Point(tri.a);
      const auto b = TopasToGeant4Point(tri.b);
      const auto c = TopasToGeant4Point(tri.c);
      bounds.Include(a);
      bounds.Include(b);
      bounds.Include(c);
      solid->AddFacet(new G4TriangularFacet(a, b, c, ABSOLUTE));
    }
    solid->SetSolidClosed(true);

    G4cout << "[Beamline CAD] Loaded " << filename << " as " << solidName
           << " with " << triangles.size() << " triangles" << G4endl;
    return solid;
  }

  G4TessellatedSolid* Load2cmApplicatorAsG4Solid(const G4String& solidName,
                                                 Bounds& bounds) {
    const std::string filename = "2cmapplicator-Cone.stl";
    const auto path = ResolveSTLPath(filename);
    std::vector<Triangle> triangles;
    try {
      triangles = LooksLikeAsciiSTL(path) ? ReadAsciiSTL(path) : ReadBinarySTL(path);
    } catch (const std::exception& e) {
      G4ExceptionDescription msg;
      msg << "Failed to parse STL file '" << path.string() << "': " << e.what();
      G4Exception("BeamlineGeometry::Load2cmApplicatorAsG4Solid",
                  "FlashElectronSim003", FatalException, msg);
    }

    auto solid = new G4TessellatedSolid(solidName);
    for (const auto& tri : triangles) {
      const auto a = Applicator2cmToGeant4Point(tri.a);
      const auto b = Applicator2cmToGeant4Point(tri.b);
      const auto c = Applicator2cmToGeant4Point(tri.c);
      bounds.Include(a);
      bounds.Include(b);
      bounds.Include(c);
      // Reversing one axis changes handedness, so swap two vertices to retain
      // the STL's outward facet orientation.
      solid->AddFacet(new G4TriangularFacet(a, c, b, ABSOLUTE));
    }
    solid->SetSolidClosed(true);
    G4cout << "[Beamline CAD] Loaded " << filename << " as " << solidName
           << " with " << triangles.size() << " triangles" << G4endl;
    return solid;
  }

  G4LogicalVolume* PlaceTopasCAD(G4LogicalVolume* worldLV,
                                 const G4String& name,
                                 const std::string& filename,
                                 G4Material* material,
                                 const G4ThreeVector& topasTranslation,
                                 const G4Colour& colour,
                                 Bounds& globalBounds) {
    Bounds localBounds;
    auto solid = LoadTopasSTLAsG4Solid(name + "Solid", filename, localBounds);
    auto logic = new G4LogicalVolume(solid, material, name + "LV");
    const auto translation = TopasToGeant4Translation(topasTranslation.x(), topasTranslation.y(), topasTranslation.z());
    new G4PVPlacement(nullptr, translation, logic, name + "PV", worldLV, false, 0, true);
    SetSolidVis(logic, colour);

    globalBounds.Include(localBounds.min + translation);
    globalBounds.Include(localBounds.max + translation);

    G4cout << "[Beamline CAD] Placed " << name << " at G4 (x,y,z)= "
           << translation.x()/mm << ", " << translation.y()/mm << ", " << translation.z()/mm
           << " mm" << G4endl;
    return logic;
  }
}

BeamlineHandles BeamlineGeometry::BuildBeamline(G4LogicalVolume* worldLV, G4double applicatorIDmm)
{
  BeamlineHandles h;
  auto nist = G4NistManager::Instance();
  auto al = nist->FindOrBuildMaterial("G4_Al");
  auto graphite = nist->FindOrBuildMaterial("G4_GRAPHITE");
  auto steel = nist->FindOrBuildMaterial("G4_STAINLESS-STEEL");
  auto plexiglass = nist->FindOrBuildMaterial("G4_PLEXIGLASS");
  auto air = nist->FindOrBuildMaterial("G4_AIR");

  // CAD placement copied from the supplied TOPAS parameter file, converted from
  // TOPAS beam axis (-Y) to this Geant4 application's beam axis (+Z).
  Bounds cadBounds;
  PlaceTopasCAD(worldLV, "UnionAl", "UnionAl.stl", al,
                G4ThreeVector(0.0*mm, 153.5*mm, -117.5*mm), TopasOrange(), cadBounds);
  PlaceTopasCAD(worldLV, "CAD4_bis", "CAD4_bis.stl", graphite,
                G4ThreeVector(0.0*mm, 64.9*mm, 0.0*mm), G4Colour::Gray(), cadBounds);
  PlaceTopasCAD(worldLV, "CAD6", "CAD6.stl", al,
                G4ThreeVector(0.0*mm, 153.5*mm, -117.5*mm), TopasOrange(), cadBounds);
  PlaceTopasCAD(worldLV, "CAD7", "CAD7.stl", steel,
                G4ThreeVector(0.0*mm, 153.48*mm, -117.5*mm), G4Colour::Gray(), cadBounds);
  PlaceTopasCAD(worldLV, "CAD11", "CAD11.stl", steel,
                G4ThreeVector(0.0*mm, 153.48*mm, -117.5*mm), G4Colour::Gray(), cadBounds);
  PlaceTopasCAD(worldLV, "CAD12", "CAD12.stl", al,
                G4ThreeVector(0.0*mm, 153.45*mm, -117.5*mm), TopasOrange(), cadBounds);

  // Keep the supplied STL applicators for the 10 cm and 2 cm options.  The
  // intermediate 5 cm option has no supplied mesh and remains an analytic tube.
  h.zAppEntrance = TopasToGeant4Translation(0.0*mm, -2.0*mm, 0.0*mm).z();
  h.zAppExit = TopasToGeant4Translation(0.0*mm, -430.0*mm, 0.0*mm).z();
  h.appLength = h.zAppExit - h.zAppEntrance;

  if (std::abs(applicatorIDmm - 20.0*mm) < 1e-6*mm) {
    Bounds localBounds;
    auto solid = Load2cmApplicatorAsG4Solid("Applicator2cmSolid", localBounds);
    auto logic = new G4LogicalVolume(solid, plexiglass, "Applicator2cmLV");

    // The transformed STL begins at local Z=0 and ends at local Z=428 mm.
    // Translating it by 2 mm matches the 10 cm applicator's Z=2..430 mm span.
    const G4ThreeVector translation(0.0, 0.0, 2.0*mm);
    new G4PVPlacement(nullptr, translation, logic, "Applicator2cmPV",
                      worldLV, false, 0, true);
    SetSolidVis(logic, ApplicatorBlue());
    h.applicatorLV = logic;
    // These radii come from the downstream face of the supplied STL. Axial
    // scaling does not modify X or Y, so its diameters remain unchanged.
    h.appInnerR = 20.0*mm;
    h.appOuterR = 65.0*mm;
    h.zAppExit = 430.0*mm;
    h.appLength = h.zAppExit - h.zAppEntrance;
    cadBounds.Include(localBounds.min + translation);
    cadBounds.Include(localBounds.max + translation);
  } else if (std::abs(applicatorIDmm - 50.0*mm) < 1e-6*mm) {
    G4double appID = 50.0*mm;
    G4double appOD = 65.0*mm;
    G4String appName = "Applicator5cm";

    h.appInnerR = appID/2.0;
    h.appOuterR = appOD/2.0;
    const auto appHalfZ = h.appLength/2.0;
    const auto appCenterZ = h.zAppEntrance + appHalfZ;
    auto appSolid = new G4Tubs(appName, h.appInnerR, h.appOuterR, appHalfZ, 0.*deg, 360.*deg);
    auto appLV = new G4LogicalVolume(appSolid, plexiglass, appName + "LV");
    new G4PVPlacement(nullptr, G4ThreeVector(0, 0, appCenterZ), appLV,
                      appName + "PV", worldLV, false, 0, true);
    SetSolidVis(appLV, ApplicatorBlue());

    h.applicatorLV = appLV;
    cadBounds.Include(G4ThreeVector(-h.appOuterR, -h.appOuterR, h.zAppEntrance));
    cadBounds.Include(G4ThreeVector( h.appOuterR,  h.appOuterR, h.zAppExit));
  } else {
    h.applicatorLV = PlaceTopasCAD(worldLV, "Applicator10cm", "Applicateaur100mmx428mm.stl", plexiglass,
                                   G4ThreeVector(0.0*mm, -68.0*mm, 0.0*mm), ApplicatorBlue(), cadBounds);
    h.appInnerR = 50.0*mm;
    h.appOuterR = 58.0*mm;
  }

  // From the TOPAS 10 cm applicator STL bounds and placement: local Y spans
  // -362..66 mm with TransY=-68 mm, so the entrance/exit faces are TOPAS
  // Y=-2/-430 mm. Under the mapping G4 Z=-TOPAS Y, all applicator options
  // span z=2..430 mm. The 2 cm STL's native 422 mm axial span is scaled to
  // 428 mm so it also ends flush with the phantom at z=430 mm.
  h.zWindow = TopasToGeant4Translation(0.0*mm, 121.65*mm, 0.0*mm).z();

  // Helper volume used only by /gps/pos/confine SourceCutoffPV in the
  // legacy Gaussian source macros. It reproduces the old hard 2.2 mm
  // source-plane cutoff while keeping the physical beamline CAD unchanged.
  auto sourceCutoffSolid = new G4Tubs("SourceCutoff", 0.0, 2.2*mm, 0.05*mm, 0.*deg, 360.*deg);
  auto sourceCutoffLV = new G4LogicalVolume(sourceCutoffSolid, air, "SourceCutoffLV");
  new G4PVPlacement(nullptr, G4ThreeVector(0, 0, h.zWindow), sourceCutoffLV,
                    "SourceCutoffPV", worldLV, false, 0, false);
  sourceCutoffLV->SetVisAttributes(G4VisAttributes::GetInvisible());

  G4cout << "[Beamline CAD] Overall G4 bounds min/max (mm): ("
         << cadBounds.min.x()/mm << ", " << cadBounds.min.y()/mm << ", " << cadBounds.min.z()/mm
         << ") / (" << cadBounds.max.x()/mm << ", " << cadBounds.max.y()/mm << ", "
         << cadBounds.max.z()/mm << ")" << G4endl;
  G4cout << "[Beamline CAD] Source plane Z = " << h.zWindow/mm << " mm" << G4endl;
  G4cout << "[Beamline CAD] Applicator entrance/exit Z = "
         << h.zAppEntrance/mm << " / " << h.zAppExit/mm << " mm" << G4endl;

  return h;
}
