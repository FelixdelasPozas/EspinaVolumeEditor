///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: VoxelVolumeRender.h
// Purpose: renders a vtkStructuredPoints volume (raycasting volume or colored meshes)
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _VOXELVOLUMERENDER_H_
#define _VOXELVOLUMERENDER_H_

// vtk includes
#include <vtkSmartPointer.h>
#include <vtkLookupTable.h>
#include <vtkStructuredPoints.h>
#include <vtkRenderer.h>
#include <vtkPiecewiseFunction.h>
#include <vtkColorTransferFunction.h>
#include <vtkVolumeRayCastMapper.h>
//#include <vtkSmartVolumeMapper.h>

// c++ includes
#include <map>

// project includes
#include "ProgressAccumulator.h"
#include "DataManager.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// VoxelVolumeRender class
//
class VoxelVolumeRender
{
    public:
        // constructor & destructor
		VoxelVolumeRender(DataManager*, vtkSmartPointer<vtkRenderer>, ProgressAccumulator*);
		~VoxelVolumeRender();

        // update color table with new alpha component
        void UpdateColorTable(int, double);

        // update focus extent for renderers clipping planes
        void UpdateFocus(unsigned short);

        // update extent of focused object without moving the camera
        void UpdateFocusExtent(void);

        // render volume as a mesh
        void ViewAsMesh();

        // render volume as a raycasted volume
        void ViewAsVolume();
    private:
        // delete all actors from renderer reset class
        void DeleteActors();

        // compute volume using raycast
        void ComputeRayCastVolume();

        // compute mesh for a label
        void ComputeMesh(unsigned short label);

        // compute volume using meshes (slower because require more filters)
        void ComputeGPURender();

        // attributes
        vtkSmartPointer<vtkRenderer>         		_renderer;
        ProgressAccumulator                 	   *_progress;
        DataManager                         	   *_dataManager;

        // to update color table
        vtkSmartPointer<vtkPiecewiseFunction>      	_opacityfunction;
        vtkSmartPointer<vtkColorTransferFunction>  	_colorfunction;

        // actor for the mesh representation of the volume
        vtkSmartPointer<vtkActor> 					_meshActor;

        // actors for the volume
        vtkSmartPointer<vtkVolume> 					_volume;

        // actual object label
        unsigned short 								_objectLabel;

        // software saycast volume mapper
        vtkSmartPointer<vtkVolumeRayCastMapper> 	_volumemapper;
//        vtkSmartPointer<vtkSmartVolumeMapper>  	_GPUmapper;
};

#endif
