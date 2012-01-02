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
#include <set>

// project includes
#include "Coordinates.h"
#include "Metadata.h"
#include "VectorSpaceAlgebra.h"

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

        // undo/redo system signaling
        void OperationStart(std::string);
        void OperationEnd();
        void OperationCancel();
        
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

        // lookuptable color modification and check
        void ColorHighlight(const unsigned short);
        void ColorDim(const unsigned short);
        void ColorHighlightExclusive(unsigned short);
        void ColorDimAll(void);
        bool ColorIsInUse(double* color);
        unsigned int GetNumberOfColors();
        void GetColorComponents(unsigned short, double*);
        void SetColorComponents(unsigned short, double*);

        // SETS /////////////////////////

        // changes voxel label
        void SetVoxelScalar(unsigned int, unsigned int, unsigned int, unsigned short);

        // changes voxel label but bypass undo/redo system, used to take back changes made to data while
        // we are inside an exception treatment code.
        void SetVoxelScalarRaw(unsigned int, unsigned int, unsigned int, unsigned short);

        // creates a new label and assigns a new scalar to that label, starting from an initial
        // optional value (firstfreevalue). modifies color table and returns new label position
        // (not scalar used for that label)
        unsigned short SetLabel(Vector3d);

        // set image StructuredPoints
    	void SetStructuredPoints(vtkSmartPointer<vtkStructuredPoints>);

        // set the first scalar value that is free to assign a label (it's NOT the label number)
        void SetFirstFreeValue(unsigned short);

        // GETS /////////////////////////

        // get the lookuptable (used only by slices because we need the same lookuptable pointer for all)
        vtkSmartPointer<vtkLookupTable> GetLookupTable();

        // get pointer to vtkStructuredPoints
        vtkSmartPointer<vtkStructuredPoints> GetStructuredPoints();
        
        // get pointer to original labelmap, this labelmap is never modified
        itk::SmartPointer<LabelMapType> GetLabelMap();
        
        // get orientation data
        Coordinates * GetOrientationData();
        
        // get scalar/label from label/scalar
        unsigned short GetScalarForLabel(unsigned short);
        unsigned short GetLabelForScalar(unsigned short);
        
        // get centroid of object with specified label
        Vector3d GetCentroidForObject(unsigned short int);

        // get scalar for voxel(x,y,z)
        unsigned short GetVoxelScalar(unsigned int, unsigned int, unsigned int);
        
        // get the first scalar value that is free to assign to a label (NOT the label number)
        unsigned short GetFirstFreeValue();
        
        // get the last used scalar in _labelValues
        unsigned short GetLastUsedValue();
        
        // get a pointer to the rgba values of a scalar (it's NOT the label number)
        double* GetRGBAColorForScalar(unsigned short);
        
        // voxel statistics per label
        unsigned long long int GetNumberOfVoxelsForLabel(unsigned short);

        // get the bounding box index and size for the object
        Vector3ui GetBoundingBoxMin(unsigned short);
        Vector3ui GetBoundingBoxMax(unsigned short);

        // get the number of labels used (including the background label)
        unsigned int GetNumberOfLabels(void);

        struct ObjectInformation
        {
        		unsigned short 			scalar;			// original scalar value in the image loaded
        		Vector3d 				centroid;		// centroid of the object
        		unsigned long long int 	sizeInVoxels;	// size of the object in voxels
        		Vector3ui	 			min;			// Bounding Box: min values
        		Vector3ui	 			max;			// Bounding Box: max values

        		ObjectInformation(): scalar(0), centroid(Vector3d(0,0,0)), sizeInVoxels(0), min(Vector3ui(0,0,0)), max(Vector3ui(0,0,0)) {};
        };

        // get the table of objects
        std::map<unsigned short, struct DataManager::ObjectInformation*>* GetObjectTablePointer();

        // MISC /////////////////////////

        // switch tables
        void SwitchLookupTables(vtkSmartPointer<vtkLookupTable>);

        friend class SaveSessionThread;
        friend class EspinaVolumeEditor;
    private:
        // resets lookuptable to initial state based on original labelmap, used during init too
        void GenerateLookupTable();

        // copy one lookuptable over another, does not change pointers (important)
        void CopyLookupTable(vtkSmartPointer<vtkLookupTable>,vtkSmartPointer<vtkLookupTable>);
        
        // voxel statistics functions, trivial but implemented as functions because it appears frecuently
        void StatisticsActionReset(void);
        void StatisticsActionJoin(void);
        void StatisticsActionClear(void);
        
        // needed data attributes
        itk::SmartPointer<LabelMapType>         	_labelMap;
        vtkSmartPointer<vtkStructuredPoints>    	_structuredPoints;
        vtkSmartPointer<vtkLookupTable>         	_lookupTable;
        Coordinates                            	   *_orientationData;
        UndoRedoSystem                         	   *_actionsBuffer;

        // first free value for new labels
        unsigned short                          	_firstFreeValue;

        // object information vector
        std::map<unsigned short, struct ObjectInformation*> ObjectVector;

        // action in progress data for voxel counting and centroid calculations.
        struct ActionInformation
        {
        		long long int 	sizeInVoxels;			// size of the action in voxels
        		Vector3ll 		temporalCentroid;		// sum of the x,y,z coords of the points added/substraced in the action
        		Vector3ui	 	min;					// Bounding Box: min values
        		Vector3ui	 	max;					// Bounding Box: max values

        		ActionInformation(): sizeInVoxels(0), temporalCentroid(Vector3ll(0,0,0)), min(Vector3ui(0,0,0)), max(Vector3ui(0,0,0)) {};
        };
        std::map<unsigned short, struct ActionInformation*> ActionInformationVector;

        std::set<unsigned short> 					_highlightedLabels;
};

#endif // _DATAMANAGER_H_
