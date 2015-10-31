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
    /** \brief UndoRedoSystem class constructor.
     * \param[in] data application data manager.
     *
     */
    UndoRedoSystem(DataManager *data);

    /** \brief UndoRedoSystem class destructor.
     *
     */
    ~UndoRedoSystem();

    enum class Type: char { UNDO, REDO, ALL, ACTUAL };

    /** \brief Do action.
     * 1 - BEWARE: this action modify current data (touches DataManager data)
     * 2 - this action doesn't modify undo/redo system capacity
     * 3 - only call this action when you're sure there is at least one action in the undo buffer
     * \param[in] type buffer type.
     *
     */
    void doAction(const Type type);

    /** \brief Clears the buffer.
     * All the elements of the buffer vector are dropped (their destructors are called) and
     * this leaves the buffer with a size of 0, freeing all the previously allocated memory
     * \param[in] type buffer type.
     *
     */
    void clear(const Type type);

    /** \brief Add a point to current undo action
     * \param[in] point point coordinates.
     * \param[in] label object label.
     *
     */
    void storePoint(const Vector3ui &point, const unsigned short label);

    /** \brief Returns true if the type buffer is empty and false otherwise.
     * \param[in] type buffer type.
     *
     */
    const bool isEmpty(const Type type) const;

    /** \brief Change undo/redo system size.
     * \param[in] size size of the buffer in bytes.
     *
     */
    void changeSize(const unsigned long int size);

    /** \brief Returns the size of the undo/redo system.
     *
     */
    const unsigned long int size() const;

    /** \brief Returns the current capacity (used bytes)
     *
     */
    const unsigned long int capacity() const;

    /** \brief Create a new action.
     * \param[in] actionString action description string.
     * \param[in] labelSet group of affected labels.
     * \param[in] lut color table for the labels.
     *
     */
    void signalBeginAction(const std::string &actionString, const std::set<unsigned short> labelSet, vtkSmartPointer<vtkLookupTable> lut);

    /** \brief Ends an action.
     *
     */
    void signalEndAction();

    /** \brief Stores a new label scalar value.
     * \param[in] object object information pair <label, object information>.
     *
     */
    void storeObject(std::pair<unsigned short, std::shared_ptr<DataManager::ObjectInformation>> object);

    /** \brief Returns the action string of the specified buffer.
     * \param[in] type buffer type.
     *
     */
    const std::string getActionString(const Type type) const;

    /** \brief Deletes all the data stored in actual action buffer.
     *
     */
    void signalCancelAction();
  private:
    /** \brief Helper method to check if we are at the limit of our buffer and if so, makes room for more actions.
     *
     */
    void checkLimits();

    // this defines an action, the data needed to store the action's effect on internal data
    // NOTE: to get size of members we'll use capacity() for vector and string, for the
    //       vtkLookupTable the size is 4*sizeof(unsigned char)*(number of colors) always)
    struct action
    {
        std::vector<std::pair<Vector3ui, unsigned short> > points;      /** points and labels information. */
        vtkSmartPointer<vtkLookupTable>                    lut;         /** color table of the labels. */
        std::string                                        description; /** description of the action. */
        std::set<unsigned short>                           labels;      /** labels of the action. */

        std::vector<std::pair<unsigned short, std::shared_ptr<DataManager::ObjectInformation>> > objects; /** list of objects. */
    };

    struct action *m_current; /** \brief Action in progress. */

    unsigned long int m_size; /** size of the undo/redo system. */
    unsigned long int m_used; /** used bytes the the undo/redo system. */

    std::list<struct action> m_undo; /** undo buffer. */
    std::list<struct action> m_redo; /** redo buffer. */

    DataManager *m_dataManager; /** application data manager. */

    bool m_bufferFull; /** true if the buffer is full and we must delete an action before storing another. */

    int m_sizePoint;  /** point storage size, can differ in different CPU word sizes. */
    int m_sizeAction; /** action storage size, can differ in different CPU word sizes. */
    int m_sizeObject; /** object storage size, can differ in different CPU word sizes. */
    int m_sizeColor;  /** color storage size, can differ in different CPU word sizes. */
    int m_sizeLabel;  /** label storage size, can differ in different CPU word sizes. */
};

#endif // _UNDOREDOSYSTEM_H_
