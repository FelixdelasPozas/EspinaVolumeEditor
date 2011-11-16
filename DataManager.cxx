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
    
    // initalize label statistics
    StatisticsReset();
    
    // create our undo/redo buffers (default capacity 150 megabytes)
    // undo/redo buffers aren't allocated at init, only when needed
    _actionsBuffer = new UndoRedoSystem(this);
}

void DataManager::Initialize(itk::SmartPointer<LabelMapType> labelMap, Coordinates *OrientationData)
{
    _orientationData = OrientationData;
    
    // initalize label statistics and label values
    _labelValues.clear();
    _labelValues.insert(std::pair<unsigned short, unsigned short>(0,0));
    StatisticsReset();

    Vector3ui transformedsize = _orientationData->GetTransformedSize();
    voxelCount[0] = static_cast<unsigned long long>(transformedsize[0])*static_cast<unsigned long long>(transformedsize[1])*static_cast<unsigned long long>(transformedsize[2]);
    
    // get voxel count for each label for statistics and "flatten" the labelmap (make all labels consecutive starting from 1)
    LabelMapType::LabelObjectContainerType::const_iterator iter;
    const LabelMapType::LabelObjectContainerType & labelObjectContainer = labelMap->GetLabelObjectContainer();

    typedef itk::ChangeLabelLabelMapFilter<LabelMapType> ChangeType;
    itk::SmartPointer<ChangeType> labelChanger = ChangeType::New();
    labelChanger->SetInput(labelMap);
    if (labelChanger->CanRunInPlace() == true)
    	labelChanger->SetInPlace(true);

    // we can't init different type variables in the for loop init, i must be init here
    unsigned short i = 1;
    for(iter = labelObjectContainer.begin(); iter != labelObjectContainer.end(); iter++, i++)
    {
        const unsigned short scalar = iter->first;
        LabelObjectType * labelObject = iter->second;

        _labelValues.insert(std::pair<unsigned short, unsigned short>(i,scalar));
        labelChanger->SetChange(scalar,i);
        voxelCount[i] += labelObject->Size();
        voxelCount[0] -= voxelCount[i];
    }

    // apply all the changes made to labels
    labelChanger->Update();
    _labelMap = LabelMapType::New();
    _labelMap = labelChanger->GetOutput();
    float origin[3] = { 0.0, 0.0, 0.0 };
    _labelMap->SetOrigin(origin);
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

    voxelActionCount[GetLabelForScalar(*pixel)]--;
    voxelActionCount[GetLabelForScalar(scalar)]++;
    
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

// we use itk::ExceptionObject
unsigned short DataManager::SetLabel(Vector3d rgb, double coloralpha) throw(itk::ExceptionObject)
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
        
        // have to check if we are generating more labels than the space reserved in the lookuptable
        if ((free == false) && (freevalue == (_lookupTable->GetNumberOfColors()-1)))
        {
            std::ostringstream message;
            message << "ERROR: DataManager(" << this << "): Reached the limit of the number of possible labels (" << static_cast<int>(_lookupTable->GetNumberOfColors()) << ")";
            itk::ExceptionObject e_(__FILE__, __LINE__, message.str().c_str(), ITK_LOCATION);
            throw e_; // Explicit naming to work around Intel compiler bug.
            return 0;
        }
        
        if (free == false)
            freevalue++;
    }
    
    voxelCount[newlabel] = 0;
    
    _labelValues.insert(std::pair<unsigned short, unsigned short>(newlabel,freevalue));
    _actionsBuffer->StoreLabelValue(std::pair<unsigned short, unsigned short>(newlabel,freevalue));

    vtkSmartPointer<vtkLookupTable> temptable = vtkSmartPointer<vtkLookupTable>::New();
    CopyLookupTable(_lookupTable, temptable);
    _actionsBuffer->StoreLookupTable(temptable);

    _lookupTable->SetNumberOfTableValues(newlabel+1);
    _lookupTable->SetTableValue(newlabel, rgb[0], rgb[1], rgb[2], coloralpha);
    _lookupTable->SetTableRange(0,newlabel);
    _lookupTable->Modified();
    
    return newlabel;
}

void DataManager::GenerateLookupTable()
{
    // compute vtkLookupTable colors for labels, first color is black for background
    // rest are precalculated by _lookupTable->Build() based on the number of labels
    double rgba[4] = { 0.05, 0.05, 0.05, 1.0 };
    unsigned int labels = _labelMap->GetNumberOfLabelObjects();
    assert(0 != labels);
    
    // initially we will define 2048 colors
    _lookupTable->Allocate(2048);
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
        _lookupTable->SetTableValue(index+1,rgba);
    }
    temporal_table->Delete();
}

void DataManager::CopyLookupTable(vtkSmartPointer<vtkLookupTable> copyFrom, vtkSmartPointer<vtkLookupTable> copyTo)
{
	// copyTo exists and i don't want to do just a DeepCopy that could release memory, i just want to copy the colors
    double rgba[4];
    
    copyTo->Allocate(copyFrom->GetNumberOfColors());
    copyTo->SetNumberOfTableValues(copyFrom->GetNumberOfTableValues());
    copyTo->SetTableRange(0,copyFrom->GetNumberOfTableValues()-1);
    
    for (int index = 0; index != copyFrom->GetNumberOfTableValues(); index++)
    {
        copyFrom->GetTableValue(index, rgba);
        copyTo->SetTableValue(index,rgba);
    }
}

void DataManager::ExchangeLookupTables(vtkSmartPointer<vtkLookupTable> table)
{
    vtkSmartPointer<vtkLookupTable> temptable = vtkSmartPointer<vtkLookupTable>::New();
    
    CopyLookupTable(_lookupTable, temptable);
    CopyLookupTable(table, _lookupTable);
    CopyLookupTable(temptable, table);
    
    _lookupTable->Modified();
}

void DataManager::OperationStart(std::string actionName)
{
    StatisticsActionReset();
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
    StatisticsActionReset();
    _actionsBuffer->DoAction(UndoRedoSystem::UNDO);
    StatisticsActionJoin();
}

void DataManager::DoRedoOperation()
{
    StatisticsActionReset();
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

void DataManager::StatisticsActionReset(void)
{
    for (unsigned int i = 0; i < 2048; i++)
        voxelActionCount[i] = 0;
}

void DataManager::StatisticsActionJoin(void)
{
    for (unsigned int i = 0; i < 2048; i++)
        voxelCount[i] += voxelActionCount[i];
}

void DataManager::StatisticsReset(void)
{
    for (unsigned int i = 0; i < 2048; i++)
        voxelCount[i] = voxelActionCount[i] = 0;
}

unsigned long long int DataManager::GetNumberOfVoxelsForLabel(unsigned short label)
{
    return voxelCount[label];
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
