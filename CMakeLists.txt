# to avoid the annoying cmake warning
cmake_minimum_required(VERSION 2.8)

PROJECT(EspinaVolumeEditor)

# use what QVTK built with
FIND_PACKAGE(Qt4 REQUIRED)
INCLUDE(${QT_USE_FILE})
ADD_DEFINITIONS(${QT_DEFINITIONS})
IF(QT_USE_FILE)
  INCLUDE(${QT_USE_FILE})
ELSE(QT_USE_FILE)
  SET(QT_LIBRARIES   ${QT_QT_LIBRARY})
ENDIF(QT_USE_FILE)

# Use the include path and library for Qt that is used by VTK.
INCLUDE_DIRECTORIES(
  ${QT_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

SET(QtSrcs QtGui.cxx QtPreferences.cxx QtRelabel.cxx QtAbout.cxx QtColorPicker.cxx)
SET(UIS QtGui.ui QtPreferences.ui QtRelabel.ui QtAbout.ui QtColorPicker.ui)
SET (QtHeaders QtGui.h QtPreferences.h QtRelabel.h QtAbout.h QtColorPicker.h)
SET (QtResources editor.qrc about.qrc colorpicker.qrc)
QT4_WRAP_UI(UIHeaders ${UIS})
QT4_WRAP_CPP(MOCSrcs ${QtHeaders} )
QT4_ADD_RESOURCES(QtResources_CXX ${QtResources})
ADD_DEFINITIONS(-DQT_GUI_LIBS -DQT_CORE_LIB -DQT3_SUPPORT)
SET_SOURCE_FILES_PROPERTIES(${QtSrcs} PROPERTIES OBJECT_DEPENDS "${UIHeaders}")

#Declare any external dependencies that your project may have here.
FIND_PACKAGE(ITK REQUIRED)
INCLUDE(${ITK_USE_FILE})
FIND_PACKAGE(VTK REQUIRED)
INCLUDE(${VTK_USE_FILE})

LINK_DIRECTORIES(/home/felix/Documents/Libs/parabuild3.12/bin)

#Set any libraries that your project depends on.
SET(Libraries
  ITKCommon
  ITKBasicFilters
  ITKIO
  ITKStatistics
  vtkRendering
  vtkVolumeRendering
  QVTK
  ${QT_LIBRARIES}
)

SET(CurrentFiles "Editor" "SliceVisualization" "UndoRedoSystem" "EditorOperations" "ProgressAccumulator" "Coordinates" "VoxelVolumeRender" "DataManager" "AxesRender" "Metadata")
ADD_EXECUTABLE(Editor ${CurrentFiles}.cxx ${QtSrcs} ${MOCSrcs} ${QtResources_CXX})
TARGET_LINK_LIBRARIES(Editor ${Libraries})
SET_TARGET_PROPERTIES(Editor PROPERTIES OUTPUT_NAME EspinaEditor)

INSTALL(TARGETS Editor DESTINATION bin)
SET(CPACK_GENERATOR "DEB")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Felix de las Pozas <fpozas@alumnos.upm.es>")
SET(CPACK_DEBIAN_PACKAGE_NAME "EspinaVolumeEditor")
SET(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
SET(CPACK_PACKAGE_VERSION_MAJOR "1")
SET(CPACK_PACKAGE_VERSION_MINOR "0")
SET(CPACK_PACKAGE_VERSION_PATCH "0")
SET(CPACK_DEBIAN_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Volume editor for Espina segmentation tool")
SET(CPACK_PACKAGE_DESCRIPTION "Volume editor for Espina segmentation tool")

INCLUDE(CPack)

