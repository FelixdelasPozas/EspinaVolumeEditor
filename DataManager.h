///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: DataManager.h
// Purpose: converts a itk::LabelMap to vtkStructuredPoints data and back 
// Notes: vtkStructuredPoints scalar values aren't the labelmap values so mapping between 
//        labelmap and structuredpoints scalars is neccesary
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _DATAMANAGER_H_
#define _DATAMANAGER_H_

// vtk includes
#include <vtkStructuredPoints.h>
#include <vtkLookupTable.h>
#include <vtkSmartPointer.h>

// itk includes
#include <itkLabelMap.h>
#include <itkSmartPointer.h>
#include <itkShapeLabelObject.h>

// c++ includes
#include <map>

// project includes
#include "Coordinates.h"
#include "UndoRedoSystem.h"
#include "VectorSpaceAlgebra.h"
#include "Metadata.h"

// defines & typedefs
typedef itk::ShapeLabelObject< unsigned short, 3 > LabelObjectType;
typedef itk::LabelMap< LabelObjectType > LabelMapType;

// UndoRedoSystem forward declaration
class UndoRedoSystem;

///////////////////////////////////////////////////////////////////////////////////////////////////
// DataManager class
//
class DataManager
{
    public:
        // constructor & destructor
        DataManager();
        ~DataManager(void);

        // class init
        void Initialize(itk::SmartPointer<LabelMapType>, Coordinates *, Metadata *);

        // set image StructuredPoints
    	void SetStructuredPoints(vtkSmartPointer<vtkStructuredPoints>);
        
        // undo/redo system signaling
        void OperationStart(std::string);
        void OperationEnd();
        void OperationCancel();
        
        // get pointer to vtkStructuredPoints
        vtkSmartPointer<vtkStructuredPoints> GetStructuredPoints();
        
        // get pointer to original labelmap, this labelmap is never modified
        itk::SmartPointer<LabelMapType> GetLabelMap();
        
        // get orientation data
        Coordinates * GetOrientationData();
        
        // get scalar/label from label/scalar
        unsigned short GetScalarForLabel(unsigned short);
        unsigned short GetLabelForScalar(unsigned short);
        
        // get table of scalar<->label values
        std::map<unsigned short, unsigned short>* GetLabelValueTable();
        
        // get centroid of object with specified label
        Vector3d GetCentroidForObject(unsigned short int);

        // get scalar for voxel(x,y,z)
        unsigned short GetVoxelScalar(unsigned int, unsigned int, unsigned int);
        
        // get the number of labels in the image
        int GetNumberOfLabels();
        
        // changes voxel label
        void SetVoxelScalar(unsigned int, unsigned int, unsigned int, unsigned short);
        
        // changes voxel label but bypass undo/redo system, used to take back changes made to data while
        // we are inside an exception treatment code.
        void SetVoxelScalarRaw(unsigned int, unsigned int, unsigned int, unsigned short);
        
        // creates a new label and assigns a new scalar to that label, starting from an initial
        // optional value. modifies color table and returns new label position (not scalar)
        unsigned short SetLabel(Vector3d);
        
        // switch tables
        void SwitchLookupTables(vtkSmartPointer<vtkLookupTable>);
        
        // get a smartpointer from the lookuptable
        vtkSmartPointer<vtkLookupTable> GetLookupTable();
        
        // set the first scalar value that is free to assign a label (it's NOT the label number)
        void SetFirstFreeValue(unsigned short);
        
        // get the first scalar value that is free to assign to a label (NOT the label number)
        unsigned short GetFirstFreeValue();
        
        // get the last used scalar in _labelValues
        unsigned short GetLastUsedValue();
        
        // get a pointer to the rgba values of a scalar (it's NOT the label number)
        double* GetRGBAColorForScalar(unsigned short);
        
        // undo/redo system forwarding as i don't want other than DataManager touch the Undo/Redo system
        // but need these actions for the GUI menu items
        std::string GetUndoActionString();
        std::string GetRedoActionString();
        std::string GetActualActionString();
        bool IsUndoBufferEmpty();
        bool IsRedoBufferEmpty();
        void DoUndoOperation();
        void DoRedoOperation();
        
        // undo/redo configuration
        void SetUndoRedoBufferSize(unsigned long int);
        unsigned long int GetUndoRedoBufferSize();
        unsigned long int GetUndoRedoBufferCapacity();
        
        // voxel statistics per label
        unsigned long long int GetNumberOfVoxelsForLabel(unsigned short);

        // get voxel count table
        std::map<unsigned short, unsigned long long int>* GetVoxelCountTable(void);

        // set the bounding box for the object
        void SetObjectBoundingBox(unsigned short, itk::Index<3>, itk::Size<3>);

        // get the bounding box index and size for the object
        itk::Index<3> GetBoundingBoxOrigin(unsigned short);
        itk::Size<3> GetBoundingBoxSize(unsigned short);

    private:
        // resets lookuptable to initial state based on original labelmap, used during init too
        void GenerateLookupTable();

        // copy one lookuptable over another, does not change pointers (important)
        void CopyLookupTable(vtkSmartPointer<vtkLookupTable>,vtkSmartPointer<vtkLookupTable>);
        
        // voxel statistics functions, trivial but implemented as functions because it appears frecuently
        void StatisticsActionReset(void);
        void StatisticsActionJoin(void);
        
        // needed data attributes
        itk::SmartPointer<LabelMapType>         	_labelMap;
        vtkSmartPointer<vtkStructuredPoints>    	_structuredPoints;
        vtkSmartPointer<vtkLookupTable>         	_lookupTable;
        Coordinates                            	   *_orientationData;

        // map image values<->internal values
        std::map<unsigned short, unsigned short>  	_labelValues;

        // map labels <-> centroids
        std::map<unsigned short, Vector3d>     		_objectCentroid;
        // undo/redo system
        UndoRedoSystem                         	   *_actionsBuffer;
        
        // first free value for new labels
        unsigned short                          	_firstFreeValue;
        
        // for voxel statistics, notice the NOT unsigned variable for actions, as those values can be negative.
        // using two arrays allows us to ignore the number of voxels changed when an exception ocurrs, and an
        // actions is ignored. the voxelCount must be always positive or null.
        std::map<unsigned short, unsigned long long int>  _voxelCount;
        std::map<unsigned short, long long int>  		  _voxelActionCount;
        std::map<unsigned short, Vector3ll>			  	  _temporalCentroid;

        // object bounding box
        // for bounding box operations
        struct BoundingBox
        {
        		itk::Index<3> origin;
        		itk::Size<3> size;
        };

        std::map<unsigned short, struct BoundingBox *> _objectBox;
};

#endif // _DATAMANAGER_H_
