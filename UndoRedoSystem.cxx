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
    
    // calculate sizes beforehand (maybe different in 32/64 bits architectures)
    _sizePoint = sizeof(std::pair<Vector3ui, unsigned short>);
    _sizeValue = sizeof(std::pair<unsigned short, unsigned short>);
    _sizeAction = sizeof(struct action);
}

UndoRedoSystem::~UndoRedoSystem()
{
    // this is automatically cleared on destruction but we made it explicit here for clarity
    _undoBuffer.clear();
    _redoBuffer.clear();
}

// All the elements of the buffer vector are dropped (their destructors are called) and
// this leaves the buffer with a size of 0, freeing all the previously allocated memory
void UndoRedoSystem::ClearBuffer(UndoRedoBuffer buffer)
{
    unsigned long int capacity = 0;
    std::list< struct action >::iterator it;
    
    switch(buffer)
    {
        case UNDO:
            for (it = _undoBuffer.begin(); it != _undoBuffer.end(); it++)
            {
                capacity += (*it).actionBuffer.size() * _sizePoint;
                capacity += (*it).actionTableValues.size() * _sizeValue;
                capacity += (*it).actionString.capacity();
                if (NULL != (*it).actionLookupTable)
                    capacity += 4*(((*it).actionLookupTable)->GetNumberOfTableValues())*sizeof(unsigned char);
                capacity += _sizeAction;
            }
            _undoBuffer.clear();
            break;
        case REDO:
            for (it = _redoBuffer.begin(); it != _redoBuffer.end(); it++)
            {
                capacity += (*it).actionBuffer.size() * _sizePoint;
                capacity += (*it).actionTableValues.size() * _sizeValue;
                capacity += (*it).actionString.capacity();
                if (NULL != (*it).actionLookupTable)
                    capacity += 4*(((*it).actionLookupTable)->GetNumberOfTableValues())*sizeof(unsigned char);
                capacity += _sizeAction;
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
void UndoRedoSystem::SignalBeginAction(std::string actionstring)
{
    unsigned long int capacity = 0;

    //because an action started, the redo buffer must be empty
    ClearBuffer(REDO);

    // create new action
    _action = new struct action;
    _action->actionString = actionstring;
    _action->actionLookupTable = NULL;

    capacity += _sizeAction;
    capacity += (*_action).actionString.capacity();

    // we need to know if we are at the limit of our buffer
    while ((_bufferUsed + capacity ) > _bufferSize)
    {
        // start deleting undo actions from the beginning of the list
        _bufferUsed -= (*_undoBuffer.begin()).actionBuffer.size() * _sizePoint;
        _bufferUsed -= (*_undoBuffer.begin()).actionTableValues.size() * _sizeValue;
        _bufferUsed -= (*_undoBuffer.begin()).actionString.capacity();
        if (NULL != (*_undoBuffer.begin()).actionLookupTable)
            _bufferUsed -= 4*(((*_undoBuffer.begin()).actionLookupTable)->GetNumberOfTableValues())*sizeof(unsigned char);
        _bufferUsed -= _sizeAction;
        
        _undoBuffer.erase(_undoBuffer.begin());
    }
    _bufferUsed += capacity;
    _bufferFull = false;
}

// must call this one after inserting voxels
void UndoRedoSystem::SignalEndAction()
{
    if (false == _bufferFull)
    {
        // if action has modified any point or label then store it, if not just discard
        if (((*_action).actionBuffer.size() != 0) || ((*_action).actionTableValues.size() != 0))
            _undoBuffer.push_back(*_action);
        
        _action = NULL;
    }
    else
        _bufferFull = false;
}

void UndoRedoSystem::AddPoint(Vector3ui point, unsigned short label)
{
    if (true == _bufferFull)
        return;

    unsigned long int initial_size = (*_action).actionBuffer.size();
    
    std::pair<Vector3ui, unsigned short> point_data = std::make_pair(point, label);
    (*_action).actionBuffer.push_back(point_data);

    _bufferUsed += ((*_action).actionBuffer.size() - initial_size) * _sizePoint;
    
    // we need to know if we are at the limit of our buffer
    while (_bufferUsed > _bufferSize)
    {
        // start deleting undo actions from the beginning of the list, check first if this action
        // fits in our buffer, and if not, delete all points entered until now
        if (_undoBuffer.size() == 0)
        {
            _bufferFull = true;
            
            _bufferUsed -= (*_action).actionBuffer.size() * _sizePoint;
            _bufferUsed -= (*_action).actionTableValues.size() * _sizeValue;
            _bufferUsed -= (*_action).actionString.capacity();
            if (NULL != (*_action).actionLookupTable)
                _bufferUsed -= 4*(((*_action).actionLookupTable)->GetNumberOfTableValues())*sizeof(unsigned char);
            _bufferUsed -= _sizeAction;
            
            delete _action;
            _action = NULL;
        }
        else
        {
            _bufferUsed -= (*_undoBuffer.begin()).actionBuffer.size() * _sizePoint;
            _bufferUsed -= (*_undoBuffer.begin()).actionTableValues.size() * _sizeValue;
            _bufferUsed -= (*_undoBuffer.begin()).actionString.capacity();
            if (NULL != (*_undoBuffer.begin()).actionLookupTable)
                _bufferUsed -= 4*(((*_undoBuffer.begin()).actionLookupTable)->GetNumberOfTableValues())*sizeof(unsigned char);
            _bufferUsed -= _sizeAction;
            
            _undoBuffer.erase(_undoBuffer.begin());
        }
    }
}

bool UndoRedoSystem::IsEmpty(UndoRedoBuffer buffer)
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

void UndoRedoSystem::ChangeSize(unsigned long int size)
{
    _bufferSize = size;
    
    if (size >= _bufferUsed)
        return;
    
    // we need to delete actions to fit in that size, first redo
    while ((_bufferUsed > size) && (!IsEmpty(REDO)))
    {
        _bufferUsed -= (*_redoBuffer.begin()).actionBuffer.size() * _sizePoint;
        _bufferUsed -= (*_redoBuffer.begin()).actionTableValues.size() * _sizeValue;
        _bufferUsed -= (*_redoBuffer.begin()).actionString.capacity();
        if (NULL != (*_redoBuffer.begin()).actionLookupTable)
            _bufferUsed -= 4*(((*_redoBuffer.begin()).actionLookupTable)->GetNumberOfTableValues())*sizeof(unsigned char);
        _bufferUsed -= _sizeAction;

        _redoBuffer.erase(_redoBuffer.begin());
    }
    
    // then undo
    while ((_bufferUsed > size) && (!IsEmpty(UNDO)))
    {
        _bufferUsed -= (*_undoBuffer.begin()).actionBuffer.size() * _sizePoint;
        _bufferUsed -= (*_undoBuffer.begin()).actionTableValues.size() * _sizeValue;
        _bufferUsed -= (*_undoBuffer.begin()).actionString.capacity();
        if (NULL != (*_undoBuffer.begin()).actionLookupTable)
            _bufferUsed -= 4*(((*_undoBuffer.begin()).actionLookupTable)->GetNumberOfTableValues())*sizeof(unsigned char);
        _bufferUsed -= _sizeAction;

        _undoBuffer.erase(_undoBuffer.begin());
    }
}

unsigned long int UndoRedoSystem::GetSize()
{
    return _bufferSize;
}

unsigned long int UndoRedoSystem::GetCapacity()
{
    return _bufferUsed;
}

void UndoRedoSystem::DoAction(UndoRedoBuffer actionBuffer)
{
    std::vector< std::pair<Vector3ui, unsigned short> > action_vector;
    
    assert(_action == NULL);
    
    _action = new struct action;
    
    // get the data from the right buffer
    switch (actionBuffer)
    {
        case UNDO:
            action_vector = _undoBuffer.back().actionBuffer;
            (*_action).actionString = _undoBuffer.back().actionString;
            (*_action).actionLookupTable = _undoBuffer.back().actionLookupTable;
            (*_action).actionTableValues = _undoBuffer.back().actionTableValues;
            break;
        case REDO:
            action_vector = _redoBuffer.back().actionBuffer;
            (*_action).actionString = _redoBuffer.back().actionString;
            (*_action).actionLookupTable = _redoBuffer.back().actionLookupTable;
            (*_action).actionTableValues = _redoBuffer.back().actionTableValues;
            break;
        default: 
            break;
    }
    
    // vector size changes while updating data, need to get the size before
    unsigned long int numberOfPoints = action_vector.size();
    
    // we "delete" the size of the points not to mess with memory while using 
    // SetVoxelScalar, at the end the memory size it's the same, in fact memory
    // use is constant during "do undo/redo action" as we delete one point while
    // adding a new one
    _bufferUsed -= (numberOfPoints * _sizePoint);

    // reverse labels from all points in the action
    for (unsigned long int i = 0; i < numberOfPoints; i++)
    {
        std::pair<Vector3ui, unsigned short> action_point = action_vector.back();
        action_vector.pop_back();

        Vector3ui point = action_point.first;
        _dataManager->SetVoxelScalar(point[0], point[1], point[2], action_point.second);
    }
    
    std::vector<std::pair<unsigned short, unsigned short> >::iterator it;
    
    // insert the changed action in the right buffer and apply the actual LookupTable and table values
    switch (actionBuffer)
    {
        case UNDO:
            _redoBuffer.push_back(*_action);
            _undoBuffer.pop_back();
            if (NULL != _redoBuffer.back().actionLookupTable)
                _dataManager->SwitchLookupTables(_redoBuffer.back().actionLookupTable);
            
            for(it = _redoBuffer.back().actionTableValues.begin(); it != _redoBuffer.back().actionTableValues.end(); it++)
                (*_dataManager->GetLabelValueTable()).erase((*it).first);

            break;
        case REDO:
            _undoBuffer.push_back(*_action);
            _redoBuffer.pop_back();
            if (NULL != _undoBuffer.back().actionLookupTable)
                _dataManager->SwitchLookupTables(_undoBuffer.back().actionLookupTable);
            
            for(it = _undoBuffer.back().actionTableValues.begin(); it != _undoBuffer.back().actionTableValues.end(); it++)
                (*_dataManager->GetLabelValueTable()).insert((*it));

            break;
        default: 
            break;
    }
    
    _action = NULL;
}

void UndoRedoSystem::StoreLabelValue(std::pair<unsigned short, unsigned short> value)
{
    (*_action).actionTableValues.push_back(value);
    _bufferUsed += _sizeValue;
}

void UndoRedoSystem::StoreLookupTable(vtkSmartPointer<vtkLookupTable> lookupTable)
{
    if ((*_action).actionLookupTable == NULL)
    {
        (*_action).actionLookupTable = lookupTable;

        _bufferUsed += 4*(((*_action).actionLookupTable)->GetNumberOfTableValues())*sizeof(unsigned char);
        
        // we need to know if we are at the limit of our buffer
        while (_bufferUsed > _bufferSize)
        {
            // start deleting undo actions from the beginning of the list, check first if this action
            // fits in our buffer, and if not, delete all points entered until now
            if (_undoBuffer.size() == 0)
            {
                _bufferFull = true;
                
                // start deleting undo actions from the beginning of the list
                _bufferUsed -= (*_action).actionBuffer.size() * _sizePoint;
                _bufferUsed -= (*_action).actionTableValues.size() * _sizeValue;
                _bufferUsed -= (*_action).actionString.capacity();
                _bufferUsed -= 4*(((*_action).actionLookupTable)->GetNumberOfTableValues())*sizeof(unsigned char);
                _bufferUsed -= _sizeAction;
                
                delete _action;
                _action = NULL;
            }
            else
            {
                _bufferUsed -= (*_undoBuffer.begin()).actionBuffer.size() * _sizePoint;
                _bufferUsed -= (*_undoBuffer.begin()).actionTableValues.size() * _sizeValue;
                _bufferUsed -= (*_undoBuffer.begin()).actionString.capacity();
                if (NULL != (*_undoBuffer.begin()).actionLookupTable)
                    _bufferUsed -= 4*(((*_undoBuffer.begin()).actionLookupTable)->GetNumberOfTableValues())*sizeof(unsigned char);
                _bufferUsed -= _sizeAction;
                
                _undoBuffer.erase(_undoBuffer.begin());
            }
        }
    }
    
    // else if actionLookupTable != NULL we rely on smartpointers to deallocate previous tables
    // automatically. In an operation we only have to store the first call (the first table), not
    // the rest, that are just updates
}

std::string UndoRedoSystem::GetActionString(UndoRedoBuffer buffer)
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
    
    _bufferUsed -= (*_action).actionBuffer.size() * _sizePoint;
    _bufferUsed -= (*_action).actionTableValues.size() * _sizeValue;
    _bufferUsed -= (*_action).actionString.capacity();
    if (NULL != (*_action).actionLookupTable)
        _bufferUsed -= 4*(((*_action).actionLookupTable)->GetNumberOfTableValues())*sizeof(unsigned char);
    _bufferUsed -= _sizeAction;

    // undo changes if there are some, bypassing the undo system as we don't want to store this
    // modifications to the data
    while ((*_action).actionBuffer.size() != 0)
    {
        std::pair<Vector3ui, unsigned short> action_point = (*_action).actionBuffer.back();
        (*_action).actionBuffer.pop_back();

        Vector3ui point = action_point.first;
        _dataManager->SetVoxelScalarRaw(point[0], point[1], point[2], action_point.second);
    }

    // also undo changes to value table
    if ((*_action).actionTableValues.size() != 0)
    {
        std::vector<std::pair<unsigned short, unsigned short> >::iterator it;
 
        for(it = (*_action).actionTableValues.begin(); it != (*_action).actionTableValues.end(); it++)
            (*_dataManager->GetLabelValueTable()).erase((*it).first);
    }
    
    delete _action;
    
    _action = NULL;
}
