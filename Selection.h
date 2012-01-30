///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: Selection.h
// Purpose: Manages selection areas
// Notes: Selected points have a value of 255 in the selection volume
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _SELECTION_H_
#define _SELECTION_H_

// vtk includes
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkActor.h>
#include <vtkStructuredPoints.h>

// project includes
#include "VectorSpaceAlgebra.h"
#include "Coordinates.h"
#include "DataManager.h"

// c++ includes
#include <vector>

// image typedefs
typedef itk::Image<unsigned short, 3> ImageType;
typedef itk::Image<unsigned char, 3> ImageTypeUC;

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
            Empty, Cube, Area
        } SelectionType;

        // initilize class
        void Initialize(Coordinates *, vtkSmartPointer<vtkRenderer>, DataManager *);

        // modifies selection area with this point
        void AddSelectionPoint(const Vector3ui);

        // contiguous area selection with the wand
        void AddArea(Vector3ui, const unsigned short);

        // deletes points and hides actor (clears buffer only between [_min, _max] bounds)
        void ClearSelection(void);

        // clear all buffer
        void ClearSelectionBuffer(void);

        // get selection type
        const SelectionType GetSelectionType(void);

        // get selection points
        const std::vector<Vector3ui> GetSelectionPoints(void);

        // get minimum selected bounds
        const Vector3ui GetSelectedMinimumBouds();

        // get maximum selected bounds
        const Vector3ui GetSelectedMaximumBouds();

        // get a itk image from the segmentation or selection, as small as the bounding box that contains it
        itk::SmartPointer<ImageType> GetSelectionItkImage(const unsigned short, const unsigned int = 0);

        // get a itk image from the whole data
        itk::SmartPointer<ImageType> GetItkImage(void);

        // returns true if the voxel coordinates refer to a selected voxel
        bool VoxelIsInsideSelection(unsigned int, unsigned int, unsigned int);
    private:
        // computes selection area
        void ComputeSelectionCube(void);

        // clears selection cube
        void FillSelectionCube(unsigned char);

        // pointer to renderer
        vtkSmartPointer<vtkRenderer> 						_renderer;

        // selection actor
        vtkSmartPointer<vtkActor> 							_actor;

        // selection volume (unsigned char type, selected points have a 255 value)
        vtkSmartPointer<vtkStructuredPoints>				_volume;

        // min/max actual selection bounds
        Vector3ui 											_max, _min;

        // maximum possible bounds ([0,0,0] to _size)
        Vector3ui 											_size;

        // selection points for box selection
        std::vector< Vector3ui > 							_selectedPoints;

        // selection type
        SelectionType										_selectionType;

        // pointer to data
        DataManager 										*_dataManager;
};

#endif // _SELECTION_H_
