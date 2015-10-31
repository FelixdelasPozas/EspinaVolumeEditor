///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: DataManager.cpp
// Purpose: converts a itk::LabelMap to vtkStructuredPoints data and back 
// Notes: vtkStructuredPoints scalar values aren't the label map values so mapping between
//        label map and vtkStructuredPoints scalars is necessary
///////////////////////////////////////////////////////////////////////////////////////////////////

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

using ChangeType = itk::ChangeLabelLabelMapFilter<LabelMapType>;
using ImageRegionType = itk::ImageRegion<3>;

//////////////////////////////////////////////////////////////////////////////////////////////////
// DataManager class
//
DataManager::DataManager()
: m_labelMap        {nullptr}
, m_structuredPoints{nullptr}
, m_lookupTable     {nullptr}
, m_orientationData {nullptr}
, m_actionsBuffer   {std::make_shared<UndoRedoSystem>(this)}
, m_firstFreeValue  {1}
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::Initialize(itk::SmartPointer<LabelMapType> labelMap, std::shared_ptr<Coordinates> coordinates, std::shared_ptr<Metadata> metadata)
{
  m_orientationData = coordinates;
  auto spacing      = m_orientationData->GetImageSpacing();
  auto imageorigin  = m_orientationData->GetImageOrigin();
  auto imagesize    = m_orientationData->GetImageSize();

  // insert background label info, initially all voxels are background, we'll subtract later
  auto object = std::make_shared<ObjectInformation>();
  object->scalar   = 0;
  object->centroid = Vector3d((imagesize[0] / 2.0) * spacing[0], (imagesize[1] / 2.0) * spacing[1], (imagesize[2] / 2.0) / spacing[2]);
  object->size     = imagesize[0] * imagesize[1] * imagesize[2];
  object->min      = Vector3ui(0, 0, 0);
  object->max      = Vector3ui(imagesize[0], imagesize[1], imagesize[2]);

  ObjectVector.insert(std::pair<unsigned short, std::shared_ptr<ObjectInformation>>(0, object));

  // evaluate shapelabelobjects to get the centroid of the object
  auto evaluator = itk::ShapeLabelMapFilter<LabelMapType>::New();
  evaluator->SetInput(labelMap);
  evaluator->ComputePerimeterOff();
  evaluator->ComputeFeretDiameterOff();
  evaluator->SetInPlace(true);
  evaluator->Update();

  // get voxel count for each label for statistics and "flatten" the labelmap (make all labels consecutive starting from 1)
  auto labelChanger = ChangeType::New();
  labelChanger->SetInput(evaluator->GetOutput());
  labelChanger->SetInPlace(true);

  ImageRegionType region;
  unsigned short i = 1;
  itk::Point<double, 3> centroid;

  for (int i = 0; i < evaluator->GetOutput()->GetNumberOfLabelObjects(); ++i)
  {
    auto labelObject = evaluator->GetOutput()->GetNthLabelObject(i);
    auto scalar = labelObject->GetLabel();
    centroid = labelObject->GetCentroid();
    region = labelObject->GetBoundingBox();
    auto regionOrigin = region.GetIndex();
    auto regionSize = region.GetSize();

    object = std::make_shared<ObjectInformation>();
    object->scalar   = scalar;
    object->centroid = Vector3d(centroid[0] / spacing[0], centroid[1] / spacing[1], centroid[2] / spacing[2]);
    object->size     = labelObject->Size();
    object->min      = Vector3ui(regionOrigin[0], regionOrigin[1], regionOrigin[2]);
    object->max      = Vector3ui(regionSize[0] + regionOrigin[0], regionSize[1] + regionOrigin[1], regionSize[2] + regionOrigin[2]) - Vector3ui(1, 1, 1);

    ObjectVector.insert(std::pair<unsigned short, std::shared_ptr<ObjectInformation>>(i, object));

    // substract the voxels of this object from the background label
    ObjectVector[0]->size -= labelObject->Size();

    // need to mark object label as used to correct errors in the segmha metadata (defined labels but empty objects)
    metadata->markAsUsed(scalar);

    // flatten label
    labelChanger->SetChange(scalar, i);
  }

  // start entering new labels at the end of the scalar range
  m_firstFreeValue = GetScalarForLabel(GetNumberOfLabels() - 1) + 1;

  // apply all the changes made to labels
  labelChanger->Update();

  m_labelMap = LabelMapType::New();
  m_labelMap = labelChanger->GetOutput();
  m_labelMap->Optimize();
  m_labelMap->Update();

  // generate the initial vtkLookupTable
  m_lookupTable = vtkSmartPointer<vtkLookupTable>::New();
  GenerateLookupTable();
  m_lookupTable->Modified();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
DataManager::~DataManager()
{
  ObjectVector.clear();
  ActionInformationVector.clear();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::SetStructuredPoints(vtkSmartPointer<vtkStructuredPoints> points)
{
  m_structuredPoints = vtkSmartPointer<vtkStructuredPoints>::New();
  m_structuredPoints->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
  m_structuredPoints->CopyInformationFromPipeline(points->GetInformation());
  m_structuredPoints->DeepCopy(points);
  m_structuredPoints->Modified();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
vtkSmartPointer<vtkStructuredPoints> DataManager::GetStructuredPoints() const
{
  return m_structuredPoints;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
itk::SmartPointer<LabelMapType> DataManager::GetLabelMap() const
{
  return m_labelMap;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<Coordinates> DataManager::GetOrientationData() const
{
  return m_orientationData;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
std::map<unsigned short, std::shared_ptr<DataManager::ObjectInformation>>* DataManager::GetObjectTablePointer()
{
  return &ObjectVector;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned short DataManager::GetVoxelScalar(const Vector3ui &point) const
{
  auto pixel = static_cast<unsigned short*>(m_structuredPoints->GetScalarPointer(point[0], point[1], point[2]));
  return *pixel;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::SetVoxelScalar(const Vector3ui &point, const unsigned short scalar)
{
  auto x = point[0];
  auto y = point[1];
  auto z = point[2];
  auto pixel = static_cast<unsigned short*>(m_structuredPoints->GetScalarPointer(x,y,z));

  if (scalar == *pixel) return;

  if (ActionInformationVector.find(*pixel) == ActionInformationVector.end())
  {
    auto action = std::make_shared<ActionInformation>();
    ActionInformationVector[*pixel] = action;
    action->min = Vector3ui(x, y, z);
    action->max = Vector3ui(x, y, z);
  }
  ActionInformationVector[*pixel]->size -= 1;
  ActionInformationVector[*pixel]->centroid[0] -= x;
  ActionInformationVector[*pixel]->centroid[1] -= y;
  ActionInformationVector[*pixel]->centroid[2] -= z;

  if (ActionInformationVector.find(scalar) == ActionInformationVector.end())
  {
    auto action = std::make_shared<ActionInformation>();
    ActionInformationVector[scalar] = action;
    action->min = Vector3ui(x, y, z);
    action->max = Vector3ui(x, y, z);
  }
  ActionInformationVector[scalar]->size += 1;
  ActionInformationVector[scalar]->centroid[0] += x;
  ActionInformationVector[scalar]->centroid[1] += y;
  ActionInformationVector[scalar]->centroid[2] += z;

  // have to check if the added voxel is out of the object region to make the bounding box grow
  Vector3ui min = ActionInformationVector[scalar]->min;
  Vector3ui max = ActionInformationVector[scalar]->max;

  if (x < min[0]) min[0] = x;

  if (x > max[0]) max[0] = x;

  if (y < min[1]) min[1] = y;

  if (y > max[1]) max[1] = y;

  if (z < min[2]) min[2] = z;

  if (z > max[2]) max[2] = z;

  ActionInformationVector[scalar]->min = min;
  ActionInformationVector[scalar]->max = max;

  m_actionsBuffer->storePoint(Vector3ui(x, y, z), *pixel);
  *pixel = scalar;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::SetVoxelScalarRaw(const Vector3ui &point, const unsigned short scalar)
{
  auto pixel = static_cast<unsigned short*>(m_structuredPoints->GetScalarPointer(point[0], point[1], point[2]));

  if (scalar == *pixel) return;

  *pixel = scalar;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned short DataManager::SetLabel(const QColor &color)
{
  // labelvalues usually goes 0-n, that's n+1 values = m_labelValues.size();
  auto newlabel = ObjectVector.size();

  // we need to find an unused scalar value in our table
  bool free = false;
  auto freevalue = m_firstFreeValue;

  while (!free)
  {
    free = true;

    // can't use find() as the value we're searching is the 'second' field
    for (auto it: ObjectVector)
      if (it.second->scalar == freevalue)
      {
        free = false;
        freevalue++;
      }
  }

  auto object = std::make_shared<ObjectInformation>();
  object->scalar   = freevalue;
  object->size     = 0;
  object->centroid = Vector3d{0, 0, 0};
  object->min      = Vector3ui{0, 0, 0};
  object->max      = Vector3ui{0, 0, 0};

  ObjectVector.insert(std::pair<unsigned short, std::shared_ptr<ObjectInformation>>(newlabel, object));

  m_actionsBuffer->storeObject(std::pair<unsigned short, std::shared_ptr<ObjectInformation>>(newlabel, object));

  auto temptable = vtkSmartPointer<vtkLookupTable>::New();
  CopyLookupTable(m_lookupTable, temptable);

  // this is a convoluted way of doing things, but SetNumberOfTableValues() seems to
  // corrup the table (due to reallocation?) and all values must be copied again.
  m_lookupTable->SetNumberOfTableValues(newlabel + 1);
  double rgba[4];
  for (int index = 0; index != temptable->GetNumberOfTableValues(); index++)
  {
    temptable->GetTableValue(index, rgba);
    m_lookupTable->SetTableValue(index, rgba);
  }
  m_lookupTable->SetTableValue(newlabel, color.redF(), color.greenF(), color.blueF(), 0.4);
  m_lookupTable->SetTableRange(0, newlabel);
  m_lookupTable->Modified();

  return newlabel;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::CopyLookupTable(vtkSmartPointer<vtkLookupTable> from, vtkSmartPointer<vtkLookupTable> to) const
{
  // copyTo exists and i don't want to do just a DeepCopy that could release memory, i just want to copy the colors
  double rgba[4];

  to->Allocate();
  to->SetNumberOfTableValues(from->GetNumberOfTableValues());

  for (int index = 0; index != from->GetNumberOfTableValues(); index++)
  {
    from->GetTableValue(index, rgba);
    to->SetTableValue(index, rgba);
  }

  to->SetTableRange(0, from->GetNumberOfTableValues() - 1);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::GenerateLookupTable()
{
  // compute vtkLookupTable colors for labels, first color is black for background
  // rest are precalculated by _lookupTable->Build() based on the number of labels
  double rgba[4]{ 0.05, 0.05, 0.05, 1.0 };
  auto labels = m_labelMap->GetNumberOfLabelObjects();
  Q_ASSERT(0 != labels);

  m_lookupTable->Allocate();
  m_lookupTable->SetNumberOfTableValues(labels + 1);
  m_lookupTable->SetTableRange(0, labels + 1);
  m_lookupTable->SetTableValue(0, rgba);

  auto temporal_table = vtkSmartPointer<vtkLookupTable>::New();
  temporal_table->SetNumberOfTableValues(labels);
  temporal_table->SetRange(1, labels);
  temporal_table->Build();

  for (unsigned int index = 0; index != labels; index++)
  {
    temporal_table->GetTableValue(index, rgba);
    m_lookupTable->SetTableValue(index + 1, rgba[0], rgba[1], rgba[2], 0.4);
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::SwitchLookupTables(vtkSmartPointer<vtkLookupTable> table)
{
  auto temptable = vtkSmartPointer<vtkLookupTable>::New();

  CopyLookupTable(m_lookupTable, temptable);
  CopyLookupTable(table, m_lookupTable);
  CopyLookupTable(temptable, table);

  m_lookupTable->Modified();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::StatisticsActionClear(void)
{
  for(auto it: ActionInformationVector)
  {
    it.second = nullptr;
  }

  ActionInformationVector.clear();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::OperationStart(const std::string &actionName)
{
  StatisticsActionClear();
  m_actionsBuffer->signalBeginAction(actionName, m_selectedLabels, m_lookupTable);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::OperationEnd()
{
  m_actionsBuffer->signalEndAction();
  StatisticsActionUpdate();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::OperationCancel()
{
  m_actionsBuffer->signalCancelAction();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
const std::string DataManager::GetUndoActionString() const
{
  return m_actionsBuffer->getActionString(UndoRedoSystem::Type::UNDO);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
const std::string DataManager::GetRedoActionString() const
{
  return m_actionsBuffer->getActionString(UndoRedoSystem::Type::REDO);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
const std::string DataManager::GetActualActionString() const
{
  return m_actionsBuffer->getActionString(UndoRedoSystem::Type::ACTUAL);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
bool DataManager::IsUndoBufferEmpty() const
{
  return m_actionsBuffer->isEmpty(UndoRedoSystem::Type::UNDO);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
bool DataManager::IsRedoBufferEmpty() const
{
  return m_actionsBuffer->isEmpty(UndoRedoSystem::Type::REDO);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::DoUndoOperation()
{
  StatisticsActionClear();
  m_actionsBuffer->doAction(UndoRedoSystem::Type::UNDO);
  StatisticsActionUpdate();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::DoRedoOperation()
{
  StatisticsActionClear();
  m_actionsBuffer->doAction(UndoRedoSystem::Type::REDO);
  StatisticsActionUpdate();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::SetUndoRedoBufferSize(unsigned long size)
{
  m_actionsBuffer->changeSize(size);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned long int DataManager::GetUndoRedoBufferSize() const
{
  return m_actionsBuffer->size();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned long int DataManager::GetUndoRedoBufferCapacity() const
{
  return m_actionsBuffer->capacity();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::SetFirstFreeValue(const unsigned short value)
{
  m_firstFreeValue = value;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned short DataManager::GetFirstFreeValue() const
{
  return m_firstFreeValue;
}

const unsigned short DataManager::GetLastUsedValue() const
{
  unsigned short value = 0;

  for (auto it: ObjectVector)
  {
    if (it.second->scalar > value) value = it.second->scalar;
  }

  return value;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
const QColor DataManager::GetRGBAColorForScalar(const unsigned short scalar) const
{
  for (auto it: ObjectVector)
    if (it.second->scalar == scalar)
    {
      double rgba[4];
      m_lookupTable->GetTableValue(it.first, rgba);
      return QColor::fromRgbF(rgba[0],rgba[1],rgba[2],rgba[3]);
    }

  // not found
  return QColor{0,0,0,0};
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::StatisticsActionUpdate(void)
{
  for (auto it: ActionInformationVector)
  {
    // the action information could refer to a deleted label (in undo/redo buffer)
    if (ObjectVector.count(it.first) == 0) continue;

    // no need to recalculate centroid or bounding box for background label
    if (it.first != 0)
    {
      // calculate new centroid based on modified voxels
      if (0 == (ObjectVector[it.first]->size + it.second->size))
      {
        ObjectVector[it.first]->centroid = Vector3d{0, 0, 0};
      }
      else
      {
        auto x = (it.second->centroid[0] / static_cast<double>(it.second->size));
        auto y = (it.second->centroid[1] / static_cast<double>(it.second->size));
        auto z = (it.second->centroid[2] / static_cast<double>(it.second->size));

        auto centroid = ObjectVector[it.first]->centroid;
        auto objectVoxels = ObjectVector[it.first]->size;

        // if the object has a centroid
        if ((Vector3d{0, 0, 0} != centroid) && (0 != objectVoxels))
        {
          auto coef_1 = objectVoxels / static_cast<double>(objectVoxels + it.second->size);
          auto coef_2 = it.second->size / static_cast<double>(objectVoxels + it.second->size);

          x = (centroid[0] * coef_1) + (x * coef_2);
          y = (centroid[1] * coef_1) + (y * coef_2);
          z = (centroid[2] * coef_1) + (z * coef_2);
        }
        ObjectVector[it.first]->centroid = Vector3d{x, y, z};
      }

      // if the object doesn't have voxels means that the calculated bounding box is the object bounding box
      if (0LL == ObjectVector[it.first]->size)
      {
        ObjectVector[it.first]->min = it.second->min;
        ObjectVector[it.first]->max = it.second->max;
      }
      // else calculate new bounding box based on modified voxels, but only if we've been adding voxels, not substracting them
      else
      {
        if (0LL < it.second->size)
        {
          if (ObjectVector[it.first]->min[0] > it.second->min[0]) ObjectVector[it.first]->min[0] = it.second->min[0];
          if (ObjectVector[it.first]->min[1] > it.second->min[1]) ObjectVector[it.first]->min[1] = it.second->min[1];
          if (ObjectVector[it.first]->min[2] > it.second->min[2]) ObjectVector[it.first]->min[2] = it.second->min[2];
          if (ObjectVector[it.first]->max[0] < it.second->max[0]) ObjectVector[it.first]->max[0] = it.second->max[0];
          if (ObjectVector[it.first]->max[1] < it.second->max[1]) ObjectVector[it.first]->max[1] = it.second->max[1];
          if (ObjectVector[it.first]->max[2] < it.second->max[2]) ObjectVector[it.first]->max[2] = it.second->max[2];
        }
      }
    }
    ObjectVector[it.first]->size += it.second->size;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
unsigned long long int DataManager::GetNumberOfVoxelsForLabel(unsigned short label)
{
  Q_ASSERT(label < ObjectVector.size());
  return ObjectVector[label]->size;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
unsigned short DataManager::GetScalarForLabel(const unsigned short label)
{
  Q_ASSERT(label < ObjectVector.size());
  return ObjectVector[label]->scalar;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
unsigned short DataManager::GetLabelForScalar(const unsigned short scalar) const
{
  for (auto it: ObjectVector)
  {
    if (it.second->scalar == scalar) return it.first;
  }

  return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
Vector3d DataManager::GetCentroidForObject(const unsigned short int label)
{
  Q_ASSERT(label < ObjectVector.size());
  return (ObjectVector[label])->centroid;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
Vector3ui DataManager::GetBoundingBoxMin(unsigned short label)
{
  Q_ASSERT(label < ObjectVector.size());
  return ObjectVector[label]->min;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
Vector3ui DataManager::GetBoundingBoxMax(unsigned short label)
{
  Q_ASSERT(label < ObjectVector.size());
  return ObjectVector[label]->max;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned int DataManager::GetNumberOfLabels(void) const
{
  return ObjectVector.size();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::ColorHighlight(const unsigned short label)
{
  if (0 == label) return;

  if (m_selectedLabels.find(label) == m_selectedLabels.end())
  {
    double rgba[4];

    m_lookupTable->GetTableValue(label, rgba);
    m_lookupTable->SetTableValue(label, rgba[0], rgba[1], rgba[2], 1);

    m_selectedLabels.insert(label);
    m_lookupTable->Modified();
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::ColorDim(const unsigned short label)
{
  if (m_selectedLabels.find(label) != m_selectedLabels.end())
  {
    double rgba[4];

    m_lookupTable->GetTableValue(label, rgba);
    m_lookupTable->SetTableValue(label, rgba[0], rgba[1], rgba[2], 0.4);

    m_selectedLabels.erase(label);
    m_lookupTable->Modified();
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::ColorHighlightExclusive(const unsigned short label)
{
  for (auto it: m_selectedLabels)
  {
    if (it == label) continue;

    double rgba[4];

    m_lookupTable->GetTableValue(it, rgba);
    m_lookupTable->SetTableValue(it, rgba[0], rgba[1], rgba[2], 0.4);
    m_selectedLabels.erase(it);
  }

  if (m_selectedLabels.find(label) == m_selectedLabels.end()) ColorHighlight(label);

  m_lookupTable->Modified();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::ColorDimAll()
{
  for (auto it: m_selectedLabels)
  {
    double rgba[4];

    m_lookupTable->GetTableValue(it, rgba);
    m_lookupTable->SetTableValue(it, rgba[0], rgba[1], rgba[2], 0.4);

    m_selectedLabels.erase(it);
  }
  m_lookupTable->Modified();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
const bool DataManager::ColorIsInUse(const QColor &color) const
{
  double rgba[4];

  for (int i = 0; i < m_lookupTable->GetNumberOfTableValues(); i++)
  {
    m_lookupTable->GetTableValue(i, rgba);
    if ((rgba[0] == color.redF()) && (rgba[1] == color.greenF()) && (rgba[2] == color.blueF())) return true;
  }

  return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned int DataManager::GetNumberOfColors() const
{
  return m_lookupTable->GetNumberOfTableValues();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
const QColor DataManager::GetColorComponents(const unsigned short label) const
{
  double rgba[4];
  m_lookupTable->GetTableValue(label, rgba);

  return QColor::fromRgbF(rgba[0], rgba[1], rgba[2], rgba[3]);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::SetColorComponents(unsigned short label, const QColor &color)
{
  m_lookupTable->SetTableValue(label, color.redF(), color.greenF(), color.blueF());
  m_lookupTable->Modified();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
vtkSmartPointer<vtkLookupTable> DataManager::GetLookupTable() const
{
  return m_lookupTable;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
const std::set<unsigned short> DataManager::GetSelectedLabelsSet(void) const
{
  return m_selectedLabels;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
const bool DataManager::IsColorSelected(unsigned short color) const
{
  return (m_selectedLabels.find(color) != m_selectedLabels.end());
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::SetSelectedLabelsSet(const std::set<unsigned short> &labelSet)
{
  m_selectedLabels = labelSet;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
const int DataManager::GetSelectedLabelSetSize(void) const
{
  return m_selectedLabels.size();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
void DataManager::SignalDataAsModified(void)
{
  m_structuredPoints->Modified();
}
