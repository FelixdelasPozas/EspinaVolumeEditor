///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: DataManager.cxx
// Purpose: converts a itk::LabelMap to vtkStructuredPoints data and back 
// Notes: vtkStructuredPoints scalar values aren't the labelmap values so mapping between 
//        labelmap and structuredpoints scalars is neccesary
///////////////////////////////////////////////////////////////////////////////////////////////////

// c++ includes
#include <cassert>

// itk includes
#include <itkMacro.h>
#include <itkChangeLabelLabelMapFilter.h>
#include <itkShapeLabelMapFilter.h>
#include <itkImageRegion.h>

// vtk includes
#include <vtkStructuredPoints.h>

// project includes
#include "DataManager.h"
#include "UndoRedoSystem.h"
#include "VectorSpaceAlgebra.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
// DataManager class
//
DataManager::DataManager()
{
    _labelMap = NULL;
    _structuredPoints = NULL;
    _lookupTable = NULL;
    _orientationData = NULL;
    _firstFreeValue = 1;
    
    // create our undo/redo buffers (default capacity 150 megabytes)
    // undo/redo buffers aren't allocated at init, only when needed
    _actionsBuffer = new UndoRedoSystem(this);
}

void DataManager::Initialize(itk::SmartPointer<LabelMapType> labelMap, Coordinates *OrientationData, Metadata *data)
{
    _orientationData = OrientationData;
  	Vector3d spacing = _orientationData->GetImageSpacing();
   	Vector3d imageorigin = _orientationData->GetImageOrigin();
   	Vector3ui imagesize = _orientationData->GetImageSize();

   	// insert background label info, initially all voxels are background, we'll subtract later
   	struct ObjectInformation *object = new struct ObjectInformation;
   	object->scalar = 0;
   	object->centroid = Vector3d((imagesize[0]/2.0)*spacing[0], (imagesize[1]/2.0)*spacing[1], (imagesize[2]/2.0)/spacing[2]);
   	object->sizeInVoxels = imagesize[0] * imagesize[1] * imagesize[2];
   	object->min = Vector3ui(0,0,0);
   	object->max = Vector3ui(imagesize[0], imagesize[1], imagesize[2]);

   	ObjectVector.insert(std::pair<unsigned short, ObjectInformation*>(0,object));

    // evaluate shapelabelobjects to get the centroid of the object
    itk::SmartPointer<itk::ShapeLabelMapFilter<LabelMapType> > evaluator = itk::ShapeLabelMapFilter<LabelMapType>::New();
    evaluator->SetInput(labelMap);
    evaluator->ComputePerimeterOff();
    evaluator->ComputeFeretDiameterOff();
    evaluator->SetInPlace(true);
    evaluator->Update();

    // get voxel count for each label for statistics and "flatten" the labelmap (make all labels consecutive starting from 1)
    typedef itk::ChangeLabelLabelMapFilter<LabelMapType> ChangeType;
    itk::SmartPointer<ChangeType> labelChanger = ChangeType::New();
    labelChanger->SetInput(evaluator->GetOutput());
  	labelChanger->SetInPlace(true);

  	typedef itk::ImageRegion<3> ImageRegionType;
  	ImageRegionType region;
    unsigned short i = 1;
    itk::Point<double, 3> centroid;

    for(int i = 0; i < evaluator->GetOutput()->GetNumberOfLabelObjects(); ++i)
    {
        LabelObjectType * labelObject = evaluator->GetOutput()->GetNthLabelObject(i);
        LabelObjectType::LabelType scalar = labelObject->GetLabel();
        centroid = labelObject->GetCentroid();
        region = labelObject->GetBoundingBox();
        itk::Index<3> regionOrigin = region.GetIndex();
        itk::Size<3> regionSize = region.GetSize();

        object = new struct ObjectInformation;
        object->scalar = scalar;
        object->centroid = Vector3d(centroid[0]/spacing[0],centroid[1]/spacing[1],centroid[2]/spacing[2]);
        object->sizeInVoxels = labelObject->Size();
        object->min = Vector3ui(regionOrigin[0], regionOrigin[1], regionOrigin[2]);
        object->max = Vector3ui(regionSize[0]+regionOrigin[0], regionSize[1]+regionOrigin[1], regionSize[2]+regionOrigin[2]) - Vector3ui(1,1,1);

        ObjectVector.insert(std::pair<unsigned short, ObjectInformation*>(i,object));

        // substract the voxels of this object from the background label
        ObjectVector[0]->sizeInVoxels -= labelObject->Size();

        // need to mark object label as used to correct errors in the segmha metadata (defined labels but empty objects)
        data->MarkObjectAsUsed(scalar);

        // flatten label
        labelChanger->SetChange(scalar,i);
    }

    // start entering new labels at the end of the scalar range
    this->_firstFreeValue = this->GetScalarForLabel(this->GetNumberOfLabels()-1)+1;

    // apply all the changes made to labels
    labelChanger->Update();

    _labelMap = LabelMapType::New();
    _labelMap = labelChanger->GetOutput();
    _labelMap->Optimize();
    _labelMap->Update();

    // generate the initial vtkLookupTable
    _lookupTable = vtkSmartPointer<vtkLookupTable>::New();
    GenerateLookupTable();
    _lookupTable->Modified();
}

DataManager::~DataManager()
{
	// delete manually allocated memory
	std::map<unsigned short, struct ObjectInformation*>::iterator objectit;
	for (objectit = ObjectVector.begin(); objectit != ObjectVector.end(); objectit++)
		delete (*objectit).second;
	ObjectVector.clear();

	std::map<unsigned short, struct ActionInformation*>::iterator actionit;
	for (actionit = ActionInformationVector.begin(); actionit != ActionInformationVector.end(); actionit++)
		delete (*actionit).second;
	ActionInformationVector.clear();

	// this deletes the objects created by datamanager and stored in the redo buffer, finally deleting all
	// dinamically allocated objects. The objects stored in the undo buffer have been already deleted
	// because they were in the ObjectVector in DataManager.
    delete _actionsBuffer;
}

void DataManager::SetStructuredPoints(vtkSmartPointer<vtkStructuredPoints> points)
{
    _structuredPoints = vtkSmartPointer<vtkStructuredPoints>::New();
    _structuredPoints->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
    _structuredPoints->CopyInformationFromPipeline(points->GetInformation());
    _structuredPoints->DeepCopy(points);
    _structuredPoints->Modified();
}

vtkSmartPointer<vtkStructuredPoints> DataManager::GetStructuredPoints()
{
    return _structuredPoints;
}

itk::SmartPointer<LabelMapType> DataManager::GetLabelMap()
{
    return _labelMap;
}

Coordinates * DataManager::GetOrientationData()
{
    return _orientationData;
}

std::map<unsigned short, struct DataManager::ObjectInformation*>* DataManager::GetObjectTablePointer()
{
    return &ObjectVector;
}

unsigned short DataManager::GetVoxelScalar(unsigned int x, unsigned int y, unsigned int z)
{
    unsigned short* pixel = static_cast<unsigned short*> (_structuredPoints->GetScalarPointer(x,y,z));
    return *pixel;
}

void DataManager::SetVoxelScalar(unsigned int x, unsigned int y, unsigned int z, unsigned short scalar)
{
    unsigned short* pixel = static_cast<unsigned short*> (_structuredPoints->GetScalarPointer(x,y,z));

    if (scalar == *pixel)
        return;

    if (ActionInformationVector.find(*pixel) == ActionInformationVector.end())
    {
    	struct ActionInformation *action = new struct ActionInformation;
    	ActionInformationVector[*pixel] = action;
    	action->min = Vector3ui(x,y,z);
    	action->max = Vector3ui(x,y,z);
    }
    ActionInformationVector[*pixel]->sizeInVoxels -= 1;
    ActionInformationVector[*pixel]->temporalCentroid[0] -= x;
    ActionInformationVector[*pixel]->temporalCentroid[1] -= y;
    ActionInformationVector[*pixel]->temporalCentroid[2] -= z;

    if (ActionInformationVector.find(scalar) == ActionInformationVector.end())
    {
    	struct ActionInformation *action = new struct ActionInformation;
    	ActionInformationVector[scalar] = action;
    	action->min = Vector3ui(x,y,z);
    	action->max = Vector3ui(x,y,z);
    }
    ActionInformationVector[scalar]->sizeInVoxels += 1;
    ActionInformationVector[scalar]->temporalCentroid[0] += x;
    ActionInformationVector[scalar]->temporalCentroid[1] += y;
    ActionInformationVector[scalar]->temporalCentroid[2] += z;

    // have to check if the added voxel is out of the object region to make the bounding box grow
    Vector3ui min = ActionInformationVector[scalar]->min;
    Vector3ui max = ActionInformationVector[scalar]->max;

    if (x < min[0])
     	min[0] = x;

    if (x > max[0])
       	max[0] = x;

    if (y < min[1])
       	min[1] = y;

    if (y > max[1])
      	max[1] = y;

    if (z < min[2])
      	min[2] = z;

    if (z > max[2])
       	max[2] = z;

    ActionInformationVector[scalar]->min = min;
    ActionInformationVector[scalar]->max = max;

    _actionsBuffer->StorePoint(Vector3ui(x,y,z), *pixel);
    *pixel = scalar;
}

void DataManager::SetVoxelScalarRaw(unsigned int x, unsigned int y, unsigned int z, unsigned short scalar)
{
    unsigned short* pixel = static_cast<unsigned short*> (_structuredPoints->GetScalarPointer(x,y,z));
    
    if (scalar == *pixel)
        return;
    
    *pixel = scalar;
}

unsigned short DataManager::SetLabel(Vector3d rgb)
{
	// labelvalues usually goes 0-n, that's n+1 values = _labelValues.size();
    unsigned short newlabel = ObjectVector.size();
    
    // we need to find an unused scalar value in our table
    bool free = false;
    unsigned short freevalue = _firstFreeValue;
    std::map<unsigned short, struct ObjectInformation*>::iterator it;
    
    while (!free)
    {
        free = true;
        
        // can't use find() as the value we're searching is the 'second' field
        for (it = ObjectVector.begin(); it != ObjectVector.end(); it++)
            if ((*it).second->scalar == freevalue)
            {
                free = false;
                freevalue++;
            }
    }
    
    struct ObjectInformation *object = new struct ObjectInformation;
    object->scalar = freevalue;
    object->sizeInVoxels = 0;
    object->centroid = Vector3d(0,0,0);
    object->min = Vector3ui(0,0,0);
    object->max = Vector3ui(0,0,0);

    ObjectVector.insert(std::pair<unsigned short, struct ObjectInformation*>(newlabel,object));

    _actionsBuffer->StoreObject(std::pair<unsigned short, struct ObjectInformation*>(newlabel,object));

    vtkSmartPointer<vtkLookupTable> temptable = vtkSmartPointer<vtkLookupTable>::New();
    CopyLookupTable(_lookupTable, temptable);

    // this is a convoluted way of doing things, but SetNumberOfTableValues() seems to
    // corrup the table (due to reallocation?) and all values must be copied again.
    _lookupTable->SetNumberOfTableValues(newlabel+1);
    double rgba[4];
    for (int index = 0; index != temptable->GetNumberOfTableValues(); index++)
    {
        temptable->GetTableValue(index, rgba);
        _lookupTable->SetTableValue(index,rgba);
    }
    _lookupTable->SetTableValue(newlabel, rgb[0], rgb[1], rgb[2], 0.4);
    _lookupTable->SetTableRange(0,newlabel);
    _lookupTable->Modified();

    return newlabel;
}

void DataManager::CopyLookupTable(vtkSmartPointer<vtkLookupTable> copyFrom, vtkSmartPointer<vtkLookupTable> copyTo)
{
	// copyTo exists and i don't want to do just a DeepCopy that could release memory, i just want to copy the colors
    double rgba[4];

    copyTo->Allocate();
    copyTo->SetNumberOfTableValues(copyFrom->GetNumberOfTableValues());

    for (int index = 0; index != copyFrom->GetNumberOfTableValues(); index++)
    {
        copyFrom->GetTableValue(index, rgba);
        copyTo->SetTableValue(index,rgba);
    }

    copyTo->SetTableRange(0,copyFrom->GetNumberOfTableValues()-1);
}

void DataManager::GenerateLookupTable()
{
    // compute vtkLookupTable colors for labels, first color is black for background
    // rest are precalculated by _lookupTable->Build() based on the number of labels
    double rgba[4] = { 0.05, 0.05, 0.05, 1.0 };
    unsigned int labels = _labelMap->GetNumberOfLabelObjects();
    assert(0 != labels);
    
    _lookupTable->Allocate();
    _lookupTable->SetNumberOfTableValues(labels+1);
    _lookupTable->SetTableRange(0,labels+1);
    _lookupTable->SetTableValue(0,rgba);
    
    vtkLookupTable *temporal_table = vtkLookupTable::New();
    temporal_table->SetNumberOfTableValues(labels);
    temporal_table->SetRange(1,labels);
    temporal_table->Build();
    
    for (unsigned int index = 0; index != labels; index++)
    {
        temporal_table->GetTableValue(index, rgba);
        _lookupTable->SetTableValue(index+1,rgba[0], rgba[1], rgba[2], 0.4);
    }
    temporal_table->Delete();
}

void DataManager::SwitchLookupTables(vtkSmartPointer<vtkLookupTable> table)
{
    vtkSmartPointer<vtkLookupTable> temptable = vtkSmartPointer<vtkLookupTable>::New();
    
    CopyLookupTable(_lookupTable, temptable);
    CopyLookupTable(table, _lookupTable);
    CopyLookupTable(temptable, table);
    
    _lookupTable->Modified();
}

void DataManager::StatisticsActionClear(void)
{
	std::map<unsigned short, struct ActionInformation*>::iterator it;
	for (it = ActionInformationVector.begin(); it != ActionInformationVector.end(); it++)
		delete (*it).second;

	ActionInformationVector.clear();
}

void DataManager::OperationStart(std::string actionName)
{
	StatisticsActionClear();
    _actionsBuffer->SignalBeginAction(actionName, this->_selectedLabels, this->_lookupTable);
}

void DataManager::OperationEnd()
{
    _actionsBuffer->SignalEndAction();
    StatisticsActionJoin();
}

void DataManager::OperationCancel()
{
    _actionsBuffer->SignalCancelAction();
}

std::string DataManager::GetUndoActionString()
{
    return _actionsBuffer->GetActionString(UndoRedoSystem::UNDO);
}

std::string DataManager::GetRedoActionString()
{
    return _actionsBuffer->GetActionString(UndoRedoSystem::REDO);
}

std::string DataManager::GetActualActionString()
{
    return _actionsBuffer->GetActionString(UndoRedoSystem::ACTUAL);
}

bool DataManager::IsUndoBufferEmpty()
{
    return _actionsBuffer->IsEmpty(UndoRedoSystem::UNDO);
}

bool DataManager::IsRedoBufferEmpty()
{
    return _actionsBuffer->IsEmpty(UndoRedoSystem::REDO);
}

void DataManager::DoUndoOperation()
{
	StatisticsActionClear();
	_actionsBuffer->DoAction(UndoRedoSystem::UNDO);
    StatisticsActionJoin();
}

void DataManager::DoRedoOperation()
{
	StatisticsActionClear();
	_actionsBuffer->DoAction(UndoRedoSystem::REDO);
    StatisticsActionJoin();
}

void DataManager::SetUndoRedoBufferSize(unsigned long size)
{
    _actionsBuffer->ChangeSize(size);
}

unsigned long int DataManager::GetUndoRedoBufferSize()
{
    return _actionsBuffer->GetSize();
}

unsigned long int DataManager::GetUndoRedoBufferCapacity()
{
    return _actionsBuffer->GetCapacity();
}

void DataManager::SetFirstFreeValue(unsigned short value)
{
    _firstFreeValue = value;
}

unsigned short DataManager::GetFirstFreeValue()
{
    return _firstFreeValue;
}

unsigned short DataManager::GetLastUsedValue()
{
    std::map<unsigned short, struct ObjectInformation*>::iterator it;
    unsigned short value = 0;

    for (it = ObjectVector.begin(); it != ObjectVector.end(); it++)
        if ((*it).second->scalar > value)
            value = (*it).second->scalar;

    return value;
}

double* DataManager::GetRGBAColorForScalar(unsigned short scalar)
{
    double* rgba = new double[4];
    
    std::map<unsigned short, struct ObjectInformation*>::iterator it;
    
    for (it = ObjectVector.begin(); it != ObjectVector.end(); it++)
        if ((*it).second->scalar == scalar)
        {
            _lookupTable->GetTableValue((*it).first, rgba);
            return rgba;
        }

    // not found
    delete rgba;
    return NULL;
}

void DataManager::StatisticsActionJoin(void)
{
    std::map<unsigned short, struct ActionInformation*>::iterator it;

    for (it = ActionInformationVector.begin(); it != ActionInformationVector.end(); it++)
    {
    	// the action information could refer to a deleted label (in undo/redo buffer)
    	if (ObjectVector.count((*it).first) == 0)
    		continue;

    	// no need to recalculate centroid or bounding box for background label
    	if ((*it).first != 0)
    	{
    		// calculate new centroid based on modified voxels
        	if (0 == (ObjectVector[(*it).first]->sizeInVoxels + (*it).second->sizeInVoxels))
        		ObjectVector[(*it).first]->centroid = Vector3d(0,0,0);
        	else
        	{
        		double x = ((*it).second->temporalCentroid[0] / static_cast<double>((*it).second->sizeInVoxels));
        		double y = ((*it).second->temporalCentroid[1] / static_cast<double>((*it).second->sizeInVoxels));
        		double z = ((*it).second->temporalCentroid[2] / static_cast<double>((*it).second->sizeInVoxels));

        		Vector3d centroid = ObjectVector[(*it).first]->centroid;
        		unsigned long long int objectVoxels = ObjectVector[(*it).first]->sizeInVoxels;

        		// if the object has a centroid
        		if ((Vector3d(0,0,0) != centroid) && (0 != objectVoxels))
        		{
        			double coef_1 = objectVoxels / static_cast<double>(objectVoxels + (*it).second->sizeInVoxels);
        			double coef_2 = (*it).second->sizeInVoxels / static_cast<double>(objectVoxels + (*it).second->sizeInVoxels);

					x = (centroid[0] * coef_1) + (x * coef_2);
					y = (centroid[1] * coef_1) + (y * coef_2);
					z = (centroid[2] * coef_1) + (z * coef_2);
        		}
        		ObjectVector[(*it).first]->centroid = Vector3d(x,y,z);
        	}

        	// if the object doesn't have voxels means that the calculated bounding box is the object bounding box
        	if (0LL == ObjectVector[(*it).first]->sizeInVoxels)
        	{
        		ObjectVector[(*it).first]->min = (*it).second->min;
        		ObjectVector[(*it).first]->max = (*it).second->max;
        	}
        	// else calculate new bounding box based on modified voxels, but only if we've been adding voxels, not substracting them
        	else
        		if (0LL < (*it).second->sizeInVoxels)
        		{
        			if (ObjectVector[(*it).first]->min[0] > (*it).second->min[0])
        		 		ObjectVector[(*it).first]->min[0] = (*it).second->min[0];

        			if (ObjectVector[(*it).first]->min[1] > (*it).second->min[1])
        				ObjectVector[(*it).first]->min[1] = (*it).second->min[1];

        			if (ObjectVector[(*it).first]->min[2] > (*it).second->min[2])
        		 		ObjectVector[(*it).first]->min[2] = (*it).second->min[2];

        			if (ObjectVector[(*it).first]->max[0] < (*it).second->max[0])
        		 		ObjectVector[(*it).first]->max[0] = (*it).second->max[0];

        			if (ObjectVector[(*it).first]->max[1] < (*it).second->max[1])
        				ObjectVector[(*it).first]->max[1] = (*it).second->max[1];

        			if (ObjectVector[(*it).first]->max[2] < (*it).second->max[2])
        		 		ObjectVector[(*it).first]->max[2] = (*it).second->max[2];
        		 }
    	}
    	ObjectVector[(*it).first]->sizeInVoxels += (*it).second->sizeInVoxels;
    }
}

unsigned long long int DataManager::GetNumberOfVoxelsForLabel(const unsigned short label)
{
	assert(label < ObjectVector.size());
    return ObjectVector[label]->sizeInVoxels;
}

unsigned short DataManager::GetScalarForLabel(const unsigned short label)
{
	assert(label < ObjectVector.size());
	return ObjectVector[label]->scalar;
}

unsigned short DataManager::GetLabelForScalar(const unsigned short scalar)
{
    std::map<unsigned short, struct ObjectInformation*>::iterator it;
    
    // find the label number of the label value (scalar), can't use find() on map
    // because we're looking the 'second' field
    for (it = ObjectVector.begin(); it != ObjectVector.end(); it++)
        if ((*it).second->scalar == scalar)
            return (*it).first;

	return 0;
}

Vector3d DataManager::GetCentroidForObject(const unsigned short int label)
{
	assert(label < ObjectVector.size());
	return ObjectVector[label]->centroid;
}

Vector3ui DataManager::GetBoundingBoxMin(const unsigned short label)
{
	assert(label < ObjectVector.size());
	return ObjectVector[label]->min;
}

Vector3ui DataManager::GetBoundingBoxMax(const unsigned short label)
{
	assert(label < ObjectVector.size());
	return ObjectVector[label]->max;
}

unsigned int DataManager::GetNumberOfLabels(void)
{
	return ObjectVector.size();
}

void DataManager::ColorHighlight(const unsigned short label)
{
	if (0 == label)
		return;

	if (_selectedLabels.find(label) == _selectedLabels.end())
	{
		double rgba[4];

		this->_lookupTable->GetTableValue(label, rgba);
		this->_lookupTable->SetTableValue(label, rgba[0], rgba[1], rgba[2], 1);

		_selectedLabels.insert(label);
		this->_lookupTable->Modified();
	}
}

void DataManager::ColorDim(const unsigned short label)
{
	if (_selectedLabels.find(label) != _selectedLabels.end())
	{
		double rgba[4];

		this->_lookupTable->GetTableValue(label, rgba);
		this->_lookupTable->SetTableValue(label, rgba[0], rgba[1], rgba[2], 0.4);

		_selectedLabels.erase(label);
		this->_lookupTable->Modified();
	}
}

void DataManager::ColorHighlightExclusive(const unsigned short label)
{
	std::set<unsigned short>::iterator it;
	for (it = _selectedLabels.begin(); it != _selectedLabels.end(); it++)
	{
		if ((*it) == label)
			continue;

		double rgba[4];

		this->_lookupTable->GetTableValue(*it, rgba);
		this->_lookupTable->SetTableValue(*it, rgba[0], rgba[1], rgba[2], 0.4);
		_selectedLabels.erase(*it);
	}

	if (_selectedLabels.find(label) == _selectedLabels.end())
		ColorHighlight(label);

	this->_lookupTable->Modified();
}

void DataManager::ColorDimAll()
{
	std::set<unsigned short>::iterator it;
	for (it = _selectedLabels.begin(); it != _selectedLabels.end(); it++)
	{
		double rgba[4];

		this->_lookupTable->GetTableValue((*it), rgba);
		this->_lookupTable->SetTableValue((*it), rgba[0], rgba[1], rgba[2], 0.4);

		_selectedLabels.erase(*it);
	}
	this->_lookupTable->Modified();
}

bool DataManager::ColorIsInUse(double* color)
{
	double rgba[4];

    for (int i = 0; i < this->_lookupTable->GetNumberOfTableValues(); i++)
    {
        this->_lookupTable->GetTableValue(i, rgba);
        if ((rgba[0] == color[0]) && (rgba[1] == color[1]) && (rgba[2] == color[2]))
            return true;
    }

    return false;
}

unsigned int DataManager::GetNumberOfColors()
{
	return this->_lookupTable->GetNumberOfTableValues();
}

void DataManager::GetColorComponents(unsigned short label, double* rgba)
{
	this->_lookupTable->GetTableValue(label, rgba);
}

void DataManager::SetColorComponents(unsigned short label, double* rgba)
{
	this->_lookupTable->SetTableValue(label, rgba);
	this->_lookupTable->Modified();
}

vtkSmartPointer<vtkLookupTable> DataManager::GetLookupTable()
{
	return this->_lookupTable;
}

const std::set<unsigned short> DataManager::GetSelectedLabelsSet(void)
{
	return this->_selectedLabels;
}

const bool DataManager::IsColorSelected(unsigned short color)
{
	return (this->_selectedLabels.find(color) != this->_selectedLabels.end());
}

void DataManager::SetSelectedLabelsSet(std::set<unsigned short> labelSet)
{
	this->_selectedLabels = labelSet;
}

const int DataManager::GetSelectedLabelSetSize(void)
{
	return this->_selectedLabels.size();
}

void DataManager::SignalDataAsModified(void)
{
	this->_structuredPoints->Modified();
}
