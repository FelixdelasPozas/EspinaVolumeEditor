///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: Selection.cpp
// Purpose: Manages selection areas
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// vtk includes
#include <vtkDiscreteMarchingCubes.h>
#include <vtkTextureMapToPlane.h>
#include <vtkTransformTextureCoords.h>
#include <vtkPolyDataMapper.h>
#include <vtkImageCanvasSource2D.h>
#include <vtkProperty.h>
#include <vtkImageClip.h>
#include <vtkWidgetRepresentation.h>
#include <vtkBoxRepresentation.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkCommand.h>
#include <vtkLinearContourLineInterpolator.h>
#include <vtkImageActorPointPlacer.h>

// itk includes
#include <itkSmartPointer.h>
#include <itkConnectedThresholdImageFilter.h>
#include <itkVTKImageExport.h>
#include <itkImageRegionConstIteratorWithIndex.h>
#include <itkImageDuplicator.h>

// project includes
#include "Selection.h"
#include "itkvtkpipeline.h"
#include "SliceVisualization.h"
#include "FocalPlanePointPlacer.h"
#include "ContourRepresentation.h"
#include "ContourRepresentationGlyph.h"
#include "BoxSelectionRepresentation2D.h"

// qt includes
#include <QtGui>

using ConnectedThresholdFilterType = itk::ConnectedThresholdImageFilter<ImageType, ImageTypeUC>;
using ITKExport = itk::VTKImageExport<ImageTypeUC>;
using ITKImport = itk::VTKImageImport<ImageType>;
using DuplicatorType = itk::ImageDuplicator<ImageType>;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Selection class
//
Selection::Selection()
: m_axial{nullptr}
, m_coronal{nullptr}
, m_sagittal{nullptr}
, m_renderer{nullptr}
, m_dataManager{nullptr}
, m_selectionType{Type::EMPTY}
, m_axialBoxWidget{nullptr}
, m_coronalBoxWidget{nullptr}
, m_sagittalBoxWidget{nullptr}
, m_contourWidget{nullptr}
, m_boxRender{nullptr}
, m_rotatedImage{nullptr}
, m_selectionIsValid{true}
, m_size{Vector3ui{0,0,0}}
, m_max{Vector3ui{0,0,0}}
, m_min{Vector3ui{0,0,0}}
, m_spacing{Vector3d{0,0,0}}
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////
Selection::~Selection()
{
  deleteSelectionActors();
  deleteSelectionVolumes();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Selection::initialize(std::shared_ptr<Coordinates> coordinates, vtkSmartPointer<vtkRenderer> renderer, std::shared_ptr<DataManager> dataManager)
{
  m_size = coordinates->GetTransformedSize() - Vector3ui(1, 1, 1);
  m_spacing = coordinates->GetImageSpacing();
  m_max = m_size;
  m_renderer = renderer;
  m_dataManager = dataManager;

  // create volume selection texture
  auto textureIcon = vtkSmartPointer<vtkImageCanvasSource2D>::New();
  textureIcon->SetScalarTypeToUnsignedChar();
  textureIcon->SetExtent(0, 15, 0, 15, 0, 0);
  textureIcon->SetNumberOfScalarComponents(4);
  textureIcon->SetDrawColor(0, 0, 0, 0);             // transparent color
  textureIcon->FillBox(0, 15, 0, 15);
  textureIcon->SetDrawColor(255, 255, 255, 150);     // "somewhat transparent" white
  textureIcon->DrawSegment(0, 0, 15, 15);
  textureIcon->DrawSegment(1, 0, 15, 14);
  textureIcon->DrawSegment(0, 1, 14, 15);
  textureIcon->DrawSegment(15, 0, 15, 0);
  textureIcon->DrawSegment(0, 15, 0, 15);

  m_texture = vtkSmartPointer<vtkTexture>::New();
  m_texture->SetInputConnection(textureIcon->GetOutputPort());
  m_texture->RepeatOn();
  m_texture->InterpolateOn();
  m_texture->ReleaseDataFlagOn();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Selection::addSelectionPoint(const Vector3ui &point)
{
  double bounds[6];

  // how many points do we have?
  if(0 == m_selectedPoints.size())
  {
    m_selectionType = Type::CUBE;
    m_selectedPoints.push_back(point);
    computeSelectionBounds();

    // 3D representation of the box
    m_boxRender = std::make_shared<BoxSelectionRepresentation3D>();
    m_boxRender->SetRenderer(m_renderer);

    bounds[0] = (static_cast<double>(m_min[0]) - 0.5) * m_spacing[0];
    bounds[1] = (static_cast<double>(m_max[0]) + 0.5) * m_spacing[0];
    bounds[2] = (static_cast<double>(m_min[1]) - 0.5) * m_spacing[1];
    bounds[3] = (static_cast<double>(m_max[1]) + 0.5) * m_spacing[1];
    bounds[4] = (static_cast<double>(m_min[2]) - 0.5) * m_spacing[2];
    bounds[5] = (static_cast<double>(m_max[2]) + 0.5) * m_spacing[2];
    m_boxRender->PlaceBox(bounds);

    // initialize 2D selection widgets
    auto rep = vtkSmartPointer<BoxSelectionRepresentation2D>::New();
    rep->Setm_minimumSelectionSize(-(m_spacing[0] / 2.0), -(m_spacing[1] / 2.0));
    rep->Setm_maximumSelectionSize((static_cast<double>(m_size[0]) + 0.5) * m_spacing[0], (static_cast<double>(m_size[1]) + 0.5) * m_spacing[1]);
    rep->Setm_minimumSize(m_spacing[0], m_spacing[1]);
    rep->Setm_spacing(m_spacing[0], m_spacing[1]);
    rep->SetBoxCoordinates(point[0], point[1], point[0], point[1]);
    m_axialBoxWidget = vtkSmartPointer<BoxSelectionWidget>::New();
    m_axialBoxWidget->SetInteractor(m_axial->renderer()->GetRenderWindow()->GetInteractor());
    m_axialBoxWidget->SetRepresentation(rep);
    m_axialBoxWidget->SetEnabled(true);

    rep = vtkSmartPointer<BoxSelectionRepresentation2D>::New();
    rep->Setm_minimumSelectionSize(-(m_spacing[0] / 2.0), -(m_spacing[2] / 2.0));
    rep->Setm_maximumSelectionSize((static_cast<double>(m_size[0]) + 0.5) * m_spacing[0], (static_cast<double>(m_size[2]) + 0.5) * m_spacing[2]);
    rep->Setm_minimumSize(m_spacing[0], m_spacing[2]);
    rep->Setm_spacing(m_spacing[0], m_spacing[2]);
    rep->SetBoxCoordinates(point[0], point[2], point[0], point[2]);
    m_coronalBoxWidget = vtkSmartPointer<BoxSelectionWidget>::New();
    m_coronalBoxWidget->SetInteractor(m_coronal->renderer()->GetRenderWindow()->GetInteractor());
    m_coronalBoxWidget->SetRepresentation(rep);
    m_coronalBoxWidget->SetEnabled(true);

    rep = vtkSmartPointer<BoxSelectionRepresentation2D>::New();
    rep->Setm_minimumSelectionSize(-(m_spacing[1] / 2.0), -(m_spacing[2] / 2.0));
    rep->Setm_maximumSelectionSize((static_cast<double>(m_size[1]) + 0.5) * m_spacing[1], (static_cast<double>(m_size[2]) + 0.5) * m_spacing[2]);
    rep->Setm_minimumSize(m_spacing[1], m_spacing[2]);
    rep->Setm_spacing(m_spacing[1], m_spacing[2]);
    rep->SetBoxCoordinates(point[1], point[2], point[1], point[2]);
    m_sagittalBoxWidget = vtkSmartPointer<BoxSelectionWidget>::New();
    m_sagittalBoxWidget->SetInteractor(m_sagittal->renderer()->GetRenderWindow()->GetInteractor());
    m_sagittalBoxWidget->SetRepresentation(rep);
    m_sagittalBoxWidget->SetEnabled(true);

    // set callbacks for widget interaction
    m_widgetsCallbackCommand = vtkSmartPointer<vtkCallbackCommand>::New();
    m_widgetsCallbackCommand->SetCallback(boxSelectionWidgetCallback);
    m_widgetsCallbackCommand->SetClientData(this);

    m_axialBoxWidget->AddObserver(vtkCommand::StartInteractionEvent, m_widgetsCallbackCommand);
    m_axialBoxWidget->AddObserver(vtkCommand::EndInteractionEvent, m_widgetsCallbackCommand);
    m_axialBoxWidget->AddObserver(vtkCommand::InteractionEvent, m_widgetsCallbackCommand);

    m_coronalBoxWidget->AddObserver(vtkCommand::StartInteractionEvent, m_widgetsCallbackCommand);
    m_coronalBoxWidget->AddObserver(vtkCommand::EndInteractionEvent, m_widgetsCallbackCommand);
    m_coronalBoxWidget->AddObserver(vtkCommand::InteractionEvent, m_widgetsCallbackCommand);

    m_sagittalBoxWidget->AddObserver(vtkCommand::StartInteractionEvent, m_widgetsCallbackCommand);
    m_sagittalBoxWidget->AddObserver(vtkCommand::EndInteractionEvent, m_widgetsCallbackCommand);
    m_sagittalBoxWidget->AddObserver(vtkCommand::InteractionEvent, m_widgetsCallbackCommand);

    // make the slices aware of a selection box, so they could hide and show it accordingly when the slice changes
    m_axial->setSliceWidget(m_axialBoxWidget);
    m_coronal->setSliceWidget(m_coronalBoxWidget);
    m_sagittal->setSliceWidget(m_sagittalBoxWidget);
  }
  else
  {
    // cases for sizes 1 and 2, "nearly" identical except a pop_back() in case 2
    if (m_selectedPoints.size() == 2) m_selectedPoints.pop_back();

    m_selectedPoints.push_back(point);
    computeSelectionBounds();

    bounds[0] = (static_cast<double>(m_min[0]) - 0.5) * m_spacing[0];
    bounds[1] = (static_cast<double>(m_max[0]) + 0.5) * m_spacing[0];
    bounds[2] = (static_cast<double>(m_min[1]) - 0.5) * m_spacing[1];
    bounds[3] = (static_cast<double>(m_max[1]) + 0.5) * m_spacing[1];
    bounds[4] = (static_cast<double>(m_min[2]) - 0.5) * m_spacing[2];
    bounds[5] = (static_cast<double>(m_max[2]) + 0.5) * m_spacing[2];
    m_boxRender->PlaceBox(bounds);

    // update widget representations
    m_axialBoxWidget->SetEnabled(false);
    m_coronalBoxWidget->SetEnabled(false);
    m_sagittalBoxWidget->SetEnabled(false);

    auto repPointer = m_axialBoxWidget->GetBorderRepresentation();
    repPointer->SetBoxCoordinates(m_min[0], m_min[1], m_max[0], m_max[1]);
    repPointer = m_coronalBoxWidget->GetBorderRepresentation();
    repPointer->SetBoxCoordinates(m_min[0], m_min[2], m_max[0], m_max[2]);
    repPointer = m_sagittalBoxWidget->GetBorderRepresentation();
    repPointer->SetBoxCoordinates(m_min[1], m_min[2], m_max[1], m_max[2]);

    m_axialBoxWidget->SetEnabled(true);
    m_coronalBoxWidget->SetEnabled(true);
    m_sagittalBoxWidget->SetEnabled(true);
  }

  computeSelectionCube();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Selection::computeSelectionBounds()
{
  if (m_selectedPoints.empty()) return;

  // initialize with first point
  m_max = m_min = m_selectedPoints[0];

  // get actual selection bounds
  for(auto it: m_selectedPoints)
  {
    if (it[0] < m_min[0]) m_min[0] = it[0];
    if (it[1] < m_min[1]) m_min[1] = it[1];
    if (it[2] < m_min[2]) m_min[2] = it[2];
    if (it[0] > m_max[0]) m_max[0] = it[0];
    if (it[1] > m_max[1]) m_max[1] = it[1];
    if (it[2] > m_max[2]) m_max[2] = it[2];
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Selection::computeSelectionCube()
{
  // clear previously selected data before creating a new selection cube, if there is any
  deleteSelectionActors();
  deleteSelectionVolumes();
  m_axial->clearSelections();
  m_coronal->clearSelections();
  m_sagittal->clearSelections();

  // bounds can be negative so we need to avoid unsigned types
  auto minBounds = Vector3i(static_cast<int>(m_min[0]), static_cast<int>(m_min[1]), static_cast<int>(m_min[2]));
  auto maxBounds = Vector3i(static_cast<int>(m_max[0]), static_cast<int>(m_max[1]), static_cast<int>(m_max[2]));

  //modify bounds so theres a point of separation between bounds and actor
  if (m_min[0] >= 0) minBounds[0]--;
  if (m_min[1] >= 0) minBounds[1]--;
  if (m_min[2] >= 0) minBounds[2]--;
  if (m_max[0] <= m_size[0]) maxBounds[0]++;
  if (m_max[1] <= m_size[1]) maxBounds[1]++;
  if (m_max[2] <= m_size[2]) maxBounds[2]++;

  // create selection volume (plus borders for correct actor generation)
  auto subvolume = vtkSmartPointer<vtkImageData>::New();
  subvolume->SetSpacing(m_spacing[0], m_spacing[1], m_spacing[2]);
  subvolume->SetExtent(minBounds[0], maxBounds[0], minBounds[1], maxBounds[1], minBounds[2], maxBounds[2]);
  subvolume->AllocateScalars(VTK_UNSIGNED_CHAR, 1);

  // fill volume
  auto voxel = static_cast<unsigned char*>(subvolume->GetScalarPointer());
  memset(voxel, 0, (maxBounds[0] - minBounds[0] + 1) * (maxBounds[1] - minBounds[1] + 1) * (maxBounds[2] - minBounds[2] + 1));
  for (unsigned int x = m_min[0]; x <= m_max[0]; x++)
  {
    for (unsigned int y = m_min[1]; y <= m_max[1]; y++)
    {
      for (unsigned int z = m_min[2]; z <= m_max[2]; z++)
      {
        voxel = static_cast<unsigned char*>(subvolume->GetScalarPointer(x, y, z));
        *voxel = VOXEL_SELECTED;
      }
    }
  }

  subvolume->Modified();

  // create textured actors for the slice views
  m_axial->setSelectionVolume(subvolume);
  m_coronal->setSelectionVolume(subvolume);
  m_sagittal->setSelectionVolume(subvolume);

  // create render actor and add it to the list of actors (but there can only be one as this is a cube selection)
  computeActor(subvolume);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Selection::clear()
{
  deleteSelectionActors();
  deleteSelectionVolumes();
  m_axial->clearSelections();
  m_coronal->clearSelections();
  m_sagittal->clearSelections();

  if (m_selectionType == Type::CUBE)
  {
    m_axial->setSliceWidget(nullptr);
    m_coronal->setSliceWidget(nullptr);
    m_sagittal->setSliceWidget(nullptr);
    m_axialBoxWidget = nullptr;
    m_coronalBoxWidget = nullptr;
    m_sagittalBoxWidget = nullptr;
    m_widgetsCallbackCommand = nullptr;

    m_boxRender = nullptr;
  }

  if (m_selectionType == Type::CONTOUR)
  {
    m_axial->setSliceWidget(nullptr);
    m_coronal->setSliceWidget(nullptr);
    m_sagittal->setSliceWidget(nullptr);
    m_contourWidget = nullptr;
    m_rotatedImage = nullptr;
    m_widgetsCallbackCommand = nullptr;
    m_selectionIsValid = true;
  }

  // clear selection points and bounds
  m_selectedPoints.clear();
  m_min = Vector3ui(0, 0, 0);
  m_max = m_size;
  m_selectionType = Type::EMPTY;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const Selection::Type Selection::type() const
{
  return m_selectionType;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Selection::addArea(const Vector3ui &point)
{
  // if the user picked in an already selected area just return
  if (isInsideSelection(point)) return;

  itk::Index<3> seed;
  seed[0] = point[0];
  seed[1] = point[1];
  seed[2] = point[2];

  auto label = m_dataManager->GetVoxelScalar(point);
  assert(label != 0);
  auto image = ImageType::New();
  image = segmentationItkImage(label);

  auto connectThreshold = ConnectedThresholdFilterType::New();
  connectThreshold->SetInput(image);
  connectThreshold->AddSeed(seed);
  connectThreshold->ReleaseDataFlagOn();
  connectThreshold->SetLower(label);
  connectThreshold->SetUpper(label);
  connectThreshold->SetReplaceValue(VOXEL_SELECTED);
  connectThreshold->SetConnectivity(ConnectedThresholdFilterType::FullConnectivity);

  try
  {
    connectThreshold->UpdateLargestPossibleRegion();
  }
  catch (itk::ExceptionObject &excp)
  {
    QMessageBox msgBox;
    msgBox.setCaption("Error while selecting");
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText("An error occurred in connected thresholding.\nThe operation has been aborted.");
    msgBox.setDetailedText(excp.what());
    msgBox.exec();
    return;
  }

  auto itkExporter = ITKExport::New();
  auto vtkImporter = vtkSmartPointer<vtkImageImport>::New();
  itkExporter->SetInput(connectThreshold->GetOutput());
  ConnectPipelines(itkExporter, vtkImporter);

  try
  {
    vtkImporter->Update();
  }
  catch (itk::ExceptionObject &excp)
  {
    QMessageBox msgBox;
    msgBox.setCaption("Error while selecting");
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText("An error occurred converting an itk image to a vtk image.\nThe operation has been aborted.");
    msgBox.setDetailedText(excp.what());
    msgBox.exec();
    return;
  }

  // extract the interesting subvolume of our selection buffer
  auto min = m_dataManager->GetBoundingBoxMin(label);
  auto max = m_dataManager->GetBoundingBoxMax(label);

  if (m_selectionType == Type::EMPTY)
  {
    m_min = min;
    m_max = max;
  }
  else
  {
    // not our first, update selection bounds
    if (m_min[0] > min[0]) m_min[0] = min[0];
    if (m_min[1] > min[1]) m_min[1] = min[1];
    if (m_min[2] > min[2]) m_min[2] = min[2];
    if (m_max[0] < max[0]) m_max[0] = max[0];
    if (m_max[1] < max[1]) m_max[1] = max[1];
    if (m_max[2] < max[2]) m_max[2] = max[2];
  }

  // create a copy of the subvolume
  auto subvolume = vtkSmartPointer<vtkImageData>::New();
  subvolume->DeepCopy(vtkImporter->GetOutput());
  subvolume->Modified();

  // add volume to list
  m_selectionVolumesList.push_back(subvolume);

  // generate textured slice actors
  m_axial->setSelectionVolume(subvolume);
  m_coronal->setSelectionVolume(subvolume);
  m_sagittal->setSelectionVolume(subvolume);

  // generate render actor
  computeActor(subvolume);

  m_selectionType = Type::VOLUME;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
itk::SmartPointer<ImageType> Selection::segmentationItkImage(const unsigned short label) const
{
  auto objectMin = m_dataManager->GetBoundingBoxMin(label);
  auto objectMax = m_dataManager->GetBoundingBoxMax(label);

  // first crop the region and then use the vtk-itk pipeline to get a itk::Image of the region
  auto imageClip = vtkSmartPointer<vtkImageClip>::New();
  imageClip->SetInputData(m_dataManager->GetStructuredPoints());
  imageClip->SetOutputWholeExtent(objectMin[0], objectMax[0], objectMin[1], objectMax[1], objectMin[2], objectMax[2]);
  imageClip->ClipDataOn();
  imageClip->Update();

  auto itkImport = ITKImport::New();
  auto vtkExport = vtkSmartPointer<vtkImageExport>::New();
  vtkExport->SetInputData(imageClip->GetOutput());
  ConnectPipelines(vtkExport, itkImport);
  itkImport->Update();

  // need to duplicate the output, or it will be destroyed when the pipeline goes
  // out of scope, and register it to increase the reference count to return the image.
  auto duplicator = DuplicatorType::New();
  duplicator->SetInputImage(itkImport->GetOutput());
  duplicator->Update();

  auto image = ImageType::New();
  image = duplicator->GetOutput();
  image->Register();

  return image;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
itk::SmartPointer<ImageType> Selection::itkImage(const unsigned short label, const unsigned int boundsGrow) const
{
  Vector3ui objectMin, objectMax;

  // first crop the region and then use the vtk-itk pipeline to get a itk::Image of the region
  auto imageClip = vtkSmartPointer<vtkImageClip>::New();
  imageClip->SetInputData(m_dataManager->GetStructuredPoints());

  switch (m_selectionType)
  {
    case Type::EMPTY:
    case Type::DISC:
      objectMin = m_dataManager->GetBoundingBoxMin(label);
      objectMax = m_dataManager->GetBoundingBoxMax(label);
      break;
    default: // VOLUME, CUBE, CONTOUR
      objectMin = m_min;
      objectMax = m_max;
      break;
  }

  // take care of the limits of the image when growing
  if (objectMin[0] < boundsGrow)
  {
    objectMin[0] = 0;
  }
  else
  {
    objectMin[0] -= boundsGrow;
  }

  if (objectMin[1] < boundsGrow)
  {
    objectMin[1] = 0;
  }
  else
  {
    objectMin[1] -= boundsGrow;
  }

  if (objectMin[2] < boundsGrow)
  {
    objectMin[2] = 0;
  }
  else
  {
    objectMin[2] -= boundsGrow;
  }

  if ((objectMax[0] + boundsGrow) > m_size[0])
  {
    objectMax[0] = m_size[0];
  }
  else
  {
    objectMax[0] += boundsGrow;
  }

  if ((objectMax[1] + boundsGrow) > m_size[1])
  {
    objectMax[1] = m_size[1];
  }
  else
  {
    objectMax[1] += boundsGrow;
  }

  if ((objectMax[2] + boundsGrow) > m_size[2])
  {
    objectMax[2] = m_size[2];
  }
  else
  {
    objectMax[2] += boundsGrow;
  }

  imageClip->SetOutputWholeExtent(objectMin[0], objectMax[0], objectMin[1], objectMax[1], objectMin[2], objectMax[2]);
  imageClip->ClipDataOn();
  imageClip->Update();

  auto itkImport = ITKImport::New();
  auto vtkExport = vtkSmartPointer<vtkImageExport>::New();
  vtkExport->SetInputData(imageClip->GetOutput());
  ConnectPipelines(vtkExport, itkImport);
  itkImport->Update();

  // need to duplicate the output, or it will be destroyed when the pipeline goes
  // out of scope, and register it to increase the reference count to return the image.
  auto duplicator = DuplicatorType::New();
  duplicator->SetInputImage(itkImport->GetOutput());
  duplicator->Update();

  auto image = ImageType::New();
  image = duplicator->GetOutput();
  image->Register();

  return image;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
itk::SmartPointer<ImageType> Selection::itkImage() const
{
  auto itkImport = ITKImport::New();
  auto vtkExport = vtkSmartPointer<vtkImageExport>::New();
  vtkExport->SetInputData(m_dataManager->GetStructuredPoints());
  ConnectPipelines(vtkExport, itkImport);
  itkImport->Update();

  // need to duplicate the output, or it will be destroyed when the pipeline goes
  // out of scope, and register it to increase the reference count to return the image.
  itk::SmartPointer<DuplicatorType> duplicator = DuplicatorType::New();
  duplicator->SetInputImage(itkImport->GetOutput());
  duplicator->Update();

  auto image = ImageType::New();
  image = duplicator->GetOutput();
  image->Register();

  return image;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const Vector3ui Selection::minimumBouds() const
{
  return m_min;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const Vector3ui Selection::maximumBouds() const
{
  return m_max;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool Selection::isInsideSelection(const Vector3ui &point) const
{

  for (auto it: m_selectionVolumesList)
  {
    if (true == isInsideSelectionSubvolume(it, point))
    {
      return true;
    }
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool Selection::isInsideSelectionSubvolume(vtkSmartPointer<vtkImageData> subvolume, const Vector3ui &point) const
{
  int extent[6];
  subvolume->GetExtent(extent);

  switch (m_selectionType)
  {
    case Type::CONTOUR:
    case Type::DISC:
    {
      double origin[3];
      subvolume->GetOrigin(origin);
      Vector3i dPoint{ static_cast<int>(point[0]) - static_cast<int>(origin[0] / m_spacing[0]),
                       static_cast<int>(point[1]) - static_cast<int>(origin[1] / m_spacing[1]),
                       static_cast<int>(point[2]) - static_cast<int>(origin[2] / m_spacing[2]) };

      if ((extent[0] <= dPoint[0]) && (extent[1] >= dPoint[0]) && (extent[2] <= dPoint[1]) && (extent[3] >= dPoint[1]) && (extent[4] <= dPoint[2]) && (extent[5] >= dPoint[2]))
      {
        auto voxel = static_cast<unsigned char*>(subvolume->GetScalarPointer(dPoint[0], dPoint[1], dPoint[2]));
        return (VOXEL_SELECTED == *voxel);
      }
      break;
    }
    default:
      if ((extent[0] <= static_cast<int>(point[0])) && (extent[1] >= static_cast<int>(point[0])) &&
          (extent[2] <= static_cast<int>(point[1])) && (extent[3] >= static_cast<int>(point[1])) &&
          (extent[4] <= static_cast<int>(point[2])) && (extent[5] >= static_cast<int>(point[2])))
      {
        auto voxel = static_cast<unsigned char*>(subvolume->GetScalarPointer(point[0], point[1], point[2]));
        return (VOXEL_SELECTED == *voxel);
      }
      break;
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Selection::computeActor(vtkSmartPointer<vtkImageData> volume)
{
  // create and setup actor for selection area... some parts of the pipeline have SetGlobalWarningDisplay(false)
  // because i don't want to generate a warning when used with empty input data (no user selection)

  // generate surface
  auto marcher = vtkSmartPointer<vtkDiscreteMarchingCubes>::New();
  marcher->SetInputData(volume);
  marcher->ReleaseDataFlagOn();
  marcher->SetGlobalWarningDisplay(false);
  marcher->SetNumberOfContours(1);
  marcher->GenerateValues(1, VOXEL_SELECTED, VOXEL_SELECTED);
  marcher->ComputeScalarsOff();
  marcher->ComputeNormalsOff();
  marcher->ComputeGradientsOff();

  // NOTE: not using normals to render the selection because we need to represent as many voxels as possible, also
  // we don't decimate our mesh for the same reason. Because the segmentations used are usually very small there
  // shouldn't be any rendering/performance problems.
  auto textureMapper = vtkSmartPointer<vtkTextureMapToPlane>::New();
  textureMapper->SetInputConnection(marcher->GetOutputPort());
  textureMapper->SetGlobalWarningDisplay(false);
  textureMapper->AutomaticPlaneGenerationOn();

  auto textureTrans = vtkSmartPointer<vtkTransformTextureCoords>::New();
  textureTrans->SetInputConnection(textureMapper->GetOutputPort());
  textureTrans->SetGlobalWarningDisplay(false);
  textureTrans->SetScale(m_size[0], m_size[1], m_size[2]);

  auto polydataMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  polydataMapper->SetInputConnection(textureTrans->GetOutputPort());
  polydataMapper->SetResolveCoincidentTopologyToOff();

  auto actor = vtkSmartPointer<vtkActor>::New();
  actor->SetMapper(polydataMapper);
  actor->SetTexture(m_texture);
  actor->GetProperty()->SetOpacity(1);
  actor->SetVisibility(true);

  m_renderer->AddActor(actor);

  // add actor to the actor list
  m_selectionActorsList.push_back(actor);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Selection::setSliceViews(std::shared_ptr<SliceVisualization> axial,
                              std::shared_ptr<SliceVisualization> coronal,
                              std::shared_ptr<SliceVisualization> sagittal)
{
  m_axial = axial;
  m_coronal = coronal;
  m_sagittal = sagittal;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Selection::deleteSelectionVolumes(void)
{
  m_selectionVolumesList.clear();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Selection::deleteSelectionActors(void)
{
  for (auto it: m_selectionActorsList)
  {
    m_renderer->RemoveActor(it);
  }

  m_selectionActorsList.clear();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Selection::setSelectionDisc(const Vector3i &point, const unsigned int radius, std::shared_ptr<SliceVisualization> view)
{
  // static vars are used when the selection changes radius or orientation;
  static int selectionRadius = 0;
  auto selectionOrientation = SliceVisualization::Orientation::None;

  // create volume if it doesn't exists
  if (m_selectionVolumesList.empty())
  {
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetSpacing(m_spacing[0], m_spacing[1], m_spacing[2]);
    image->SetOrigin(0.0, 0.0, 0.0);

    int extent[6] =
    { 0, 0, 0, 0, 0, 0 };

    switch (view->orientationType())
    {
      case SliceVisualization::Orientation::Axial:
        extent[1] = extent[3] = (radius * 2) - 2;
        break;
      case SliceVisualization::Orientation::Coronal:
        extent[1] = extent[5] = (radius * 2) - 2;
        break;
      case SliceVisualization::Orientation::Sagittal:
        extent[3] = extent[5] = (radius * 2) - 2;
        break;
      default:
        break;
    }
    image->SetExtent(extent);
    image->AllocateScalars(VTK_INT, 1);

    // after creating the image, fill it according to radius value
    switch (view->orientationType())
    {
      case SliceVisualization::Orientation::Axial:
        for (int a = 0; a <= extent[1]; a++)
        {
          for (int b = 0; b <= extent[3]; b++)
          {
            auto pointer = static_cast<int*>(image->GetScalarPointer(a, b, 0));
            if (((pow(abs(radius - 1 - a), 2) + pow(abs(radius - 1 - b), 2))) <= pow(radius - 1, 2))
              *pointer = static_cast<int>(Selection::VOXEL_SELECTED);
            else
              *pointer = static_cast<int>(Selection::VOXEL_UNSELECTED);
          }
        }
        break;
      case SliceVisualization::Orientation::Coronal:
        for (int a = 0; a <= extent[1]; a++)
        {
          for (int b = 0; b <= extent[5]; b++)
          {
            auto pointer = static_cast<int*>(image->GetScalarPointer(a, 0, b));
            if (((pow(abs(radius - 1 - a), 2) + pow(abs(radius - 1 - b), 2))) <= pow(radius - 1, 2))
              *pointer = static_cast<int>(Selection::VOXEL_SELECTED);
            else
              *pointer = static_cast<int>(Selection::VOXEL_UNSELECTED);
          }
        }
        break;
      case SliceVisualization::Orientation::Sagittal:
        for (int a = 0; a <= extent[3]; a++)
        {
          for (int b = 0; b <= extent[5]; b++)
          {
            auto pointer = static_cast<int*>(image->GetScalarPointer(0, a, b));
            if (((pow(abs(radius - 1 - a), 2) + pow(abs(radius - 1 - b), 2))) <= pow(radius - 1, 2))
              *pointer = static_cast<int>(Selection::VOXEL_SELECTED);
            else
              *pointer = static_cast<int>(Selection::VOXEL_UNSELECTED);
          }
        }
        break;
      default:
        break;
    }
    image->Modified();

    // create clipper and changer to set the pipeline, but update them later
    int clipperExtent[6] = { 0, 0, 0, 0, 0, 0 };
    m_clipper = vtkSmartPointer<vtkImageClip>::New();
    m_clipper->SetInputData(image);
    m_clipper->ClipDataOn();
    m_clipper->SetOutputWholeExtent(clipperExtent);

    m_changer = vtkSmartPointer<vtkImageChangeInformation>::New();
    m_changer->SetInputData(m_clipper->GetOutput());

    auto translatedVolume = m_changer->GetOutput();
    m_selectionVolumesList.push_back(translatedVolume);
    m_axial->setSelectionVolume(translatedVolume, false);
    m_coronal->setSelectionVolume(translatedVolume, false);
    m_sagittal->setSelectionVolume(translatedVolume, false);
    selectionRadius = radius;
    selectionOrientation = view->orientationType();
    m_selectionType = Type::DISC;
  }
  else
  {
    if ((selectionOrientation != view->orientationType()) || (selectionRadius != radius))
    {
      // recompute volume when the user changes parameters or goes from one view to another
      m_axial->clearSelections();
      m_coronal->clearSelections();
      m_sagittal->clearSelections();
      m_selectionVolumesList.pop_back();
      setSelectionDisc(point, radius, view);
    }
  }

  // update the disc representation according to parameters
  int clipperExtent[6] = { 0, 0, 0, 0, 0, 0 };
  switch (view->orientationType())
  {
    case SliceVisualization::Orientation::Axial:
      clipperExtent[0] = ((point[0] - radius) < 0) ? abs(point[0] - radius) : 0;
      clipperExtent[1] = ((point[0] + radius) > m_size[0]) ? (radius - point[0] + m_size[0]) : (radius * 2) - 2;
      clipperExtent[2] = ((point[1] - radius) < 0) ? abs(point[1] - radius) : 0;
      clipperExtent[3] = ((point[1] + radius) > m_size[1]) ? (radius - point[1] + m_size[1]) : (radius * 2) - 2;
      break;
    case SliceVisualization::Orientation::Coronal:
      clipperExtent[0] = ((point[0] - radius) < 0) ? abs(point[0] - radius) : 0;
      clipperExtent[1] = ((point[0] + radius) > m_size[0]) ? (radius - point[0] + m_size[0]) : (radius * 2) - 2;
      clipperExtent[4] = ((point[2] - radius) < 0) ? abs(point[2] - radius) : 0;
      clipperExtent[5] = ((point[2] + radius) > m_size[2]) ? (radius - point[2] + m_size[2]) : (radius * 2) - 2;
      break;
    case SliceVisualization::Orientation::Sagittal:
      clipperExtent[2] = ((point[1] - radius) < 0) ? abs(point[1] - radius) : 0;
      clipperExtent[3] = ((point[1] + radius) > m_size[1]) ? (radius - point[1] + m_size[1]) : (radius * 2) - 2;
      clipperExtent[4] = ((point[2] - radius) < 0) ? abs(point[2] - radius) : 0;
      clipperExtent[5] = ((point[2] + radius) > m_size[2]) ? (radius - point[2] + m_size[2]) : (radius * 2) - 2;
      break;
    default:
      break;
  }
  m_clipper->SetOutputWholeExtent(clipperExtent);
  m_clipper->Update();

  switch (view->orientationType())
  {
    case SliceVisualization::Orientation::Axial:
      m_changer->SetOutputOrigin((point[0] - radius) * m_spacing[0], (point[1] - radius) * m_spacing[1], point[2] * m_spacing[2]);
      m_min = Vector3ui(((point[0] - radius) > 0 ? (point[0] - radius) : 0), ((point[1] - radius) > 0 ? (point[1] - radius) : 0), static_cast<unsigned int>(point[2]));
      m_max = Vector3ui(((point[0] + radius) < m_size[0] ? (point[0] + radius) : m_size[0]), ((point[1] + radius) < m_size[1] ? (point[1] + radius) : m_size[1]), static_cast<unsigned int>(point[2]));
      break;
    case SliceVisualization::Orientation::Coronal:
      m_changer->SetOutputOrigin((point[0] - radius) * m_spacing[0], point[1] * m_spacing[1], (point[2] - radius) * m_spacing[2]);
      m_min = Vector3ui(((point[0] - radius) > 0 ? (point[0] - radius) : 0), static_cast<unsigned int>(point[1]), ((point[2] - radius) > 0 ? (point[2] - radius) : 0));
      m_max = Vector3ui(((point[0] + radius) < m_size[0] ? (point[0] + radius) : m_size[0]), static_cast<unsigned int>(point[1]), ((point[2] + radius) < m_size[2] ? (point[2] + radius) : m_size[2]));
      break;
    case SliceVisualization::Orientation::Sagittal:
      m_changer->SetOutputOrigin(point[0] * m_spacing[0], (point[1] - radius) * m_spacing[1], (point[2] - radius) * m_spacing[2]);
      m_min = Vector3ui(static_cast<unsigned int>(point[0]), ((point[1] - radius) > 0 ? (point[1] - radius) : 0), ((point[2] - radius) > 0 ? (point[2] - radius) : 0));
      m_max = Vector3ui(static_cast<unsigned int>(point[0]), ((point[1] + radius) < m_size[1] ? (point[1] + radius) : m_size[1]), ((point[2] + radius) < m_size[2] ? (point[2] + radius) : m_size[2]));
      break;
    default:
      break;
  }
  m_changer->Update();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Selection::boxSelectionWidgetCallback(vtkObject *caller, unsigned long event, void *clientdata, void *calldata)
{
  Selection *self = static_cast<Selection *>(clientdata);
  auto min = self->minimumBouds();
  auto max = self->maximumBouds();

  auto callerWidget = static_cast<BoxSelectionWidget*>(caller);
  auto callerRepresentation = static_cast<BoxSelectionRepresentation2D*>(callerWidget->GetRepresentation());
  double *spacing = callerRepresentation->Getm_spacing();
  double *pos1 = callerRepresentation->GetPosition();
  double *pos2 = callerRepresentation->GetPosition2();

  unsigned int pos1i[2] = { static_cast<unsigned int>(std::floor(pos1[0] / spacing[0]) + 1), static_cast<unsigned int>(floor(pos1[1] / spacing[1]) + 1) };
  unsigned int pos2i[2] = { static_cast<unsigned int>(std::floor(pos2[0] / spacing[0])), static_cast<unsigned int>(floor(pos2[1] / spacing[1])) };

  if (self->m_axialBoxWidget == callerWidget)
  {
    // axial coordinates refer to first and second 3D coordinates
    if ((pos1i[0] != min[0]) || (pos1i[1] != min[1]) || (pos2i[0] != max[0]) || (pos2i[1] != max[1]))
    {
      callerRepresentation = static_cast<BoxSelectionRepresentation2D*>(self->m_coronalBoxWidget->GetRepresentation());
      callerRepresentation->SetBoxCoordinates(pos1i[0], min[2], pos2i[0], max[2]);

      callerRepresentation = static_cast<BoxSelectionRepresentation2D*>(self->m_sagittalBoxWidget->GetRepresentation());
      callerRepresentation->SetBoxCoordinates(pos1i[1], min[2], pos2i[1], max[2]);

      self->m_min = Vector3ui(pos1i[0], pos1i[1], min[2]);
      self->m_max = Vector3ui(pos2i[0], pos2i[1], max[2]);
      self->m_selectedPoints.clear();
      self->m_selectedPoints.push_back(Vector3ui(pos1i[0], pos1i[1], min[2]));
      self->m_selectedPoints.push_back(Vector3ui(pos2i[0], pos2i[1], max[2]));
    }
  }

  if (self->m_coronalBoxWidget == callerWidget)
  {
    // coronal coordinates refer to first and third 3D coordinates
    if ((pos1i[0] != min[0]) || (pos1i[1] != min[2]) || (pos2i[0] != max[0]) || (pos2i[1] != max[2]))
    {
      callerRepresentation = static_cast<BoxSelectionRepresentation2D*>(self->m_axialBoxWidget->GetRepresentation());
      callerRepresentation->SetBoxCoordinates(pos1i[0], min[1], pos2i[0], max[1]);

      callerRepresentation = static_cast<BoxSelectionRepresentation2D*>(self->m_sagittalBoxWidget->GetRepresentation());
      callerRepresentation->SetBoxCoordinates(min[1], pos1i[1], max[1], pos2i[1]);

      self->m_min = Vector3ui(pos1i[0], min[1], pos1i[1]);
      self->m_max = Vector3ui(pos2i[0], max[1], pos2i[1]);
      self->m_selectedPoints.clear();
      self->m_selectedPoints.push_back(Vector3ui(pos1i[0], min[1], pos1i[1]));
      self->m_selectedPoints.push_back(Vector3ui(pos2i[0], max[1], pos2i[1]));
    }
  }

  if (self->m_sagittalBoxWidget == callerWidget)
  {
    // sagittal coordinates refer to second and third 3D coordinates
    if ((pos1i[0] != min[1]) || (pos1i[1] != min[2]) || (pos2i[0] != max[1]) || (pos2i[1] != max[2]))
    {
      callerRepresentation = static_cast<BoxSelectionRepresentation2D*>(self->m_axialBoxWidget->GetRepresentation());
      callerRepresentation->SetBoxCoordinates(min[0], pos1i[0], max[0], pos2i[0]);

      callerRepresentation = static_cast<BoxSelectionRepresentation2D*>(self->m_coronalBoxWidget->GetRepresentation());
      callerRepresentation->SetBoxCoordinates(min[0], pos1i[1], max[0], pos2i[1]);

      self->m_min = Vector3ui(min[0], pos1i[0], pos1i[1]);
      self->m_max = Vector3ui(max[0], pos2i[0], pos2i[1]);
      self->m_selectedPoints.clear();
      self->m_selectedPoints.push_back(Vector3ui(min[0], pos1i[0], pos1i[1]));
      self->m_selectedPoints.push_back(Vector3ui(max[0], pos2i[0], pos2i[1]));
    }
  }

  self->computeSelectionCube();

  double bounds[6] = { (static_cast<double>(self->m_min[0]) - 0.5) * self->m_spacing[0], (static_cast<double>(self->m_max[0]) + 0.5) * self->m_spacing[0],
                       (static_cast<double>(self->m_min[1]) - 0.5) * self->m_spacing[1], (static_cast<double>(self->m_max[1]) + 0.5) * self->m_spacing[1],
                       (static_cast<double>(self->m_min[2]) - 0.5) * self->m_spacing[2], (static_cast<double>(self->m_max[2]) + 0.5) * self->m_spacing[2] };
  self->m_boxRender->PlaceBox(bounds);

  // update renderers, but only update the 3D render when the widget interaction has finished to avoid performance problems
  self->m_axialBoxWidget->GetInteractor()->GetRenderWindow()->Render();
  self->m_coronalBoxWidget->GetInteractor()->GetRenderWindow()->Render();
  self->m_sagittalBoxWidget->GetInteractor()->GetRenderWindow()->Render();

  if (event == vtkCommand::EndInteractionEvent) self->m_renderer->GetRenderWindow()->Render();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Selection::computeLassoBounds(int *iBounds)
{
  auto rep = static_cast<ContourRepresentationGlyph *>(m_contourWidget->GetRepresentation());
  auto bounds = rep->GetBounds();

  // this copy is needed because we want to change the values and we mustn't do it in the original pointer or we will mess
  // with the rep
  double dBounds[6] = {bounds[0], bounds[1], bounds[2], bounds[3], bounds[4], bounds[5]};

  switch (m_contourWidget->GetOrientation())
  {
    case 0: // axial
      // first check if the contour is inside the slice
      if ((dBounds[1] < 0) || (dBounds[3] < 0) || (dBounds[0] > (m_size[0] * m_spacing[0])) || (dBounds[2] > (m_size[1] * m_spacing[1])))
      {
        m_selectionIsValid = false;
      }
      else
      {
        m_selectionIsValid = true;
      }

      dBounds[4] = dBounds[5] = m_min[2] * m_spacing[2];
      break;
    case 1: // coronal
      if ((dBounds[1] < 0) || (dBounds[3] < 0) || (dBounds[0] > (m_size[0] * m_spacing[0])) || (dBounds[2] > (m_size[2] * m_spacing[2])))
      {
        m_selectionIsValid = false;
      }
      else
      {
        m_selectionIsValid = true;
      }

      dBounds[4] = dBounds[2];
      dBounds[5] = dBounds[3];
      dBounds[2] = dBounds[3] = m_min[1] * m_spacing[1];
      break;
    case 2: // sagittal
      if ((dBounds[1] < 0) || (dBounds[3] < 0) || (dBounds[0] > (m_size[1] * m_spacing[1])) || (dBounds[2] > (m_size[2] * m_spacing[2])))
      {
        m_selectionIsValid = false;
      }
      else
      {
        m_selectionIsValid = true;
      }

      dBounds[4] = dBounds[2];
      dBounds[5] = dBounds[3];
      dBounds[2] = dBounds[0];
      dBounds[3] = dBounds[1];
      dBounds[0] = dBounds[1] = m_min[0] * m_spacing[0];
      break;
  }

  for (unsigned int i: {0,1,2})
  {
    unsigned int j = 2 * i;
    unsigned int k = j + 1;

    if (dBounds[j] < 0)
    {
      iBounds[j] = 0;
    }
    else
    {
      if (dBounds[j] > m_size[i] * m_spacing[i])
      {
        iBounds[j] = m_size[i];
      }
      else
      {
        iBounds[j] = floor(dBounds[j] / m_spacing[i]);

        if (fmod(dBounds[j], m_spacing[i]) > (0.5 * m_spacing[i]))
        {
          iBounds[j]++;
        }
      }
    }

    if (dBounds[k] < 0)
    {
      iBounds[k] = 0;
    }
    else
    {
      if (dBounds[k] > m_size[i] * m_spacing[i])
      {
        iBounds[k] = m_size[i];
      }
      else
      {
        iBounds[k] = floor(dBounds[k] / m_spacing[i]);

        if (fmod(dBounds[k], m_spacing[i]) > (0.5 * m_spacing[i]))
        {
          iBounds[k]++;
        }
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Selection::addContourInitialPoint(const Vector3ui &point, std::shared_ptr<SliceVisualization> callerSlice)
{
  // if there is an existing contour just return
  if (m_contourWidget != nullptr) return;

  m_selectionType = Type::CONTOUR;
  m_min = point;
  m_max = point;
  double worldPos[3] = { 0.0, 0.0, 0.0 };

  // interpolate points with lines
  auto interpolator = vtkLinearContourLineInterpolator::New();
  auto pointPlacer = FocalPlanePointPlacer::New();
  auto representation = ContourRepresentationGlyph::New();

  // create widget and representation
  m_contourWidget = vtkSmartPointer<ContourWidget>::New();
  m_contourWidget->SetInteractor(callerSlice->renderer()->GetRenderWindow()->GetInteractor());
  m_contourWidget->SetContinuousDraw(true);
  m_contourWidget->SetFollowCursor(true);

  switch (callerSlice->orientationType())
  {
    case SliceVisualization::Orientation::Axial:
      m_contourWidget->SetOrientation(0);
      pointPlacer->SetSpacing(m_spacing[0], m_spacing[1]);
      representation->SetSpacing(m_spacing[0], m_spacing[1]);
      worldPos[0] = point[0] * m_spacing[0];
      worldPos[1] = point[1] * m_spacing[1];
      break;
    case SliceVisualization::Orientation::Coronal:
      m_contourWidget->SetOrientation(1);
      pointPlacer->SetSpacing(m_spacing[0], m_spacing[2]);
      representation->SetSpacing(m_spacing[0], m_spacing[2]);
      worldPos[0] = point[0] * m_spacing[0];
      worldPos[1] = point[2] * m_spacing[2];
      break;
    case SliceVisualization::Orientation::Sagittal:
      m_contourWidget->SetOrientation(2);
      pointPlacer->SetSpacing(m_spacing[1], m_spacing[2]);
      representation->SetSpacing(m_spacing[1], m_spacing[2]);
      worldPos[0] = point[1] * m_spacing[1];
      worldPos[1] = point[2] * m_spacing[2];
      break;
    default:
      break;
  }

  pointPlacer->UpdateInternalState();

  representation->SetPointPlacer(pointPlacer);
  representation->SetLineInterpolator(interpolator);

  m_contourWidget->SetRepresentation(representation);
  m_contourWidget->SetEnabled(true);
  m_contourWidget->On();

  representation->SetVisibility(true);

  // set callbacks for widget interaction
  m_widgetsCallbackCommand = vtkSmartPointer<vtkCallbackCommand>::New();
  m_widgetsCallbackCommand->SetCallback(contourSelectionWidgetCallback);
  m_widgetsCallbackCommand->SetClientData(this);

  m_contourWidget->AddObserver(vtkCommand::StartInteractionEvent, m_widgetsCallbackCommand.Get());
  m_contourWidget->AddObserver(vtkCommand::EndInteractionEvent, m_widgetsCallbackCommand.Get());
  m_contourWidget->AddObserver(vtkCommand::InteractionEvent, m_widgetsCallbackCommand.Get());
  m_contourWidget->AddObserver(vtkCommand::KeyPressEvent, m_widgetsCallbackCommand.Get());

  // make the slice aware of a contour selection
  callerSlice->setSliceWidget(m_contourWidget);

  // create the volume that will represent user selected points. the spacing is 1 because we will not
  // use it's output directly, we will create our own volume instead with ComputeContourSelectionVolume()
  m_polyDataToStencil = vtkSmartPointer<vtkPolyDataToImageStencil>::New();
  m_polyDataToStencil->SetInputData(representation->GetContourRepresentationAsPolyData());
  m_polyDataToStencil->SetOutputOrigin(0, 0, 0);

  // change stencil properties according to slice orientation
  switch (callerSlice->orientationType())
  {
    case SliceVisualization::Orientation::Axial:
      m_polyDataToStencil->SetOutputSpacing(m_spacing[0], m_spacing[1], m_spacing[2]);
      m_polyDataToStencil->SetOutputWholeExtent(0, m_size[0], 0, m_size[1], 0, 0);
      break;
    case SliceVisualization::Orientation::Coronal:
      m_polyDataToStencil->SetOutputSpacing(m_spacing[0], m_spacing[2], m_spacing[1]);
      m_polyDataToStencil->SetOutputWholeExtent(0, m_size[0], 0, m_size[2], 0, 0);
      break;
    case SliceVisualization::Orientation::Sagittal:
      m_polyDataToStencil->SetOutputSpacing(m_spacing[1], m_spacing[2], m_spacing[0]);
      m_polyDataToStencil->SetOutputWholeExtent(0, m_size[1], 0, m_size[2], 0, 0);
      break;
    default:
      break;
  }
  m_polyDataToStencil->SetTolerance(0);

  m_stencilToImage = vtkSmartPointer<vtkImageStencilToImage>::New();
  m_stencilToImage->SetInputConnection(m_polyDataToStencil->GetOutputPort());
  m_stencilToImage->SetOutputScalarTypeToInt();
  m_stencilToImage->SetInsideValue(VOXEL_SELECTED);
  m_stencilToImage->SetOutsideValue(VOXEL_UNSELECTED);

  // bootstrap operations
  m_contourWidget->SelectAction(m_contourWidget);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Selection::contourSelectionWidgetCallback(vtkObject *caller, unsigned long event, void *clientdata, void *calldata)
{
  auto self = static_cast<Selection *>(clientdata);
  auto widget = static_cast<ContourWidget*>(caller);
  auto rep = static_cast<ContourRepresentationGlyph *>(widget->GetRepresentation());

  // if bounds are not defined or are invalid means that the rep is not valid
  auto bounds = rep->GetBounds();
  if (!bounds || (bounds[1] < bounds[0]) || (bounds[3] < bounds[2]) || (bounds[5] < bounds[4])) return;

  // calculate correct bounds for min/max vectors and for correctly cutting the selection volume
  int iBounds[6];
  self->computeLassoBounds(iBounds);

  self->m_min = Vector3ui(iBounds[0], iBounds[2], iBounds[4]);
  self->m_max = Vector3ui(iBounds[1], iBounds[3], iBounds[5]);

  auto repre = rep->GetContourRepresentationAsPolyData();
  if(!repre || repre->GetNumberOfPoints() < 3) return;

  if (vtkCommand::EndInteractionEvent == event) rep->PlaceFinalPoints();

  // update the representation volume (update the pipeline)
  self->m_polyDataToStencil->Modified();

  // adquire new rotated and clipped image
  self->computeContourSelectionVolume(iBounds);

  if (self->m_rotatedImage != nullptr)
  {
    self->deleteSelectionActors();
    self->deleteSelectionVolumes();
    self->m_axial->clearSelections();
    self->m_coronal->clearSelections();
    self->m_sagittal->clearSelections();

    self->m_selectionVolumesList.push_back(self->m_rotatedImage);
    self->m_axial->setSelectionVolume(self->m_rotatedImage, true);
    self->m_coronal->setSelectionVolume(self->m_rotatedImage, true);
    self->m_sagittal->setSelectionVolume(self->m_rotatedImage, true);

    // update renderers
    self->m_axial->renderer()->GetRenderWindow()->Render();
    self->m_coronal->renderer()->GetRenderWindow()->Render();
    self->m_sagittal->renderer()->GetRenderWindow()->Render();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Selection::computeContourSelectionVolume(const int *bounds)
{
  if((bounds[1] < bounds[0]) || (bounds[3] < bounds[2]) || (bounds[5] < bounds[4])) return;

  auto image = m_stencilToImage->GetOutput();
  image->Modified();

  // need to create a volume with the extent changed (1 voxel thicker on every axis) for the slices
  // to correctly hide/show the volume and the widgets
  int extent[6];
  image->GetExtent(extent);

  // do not operate on empty/uninitialized images
  if ((extent[1] < extent[0]) || (extent[3] < extent[2]) || extent[5] < extent[4]) return;

  if (m_rotatedImage)
  {
    m_rotatedImage = nullptr;
  }

  // create the volume and properties according to the slice orientation and use contour representation bounds to make it smaller.
  m_rotatedImage = vtkImageData::New();
  m_rotatedImage->SetSpacing(m_spacing[0], m_spacing[1], m_spacing[2]);
  m_rotatedImage->SetOrigin((static_cast<int>(bounds[0]) - 1) * m_spacing[0], (static_cast<int>(bounds[2]) - 1) * m_spacing[1], (static_cast<int>(bounds[4]) - 1) * m_spacing[2]);
  m_rotatedImage->SetDimensions(bounds[1] - bounds[0] + 3, bounds[3] - bounds[2] + 3, bounds[5] - bounds[4] + 3);
  m_rotatedImage->AllocateScalars(VTK_INT, 1);
  memset(m_rotatedImage->GetScalarPointer(), 0, m_rotatedImage->GetScalarSize() * (bounds[1] - bounds[0] + 3) * (bounds[3] - bounds[2] + 3) * (bounds[5] - bounds[4] + 3));

  if (m_selectionIsValid)
  {
    switch (m_contourWidget->GetOrientation())
    {
      case 0: // axial
        for (unsigned int i = bounds[0]; i <= bounds[1]; i++)
        {
          for (unsigned int j = bounds[2]; j <= bounds[3]; j++)
          {
            auto pixel = static_cast<int*>(image->GetScalarPointer(i, j, 0));
            auto rotatedPixel = static_cast<int*>(m_rotatedImage->GetScalarPointer(i - bounds[0] + 1, j - bounds[2] + 1, 1));
            *rotatedPixel = *pixel;
          }
        }
        break;
      case 1: // coronal
        for (unsigned int i = bounds[0]; i <= bounds[1]; i++)
        {
          for (unsigned int j = bounds[4]; j <= bounds[5]; j++)
          {
            auto pixel = static_cast<int*>(image->GetScalarPointer(i, j, 0));
            auto rotatedPixel = static_cast<int*>(m_rotatedImage->GetScalarPointer(i - bounds[0] + 1, 1, j - bounds[4] + 1));
            *rotatedPixel = *pixel;
          }
        }
        break;
      case 2: // sagittal
        for (unsigned int i = bounds[2]; i <= bounds[3]; i++)
        {
          for (unsigned int j = bounds[4]; j <= bounds[5]; j++)
          {
            auto pixel = static_cast<int*>(image->GetScalarPointer(i, j, 0));
            auto rotatedPixel = static_cast<int*>(m_rotatedImage->GetScalarPointer(1, i - bounds[2] + 1, j - bounds[4] + 1));
            *rotatedPixel = *pixel;
          }
        }
        break;
      default: // can't happen
        break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Selection::updateContourSlice(const Vector3ui &point)
{
  if (m_selectionType != Type::CONTOUR) return;

  if(m_rotatedImage)
  {
    double origin[3];
    m_rotatedImage->GetOrigin(origin);

    switch (m_contourWidget->GetOrientation())
    {
      case 0: // axial
        if (point[2] == m_min[2]) return;

        m_min[2] = m_max[2] = point[2];
        origin[2] = (static_cast<int>(point[2]) - 1) * m_spacing[2];
        break;
      case 1: // coronal
        if (point[1] == m_min[1]) return;

        m_min[1] = m_max[1] = point[1];
        origin[1] = (static_cast<int>(point[1]) - 1) * m_spacing[1];
        break;
      case 2: // sagittal
        if (point[0] == m_min[0]) return;

        m_min[0] = m_max[0] = point[0];
        origin[0] = (static_cast<int>(point[0]) - 1) * m_spacing[0];
        break;
      default:
        break;
    }

    m_changer = vtkSmartPointer<vtkImageChangeInformation>::New();
    m_changer->SetInputData(m_rotatedImage);
    m_changer->SetOutputOrigin(origin);
    m_changer->Update();

    deleteSelectionActors();
    deleteSelectionVolumes();
    m_axial->clearSelections();
    m_coronal->clearSelections();
    m_sagittal->clearSelections();

    m_selectionVolumesList.push_back(m_changer->GetOutput());
    m_axial->setSelectionVolume(m_changer->GetOutput(), true);
    m_coronal->setSelectionVolume(m_changer->GetOutput(), true);
    m_sagittal->setSelectionVolume(m_changer->GetOutput(), true);
  }
}
