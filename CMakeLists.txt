# to avoid the annoying cmake warning
cmake_minimum_required(VERSION 2.8)

PROJECT(EspinaVolumeEditor)

# Version Number
set (ESPINAEDITOR_VERSION_MAJOR 2)
set (ESPINAEDITOR_VERSION_MINOR 0)
set (ESPINAEDITOR_VERSION_PATCH 0)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Find the QtWidgets library
find_package(Qt5 COMPONENTS Widgets)

# Use the include path and library for Qt that is used by VTK.
INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -mwindows -m64")

if (CMAKE_BUILD_TYPE MATCHES Debug)
  set(CORE_EXTERNAL_LIBS ${CORE_EXTERNAL_LIBS} ${QT_QTTEST_LIBRARY})
endif (CMAKE_BUILD_TYPE MATCHES Debug)

if(DEFINED MINGW)
  configure_file("${PROJECT_SOURCE_DIR}/EspinaEditor.rc.in" "${PROJECT_BINARY_DIR}/EspinaEditor.rc")
  set(RC_FILE ${CMAKE_CURRENT_BINARY_DIR}/ESPINAEDITOR.rc)
  set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff -o <OBJECT> -i <SOURCE>")
  enable_language(RC)
endif(DEFINED MINGW)

SET(UIS 
  QtPreferences.ui 
  QtRelabel.ui 
  QtAbout.ui 
  QtColorPicker.ui 
  QtSessionInfo.ui 
  QtKeyboardHelp.ui 
  EspinaVolumeEditor.ui
)

SET (MOCS 
  EspinaVolumeEditor.h 
  QtPreferences.h 
  QtRelabel.h 
  QtAbout.h 
  QtColorPicker.h 
  SaveSession.h 
  QtSessionInfo.h 
  QtKeyboardHelp.h
  AxesRender.h
  DataManager.h
  SliceVisualization.h
  VoxelVolumeRender.h
)

SET (QtResources 
  editor.qrc 
  about.qrc 
  colorpicker.qrc 
  keyboardhelp.qrc
)

SET(CurrentFiles 
  Editor.cpp
  SliceVisualization.cpp
  UndoRedoSystem.cpp
  EditorOperations.cpp
  ProgressAccumulator.cpp
  Coordinates.cpp
  VoxelVolumeRender.cpp
  DataManager.cpp
  Metadata.cpp
  SaveSession.cpp
  Selection.cpp
  BoxSelectionRepresentation2D.cpp
  BoxSelectionWidget.cpp
  BoxSelectionRepresentation3D.cpp
  FocalPlanePointPlacer.cpp
  ContourWidget.cpp
  ContourRepresentation.cpp
  ContourRepresentationGlyph.cpp
  EspinaVolumeEditor.cpp 
  QtPreferences.cpp 
  QtRelabel.cpp 
  QtAbout.cpp 
  QtColorPicker.cpp 
  SaveSession.cpp 
  QtSessionInfo.cpp 
  QtKeyboardHelp.cpp
  AxesRender.cpp
)

qt5_wrap_ui(UIHeaders ${UIS})
qt5_add_resources(QtResources_cpp ${QtResources})
SET_SOURCE_FILES_PROPERTIES(${CurrentFiles} PROPERTIES OBJECT_DEPENDS "${UIHeaders}")

#Declare any external dependencies that your project may have here.
FIND_PACKAGE(VTK REQUIRED)
INCLUDE(${VTK_USE_FILE})

FIND_PACKAGE(ITK REQUIRED)
INCLUDE(${ITK_USE_FILE})

#Set any libraries that your project depends on.
SET(Libraries
  ${ITK_LIBRARIES}
  ${VTK_LIBRARIES}
  ${QT_LIBRARIES}
  Qt5::Widgets
  opengl32
  glu32
)

ADD_EXECUTABLE(EspinaEditor ${CurrentFiles} ${MOCSrcs} ${QtResources_cpp} ${RC_FILE})
TARGET_LINK_LIBRARIES(EspinaEditor ${Libraries})
qt5_use_modules(EspinaEditor Widgets)