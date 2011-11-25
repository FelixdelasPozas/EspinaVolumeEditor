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
#include <itkExceptionObject.h>
#include <itkChangeLabelLabelMapFilter.h>
#include <itkShapeLabelMapFilter.h>

// project includes
#include "DataManager.h"
#include "VectorSpaceAlgebra.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
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

void DataManager::Initialize(itk::SmartPointer<LabelMapType> labelMap, Coordinates *OrientationData, Metadata* data)
{
    _orientationData = OrientationData;
    
    // initalize label values
    _labelValues.clear();
    _labelValues.insert(std::pair<unsigned short, unsigned short>(0,0));

    // clear voxel statistics and set data from the background label
    Vector3ui transformedsize = _orientationData->GetTransformedSize();
    _voxelCount.clear();
    _voxelCount.insert(std::pair<unsigned short, unsigned long long int>(0,static_cast<unsigned long long>(transformedsize[0])*static_cast<unsigned long long>(transformedsize[1])*static_cast<unsigned long long>(transformedsize[2])));


    // evaluate shapelabelobjects to get the centroid of the object
    itk::SmartPointer<itk::ShapeLabelMapFilter<LabelMapType> > evaluator = itk::ShapeLabelMapFilter<LabelMapType>::New();
    evaluator->SetInput(labelMap);
    evaluator->ComputePerimeterOff();
    evaluator->ComputeFeretDiameterOff();
    evaluator->SetInPlace(true);
    evaluator->Update();

    // get voxel count for each label for statistics and "flatten" the labelmap (make all labels consecutive starting from 1)
    LabelMapType::LabelObjectContainerType::const_iterator iter;
    const LabelMapType::LabelObjectContainerType & labelObjectContainer = evaluator->GetOutput()->GetLabelObjectContainer();

    typedef itk::ChangeLabelLabelMapFilter<LabelMapType> ChangeType;
    itk::SmartPointer<ChangeType> labelChanger = ChangeType::New();
    labelChanger->SetInput(evaluator->GetOutput());
  	labelChanger->SetInPlace(true);

    // we can't init different type variables in the for loop init, i must be init here
    unsigned short i = 1;
    itk::Point<double, 3> centroid;
    Vector3d spacing = _orientationData->GetImageSpacing();

    for(iter = labelObjectContainer.begin(); iter != labelObjectContainer.end(); iter++, i++)
    {
        const unsigned short scalar = iter->first;
        LabelObjectType * labelObject = iter->second;
        centroid = labelObject->GetCentroid();

        _labelValues.insert(std::pair<unsigned short, unsigned short>(i,scalar));
        _voxelCount.insert(std::pair<unsigned short, unsigned long long int>(i, labelObject->Size()));
        _voxelCount[0] -= _voxelCount[i];
        _objectCentroid.insert(std::pair<unsigned short, Vector3d>(i, Vector3d(centroid[0]/spacing[0],centroid[1]/spacing[1],centroid[2]/spacing[2])));

        data->MarkObjectAsUsed(scalar);
        labelChanger->SetChange(scalar,i);
    }

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
    // just delete the undo/redo system, as the rest are handled by smartpointers
    delete _actionsBuffer;
}

void DataManager::SetStructuredPoints(vtkSmartPointer<vtkStructuredPoints> points)
{
    _structuredPoints = vtkSmartPointer<vtkStructuredPoints>::New();
    _structuredPoints->SetNumberOfScalarComponents(1);
    _structuredPoints->SetScalarTypeToUnsignedChar();
    _structuredPoints->CopyInformation(points);
    _structuredPoints->AllocateScalars();
    _structuredPoints->DeepCopy(points);
    _structuredPoints->Update();
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

std::map<unsigned short, unsigned short>* DataManager::GetLabelValueTable()
{
    return &_labelValues;
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

    _voxelActionCount[*pixel]--;
    _voxelActionCount[scalar]++;
    
    // calculate centroid variation for both objects
    Vector3d centroid = _objectCentroid[(*pixel)];
    _temporalCentroid[*pixel][0] -= x;
    _temporalCentroid[*pixel][1] -= y;
    _temporalCentroid[*pixel][2] -= z;
    centroid = _objectCentroid[scalar];
    _temporalCentroid[scalar][0] += x;
    _temporalCentroid[scalar][1] += y;
    _temporalCentroid[scalar][2] += z;

    _actionsBuffer->AddPoint(Vector3ui(x,y,z), *pixel);
    *pixel = scalar;
    _structuredPoints->Modified();
}

void DataManager::SetVoxelScalarRaw(unsigned int x, unsigned int y, unsigned int z, unsigned short scalar)
{
    unsigned short* pixel = static_cast<unsigned short*> (_structuredPoints->GetScalarPointer(x,y,z));
    
    if (scalar == *pixel)
        return;
    
    *pixel = scalar;
    _structuredPoints->Modified();
}

int DataManager::GetNumberOfLabels()
{
    // all labels plus background label
    return _labelValues.size()+1;
}

vtkSmartPointer<vtkLookupTable> DataManager::GetLookupTable()
{
    return _lookupTable;
}

unsigned short DataManager::SetLabel(Vector3d rgb)
{
	// labelvalues usually goes 0-n, that's n+1 values = _labelValues.size();
    unsigned short newlabel = _labelValues.size();
    
    // we need to find an unused value in our table 
    bool free = false;
    unsigned short freevalue = _firstFreeValue;
    std::map<unsigned short, unsigned short>::iterator it;
    
    while (!free)
    {
        free = true;
        
        // can't use find() as the value we're searching is the 'second' field
        for (it = _labelValues.begin(); it != _labelValues.end(); it++)
            if ((*it).second == freevalue)
                free = false;
        
        if (free == false)
            freevalue++;
    }
    
    _labelValues.insert(std::pair<unsigned short, unsigned short>(newlabel,freevalue));
    _voxelCount.insert(std::pair<unsigned short, unsigned long long int>(newlabel, 0));
    _objectCentroid.insert(std::pair<unsigned short, Vector3d>(newlabel,Vector3d(0,0,0)));
    _actionsBuffer->StoreLabelValue(std::pair<unsigned short, unsigned short>(newlabel,freevalue));

    vtkSmartPointer<vtkLookupTable> temptable = vtkSmartPointer<vtkLookupTable>::New();
    CopyLookupTable(_lookupTable, temptable);
    _actionsBuffer->StoreLookupTable(temptable);

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

void DataManager::OperationStart(std::string actionName)
{
    _voxelActionCount.clear();
    _temporalCentroid.clear();
    _actionsBuffer->SignalBeginAction(actionName);
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
   	_voxelActionCount.clear();
   	_temporalCentroid.clear();
    _actionsBuffer->DoAction(UndoRedoSystem::UNDO);
    StatisticsActionJoin();
}

void DataManager::DoRedoOperation()
{
	_voxelActionCount.clear();
	_temporalCentroid.clear();
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
    std::map<unsigned short, unsigned short>::iterator it;
    unsigned short value = 0;

    for (it = _labelValues.begin(); it != _labelValues.end(); it++)
        if ((*it).second > value)
            value = (*it).second;

    return value;
}

double* DataManager::GetRGBAColorForScalar(unsigned short scalar)
{
    double* rgba = new double[4];
    
    std::map<unsigned short, unsigned short>::iterator it;
    
    for (it = _labelValues.begin(); it != _labelValues.end(); it++)
        if ((*it).second == scalar)
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
    std::map<unsigned short, long long int>::iterator it;

    for (it = _voxelActionCount.begin(); it != _voxelActionCount.end(); it++)
    {
    	if ((*it).first != 0)
    	{
        	if (0 == (_voxelCount[(*it).first] + (*it).second))
        		_objectCentroid[(*it).first] = Vector3d(0,0,0);
        	else
        	{
        		double x = (_temporalCentroid[(*it).first][0] / static_cast<double>((*it).second));
        		double y = (_temporalCentroid[(*it).first][1] / static_cast<double>((*it).second));
        		double z = (_temporalCentroid[(*it).first][2] / static_cast<double>((*it).second));

        		Vector3d centroid = _objectCentroid[(*it).first];
        		unsigned long long int objectVoxels = GetNumberOfVoxelsForLabel((*it).first);

        		// if the object has a centroid
         		if ((Vector3d(0.0,0.0,0.0) != centroid) && (0 != objectVoxels))
        		{
        			double coef_1 = objectVoxels / static_cast<double>(objectVoxels +(*it).second);
        			double coef_2 = (*it).second / static_cast<double>(objectVoxels +(*it).second);

					x = (centroid[0] * coef_1) + (x * coef_2);
					y = (centroid[1] * coef_1) + (y * coef_2);
					z = (centroid[2] * coef_1) + (z * coef_2);
        		}
        		_objectCentroid[(*it).first] = Vector3d(x,y,z);
        	}
    	}
    	_voxelCount[(*it).first] += (*it).second;
    }
}

unsigned long long int DataManager::GetNumberOfVoxelsForLabel(unsigned short label)
{
    return _voxelCount[label];
}

unsigned short DataManager::GetScalarForLabel(unsigned short colorNum)
{
	return _labelValues[colorNum];
}

unsigned short DataManager::GetLabelForScalar(unsigned short scalar)
{
    std::map<unsigned short, unsigned short>::iterator it;
    
    // find the label number of the label value (scalar), can't use find() on map
    // because we're looking the 'second' field
    for (it = _labelValues.begin(); it != _labelValues.end(); it++)
        if ((*it).second == scalar)
            return (*it).first;

	return 0;
}

Vector3d DataManager::GetCentroidForObject(unsigned short int label)
{
	return _objectCentroid[label];
}
