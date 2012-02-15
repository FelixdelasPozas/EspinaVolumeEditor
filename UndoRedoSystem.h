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
        void DoAction(const UndoRedoBuffer);
        
        // All the elements of the buffer vector are dropped (their destructors are called) and
        // this leaves the buffer with a size of 0, freeing all the previously allocated memory
        void ClearBuffer(const UndoRedoBuffer);
        
        // add a point to current undo action
        void StorePoint(const Vector3ui, const unsigned short);

        // check if one of the buffers is empty (to disable it's menu command)
        const bool IsEmpty(const UndoRedoBuffer);
        
        // change undo/redo system size
        void ChangeSize(const unsigned long int);
        
        // get undo/redo system size
        const unsigned long int GetSize();

        // get undo/redo system capacity (used bytes)
        const unsigned long int GetCapacity();

        // create a new action
        void SignalBeginAction(const std::string, const std::set<unsigned short>, vtkSmartPointer<vtkLookupTable>);
        
        // we need to know when an action ends
        void SignalEndAction();
        
        // stores a new label scalar value
        void StoreObject(std::pair<unsigned short, struct DataManager::ObjectInformation*>);
        
        // returns the action string of the buffer specified
        const std::string GetActionString(const UndoRedoBuffer buffer);
        
        // deletes all the data stored in actual action buffer
        void SignalCancelAction();
    private:
        // checks if we are at the limit of our buffer and if so, makes room for more actions
        void CheckBufferLimits(void);

        // this defines an action, the data needed to store the action's effect on internal data
        // NOTE: to get size of members we'll use capacity() for vector and string, for the
        //       vtkLookupTable the size is 4*sizeof(unsigned char)*(number of colors) always)
        struct action
        {
        	std::vector< std::pair<Vector3ui, unsigned short> >       							actionPoints;
            vtkSmartPointer<vtkLookupTable>                           							actionLookupTable;
            std::string                                               							actionString;
            std::vector< std::pair<unsigned short, struct DataManager::ObjectInformation*> >  	actionObjects;
            std::set<unsigned short>															actionLabels;
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
        
        // signals is the we have reached maximum capacity with only one action, if there is more than one
        // action in the buffer, the system deletes the older first to make room for the new one
        bool _bufferFull;
        
        // element sizes, should differ in different CPU word sizes
        int _sizePoint;
        int _sizeAction;
        int _sizeObject;
        int _sizeColor;
        int _sizeLabel;
};

#endif // _UNDOREDOSYSTEM_H_
