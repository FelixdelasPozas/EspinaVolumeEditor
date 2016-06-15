EspINA Volume Editor
====================

# Summary
- [Description](#description)
- [Compilation](#compilation-requirements)
- [Install](#install)
- [Screenshots](#screenshots)
- [Repository information](#repository-information)

# Description
EspINA Volume Editor is an application to reconstruct and edit segmentations of electron microscopy or optic microscopy images generated with old versions of the application [EspINA](http://cajalbbp.cesvima.upm.es/espina/) (EspINA Interactive Neuron Analyzer). Originally written for Linux operative systems this is a Windows port of my final project to obtain my degree in Computer Science and it's capabilities have been integrated in the latest versions of EspINA. Because the file format used by the application it's no longer the one used by EspINA it's not really useful anymore and it's shown here as an exercise in building applications with the ITK and VTK libraries. 

The application provides several tools to modify the segmentations: 
* Manual edition or creation of segmentations (paint or erase using a variable radius brush). 
* Morphological operations (open, close, dilate and erode with variable radius).
* Segmentation splitting using the Watershed algorithm. 

The area of application of the tools can be selected using:
* Box selection.
* Lasso selection (contour that can be modified after it's definition translating, adding and removing nodes, and translating the whole contour).
* Magic wand (to select contiguous voxels using a thresholding algorithm).

The repository includes an example image in the 'images' directory. 

## Options
Several options can be configured:
* The size of the memory buffer used for the undo/redo system.
* Paint/erase brush radius.
* Morphological operations' radius. 
* Watershed flood level. 
* Opacity of the segmentations (to see the value of the voxels that are contained in it).
* Time of the auto-save feature (takes snapshots of the current session to disk in case the program crashes). 

## File formats
The application reads old EspINA segmha files that contains the segmentations and MHD (Metaimage format) files of the microscopy images. 

# Compilation requirements
## To build the tool:
* cross-platform build system: [CMake](http://www.cmake.org/cmake/resources/software.html).
* compiler: [Mingw64](http://sourceforge.net/projects/mingw-w64/) on Windows.

## External dependencies
The following libraries are required:
* [ITK](https://itk.org/) - Insight Segmentation and Registration Toolkit.
* [VTK](http://www.vtk.org/) - The Visualization Toolkit.
* [Qt 4 opensource framework](http://www.qt.io/).

# Install
The only current option is build from source as binaries are not provided. 

# Screenshots

The main application provides four views: three orthogonal (axial, coronal and sagittal planar views) and a 3D view that provides a volumetric representation of the segmentations. 

![maindialog](https://cloud.githubusercontent.com/assets/12167134/16097616/fbe7b6ba-334e-11e6-9867-a021ba855ae0.jpg)

Each view can be individually maximized. The 3D view provides two visualization alternatives: voxel and mesh. 

![voxel](https://cloud.githubusercontent.com/assets/12167134/16097617/fc487798-334e-11e6-8236-fba813b3cb50.jpg)

The configuration dialog.

![configuration](https://cloud.githubusercontent.com/assets/12167134/16097613/fbaeaca8-334e-11e6-8949-6ddbb2ed932e.jpg)

The session information dialog provides information about the properties of the image and the segmentations. 

![information](https://cloud.githubusercontent.com/assets/12167134/16097614/fbdc2d40-334e-11e6-99d2-075a9e9ab979.jpg)

The lasso tool in action over the sagittal view, showing the selected voxels. 

![lasso](https://cloud.githubusercontent.com/assets/12167134/16097615/fbe50b9a-334e-11e6-9970-dec26e2ffaf4.jpg)

# Repository information

**Version**: 2.0.0

**Status**: finished

**cloc statistics**

| Language                     |files          |blank        |comment      |code    |
|:-----------------------------|--------------:|------------:|------------:|-------:|
| C++                          |  26           |  2267       |   1448      | 10808  |
| C/C++ Header                 |  27           |   875       |   2207      |  2052  |
| CMake                        |   1           |   18        |     6       |   116  |
| **Total**                    | **54**        | **3160**    |   **3661**  | **12976** |
