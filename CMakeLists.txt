cmake_minimum_required(VERSION 3.16...3.27)
project(FlashElectronSim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Geant4 with UI + Vis
find_package(Geant4 REQUIRED ui_all vis_all)
include(${Geant4_USE_FILE})

# Collect sources/headers. Keep the source list explicit so local backup or
# scratch files such as BeamlineGeometry_hybrid.cc are not linked into the
# executable and do not duplicate BeamlineGeometry::BuildBeamline.
set(sources
  ${PROJECT_SOURCE_DIR}/src/ActionInitialization.cc
  ${PROJECT_SOURCE_DIR}/src/Analysis.cc
  ${PROJECT_SOURCE_DIR}/src/BeamlineGeometry.cc
  ${PROJECT_SOURCE_DIR}/src/DetectorConstruction.cc
  ${PROJECT_SOURCE_DIR}/src/PhantomGeometry.cc
  ${PROJECT_SOURCE_DIR}/src/PrimaryGeneratorAction.cc
  ${PROJECT_SOURCE_DIR}/src/RunAction.cc
  ${PROJECT_SOURCE_DIR}/src/SteppingAction.cc
  ${PROJECT_SOURCE_DIR}/src/TrackingAction.cc
  ${PROJECT_SOURCE_DIR}/src/main.cc
)

file(GLOB headers CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/include/*.hh)

add_executable(FlashElectronSim ${sources} ${headers})

# Include path
target_include_directories(FlashElectronSim PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(FlashElectronSim PRIVATE PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# Link Geant4
target_link_libraries(FlashElectronSim ${Geant4_LIBRARIES})

# Convenience lowercase launcher name for users who type ./flashelectronsim.
add_custom_command(TARGET FlashElectronSim POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          $<TARGET_FILE_NAME:FlashElectronSim>
          ${PROJECT_BINARY_DIR}/flashelectronsim)

# ---- Copy macros into build/macros ----
file(GLOB MACRO_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/macros/*.mac")

file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/macros)

foreach(_file ${MACRO_FILES})
  get_filename_component(_fname ${_file} NAME)
  configure_file(${_file} ${PROJECT_BINARY_DIR}/macros/${_fname} COPYONLY)
endforeach()


foreach(_script run_plate_study.sh run_plate_comparison.sh)
  if(EXISTS "${PROJECT_SOURCE_DIR}/${_script}")
    file(COPY "${PROJECT_SOURCE_DIR}/${_script}"
         DESTINATION ${PROJECT_BINARY_DIR}
         FILE_PERMISSIONS
           OWNER_READ OWNER_WRITE OWNER_EXECUTE
           GROUP_READ GROUP_EXECUTE
           WORLD_READ WORLD_EXECUTE)
  endif()
endforeach()
       
       

if(EXISTS "${PROJECT_SOURCE_DIR}/plate_study_analysis.ipynb")
  configure_file(${PROJECT_SOURCE_DIR}/plate_study_analysis.ipynb
                 ${PROJECT_BINARY_DIR}/plate_study_analysis.ipynb COPYONLY)
else()
  message(STATUS
          "Optional plate_study_analysis.ipynb not found; skipping notebook copy")
endif()


# ---- Copy TOPAS CAD STL files into the build directory ----
file(GLOB STL_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/*.stl" "${PROJECT_SOURCE_DIR}/*.STL")

foreach(_file ${STL_FILES})
  get_filename_component(_fname ${_file} NAME)
  configure_file(${_file} ${PROJECT_BINARY_DIR}/${_fname} COPYONLY)
endforeach()
