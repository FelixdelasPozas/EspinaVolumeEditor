///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: AxesRender.h
// Purpose: generate slice planes and crosshair for 3d render view
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _AXESRENDER_H_
#define _AXESRENDER_H_

// vtk includes
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkLineSource.h>
#include <vtkPoints.h>
#include <vtkPlaneSource.h>
#include <vtkOrientationMarkerWidget.h>

// c++ includes
#include <vector>

// project includes
#include "Coordinates.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// AxesRender class
//
class AxesRender
{
    public:
        // type for orientation
        typedef enum
        {
            Sagittal, Coronal, Axial
        } OrientationType;

        // constructor & destructor
        AxesRender(vtkSmartPointer<vtkRenderer>, Coordinates *);
        ~AxesRender();
        
        // update slice planes in 3d render view to the new point
        void Update(Vector3ui);
        
        // are the axes and the crosshair visible in the render viewport?
        bool isVisible();
        
        // set axes and crosshair visibility
        void setVisible(bool);
    private:
        // generate plane actors pointing to the center of the volume
        void GenerateSlicePlanes(vtkSmartPointer<vtkRenderer> renderer);
        
        // generate voxel crosshair pointing to the center of the volume
        void GenerateVoxelCrosshair(vtkSmartPointer<vtkRenderer> renderer);
        
        // update slice planes in 3d render view to the new point
        void UpdateSlicePlanes(Vector3ui);
        
        // update voxel crosshair in 3d render view to the new point
        void UpdateVoxelCrosshair(Vector3ui);
        
        // creates and maintains a pointer to the orientation marker
        // widget (axes orientation) 
        void CreateOrientationWidget(vtkSmartPointer<vtkRenderer> renderer);

        // Attributes
        //
        // renderer
        vtkSmartPointer<vtkRenderer> _renderer;
        //
        // 3d crosshair lines
        std::vector<vtkSmartPointer<vtkLineSource> > _POILines;
        //
        // 3d slice planes
        std::vector<vtkSmartPointer<vtkPlaneSource> > _planes;
        //
        // construction vars
        Vector3d _spacing;
        Vector3d _max;
        //
        // actor pointers, because i don't want to destroy the actors, just make it
        // invisible
        std::vector< vtkSmartPointer<vtkActor> > _planesActors;
        std::vector< vtkSmartPointer<vtkActor> > _crossActors;
        bool _visible;
        
        // this pointer is needed just to mantain the reference counter for the
        // orientation marker widget, or it will crash the application when it
        // goes out of scope
        vtkSmartPointer < vtkOrientationMarkerWidget > _axesWidget;
        
};

#endif
