///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: UndoRedoSystem.h
// Purpose: Manages the undo/redo operation buffers
// Notes: Not the most elegant implementation, but it's easy and can be expanded.
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _UNDOREDOSYSTEM_H_
#define _UNDOREDOSYSTEM_H_

// vtk include
#include <vtkSmartPointer.h>
#include <vtkLookupTable.h>

// project includes
#include "VectorSpaceAlgebra.h"
#include "DataManager.h"

// c++ includes
#include <vector>
#include <list>

// DataManager forward declaration
class DataManager;

class UndoRedoSystem
{
    public:
        // constructor & destructor
        UndoRedoSystem(DataManager*);
        ~UndoRedoSystem();
        
        typedef enum
        {
            UNDO, REDO, ALL, ACTUAL
        } UndoRedoBuffer;
        
        // do action
        // 1 - BEWARE: this action modify current data (touches DataManager data)
        // 2 - this action doesn't modify undo/redo system capacity
        // 3 - only call this action when you're sure there is at least one action in the undo buffer
        void DoAction(UndoRedoBuffer);
        
        // All the elements of the buffer vector are dropped (their destructors are called) and
        // this leaves the buffer with a size of 0, freeing all the previously allocated memory
        void ClearBuffer(UndoRedoBuffer);
        
        // add a point to current undo action
        void AddPoint(Vector3ui, unsigned short);
        
        // check if one of the buffers is empty (to disable it's menu command)
        bool IsEmpty(UndoRedoBuffer);
        
        // change undo/redo system size
        void ChangeSize(unsigned long int);
        
        // get undo/redo system size
        unsigned long int GetSize();

        // get undo/redo system capacity (used bytes)
        unsigned long int GetCapacity();

        // create a new action
        void SignalBeginAction(std::string);
        
        // we need to know when an action ends
        void SignalEndAction();
        
        // stores actual lookuptable in actual action pointer
        void StoreLookupTable(vtkSmartPointer<vtkLookupTable>);
        
        // stores a new label scalar value
        void StoreLabelValue(std::pair<unsigned short, unsigned short>);
        
        // returns the action string of the buffer specified
        std::string GetActionString(UndoRedoBuffer buffer);
        
        // deletes all the data stored in actual action buffer
        void SignalCancelAction();
    private:
        // this defines an action, the data needed to store the action's effect on internal data
        // NOTE: to get size of members we'll use capacity() for vector and string, for the
        //       vtkLookupTable the size is 4*sizeof(unsigned char)*(number of possible colors) always)
        struct action
        {
            std::vector< std::pair<Vector3ui, unsigned short> >       actionBuffer;
            vtkSmartPointer<vtkLookupTable>                           actionLookupTable;
            std::string                                               actionString;
            std::vector< std::pair<unsigned short, unsigned short> >  actionTableValues;
        };
        
        // action in progress
        struct action* _action;
        
        // capacity of the undo and redo buffers
        unsigned long int _bufferSize;
        unsigned long int _bufferUsed;
        
        // undo and redo buffers (a list of actions because we will need to erase elements
        // from the front and the back, and a list is faster than a vector for that)
        std::list<struct action> _undoBuffer;
        std::list<struct action> _redoBuffer;
        
        // pointer to data
        DataManager* _dataManager;
        
        // signals is the we have reached maximum capacity
        bool _bufferFull;
        
        // element sizes, should differ in different CPU word sizes
        int _sizePoint;
        int _sizeAction;
        int _sizeValue;
};

#endif // _UNDOREDOSYSTEM_H_
