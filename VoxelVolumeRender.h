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
        typedef enum
        {
            RayCast, Meshes
        } RenderType;
        
        // constructor & destructor
        VoxelVolumeRender(
                vtkSmartPointer<vtkStructuredPoints>, 
                vtkSmartPointer<vtkLookupTable>, 
                vtkSmartPointer<vtkRenderer>, 
                RenderType,
                ProgressAccumulator*);
        ~VoxelVolumeRender();
        
        // compute render
        void Compute();
        
        // update color table with new alpha component
        void UpdateColorTable(int, double);
        
        // get render type
        RenderType GetRenderType();
    private:
        // delete all actors from renderer reset class
        void DeleteActors();

        // compute volume using raycast
        void ComputeRayCastVolume();
        
        // compute volume using meshes (slower because require more filters)
        void ComputeMeshes();
        
        // attributes
        vtkSmartPointer<vtkStructuredPoints> _structuredPoints;
        vtkSmartPointer<vtkLookupTable>      _lookupTable;
        vtkSmartPointer<vtkRenderer>         _renderer;
        RenderType                           _renderType;
        ProgressAccumulator*                 _progress;
        
        // to update color table
        vtkSmartPointer<vtkPiecewiseFunction>      _opacityfunction;
        std::map< int, vtkSmartPointer<vtkActor> > _actormap;
        
        // actors to delete to clear viewports (we get the actors of the meshes from the std::map)
        vtkSmartPointer<vtkVolume> _volume;
};

#endif
