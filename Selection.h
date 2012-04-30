///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: Selection.h
// Purpose: Manages selection areas
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _SELECTION_H_
#define _SELECTION_H_

// vtk includes
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkActor.h>
#include <vtkStructuredPoints.h>
#include <vtkImageChangeInformation.h>
#include <vtkImageClip.h>
#include <vtkBoxRepresentation.h>
#include <vtkBoxWidget2.h>
#include <vtkImageStencilToImage.h>
#include <vtkPolyDataToImageStencil.h>

// project includes
#include "VectorSpaceAlgebra.h"
#include "Coordinates.h"
#include "DataManager.h"
#include "BoxSelectionWidget.h"
#include "BoxSelectionRepresentation2D.h"
#include "BoxSelectionRepresentation3D.h"
#include "ContourWidget.h"

// c++ includes
#include <vector>

// image typedefs
typedef itk::Image<unsigned short, 3> ImageType;
typedef itk::Image<unsigned char, 3> ImageTypeUC;

// forward declarations
class SliceVisualization;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Selection class
//
class Selection
{
    public:
        // constructor & destructor
        Selection();
        ~Selection();

        // type of selections
        typedef enum
        {
            EMPTY, CUBE, VOLUME, DISC, CONTOUR
        } SelectionType;

        // initilize class
        void Initialize(Coordinates *, vtkSmartPointer<vtkRenderer>, DataManager *);

        // modifies selection area with this point
        void AddSelectionPoint(const Vector3ui);

        // contiguous area selection with the wand
        void AddArea(const Vector3ui);

        // deletes points and hides actor (clears buffer only between [_min, _max] bounds)
        void ClearSelection(void);

        // get selection type
        const SelectionType GetSelectionType(void);

        // get minimum selected bounds
        const Vector3ui GetSelectedMinimumBouds();

        // get maximum selected bounds
        const Vector3ui GetSelectedMaximumBouds();

        // returns true if the voxel is inside the selection
        bool VoxelIsInsideSelection(unsigned int, unsigned int, unsigned int);

        // get a itk image from the selection, or the segmentation if there is nothing selected. the image bounds
        // are adjusted for filter radius
        itk::SmartPointer<ImageType> GetSelectionItkImage(const unsigned short, const unsigned int = 0);

        // get a itk image from the segmentation
        itk::SmartPointer<ImageType> GetSegmentationItkImage(const unsigned short);

        // get a itk image from the whole data
        itk::SmartPointer<ImageType> GetItkImage(void);

        // set the views to pass selection volumes
        void SetSliceViews(SliceVisualization*, SliceVisualization*, SliceVisualization*);

        void SetSelectionDisc(int, int, int, int, SliceVisualization*);

        // values used in the selection buffer
        enum SelectionValues
        {
        	VOXEL_UNSELECTED = 0,
        	SELECTION_UNUSED_VALUE,
        	VOXEL_SELECTED,
        };

        // callbacks for widget interaction
        static void BoxSelectionWidgetCallback (vtkObject*, unsigned long, void*, void *);
        static void ContourSelectionWidgetCallback (vtkObject*, unsigned long, void*, void *);

        // starts lasso/polygon selection
        void AddContourInitialPoint(const Vector3ui, SliceVisualization*);

        // moves the contour selection volume if there is a change in slice
        void UpdateContourSlice(Vector3ui point);
    private:
        // private methods
        //
        // computes box selection area actor and parameters
        void ComputeSelectionCube(void);
        void ComputeSelectionBounds(void);
        //
        // computes lasso bounds for min/max selection determination and actor visibility (clipper)
        void ComputeLassoBounds(unsigned int *);
        //
        // generates a rotated volume according to orientation (0 = axial, 1 = coronal, 2 = sagittal)
        void ComputeContourSelectionVolume(const unsigned int *);
        //
        // computes actor from selected volume
        void ComputeActor(vtkSmartPointer<vtkImageData>);
        //
        // deletes all selection volumes
        void DeleteSelectionVolumes(void);
        //
        // deletes all selection actors for render view
        void DeleteSelectionActors(void);
        //
        // returns true if the voxel coordinates refer to a selected voxel
        bool VoxelIsInsideSelectionSubvolume(vtkSmartPointer<vtkImageData>, unsigned int, unsigned int, unsigned int);

        // pointer to renderer
        vtkSmartPointer<vtkRenderer> 						_renderer;

        // min/max actual selection bounds
        Vector3ui 											_max, _min;

        // maximum possible bounds ([0,0,0] to _size)
        Vector3ui 											_size;

        // image spacing (needed for subvolume creation)
        Vector3d											_spacing;

        // selection points for box selection
        std::vector< Vector3ui > 							_selectedPoints;

        // selection type
        SelectionType										_selectionType;

        // pointer to data
        DataManager 										*_dataManager;

        // selection actor texture for render view
        vtkSmartPointer<vtkTexture> 						_texture;

        // slice views
        SliceVisualization*									_axialView;
        SliceVisualization*									_coronalView;
        SliceVisualization*									_sagittalView;

        // list of selection actor for render view
        std::vector<vtkSmartPointer<vtkActor> >				_selectionActorsList;

        // list of selection volumes (unsigned char scalar size)
        std::vector<vtkSmartPointer<vtkImageData> >			_selectionVolumesList;

        // to change origin for erase/paint volume on the fly
        vtkSmartPointer<vtkImageChangeInformation> 			_changer;
        vtkSmartPointer<vtkImageClip> 						_clipper;

        // widgets for box selection, one per view
        vtkSmartPointer<BoxSelectionWidget>					_axialBoxWidget;
        vtkSmartPointer<BoxSelectionWidget>					_coronalBoxWidget;
        vtkSmartPointer<BoxSelectionWidget>					_sagittalBoxWidget;

        // box selection representation for 3d render
        BoxSelectionRepresentation3D						*_boxRender;

        // callback for box selection interaction
        vtkSmartPointer<vtkCallbackCommand> 				_widgetsCallbackCommand;

        // contour selection widget
        vtkSmartPointer<ContourWidget>						_contourWidget;

        // converts VtkPolyData from a contour to a stencil that later gets converted to a volume
        vtkSmartPointer<vtkPolyDataToImageStencil>			_polyDataToStencil;
        vtkSmartPointer<vtkImageStencilToImage>				_stencilToImage;
        vtkImageData										*_rotatedImage;
        bool 												_selectionIsValid;
};

#endif // _SELECTION_H_
