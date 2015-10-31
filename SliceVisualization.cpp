///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: SliceVisualization.cpp
// Purpose: generate slices & crosshairs for axial, coronal and sagittal views. Also handles
//          pick function and selection of slice pixels.
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

// c++ includes
#include <sstream>

// vtk includes
#include <vtkImageReslice.h>
#include <vtkDataSetMapper.h>
#include <vtkProperty.h>
#include <vtkCellArray.h>
#include <vtkTextProperty.h>
#include <vtkCoordinate.h>
#include <vtkImageCanvasSource2D.h>
#include <vtkTexture.h>
#include <vtkPolyDataMapper.h>
#include <vtkImageItem.h>
#include <vtkRenderWindow.h>
#include <vtkLine.h>
#include <vtkIconGlyphFilter.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkImageCast.h>
#include <vtkImageDataGeometryFilter.h>
#include <vtkMath.h>
#include <vtkImageMapper3D.h>

// project includes
#include "SliceVisualization.h"
#include "Selection.h"
#include "BoxSelectionWidget.h"
#include "BoxSelectionRepresentation2D.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// SliceVisualization class
//
SliceVisualization::SliceVisualization(Orientation type)
: m_orientation{type}
, m_picker{nullptr}
, m_renderer{nullptr}
, m_thumbRenderer{nullptr}
, m_widget{nullptr}
, m_textActor{nullptr}
, m_axesMatrix{nullptr}
, m_horizontalCrosshair{nullptr}
, m_horizontalCrosshairActor{nullptr}
, m_verticalCrosshair{nullptr}
, m_verticalCrosshairActor{nullptr}
, m_focusData{nullptr}
, m_focusActor{nullptr}
, m_referenceReslice{nullptr}
, m_referenceImageMapper{nullptr}
, m_imageBlender{nullptr}
, m_segmentationReslice{nullptr}
, m_segmentationsMapper{nullptr}
, m_segmentationsActor{nullptr}
, m_selectionReslice{nullptr}
, m_caster{nullptr}
, m_geometryFilter{nullptr}
, m_iconFilter{nullptr}
, m_selectionMapper{nullptr}
, m_selectionActor{nullptr}
, m_segmentationOpacity{0.75}
, m_segmentationHidden{false}
{
  // create 2D actors texture
  auto texture = vtkSmartPointer<vtkImageCanvasSource2D>::New();
  texture->SetScalarTypeToUnsignedChar();
  texture->SetExtent(0, 23, 0, 7, 0, 0);
  texture->SetNumberOfScalarComponents(4);
  texture->SetDrawColor(0, 0, 0, 0);
  texture->FillBox(0, 23, 0, 7);
  texture->SetDrawColor(0, 0, 0, 100);
  texture->FillBox(16, 19, 0, 3);
  texture->FillBox(20, 24, 4, 7);
  texture->SetDrawColor(255, 255, 255, 100);
  texture->FillBox(16, 19, 4, 7);
  texture->FillBox(20, 24, 0, 3);

  // vtk texture representation
  m_texture = vtkSmartPointer<vtkTexture>::New();
  m_texture->SetInputConnection(texture->GetOutputPort());
  m_texture->SetInterpolate(false);
  m_texture->SetRepeat(false);
  m_texture->SetEdgeClamp(false);
  m_texture->SetBlendingMode(vtkTexture::VTK_TEXTURE_BLENDING_MODE_NONE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void SliceVisualization::initialize(vtkSmartPointer<vtkStructuredPoints> data,
                                    vtkSmartPointer<vtkLookupTable> colorTable,
                                    vtkSmartPointer<vtkRenderer> renderer,
                                    std::shared_ptr<Coordinates> coordinates)
{
  // get data properties
  m_size    = coordinates->GetTransformedSize();
  m_spacing = coordinates->GetImageSpacing();
  m_max     = Vector3d((m_size[0] - 1) * m_spacing[0], (m_size[1] - 1) * m_spacing[1], (m_size[2] - 1) * m_spacing[2]);

  m_axesMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  m_renderer   = renderer;

  // define reslice matrix
  auto point = (m_size / 2) - Vector3ui(1); // unsigned int, not double because we want the slice index
  double matrix[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
  switch (m_orientation)
  {
    case Orientation::Sagittal:
      matrix[3] = point[0] * m_spacing[0];
      matrix[2] = matrix[4] = matrix[9] = 1;
      break;
    case Orientation::Coronal:
      matrix[7] = point[1] * m_spacing[1], matrix[0] = matrix[6] = matrix[9] = 1;
      break;
    case Orientation::Axial:
      matrix[11] = point[2] * m_spacing[2];
      matrix[0] = matrix[5] = matrix[10] = 1;
      break;
    default:
      break;
  }
  m_axesMatrix->DeepCopy(matrix);

  // generate all actors
  generateSlice(data, colorTable);
  generateCrosshair();

  // create text legend
  m_textActor = vtkSmartPointer<vtkTextActor>::New();

  auto textbuffer = std::string("None");
  m_textActor->SetInput(textbuffer.c_str());
  m_textActor->SetTextScaleMode(vtkTextActor::TEXT_SCALE_MODE_NONE);

  auto textcoord = m_textActor->GetPositionCoordinate();
  textcoord->SetCoordinateSystemToNormalizedViewport();
  textcoord->SetValue(0.02, 0.02);

  auto textproperty = m_textActor->GetTextProperty();
  textproperty->SetColor(1, 1, 1);
  textproperty->SetFontFamilyToArial();
  textproperty->SetFontSize(11);
  textproperty->BoldOff();
  textproperty->ItalicOff();
  textproperty->ShadowOff();
  textproperty->SetJustificationToLeft();
  textproperty->SetVerticalJustificationToBottom();
  m_textActor->Modified();
  m_textActor->PickableOff();
  m_renderer->AddViewProp(m_textActor);

  // we set point out of range to force an update of the first call as all components will be
  // different from the update point
  m_point = m_size + Vector3ui(1);

  generateThumbnail();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
SliceVisualization::~SliceVisualization()
{
  clearSelections();

  if (m_widget)
  {
    m_widget = nullptr;
  }

  // remove thumb renderer
  if (m_thumbRenderer)
  {
    m_thumbRenderer->RemoveAllViewProps();
  }

  if (m_renderer)
  {
    m_renderer->GetRenderWindow()->RemoveRenderer(m_thumbRenderer);
    m_renderer->RemoveAllViewProps();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void SliceVisualization::generateSlice(vtkSmartPointer<vtkStructuredPoints> data, vtkSmartPointer<vtkLookupTable> colorTable)
{
  m_segmentationReslice = vtkSmartPointer<vtkImageReslice>::New();
  m_segmentationReslice->SetOptimization(true);
  m_segmentationReslice->BorderOn();
  m_segmentationReslice->SetInputData(data);
  m_segmentationReslice->SetOutputDimensionality(2);
  m_segmentationReslice->SetResliceAxes(m_axesMatrix);
  m_segmentationReslice->Update();

  m_segmentationsMapper = vtkSmartPointer<vtkImageMapToColors>::New();
  m_segmentationsMapper->SetLookupTable(colorTable);
  m_segmentationsMapper->SetOutputFormatToRGBA();
  m_segmentationsMapper->SetInputConnection(m_segmentationReslice->GetOutputPort());
  m_segmentationsMapper->Update();

  m_segmentationsActor = vtkSmartPointer<vtkImageActor>::New();
  m_segmentationsActor->SetInputData(m_segmentationsMapper->GetOutput());
  m_segmentationsActor->SetInterpolate(false);
  m_segmentationsActor->PickableOn();
  m_segmentationsActor->Update();

  m_picker = vtkSmartPointer<vtkPropPicker>::New();
  m_picker->PickFromListOn();
  m_picker->InitializePickList();
  m_picker->AddPickList(m_segmentationsActor);

  m_renderer->AddActor(m_segmentationsActor);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void SliceVisualization::generateCrosshair()
{
  QColor horizontalColor, verticalColor;
  switch(m_orientation)
  {
    case Orientation::Sagittal:
      horizontalColor = Qt::blue;
      verticalColor   = Qt::green;
      break;
    case Orientation::Coronal:
      horizontalColor = Qt::blue;
      verticalColor   = Qt::red;
      break;
    case Orientation::Axial:
      horizontalColor = Qt::green;
      verticalColor   = Qt::red;
      break;
    default:
      Q_ASSERT(false);
      break;
  }

  auto verticaldata   = vtkSmartPointer<vtkPolyData>::New();
  auto verticalmapper = vtkSmartPointer<vtkDataSetMapper>::New();
  verticalmapper->SetInputData(verticaldata);
  m_verticalCrosshairActor = vtkSmartPointer<vtkActor>::New();
  m_verticalCrosshairActor->SetMapper(verticalmapper);
  m_verticalCrosshairActor->GetProperty()->SetColor(verticalColor.redF(), verticalColor.greenF(), verticalColor.blueF());
  m_verticalCrosshairActor->GetProperty()->SetLineStipplePattern(0xF0F0);
  m_verticalCrosshairActor->GetProperty()->SetLineStippleRepeatFactor(1);
  m_verticalCrosshairActor->GetProperty()->SetPointSize(1);
  m_verticalCrosshairActor->GetProperty()->SetLineWidth(2);
  m_verticalCrosshairActor->SetPickable(false);

  auto horizontaldata = vtkSmartPointer<vtkPolyData>::New();
  auto horizontalmapper = vtkSmartPointer<vtkDataSetMapper>::New();
  horizontalmapper->SetInputData(horizontaldata);
  m_horizontalCrosshairActor = vtkSmartPointer<vtkActor>::New();
  m_horizontalCrosshairActor->SetMapper(horizontalmapper);
  m_horizontalCrosshairActor->GetProperty()->SetColor(horizontalColor.redF(), horizontalColor.greenF(), horizontalColor.blueF());
  m_horizontalCrosshairActor->GetProperty()->SetLineStipplePattern(0xF0F0);
  m_horizontalCrosshairActor->GetProperty()->SetLineStippleRepeatFactor(1);
  m_horizontalCrosshairActor->GetProperty()->SetPointSize(1);
  m_horizontalCrosshairActor->GetProperty()->SetLineWidth(2);
  m_horizontalCrosshairActor->SetPickable(false);

  m_renderer->AddActor(m_verticalCrosshairActor);
  m_renderer->AddActor(m_horizontalCrosshairActor);

  m_horizontalCrosshair = horizontaldata;
  m_verticalCrosshair = verticaldata;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void SliceVisualization::update(const Vector3ui &point)
{
  if (m_point == point) return;

  updateSlice(point);
  updateCrosshair(point);

  m_thumbRenderer->Render();
  m_renderer->Render();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void SliceVisualization::updateSlice(const Vector3ui &point)
{
  std::stringstream out;
  auto textbuffer = std::string("Slice ");

  // change slice by changing reslice axes
  auto index = static_cast<int>(m_orientation);

  m_point[index] = point[index];
  out << m_point[index] + 1 << " of " << m_size[index];

  auto slice_point = m_point[index] * m_spacing[index];
  m_axesMatrix->SetElement(index, 3, slice_point);
  m_axesMatrix->Modified();

  for(auto actor: m_actorList)
  {
    updateActorVisibility(actor);
  }

  textbuffer += out.str();
  m_textActor->SetInput(textbuffer.c_str());
  m_textActor->Modified();

  updateActors();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void SliceVisualization::updateActors()
{
  m_segmentationReslice->Update();
  m_segmentationsMapper->Update();

  if(m_referenceImageMapper)
  {
    m_referenceReslice->Update();
    m_referenceImageMapper->Update();
    m_imageBlender->Update();
  }
  m_segmentationsActor->Update();

  if(m_selectionMapper)
  {
    m_selectionReslice->Update();
    m_caster->Update();
    m_geometryFilter->Update();
    m_iconFilter->Update();
    m_selectionMapper->Update();
    m_selectionActor->Modified();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void SliceVisualization::updateCrosshair(const Vector3ui &point)
{
  auto verticalpoints = vtkSmartPointer<vtkPoints>::New();
  auto horizontalpoints = vtkSmartPointer<vtkPoints>::New();

  auto point3d = Vector3d{point[0] * m_spacing[0], point[1] * m_spacing[1], point[2] * m_spacing[2]};

  switch (m_orientation)
  {
    case Orientation::Sagittal:
      if ((m_point[1] == point[1]) && (m_point[2] == point[2])) return;
      horizontalpoints->InsertNextPoint(0, point3d[2], 0);
      horizontalpoints->InsertNextPoint(m_max[1], point3d[2], 0);
      verticalpoints->InsertNextPoint(point3d[1], 0, 0);
      verticalpoints->InsertNextPoint(point3d[1], m_max[2], 0);
      m_point[1] = point[1];
      m_point[2] = point[2];
      break;
    case Orientation::Coronal:
      if ((m_point[0] == point[0]) && (m_point[2] == point[2])) return;
      horizontalpoints->InsertNextPoint(0, point3d[2], 0);
      horizontalpoints->InsertNextPoint(m_max[0], point3d[2], 0);
      verticalpoints->InsertNextPoint(point3d[0], 0, 0);
      verticalpoints->InsertNextPoint(point3d[0], m_max[2], 0);
      m_point[0] = point[0];
      m_point[2] = point[2];
      break;
    case Orientation::Axial:
      if ((m_point[0] == point[0]) && (m_point[1] == point[1])) return;
      horizontalpoints->InsertNextPoint(0, point3d[1], 0);
      horizontalpoints->InsertNextPoint(m_max[0], point3d[1], 0);
      verticalpoints->InsertNextPoint(point3d[0], 0, 0);
      verticalpoints->InsertNextPoint(point3d[0], m_max[1], 0);
      m_point[0] = point[0];
      m_point[1] = point[1];
      break;
    default:
      break;
  }

  auto verticalline = vtkSmartPointer<vtkCellArray>::New();
  verticalline->InsertNextCell(2);
  verticalline->InsertCellPoint(0);
  verticalline->InsertCellPoint(1);

  auto horizontalline = vtkSmartPointer<vtkCellArray>::New();
  horizontalline->InsertNextCell(2);
  horizontalline->InsertCellPoint(0);
  horizontalline->InsertCellPoint(1);

  m_horizontalCrosshair->Reset();
  m_horizontalCrosshair->SetPoints(horizontalpoints);
  m_horizontalCrosshair->SetLines(horizontalline);
  m_verticalCrosshair->Reset();
  m_verticalCrosshair->SetPoints(verticalpoints);
  m_verticalCrosshair->SetLines(verticalline);

  m_horizontalCrosshair->Modified();
  m_verticalCrosshair->Modified();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
SliceVisualization::PickType SliceVisualization::pickData(int* X, int* Y) const
{
  PickType pickedProp{PickType::None};

  // thumbnail must be onscreen to be really picked
  m_picker->Pick(*X, *Y, 0.0, m_thumbRenderer);
  if ((m_picker->GetViewProp()) && (true == m_thumbRenderer->GetDraw()))
  {
    pickedProp = PickType::Thumbnail;
  }
  else
  {
    // nope, did the user pick the slice?
    m_picker->Pick(*X, *Y, 0.0, m_renderer);
    if (m_picker->GetViewProp())
    {
      pickedProp = PickType::Slice;
    }
    else
    {
      return pickedProp;
    }
  }

  // or the thumbnail or the slice have been picked
  double point[3];
  m_picker->GetPickPosition(point);

  // picked values passed by reference, not elegant, we first use round to get the nearest point
  switch (m_orientation)
  {
    case Orientation::Axial:
      *X = vtkMath::Round(point[0] / m_spacing[0]);
      *Y = vtkMath::Round(point[1] / m_spacing[1]);
      break;
    case Orientation::Coronal:
      *X = vtkMath::Round(point[0] / m_spacing[0]);
      *Y = vtkMath::Round(point[1] / m_spacing[2]);
      break;
    case Orientation::Sagittal:
      *X = vtkMath::Round(point[0] / m_spacing[1]);
      *Y = vtkMath::Round(point[1] / m_spacing[2]);
      break;
    default:
      break;
  }

  return pickedProp;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void SliceVisualization::clearSelections()
{
  while (!m_actorList.empty())
  {
    auto actorInformation = m_actorList.back();
    m_renderer->RemoveActor(actorInformation->actor);
    actorInformation->actor = nullptr;
    m_actorList.pop_back();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void SliceVisualization::generateThumbnail()
{
  // get image bounds for thumbnail activation, we're only interested in bounds[1] & bounds[3]
  // as we know already the values of the others
  double bounds[6];
  m_segmentationsActor->GetBounds(bounds);
  auto boundsX = bounds[1];
  auto boundsY = bounds[3];

  // create thumbnail renderer
  m_thumbRenderer = vtkSmartPointer<vtkRenderer>::New();
  m_thumbRenderer->AddActor(m_segmentationsActor);
  m_thumbRenderer->ResetCamera();
  m_thumbRenderer->SetInteractive(false);

  // coordinates are normalized display coords (range 0-1 in double)
  m_thumbRenderer->SetViewport(0.65, 0.0, 1.0, 0.35);
  m_renderer->GetRenderWindow()->AddRenderer(m_thumbRenderer);

  m_renderer->GetRenderWindow()->AlphaBitPlanesOn();
  m_renderer->GetRenderWindow()->SetDoubleBuffer(true);
  m_renderer->GetRenderWindow()->SetNumberOfLayers(2);
  m_renderer->SetLayer(0);
  m_thumbRenderer->SetLayer(1);
  m_thumbRenderer->DrawOff();

  // create the slice border points
  double p0[3] =  { 0.0, 0.0, 0.0 };
  double p1[3] =  { boundsX, 0.0, 0.0 };
  double p2[3] =  { boundsX, boundsY, 0.0 };
  double p3[3] =  { 0.0, boundsY, 0.0 };

  auto points = vtkSmartPointer<vtkPoints>::New();
  points->InsertNextPoint(p0);
  points->InsertNextPoint(p1);
  points->InsertNextPoint(p2);
  points->InsertNextPoint(p3);
  points->InsertNextPoint(p0);

  auto lines = vtkSmartPointer<vtkCellArray>::New();
  for (unsigned int i = 0; i < 4; i++)
  {
    auto line = vtkSmartPointer<vtkLine>::New();
    line->GetPointIds()->SetId(0, i);
    line->GetPointIds()->SetId(1, i + 1);
    lines->InsertNextCell(line);
  }

  auto sliceborder = vtkSmartPointer<vtkPolyData>::New();
  sliceborder->SetPoints(points);
  sliceborder->SetLines(lines);
  sliceborder->Modified();

  auto slicemapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  slicemapper->SetInputData(sliceborder);
  slicemapper->Update();

  auto square = vtkSmartPointer<vtkActor>::New();
  square->SetMapper(slicemapper);
  square->GetProperty()->SetColor(1, 1, 1);
  square->GetProperty()->SetPointSize(0);
  square->GetProperty()->SetLineWidth(2);
  square->SetPickable(false);

  m_thumbRenderer->AddActor(square);

  // Create a polydata to store the selection box
  m_focusData = vtkSmartPointer<vtkPolyData>::New();

  auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  mapper->SetInputData(m_focusData);

  m_focusActor = vtkSmartPointer<vtkActor>::New();
  m_focusActor->SetMapper(mapper);
  m_focusActor->GetProperty()->SetColor(1, 1, 1);
  m_focusActor->GetProperty()->SetPointSize(1);
  m_focusActor->GetProperty()->SetLineWidth(2);
  m_focusActor->SetPickable(false);

  m_thumbRenderer->AddActor(m_focusActor);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void SliceVisualization::zoomEvent()
{
  double xy[3];
  double *value;
  double lowerleft[2];
  double upperright[2];

  auto coords = vtkSmartPointer<vtkCoordinate>::New();
  coords->SetViewport(m_renderer);
  coords->SetCoordinateSystemToNormalizedViewport();

  xy[0] = 0.0;
  xy[1] = 0.0;
  xy[2] = 0.0;
  coords->SetValue(xy);
  value = coords->GetComputedWorldValue(m_renderer);
  lowerleft[0] = value[0];
  lowerleft[1] = value[1];

  xy[0] = 1.0;
  xy[1] = 1.0;
  xy[2] = 0.0;
  coords->SetValue(xy);
  value = coords->GetComputedWorldValue(m_renderer);
  upperright[0] = value[0];
  upperright[1] = value[1];

  // is the slice completely inside the viewport?
  double bounds[6];
  m_segmentationsActor->GetBounds(bounds);
  auto boundsX = bounds[1];
  auto boundsY = bounds[3];

  if ((0.0 >= lowerleft[0]) && (0.0 >= lowerleft[1]) && (boundsX <= upperright[0]) && (boundsY <= upperright[1]))
  {
    m_thumbRenderer->DrawOff();
    m_thumbRenderer->GetRenderWindow()->Render();
  }
  else
  {
    double p0[3] = { lowerleft[0], lowerleft[1], 0.0 };
    double p1[3] = { lowerleft[0], upperright[1], 0.0 };
    double p2[3] = { upperright[0], upperright[1], 0.0 };
    double p3[3] = { upperright[0], lowerleft[1], 0.0 };

    // Create a vtkPoints object and store the points in it
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->InsertNextPoint(p0);
    points->InsertNextPoint(p1);
    points->InsertNextPoint(p2);
    points->InsertNextPoint(p3);
    points->InsertNextPoint(p0);

    // Create a cell array to store the lines in and add the lines to it
    auto lines = vtkSmartPointer<vtkCellArray>::New();

    for (unsigned int i = 0; i < 4; i++)
    {
      auto line = vtkSmartPointer<vtkLine>::New();
      line->GetPointIds()->SetId(0, i);
      line->GetPointIds()->SetId(1, i + 1);
      lines->InsertNextCell(line);
    }

    // Add the points & lines to the dataset
    m_focusData->Reset();
    m_focusData->SetPoints(points);
    m_focusData->SetLines(lines);
    m_focusData->Modified();

    m_thumbRenderer->DrawOn();
    m_thumbRenderer->GetRenderWindow()->Render();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void SliceVisualization::setReferenceImage(vtkSmartPointer<vtkStructuredPoints> data)
{
  m_referenceReslice = vtkSmartPointer<vtkImageReslice>::New();
  m_referenceReslice->SetOptimization(true);
  m_referenceReslice->BorderOn();
  m_referenceReslice->SetInputData(data);
  m_referenceReslice->SetOutputDimensionality(2);
  m_referenceReslice->SetResliceAxes(m_axesMatrix);
  m_referenceReslice->Update();

  // the image is grayscale, so only uses 256 colors
  auto colorTable = vtkSmartPointer<vtkLookupTable>::New();
  colorTable->SetTableRange(0, 255);
  colorTable->SetValueRange(0.0, 1.0);
  colorTable->SetSaturationRange(0.0, 0.0);
  colorTable->SetHueRange(0.0, 0.0);
  colorTable->SetAlphaRange(1.0, 1.0);
  colorTable->SetNumberOfColors(256);
  colorTable->Build();

  m_referenceImageMapper = vtkSmartPointer<vtkImageMapToColors>::New();
  m_referenceImageMapper->SetInputData(m_referenceReslice->GetOutput());
  m_referenceImageMapper->SetLookupTable(colorTable);
  m_referenceImageMapper->SetOutputFormatToRGBA();
  m_referenceImageMapper->SetUpdateExtentToWholeExtent();
  m_referenceImageMapper->Update();

	// remove actual actor and add the blended actor with both the segmentation and the stack
	m_imageBlender = vtkSmartPointer<vtkImageBlend>::New();
	m_imageBlender->SetInputConnection(0, m_referenceImageMapper->GetOutputPort());
	m_imageBlender->AddInputConnection(0, m_segmentationsMapper->GetOutputPort());
	m_imageBlender->SetOpacity(1, m_segmentationOpacity);
	m_imageBlender->SetBlendModeToNormal();
	m_imageBlender->SetNumberOfThreads(1);
	m_imageBlender->SetUpdateExtentToWholeExtent();
	m_imageBlender->Update();

  m_segmentationsActor->SetInputData(m_imageBlender->GetOutput());
  m_segmentationsActor->PickableOn();
  m_segmentationsActor->SetInterpolate(false);
  m_segmentationsActor->Update();

  // this stupid action allows the selection actors to be seen, if we don't do this actors are occluded by
  // the blended one
  for(auto actor: m_actorList)
  {
    updateActorVisibility(actor);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
double SliceVisualization::segmentationOpacity() const
{
  return m_segmentationOpacity;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void SliceVisualization::setSegmentationOpacity(const double opacity)
{
  m_segmentationOpacity = opacity;

  if (m_segmentationHidden) return;

  if (m_imageBlender)
  {
    m_imageBlender->SetOpacity(1, opacity);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void SliceVisualization::toggleSegmentationView(void)
{
  double opacity = 0.0;

  if (m_segmentationHidden)
  {
    m_segmentationHidden = false;
    opacity = m_segmentationOpacity;
  }
  else
  {
    m_segmentationHidden = true;
  }

  if (m_imageBlender)
  {
    m_imageBlender->SetOpacity(1, opacity);
  }

  for(auto actor: m_actorList)
  {
    updateActorVisibility(actor);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void SliceVisualization::updateActorVisibility(std::shared_ptr<struct ActorData> actorInformation)
{
  if (m_segmentationHidden == true)
  {
    actorInformation->actor->SetVisibility(false);

    if (m_widget)
    {
      m_widget->GetRepresentation()->SetVisibility(false);
      m_widget->SetEnabled(false);
    }
  }
  else
  {
    auto slice = m_point[static_cast<int>(m_orientation)];
    auto minSlice = actorInformation->minSlice;
    auto maxSlice = actorInformation->maxSlice;

    auto enabled = (minSlice <= slice) && (maxSlice >= slice);
    actorInformation->actor->SetVisibility(enabled);

    if (m_widget)
    {
      // correct the fact that selection volumes has a minSlice-1 and maxSlice+1 for correct marching cubes
      minSlice++;
      maxSlice--;

      enabled = (minSlice <= slice) && (maxSlice >= slice);
      m_widget->GetRepresentation()->SetVisibility(enabled);
      m_widget->SetEnabled(enabled);
    }
  }
  actorInformation->actor->GetMapper()->Update();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void SliceVisualization::setSelectionVolume(const vtkSmartPointer<vtkImageData> selectionBuffer, bool useActorBounds)
{
  m_selectionReslice = vtkSmartPointer<vtkImageReslice>::New();
  m_selectionReslice->SetOptimization(true);
  m_selectionReslice->BorderOn();
  m_selectionReslice->SetInputData(selectionBuffer);
  m_selectionReslice->SetOutputDimensionality(2);
  m_selectionReslice->SetResliceAxes(m_axesMatrix);
  m_selectionReslice->Update();

  // we need integer indexes, must cast first
  m_caster = vtkSmartPointer<vtkImageCast>::New();
  m_caster->SetInputData(m_selectionReslice->GetOutput());
  m_caster->SetOutputScalarTypeToInt();
  m_caster->Update();

  // transform the vtkImageData to vtkPolyData
  m_geometryFilter = vtkSmartPointer<vtkImageDataGeometryFilter>::New();
  m_geometryFilter->SetInputData(m_caster->GetOutput());
  m_geometryFilter->SetGlobalWarningDisplay(false);
  m_geometryFilter->SetThresholdCells(true);
  m_geometryFilter->SetThresholdValue(Selection::SELECTION_UNUSED_VALUE);
  m_geometryFilter->Update();

  // create filter to apply the same texture to every point in the set
  m_iconFilter = vtkSmartPointer<vtkIconGlyphFilter>::New();
  m_iconFilter->SetInputData(m_geometryFilter->GetOutput());
  m_iconFilter->SetIconSize(8, 8);
  m_iconFilter->SetUseIconSize(false);
  // if spacing < 1, we're fucked, literally
  switch (m_orientation)
  {
    case Orientation::Axial:
      m_iconFilter->SetDisplaySize(static_cast<int>(m_spacing[0]), static_cast<int>(m_spacing[1]));
      break;
    case Orientation::Coronal:
      m_iconFilter->SetDisplaySize(static_cast<int>(m_spacing[0]), static_cast<int>(m_spacing[2]));
      break;
    case Orientation::Sagittal:
      m_iconFilter->SetDisplaySize(static_cast<int>(m_spacing[1]), static_cast<int>(m_spacing[2]));
      break;
    default:
      Q_ASSERT(false);
      break;
  }
  m_iconFilter->SetIconSheetSize(24, 8);
  m_iconFilter->SetGravityToCenterCenter();

  m_selectionMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  m_selectionMapper->SetInputConnection(m_iconFilter->GetOutputPort());

  m_selectionActor = vtkSmartPointer<vtkActor>::New();
  m_selectionActor->SetMapper(m_selectionMapper);
  m_selectionActor->SetTexture(m_texture);
  m_selectionActor->SetDragable(false);
  m_selectionActor->UseBoundsOff();

  m_renderer->AddActor(m_selectionActor);

  double bounds[6];
  selectionBuffer->GetBounds(bounds);

  auto actorInformation = std::make_shared<struct ActorData>();
  actorInformation->actor = m_selectionActor;

  switch (m_orientation)
  {
    case Orientation::Sagittal:
      if (useActorBounds)
      {
        actorInformation->minSlice = static_cast<int>(bounds[0] / m_spacing[0]);
        actorInformation->maxSlice = static_cast<int>(bounds[1] / m_spacing[0]);
      }
      else
      {
        actorInformation->minSlice = 0;
        actorInformation->maxSlice = m_size[0];
      }
      break;
    case Orientation::Coronal:
      if (useActorBounds)
      {
        actorInformation->minSlice = static_cast<int>(bounds[2] / m_spacing[1]);
        actorInformation->maxSlice = static_cast<int>(bounds[3] / m_spacing[1]);
      }
      else
      {
        actorInformation->minSlice = 0;
        actorInformation->maxSlice = m_size[1];
      }
      break;
    case Orientation::Axial:
      if (useActorBounds)
      {
        actorInformation->minSlice = static_cast<int>(bounds[4] / m_spacing[2]);
        actorInformation->maxSlice = static_cast<int>(bounds[5] / m_spacing[2]);
      }
      else
      {
        actorInformation->minSlice = 0;
        actorInformation->maxSlice = m_size[2];
      }
      break;
    default:
      break;
  }

  m_actorList.push_back(actorInformation);
  updateActorVisibility(actorInformation);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const SliceVisualization::Orientation SliceVisualization::orientationType() const
{
  return m_orientation;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
vtkSmartPointer<vtkRenderer> SliceVisualization::renderer() const
{
  return m_renderer;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void SliceVisualization::setSliceWidget(vtkSmartPointer<vtkAbstractWidget> widget)
{
  m_widget = widget;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
vtkSmartPointer<vtkImageActor> SliceVisualization::actor() const
{
  return m_segmentationsActor;
}
