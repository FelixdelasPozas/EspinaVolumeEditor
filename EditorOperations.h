///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: EditorOperations.h
// Purpose: Manages selection area and editor operations & filters
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _EDITOROPERATIONS_H_
#define _EDITOROPERATIONS_H_

// itk includes
#include <itkImage.h>
#include <itkSmartPointer.h>

// vtk includes 
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkCubeSource.h>
#include <vtkStructuredPoints.h>

// project includes 
#include "VectorSpaceAlgebra.h"
#include "Coordinates.h"
#include "ProgressAccumulator.h"
#include "DataManager.h"
#include "Metadata.h"
#include "Selection.h"

// qt includes
#include <QtGui>

// typedefs
typedef itk::Image<unsigned short, 3> ImageType;

// forward declarations
class SliceVisualization;

///////////////////////////////////////////////////////////////////////////////////////////////////
// EditorOperations class
//
class EditorOperations
{
    public:
        // constructor & destructor
        EditorOperations(DataManager*);
        ~EditorOperations();
        
        // initializes class
        void Initialize(vtkSmartPointer<vtkRenderer>, Coordinates *, ProgressAccumulator *);
        
        // deletes voxels depending on selected label
        void Cut(std::set<unsigned short>);
        
        // changes label of selected voxels to a different one
        bool Relabel(QWidget *parent, Metadata*, std::set<unsigned short>*, bool*);

        // save volume to disk in MHA format
        void SaveImage(const std::string);
        
        // get the labelmap representation of the volume
        itk::SmartPointer<LabelMapType> GetImageLabelMap();
        
        // set the first scalar value that is free to assign a label (it's NOT the label number)
        void SetFirstFreeValue(const unsigned short);
        
        // get/set preferences
        const unsigned int GetFiltersRadius(void);
        void SetFiltersRadius(const unsigned int);
        const double GetWatershedLevel(void);
        void SetWatershedLevel(const double);
        
        // filters, operates on selected area if set, if not set operates on the whole segmentation
        // selected clipping the volume first (for speed)
        void Erode(const unsigned short);
        void Dilate(const unsigned short);
        void Close(const unsigned short);
        void Open(const unsigned short);
        std::set<unsigned short> Watershed(const unsigned short);

        // Selection class methods, see Selection.* for details
        void AddSelectionPoint(const Vector3ui);
        void ContiguousAreaSelection(const Vector3ui);
        const Vector3ui GetSelectedMinimumBouds();
        const Vector3ui GetSelectedMaximumBouds();
        void ClearSelection();
        Selection::SelectionType GetSelectionType(void);
        const bool IsFirstColorSelected(void);
        void SetSliceViews(SliceVisualization*, SliceVisualization*, SliceVisualization*);

        // SaveSessionThread need to touch private attributes
        friend class SaveSessionThread;
    private:
        // private methods
        //
        // shows a message and gives the details of the exception error
        void EditorError(itk::ExceptionObject &);
        //
        // ereses all points in the image that are not from the label
        void CleanImage(itk::SmartPointer<ImageType>, const unsigned short);
        //
        // dump a itk::image back to vtkstructuredpoints
        void ItkImageToPoints(itk::SmartPointer<ImageType>);
        
        // attributes
        //
        // pointer to orientation data
        Coordinates 										*_orientation;
        //
        // pointer to data
        DataManager 										*_dataManager;
        //
        // progress accumulator
        ProgressAccumulator 								*_progress;
        //
        // configuration option for erode/dilate/open/close filters (structuring element radius)
        unsigned int 										_filtersRadius;
        //
        // configuration option for watershed filter (flood level)
        double 												_watershedLevel;
        //
        // selected area
        Selection											*_selection;
};

#endif // _EDITOROPERATIONS_H_
