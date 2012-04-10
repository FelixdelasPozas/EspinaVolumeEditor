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

// forward declarations
class BoxSelectionWidget;

///////////////////////////////////////////////////////////////////////////////////////////////////
// SliceVisualization class
//
class SliceVisualization
{
    public:
        // type for orientation
        typedef enum
        {
            Sagittal, Coronal, Axial, NoOrientation
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
        
        // returns true if the prop has been picked, and the picked coords are returned by reference
        SliceVisualization::PickingType GetPickData(int*, int*);
        
        // manages thumbnail visibility
        void ZoomEvent();
        
        // segmentation reference image 
        void SetReferenceImage(vtkSmartPointer<vtkStructuredPoints> image);
        
        // segmentation opacity get/set
        unsigned int GetSegmentationOpacity();
        void SetSegmentationOpacity(unsigned int);

        // toggle segmentation view (switches from 0 opacity to previously set opacity and viceversa)
        void ToggleSegmentationView(void);

        // set selection volume to reslice and create actos
        void SetSelectionVolume(const vtkSmartPointer<vtkImageData>, bool = true);

        // clear selection
        void ClearSelections();

        // get orientation type of the object
        SliceVisualization::OrientationType GetOrientationType(void);

        // get the slice renderer
        vtkSmartPointer<vtkRenderer> GetViewRenderer(void);

        // get a pointer to the box representation so we can hide and show it, also we need to
        // deactivate/reactivate the widget depending on the slice
        void SetBoxSelectionWidget(vtkSmartPointer<BoxSelectionWidget>);

        // get a pointer to the imageactor of the slice, needed for polygon/lasso widget interaction
        vtkImageActor* GetSliceActor(void);
    private:
        // private methods
        //
        // generate imageactor and adds it to renderer
        void GenerateSlice(vtkSmartPointer<vtkStructuredPoints>,vtkSmartPointer<vtkLookupTable>);
        //
        // generate crosshairs actors and adds them to renderer
        void GenerateCrosshair();
        //
        // generate thumbnail actors and adds them to thumbnail renderer
        void GenerateThumbnail();
        
        // attributes
        OrientationType                 		_orientation;
        Vector3d                        		_spacing;
        Vector3d                        		_max;
        Vector3ui                       		_size;
        //
        // used to not redraw the viewport if slider is already in the correct position
        Vector3ui                       		_point;
        //
        // reslice axes pointer, used for updating slice
        vtkSmartPointer<vtkMatrix4x4>   		_axesMatrix;
        //
        // crosshair lines pointers, used for updating crosshair
        vtkSmartPointer<vtkPolyData>   		 	_POIHorizontalLine;
        vtkSmartPointer<vtkPolyData>    		_POIVerticalLine;
        //
        // picking
        vtkSmartPointer<vtkPropPicker>  		_picker;
        vtkSmartPointer<vtkRenderer>    		_renderer;
        //
        // text legend
        std::string _textbuffer;
        vtkSmartPointer<vtkTextActor>   		_text;
        //
        // blender
        vtkSmartPointer<vtkImageBlend>  		_blendimages;
        // 
        // actors
        vtkSmartPointer<vtkImageActor>  		_segmentationActor;
        vtkSmartPointer<vtkImageActor>  		_blendActor;
        vtkSmartPointer<vtkActor>       		_horiactor;
        vtkSmartPointer<vtkActor>       		_vertactor;
        vtkSmartPointer<vtkActor>				_squareActor;
        vtkSmartPointer<vtkActor> 				_sliceActor;
        //
        // source for imageactors
        vtkSmartPointer<vtkImageMapToColors> 	_segmentationcolors;
        //
        // for thumbnail rendering
        vtkSmartPointer<vtkRenderer>    		_thumbRenderer;
        double                          		_boundsX;
        double                          		_boundsY;
        vtkSmartPointer<vtkPolyData>    		_square;
        //
        // configuration options
        unsigned int 							_segmentationOpacity;
        bool									_segmentationHidden;
        //
        // actor texture
        vtkSmartPointer<vtkTexture>				_texture;

        // actor data for selected volumes
        struct ActorData
        {
        		vtkSmartPointer<vtkActor> actor;
        		int minSlice;
        		int maxSlice;
        		ActorData(): minSlice(0), maxSlice(0) { actor = NULL; };
        };
        // selection volumes actors vector
        std::vector<struct ActorData*>			_actorList;
        //
        // hides or shows actors depending on the visibility
        void ModifyActorVisibility(struct ActorData*);

        // box selection representation 2D, just the box texture is handled here
        vtkSmartPointer<BoxSelectionWidget>		_boxWidget;
};

#endif // _SLICEVISUALIZATION_H_
