///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: UndoRedoSystem.cxx
// Purpose: Manages the undo/redo operation buffers
// Notes: Not the most elegant implementation, but it's easy and can be expanded.
///////////////////////////////////////////////////////////////////////////////////////////////////

// qt includes
#include <QMessageBox>
#include <QObject>
#include <QAbstractButton>
#include <QPushButton>

// project includes
#include "UndoRedoSystem.h"

UndoRedoSystem::UndoRedoSystem(DataManager *data)
{
    // initial buffersize = 150 megabytes, not really alllocated at init, only when needed
    _bufferSize = 150 * 1024 * 1024;
    _bufferUsed = 0;
    
    // pointer to data, need this to access data while undoing or redoing actions
    _dataManager = data;
    
    // buffer is, obviously, not full and we don't have an action right now
    _bufferFull = false;
    _action = NULL;
    
    // calculate sizes beforehand (may be different in different architectures)
    _sizePoint = sizeof(std::pair<Vector3ui, unsigned short>);
    _sizeObject = sizeof(std::pair<unsigned short, struct DataManager::ObjectInformation*>);
    _sizeAction = sizeof(struct action);
    _sizeColor = 4 * sizeof(unsigned char);
    _sizeLabel = sizeof(unsigned short);
}

UndoRedoSystem::~UndoRedoSystem()
{
	// NOTE: for this and the rest of UndoRedoSystem operations: the pointers to objects allocated
	// in the undo buffer are never freed because are in use by DataManager and it will free them
	// on destruction. On the other hand clearing the redo buffer deletes dinamically allocated
	// objects created by DataManager, stored in UndoRedoSystem and NOT in use by DataManager.
	ClearBuffer(ALL);
}

// All the elements of the buffer vector are dropped (their destructors are called) and
// this leaves the buffer with a size of 0, freeing all the previously allocated memory
void UndoRedoSystem::ClearBuffer(const UndoRedoBuffer buffer)
{
	// in destruction of the object we must check if objects are NULL because some could have
	// been previously deallocated while DataManager object destruction. we don't check for
	// NULL on the rest of the class because we know that we're deleting existing objects.
    unsigned long int capacity = 0;
    std::list< struct action >::iterator it;
    
    switch(buffer)
    {
        case UNDO:
            for (it = _undoBuffer.begin(); it != _undoBuffer.end(); it++)
            {
            	capacity += (*it).actionPoints.size() * _sizePoint;
                capacity += (*it).actionObjects.size() * _sizeObject;
                capacity += (*it).actionString.capacity();
                capacity += ((*it).actionLookupTable)->GetNumberOfTableValues()* _sizeColor;
                capacity += (*it).actionLabels.size() * _sizeLabel;
                capacity += _sizeAction;
            }
            _undoBuffer.clear();
            break;
        case REDO:
            for (it = _redoBuffer.begin(); it != _redoBuffer.end(); it++)
            {
            	capacity += (*it).actionPoints.size() * _sizePoint;
                capacity += (*it).actionObjects.size() * _sizeObject;
                capacity += (*it).actionString.capacity();
                capacity += ((*it).actionLookupTable)->GetNumberOfTableValues() * _sizeColor;
                capacity += (*it).actionLabels.size() * _sizeLabel;
                capacity += _sizeAction;

                // delete dinamically allocated objects no longer needed by DataManager
                while (!(*it).actionObjects.empty())
                {
                	delete (*it).actionObjects.back().second;
                	(*it).actionObjects.pop_back();
                }
            }
            _redoBuffer.clear();
            break;
        case ALL:
            ClearBuffer(REDO);
            ClearBuffer(UNDO);
            break;
        default: 
            break;
    }

    _bufferUsed -= capacity;
}

// must call this one before inserting voxels
void UndoRedoSystem::SignalBeginAction(const std::string actionstring, const std::set<unsigned short> labelSet, vtkSmartPointer<vtkLookupTable> lut)
{
    unsigned long int capacity = 0;

    //because an action started, the redo buffer must be empty
    ClearBuffer(REDO);

    // we need to copy the values of the table
    double rgba[4];
    vtkSmartPointer<vtkLookupTable> lutCopy = vtkSmartPointer<vtkLookupTable>::New();
    lutCopy->Allocate();
    lutCopy->SetNumberOfTableValues(lut->GetNumberOfTableValues());

    for (int index = 0; index != lut->GetNumberOfTableValues(); index++)
    {
        lut->GetTableValue(index, rgba);
        lutCopy->SetTableValue(index,rgba);
    }
    lutCopy->SetTableRange(0,lut->GetNumberOfTableValues()-1);

    // create new action
    _action = new struct action;
    _action->actionString = actionstring;
    _action->actionLabels = labelSet;
    _action->actionLookupTable = lutCopy;

    capacity += _sizeAction;
    capacity += (*_action).actionString.capacity();
    capacity += (*_action).actionLabels.size() * _sizeLabel;
    capacity += (*_action).actionLookupTable->GetNumberOfTableValues() * _sizeColor;

    // we need to know if we are at the limit of our buffer
    while ((_bufferUsed + capacity ) > _bufferSize)
    {
    	assert(!_undoBuffer.empty());

		// start deleting undo actions from the beginning of the list
		_bufferUsed -= (*_undoBuffer.begin()).actionPoints.size() * _sizePoint;
		_bufferUsed -= (*_undoBuffer.begin()).actionObjects.size() * _sizeObject;
		_bufferUsed -= (*_undoBuffer.begin()).actionString.capacity();
		_bufferUsed -= ((*_undoBuffer.begin()).actionLookupTable)->GetNumberOfTableValues() * _sizeColor;
		_bufferUsed -= (*_undoBuffer.begin()).actionLabels.size() * _sizeLabel;
		_bufferUsed -= _sizeAction;

		_undoBuffer.erase(_undoBuffer.begin());
    }

    _bufferUsed += capacity;
    _bufferFull = false;
}

// must call this one after inserting voxels
void UndoRedoSystem::SignalEndAction(void)
{
    if (false == _bufferFull)
    {
        _undoBuffer.push_back(*_action);
        _action = NULL;
    }
    else
        _bufferFull = false;
}

void UndoRedoSystem::StorePoint(const Vector3ui point, const unsigned short label)
{
	// if buffer has been marked as full (one action is too big and doesn't fit in the
	// buffer) don't do anything else.
    if (true == _bufferFull)
        return;

    (*_action).actionPoints.push_back(std::pair<Vector3ui, unsigned short>(point, label));
    _bufferUsed += _sizePoint;
    
    // we need to know if we are at the limit of our buffer
    CheckBufferLimits();
}

void UndoRedoSystem::StoreObject(std::pair<unsigned short, struct DataManager::ObjectInformation*> value)
{
	// if buffer marked as full, just return. complete action doesn't fit into memory
	if (true == _bufferFull)
		return;

    (*_action).actionObjects.push_back(value);
    _bufferUsed += _sizeObject;

    // we need to know if we are at the limit of our buffer
    CheckBufferLimits();
}

void UndoRedoSystem::CheckBufferLimits(void)
{
    while (_bufferUsed > _bufferSize)
    {
        // start deleting undo actions from the beginning of the list, check first if this action
    	// fits in our buffer, and if not, delete all points entered until now and mark buffer as
    	// completely full (the action doesn't fit)
        if (_undoBuffer.empty())
        {
            _bufferFull = true;
            
            _bufferUsed -= (*_action).actionPoints.size() * _sizePoint;
            _bufferUsed -= (*_action).actionObjects.size() * _sizeObject;
            _bufferUsed -= (*_action).actionString.capacity();
            _bufferUsed -= ((*_action).actionLookupTable)->GetNumberOfTableValues() * _sizeColor;
            _bufferUsed -= (*_action).actionLabels.size() * _sizeLabel;
            _bufferUsed -= _sizeAction;
            
            delete _action;
            _action = NULL;
        }
        else
        {
        	_bufferUsed -= (*_undoBuffer.begin()).actionPoints.size() * _sizePoint;
            _bufferUsed -= (*_undoBuffer.begin()).actionObjects.size() * _sizeObject;
            _bufferUsed -= (*_undoBuffer.begin()).actionString.capacity();
            _bufferUsed -= ((*_undoBuffer.begin()).actionLookupTable)->GetNumberOfTableValues() * _sizeColor;
            _bufferUsed -= (*_undoBuffer.begin()).actionLabels.size() * _sizeLabel;
            _bufferUsed -= _sizeAction;
            
            _undoBuffer.erase(_undoBuffer.begin());
        }
    }
}

const bool UndoRedoSystem::IsEmpty(const UndoRedoBuffer buffer)
{
    switch(buffer)
    {
        case UNDO:
            return (0 == _undoBuffer.size());
            break;
        case REDO:
            return (0 == _redoBuffer.size());
            break;
        default:
            break;
    }
    
    // not dead code
    return true;
}

void UndoRedoSystem::ChangeSize(const unsigned long int size)
{
    _bufferSize = size;
    
    if (size >= _bufferUsed)
        return;
    
    // we need to delete actions to fit in that size, first redo
    while ((_bufferUsed > size) && (!IsEmpty(REDO)))
    {
    	_bufferUsed -= (*_redoBuffer.begin()).actionPoints.size() * _sizePoint;
        _bufferUsed -= (*_redoBuffer.begin()).actionObjects.size() * _sizeObject;
        _bufferUsed -= (*_redoBuffer.begin()).actionString.capacity();
        _bufferUsed -= ((*_redoBuffer.begin()).actionLookupTable)->GetNumberOfTableValues() * _sizeColor;
        _bufferUsed -= (*_redoBuffer.begin()).actionLabels.size() * _sizeLabel;
        _bufferUsed -= _sizeAction;

        // delete dinamically allocated objects in redo
        while (!(*_redoBuffer.begin()).actionObjects.empty())
        {
        	delete (*_redoBuffer.begin()).actionObjects.back().second;
        	(*_redoBuffer.begin()).actionObjects.pop_back();
        }
        _redoBuffer.erase(_redoBuffer.begin());
    }
    
    // then undo
    while ((_bufferUsed > size) && (!IsEmpty(UNDO)))
    {
    	_bufferUsed -= (*_undoBuffer.begin()).actionPoints.size() * _sizePoint;
        _bufferUsed -= (*_undoBuffer.begin()).actionObjects.size() * _sizeObject;
        _bufferUsed -= (*_undoBuffer.begin()).actionString.capacity();
        _bufferUsed -= ((*_undoBuffer.begin()).actionLookupTable)->GetNumberOfTableValues() * _sizeColor;
        _bufferUsed -= (*_undoBuffer.begin()).actionLabels.size() * _sizeLabel;
        _bufferUsed -= _sizeAction;

        _undoBuffer.erase(_undoBuffer.begin());
    }
}

const unsigned long int UndoRedoSystem::GetSize()
{
    return _bufferSize;
}

const unsigned long int UndoRedoSystem::GetCapacity()
{
    return _bufferUsed;
}

// NOTE: DoAction() can increase size of used buffer memory of there is a
//       exchange of vtkLookupTables with DataManager.
void UndoRedoSystem::DoAction(const UndoRedoBuffer actionBuffer)
{
    std::vector< std::pair<Vector3ui, unsigned short> > action_vector;
    
    assert(_action == NULL);
    
    _action = new struct action;
    
    // get the data from the right buffer
    switch (actionBuffer)
    {
        case UNDO:
        	action_vector = _undoBuffer.back().actionPoints;
            (*_action).actionString = _undoBuffer.back().actionString;
            (*_action).actionLookupTable = _undoBuffer.back().actionLookupTable;
            (*_action).actionObjects = _undoBuffer.back().actionObjects;
            (*_action).actionLabels = _undoBuffer.back().actionLabels;
            break;
        case REDO:
        	action_vector = _redoBuffer.back().actionPoints;
            (*_action).actionString = _redoBuffer.back().actionString;
            (*_action).actionLookupTable = _redoBuffer.back().actionLookupTable;
            (*_action).actionObjects = _redoBuffer.back().actionObjects;
            (*_action).actionLabels = _redoBuffer.back().actionLabels;
            break;
        default: 
            break;
    }
    
    // vector size changes while updating data, need to get the size before
    unsigned long int numberOfPoints = action_vector.size();
    
    // we "delete" the size of the points not to mess with memory while using 
    // SetVoxelScalar, at the end the memory size it's the same, in fact memory
    // used for points is constant during "do undo/redo action" as we delete
    // one point while adding a new one to the opposite action, so the memory
    // variation is about the size of a point plus the size of a label.
    _bufferUsed -= (numberOfPoints * _sizePoint);

    // reverse labels from all points in the action
    for (unsigned long int i = 0; i < numberOfPoints; i++)
    {
        std::pair<Vector3ui, unsigned short> action_point = action_vector.back();
        action_vector.pop_back();

        Vector3ui point = action_point.first;
        _dataManager->SetVoxelScalar(point[0], point[1], point[2], action_point.second);
    }

    std::vector<std::pair<unsigned short, struct DataManager::ObjectInformation*> >::iterator it;

    // insert the changed action in the right buffer and apply the actual LookupTable and table values
    // in fact only the pointers in the original ObjectVector (a std::map) are changed the objects are
    // only deleted when the undoredosystem deletes them
    std::set<unsigned short> temporalSet = this->_dataManager->GetSelectedLabelsSet();
    switch (actionBuffer)
    {
        case UNDO:
            _redoBuffer.push_back(*_action);
            _undoBuffer.pop_back();

            _bufferUsed -= _redoBuffer.back().actionLookupTable->GetNumberOfColors() * _sizeColor;
            _dataManager->SwitchLookupTables(_redoBuffer.back().actionLookupTable);
            _bufferUsed += _redoBuffer.back().actionLookupTable->GetNumberOfColors() * _sizeColor;

            _bufferUsed -= _redoBuffer.back().actionLabels.size() * _sizeLabel;
            this->_dataManager->SetSelectedLabelsSet(_redoBuffer.back().actionLabels);
            _redoBuffer.back().actionLabels = temporalSet;
            _bufferUsed += _redoBuffer.back().actionLabels.size() * _sizeLabel;
            
            for(it = _redoBuffer.back().actionObjects.begin(); it != _redoBuffer.back().actionObjects.end(); it++)
            	(*_dataManager->GetObjectTablePointer()).erase((*it).first);

            break;
        case REDO:
            _undoBuffer.push_back(*_action);
            _redoBuffer.pop_back();

            _bufferUsed -= _undoBuffer.back().actionLookupTable->GetNumberOfColors() * _sizeColor;
            _dataManager->SwitchLookupTables(_undoBuffer.back().actionLookupTable);
            _bufferUsed += _undoBuffer.back().actionLookupTable->GetNumberOfColors() * _sizeColor;

            _bufferUsed -= _undoBuffer.back().actionLabels.size() * _sizeLabel;
            this->_dataManager->SetSelectedLabelsSet(_undoBuffer.back().actionLabels);
            _undoBuffer.back().actionLabels = temporalSet;
            _bufferUsed += _undoBuffer.back().actionLabels.size() * _sizeLabel;

            for(it = _undoBuffer.back().actionObjects.begin(); it != _undoBuffer.back().actionObjects.end(); it++)
            	(*_dataManager->GetObjectTablePointer())[(*it).first] = (*it).second;

            break;
        default: 
            break;
    }
    
    CheckBufferLimits();
    _action = NULL;
}

const std::string UndoRedoSystem::GetActionString(const UndoRedoBuffer buffer)
{
    switch (buffer)
    {
        case UNDO:
            if (!IsEmpty(UNDO))
                return _undoBuffer.back().actionString;
            else 
                return "";
            break;
        case REDO:
            if (!IsEmpty(REDO))
                return _redoBuffer.back().actionString;
            else
                return "";
            break;
        case ACTUAL:
            if (_action != NULL)
                return (*_action).actionString;
            else
                return "";
            break;
        case ALL:
        default:
            break;
    }
    return "";
}

void UndoRedoSystem::SignalCancelAction()
{
    _bufferFull = false;
    
    _bufferUsed -= (*_action).actionPoints.size() * _sizePoint;
    _bufferUsed -= (*_action).actionObjects.size() * _sizeObject;
    _bufferUsed -= (*_action).actionString.capacity();
    _bufferUsed -= ((*_action).actionLookupTable)->GetNumberOfTableValues() * _sizeColor;
    _bufferUsed -= (*_action).actionLabels.size() * _sizeLabel;
    _bufferUsed -= _sizeAction;

    // undo changes if there are some, bypassing the undo system as we don't want to store this
    // modifications to the data
    while ((*_action).actionPoints.size() != 0)
    {
        std::pair<Vector3ui, unsigned short> action_point = (*_action).actionPoints.back();
        (*_action).actionPoints.pop_back();

        Vector3ui point = action_point.first;
        _dataManager->SetVoxelScalarRaw(point[0], point[1], point[2], action_point.second);
    }

    // delete dinamically allocated objects
    while (!(*_action).actionObjects.empty())
    {
    	std::pair<unsigned short, struct DataManager::ObjectInformation*> pair = (*_action).actionObjects.back();
    	(*_dataManager->GetObjectTablePointer()).erase(pair.first);
    	delete pair.second;
    	(*_action).actionObjects.pop_back();
    }
    
    delete _action;
    
    _action = NULL;
}
