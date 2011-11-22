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

// qt includes
#include <QtGui>

// typedefs
typedef itk::Image<unsigned short, 3> ImageType;

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
        
        // modifies selection area with this point
        void AddSelectionPoint(Vector3ui point);
        
        // computes selection area
        void ComputeSelectionCube();
        
        // deletes points and actor
        void ClearSelection();
        
        // get selection points
        std::vector<Vector3ui> GetSelection();
        
        // deletes voxels depending on selected label
        void CutSelection(unsigned short);
        
        // changes label of selected voxels to a different one
        bool RelabelSelection(QWidget *parent, unsigned short, vtkSmartPointer<vtkLookupTable>, Metadata*);

        // save volume as an GIPL image on disk
        void SaveImage(std::string);
        
        // get the labelmap out of data
        itk::SmartPointer<LabelMapType> GetImageLabelMap();
        
        // set the first scalar value that is free to assign a label (it's NOT the label number)
        void SetFirstFreeValue(unsigned short);
        
        // get/set preferences
        unsigned int GetFiltersRadius(void);
        void SetFiltersRadius(unsigned int);
        double GetWatershedLevel(void);
        void SetWatershedLevel(double);
        
        // filters, operates on selection or, if there is not a selection, on the whole dataset
        void ErodeSelection(unsigned short);
        void DilateSelection(unsigned short);
        void CloseSelection(unsigned short);
        void OpenSelection(unsigned short);
        void WatershedSelection(unsigned short);
    private:
        // shows a message and gives the details of the exception error
        void EditorError(itk::ExceptionObject &);
        
        // temp description
        void CleanImage(itk::SmartPointer<ImageType>, unsigned short);

        // get a itk image from selected region to use it with filters
        itk::SmartPointer<ImageType> GetItkImageFromSelection();

        // dump a itk::image back to vtkstructuredpoints
        void ItkImageToPoints(itk::SmartPointer<ImageType>);
        
        // pointer to renderer
        vtkSmartPointer<vtkRenderer> _renderer;
        
        // poiner to orienation data
        Coordinates *_orientation;

        // selection bounds
        Vector3ui _max, _min;
        
        // maximum bounds
        Vector3ui _size;

        // selection points
        std::vector< Vector3ui > _selectedPoints;
        
        // representation of the selection
        vtkSmartPointer<vtkCubeSource> _selectionCube;
        
        // box actor
        vtkSmartPointer<vtkActor> _actor;
        
        // pointer to data
        DataManager* _dataManager;
        
        // progress accumulator
        ProgressAccumulator *_progress;
        
        // configuration options selectable by the user for erode/dilate/open/close filters (structuring element radius)
        unsigned int _filtersRadius;
        
        // configuration option selectable by te user for watershed filter (flood level)
        double _watershedLevel;
};

#endif // _EDITOROPERATIONS_H_
