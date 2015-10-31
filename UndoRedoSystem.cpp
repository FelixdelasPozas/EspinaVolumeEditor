///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: UndoRedoSystem.cpp
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

///////////////////////////////////////////////////////////////////////////////////////////////////
UndoRedoSystem::UndoRedoSystem(DataManager *dataManager)
: m_current{nullptr}
, m_size{150 * 1024 * 1024}
, m_used{0}
, m_dataManager{dataManager}
, m_bufferFull{false}
, m_sizePoint{sizeof(std::pair<Vector3ui, unsigned short>)}
, m_sizeAction{sizeof(struct action)}
, m_sizeObject{sizeof(std::pair<unsigned short, struct DataManager::ObjectInformation*>)}
, m_sizeColor{4 * sizeof(unsigned char)}
, m_sizeLabel{sizeof(unsigned short)}
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////
UndoRedoSystem::~UndoRedoSystem()
{
  // NOTE: for this and the rest of UndoRedoSystem operations: the pointers to objects allocated
  // in the undo buffer are never freed because are in use by DataManager and it will free them
  // on destruction. On the other hand clearing the redo buffer deletes dinamically allocated
  // objects created by DataManager, stored in UndoRedoSystem and NOT in use by DataManager.
  clear(Type::ALL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void UndoRedoSystem::clear(const Type type)
{
  // in destruction of the object we must check if objects are NULL because some could have
  // been previously deallocated while DataManager object destruction. we don't check for
  // NULL on the rest of the class because we know that we're deleting existing objects.
  unsigned long int capacity = 0;

  switch (type)
  {
    case Type::UNDO:
      for (auto it: m_undo)
      {
        capacity += it.points.size() * m_sizePoint;
        capacity += it.objects.size() * m_sizeObject;
        capacity += it.description.capacity();
        capacity += it.lut->GetNumberOfTableValues() * m_sizeColor;
        capacity += it.labels.size() * m_sizeLabel;
        capacity += m_sizeAction;
      }
      m_undo.clear();
      break;
    case Type::REDO:
      for (auto it: m_redo)
      {
        capacity += it.points.size() * m_sizePoint;
        capacity += it.objects.size() * m_sizeObject;
        capacity += it.description.capacity();
        capacity += it.lut->GetNumberOfTableValues() * m_sizeColor;
        capacity += it.labels.size() * m_sizeLabel;
        capacity += m_sizeAction;

        // delete dinamically allocated objects no longer needed by DataManager
        while (!it.objects.empty())
        {
          it.objects.back().second = nullptr;
          it.objects.pop_back();
        }
      }
      m_redo.clear();
      break;
    case Type::ALL:
      clear(Type::REDO);
      clear(Type::UNDO);
      break;
    default:
      break;
  }

  m_used -= capacity;
  Q_ASSERT(m_used >= 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void UndoRedoSystem::signalBeginAction(const std::string &actionstring, const std::set<unsigned short> labelSet, vtkSmartPointer<vtkLookupTable> lut)
{
  unsigned long int capacity = 0;

  //because an action started, the redo buffer must be empty
  clear(Type::REDO);

  // we need to copy the values of the table
  double rgba[4];
  vtkSmartPointer<vtkLookupTable> lutCopy = vtkSmartPointer<vtkLookupTable>::New();
  lutCopy->Allocate();
  lutCopy->SetNumberOfTableValues(lut->GetNumberOfTableValues());

  for (int index = 0; index != lut->GetNumberOfTableValues(); index++)
  {
    lut->GetTableValue(index, rgba);
    lutCopy->SetTableValue(index, rgba);
  }
  lutCopy->SetTableRange(0, lut->GetNumberOfTableValues() - 1);

  // create new action
  m_current = new struct action;
  m_current->description = actionstring;
  m_current->labels = labelSet;
  m_current->lut = lutCopy;

  capacity += m_sizeAction;
  capacity += (*m_current).description.capacity();
  capacity += (*m_current).labels.size() * m_sizeLabel;
  capacity += (*m_current).lut->GetNumberOfTableValues() * m_sizeColor;

  // we need to know if we are at the limit of our buffer
  while ((m_used + capacity) > m_size)
  {
    assert(!m_undo.empty());

    // start deleting undo actions from the beginning of the list
    m_used -= (*m_undo.begin()).points.size() * m_sizePoint;
    m_used -= (*m_undo.begin()).objects.size() * m_sizeObject;
    m_used -= (*m_undo.begin()).description.capacity();
    m_used -= ((*m_undo.begin()).lut)->GetNumberOfTableValues() * m_sizeColor;
    m_used -= (*m_undo.begin()).labels.size() * m_sizeLabel;
    m_used -= m_sizeAction;

    m_undo.erase(m_undo.begin());
  }

  m_used += capacity;
  m_bufferFull = false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void UndoRedoSystem::signalEndAction(void)
{
  if (!m_bufferFull)
  {
    m_undo.push_back(*m_current);
    m_current = nullptr;
  }
  else
  {
    m_bufferFull = false;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void UndoRedoSystem::storePoint(const Vector3ui &point, const unsigned short label)
{
  // if buffer has been marked as full (one action is too big and doesn't fit in the
  // buffer) don't do anything else.
  if (m_bufferFull) return;

  (*m_current).points.push_back(std::pair<Vector3ui, unsigned short>(point, label));
  m_used += m_sizePoint;

  // we need to know if we are at the limit of our buffer
  checkLimits();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void UndoRedoSystem::storeObject(std::pair<unsigned short, std::shared_ptr<DataManager::ObjectInformation>> value)
{
  // if buffer marked as full, just return. complete action doesn't fit into memory
  if (m_bufferFull) return;

  (*m_current).objects.push_back(value);
  m_used += m_sizeObject;

  // we need to know if we are at the limit of our buffer
  checkLimits();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void UndoRedoSystem::checkLimits()
{
  while (m_used > m_size)
  {
    // start deleting undo actions from the beginning of the list, check first if this action
    // fits in our buffer, and if not, delete all points entered until now and mark buffer as
    // completely full (the action doesn't fit)
    if (m_undo.empty())
    {
      m_bufferFull = true;

      m_used -= (*m_current).points.size() * m_sizePoint;
      m_used -= (*m_current).objects.size() * m_sizeObject;
      m_used -= (*m_current).description.capacity();
      m_used -= ((*m_current).lut)->GetNumberOfTableValues() * m_sizeColor;
      m_used -= (*m_current).labels.size() * m_sizeLabel;
      m_used -= m_sizeAction;

      delete m_current;
      m_current = nullptr;
    }
    else
    {
      m_used -= (*m_undo.begin()).points.size() * m_sizePoint;
      m_used -= (*m_undo.begin()).objects.size() * m_sizeObject;
      m_used -= (*m_undo.begin()).description.capacity();
      m_used -= ((*m_undo.begin()).lut)->GetNumberOfTableValues() * m_sizeColor;
      m_used -= (*m_undo.begin()).labels.size() * m_sizeLabel;
      m_used -= m_sizeAction;

      m_undo.erase(m_undo.begin());
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const bool UndoRedoSystem::isEmpty(const Type type) const
{
  switch (type)
  {
    case Type::UNDO:
      return (0 == m_undo.size());
      break;
    case Type::REDO:
      return (0 == m_redo.size());
      break;
    default:
      break;
  }

  // not dead code
  return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void UndoRedoSystem::changeSize(const unsigned long int size)
{
  m_size = size;

  if (size >= m_used) return;

  // we need to delete actions to fit in that size, first redo
  while ((m_used > size) && (!isEmpty(Type::REDO)))
  {
    m_used -= (*m_redo.begin()).points.size() * m_sizePoint;
    m_used -= (*m_redo.begin()).objects.size() * m_sizeObject;
    m_used -= (*m_redo.begin()).description.capacity();
    m_used -= ((*m_redo.begin()).lut)->GetNumberOfTableValues() * m_sizeColor;
    m_used -= (*m_redo.begin()).labels.size() * m_sizeLabel;
    m_used -= m_sizeAction;

    // delete dinamically allocated objects in redo
    while (!(*m_redo.begin()).objects.empty())
    {
      (*m_redo.begin()).objects.back().second = nullptr;
      (*m_redo.begin()).objects.pop_back();
    }
    m_redo.erase(m_redo.begin());
  }

  // then undo
  while ((m_used > size) && (!isEmpty(Type::UNDO)))
  {
    m_used -= (*m_undo.begin()).points.size() * m_sizePoint;
    m_used -= (*m_undo.begin()).objects.size() * m_sizeObject;
    m_used -= (*m_undo.begin()).description.capacity();
    m_used -= ((*m_undo.begin()).lut)->GetNumberOfTableValues() * m_sizeColor;
    m_used -= (*m_undo.begin()).labels.size() * m_sizeLabel;
    m_used -= m_sizeAction;

    m_undo.erase(m_undo.begin());
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned long int UndoRedoSystem::size() const
{
  return m_size;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned long int UndoRedoSystem::capacity() const
{
  return m_used;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void UndoRedoSystem::doAction(const Type type)
{
  std::vector<std::pair<Vector3ui, unsigned short> > action_vector;

  assert(!m_current);

  m_current = new struct action;

  // get the data from the right buffer
  switch (type)
  {
    case Type::UNDO:
      action_vector = m_undo.back().points;
      (*m_current).description = m_undo.back().description;
      (*m_current).lut = m_undo.back().lut;
      (*m_current).objects = m_undo.back().objects;
      (*m_current).labels = m_undo.back().labels;
      break;
    case Type::REDO:
      action_vector = m_redo.back().points;
      (*m_current).description = m_redo.back().description;
      (*m_current).lut = m_redo.back().lut;
      (*m_current).objects = m_redo.back().objects;
      (*m_current).labels = m_redo.back().labels;
      break;
    default:
      break;
  }

  // vector size changes while updating data, need to get the size before
  auto numberOfPoints = action_vector.size();

  // we "delete" the size of the points not to mess with memory while using
  // SetVoxelScalar, at the end the memory size it's the same, in fact memory
  // used for points is constant during "do undo/redo action" as we delete
  // one point while adding a new one to the opposite action, so the memory
  // variation is about the size of a point plus the size of a label.
  m_used -= (numberOfPoints * m_sizePoint);

  // reverse labels from all points in the action
  for (unsigned long int i = 0; i < numberOfPoints; i++)
  {
    auto action_point = action_vector.back();
    action_vector.pop_back();

    Vector3ui point = action_point.first;
    m_dataManager->SetVoxelScalar(point, action_point.second);
  }

  m_dataManager->SignalDataAsModified();

  // insert the changed action in the right buffer and apply the actual LookupTable and table values
  // in fact only the pointers in the original ObjectVector (a std::map) are changed the objects are
  // only deleted when the undoredosystem deletes them
  auto temporalSet = m_dataManager->GetSelectedLabelsSet();
  switch (type)
  {
    case Type::UNDO:
      m_redo.push_back(*m_current);
      m_undo.pop_back();

      m_used -= m_redo.back().lut->GetNumberOfColors() * m_sizeColor;
      m_dataManager->SwitchLookupTables(m_redo.back().lut);
      m_used += m_redo.back().lut->GetNumberOfColors() * m_sizeColor;

      m_used -= m_redo.back().labels.size() * m_sizeLabel;
      m_dataManager->SetSelectedLabelsSet(m_redo.back().labels);
      m_redo.back().labels = temporalSet;
      m_used += m_redo.back().labels.size() * m_sizeLabel;

      for (auto it: m_redo.back().objects)
      {
        (*m_dataManager->GetObjectTablePointer()).erase(it.first);
      }

      break;
    case Type::REDO:
      m_undo.push_back(*m_current);
      m_redo.pop_back();

      m_used -= m_undo.back().lut->GetNumberOfColors() * m_sizeColor;
      m_dataManager->SwitchLookupTables(m_undo.back().lut);
      m_used += m_undo.back().lut->GetNumberOfColors() * m_sizeColor;

      m_used -= m_undo.back().labels.size() * m_sizeLabel;
      m_dataManager->SetSelectedLabelsSet(m_undo.back().labels);
      m_undo.back().labels = temporalSet;
      m_used += m_undo.back().labels.size() * m_sizeLabel;

      for (auto it: m_undo.back().objects)
      {
        (*m_dataManager->GetObjectTablePointer())[it.first] = it.second;
      }

      break;
    default:
      break;
  }

  checkLimits();
  m_current = nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const std::string UndoRedoSystem::getActionString(const Type type) const
{
  switch (type)
  {
    case Type::UNDO:
      if (!isEmpty(Type::UNDO))
      {
        return m_undo.back().description;
      }
      break;
    case Type::REDO:
      if (!isEmpty(Type::REDO))
      {
        return m_redo.back().description;
      }
      break;
    case Type::ACTUAL:
      if (m_current)
      {
        return (*m_current).description;
      }
      break;
    case Type::ALL:
    default:
      break;
  }

  return std::string();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void UndoRedoSystem::signalCancelAction()
{
  m_bufferFull = false;

  m_used -= (*m_current).points.size() * m_sizePoint;
  m_used -= (*m_current).objects.size() * m_sizeObject;
  m_used -= (*m_current).description.capacity();
  m_used -= ((*m_current).lut)->GetNumberOfTableValues() * m_sizeColor;
  m_used -= (*m_current).labels.size() * m_sizeLabel;
  m_used -= m_sizeAction;

  // undo changes if there are some, bypassing the undo system as we don't want to store this
  // modifications to the data
  while ((*m_current).points.size() != 0)
  {
    auto action_point = (*m_current).points.back();
    (*m_current).points.pop_back();

    auto point = action_point.first;
    m_dataManager->SetVoxelScalarRaw(point, action_point.second);
  }

  m_dataManager->SignalDataAsModified();

  // delete dinamically allocated objects
  while (!(*m_current).objects.empty())
  {
    auto pair = (*m_current).objects.back();
    (*m_dataManager->GetObjectTablePointer()).erase(pair.first);
    pair.second = nullptr;
    (*m_current).objects.pop_back();
  }

  delete m_current;

  m_current = nullptr;
}
