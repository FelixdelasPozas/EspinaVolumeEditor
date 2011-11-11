///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: SliceVisualization.h
// Purpose: generate slices & crosshairs for axial, coronal and sagittal views. Also handles
//          pick function and selection of slice pixels.
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _SLICEVISUALIZATION_H_
#define _SLICEVISUALIZATION_H_

// vtk includes
#include <vtkSmartPointer.h>
#include <vtkStructuredPoints.h>
#include <vtkLookupTable.h>
#include <vtkRenderer.h>
#include <vtkMatrix4x4.h>
#include <vtkPolyData.h>
#include <vtkPropPicker.h>
#include <vtkTextActor.h>
#include <vtkImageMapToColors.h>
#include <vtkActor.h>
#include <vtkPlaneSource.h>
#include <vtkImageActor.h>
#include <vtkImageBlend.h>

// project includes
#include "Coordinates.h"
#include "VectorSpaceAlgebra.h"
#include "EditorOperations.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// SliceVisualization class
//
class SliceVisualization
{
    public:
        // type for orientation
        typedef enum
        {
            Sagittal, Coronal, Axial
        } OrientationType;
        
        // type for picking
        typedef enum
        {
            Thumbnail, Slice, None
        } PickingType;
        
        // constructor & destructor
        SliceVisualization(OrientationType);
        ~SliceVisualization();
        
        // initialize class with data
        void Initialize(vtkSmartPointer<vtkStructuredPoints>, 
                vtkSmartPointer<vtkLookupTable>,
                vtkSmartPointer<vtkRenderer>,
                Coordinates*);
        
        // update both slice and crosshair
        void Update(Vector3ui);
        
        // updates crosshair
        void UpdateCrosshair(Vector3ui);
        
        // updates slice
        void UpdateSlice(Vector3ui);
        
        // returns true if the prop has been picked
        SliceVisualization::PickingType GetPickPosition(int*, int*);
        
        // set selection
        void SetSelection(std::vector< Vector3ui >);
        
        // clear selection
        void ClearSelection();
        
        // manages thumbnail visibility
        void ZoomEvent();
        
        // segmentation reference image 
        void SetReferenceImage(vtkSmartPointer<vtkStructuredPoints> image);
        
        // get segmentation opacity
        unsigned int GetSegmentationOpacity();
        
        // set segmentation opacity
        void SetSegmentationOpacity(unsigned int);
    private:
        // generate imageactor and adds it to renderer
        void GenerateSlice(
                vtkSmartPointer<vtkStructuredPoints>, 
                vtkSmartPointer<vtkLookupTable>);
        
        // generate crosshairs actors and adds them to renderer
        void GenerateCrosshair();
        
        // generate thumbnail actors and adds them to thumbnail renderer
        void GenerateThumbnail();
        
        // attributes
        //
        OrientationType                 _orientation;
        Vector3d                        _spacing;
        Vector3d                        _max;
        Vector3ui                       _size;
        //
        // used to not redraw the viewport if slider is already in the correct position
        Vector3ui                       _point;
        //
        // reslice axes pointer, used for updating slice
        vtkSmartPointer<vtkMatrix4x4>   _axesMatrix;
        //
        // crosshair lines pointers, used for updating crosshair
        vtkSmartPointer<vtkPolyData>    _POIHorizontalLine;
        vtkSmartPointer<vtkPolyData>    _POIVerticalLine;
        //
        // for picking
        vtkSmartPointer<vtkPropPicker>  _picker;
        vtkSmartPointer<vtkRenderer>    _renderer;
        //
        // text legend
        char _textbuffer[20];
        vtkSmartPointer<vtkTextActor>   _text;
        //
        // selection actors & planesource
        vtkSmartPointer<vtkActor>       _actor;
        vtkSmartPointer<vtkPlaneSource> _selectionPlane;
        Vector3ui                       _maxSelection;
        Vector3ui                       _minSelection;
        //
        // blender
        vtkSmartPointer<vtkImageBlend>  _blendimages;
        // 
        // actors
        vtkSmartPointer<vtkImageActor>  _imageactor;
        vtkSmartPointer<vtkImageActor>  _blendactor;
        vtkSmartPointer<vtkActor>       _horiactor;
        vtkSmartPointer<vtkActor>       _vertactor;
        //
        // source for imageactors
        vtkSmartPointer<vtkImageMapToColors> _segmentationcolors;
        //
        // for thumbnail rendering
        vtkSmartPointer<vtkRenderer>    _thumbRenderer;
        double                          _boundsX;
        double                          _boundsY;
        vtkSmartPointer<vtkPolyData>    _square;
        //
        // configuration options
        unsigned int _segmentationOpacity;
};

#endif // _SLICEVISUALIZATION_H_
