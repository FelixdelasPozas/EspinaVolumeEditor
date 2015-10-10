///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: AxesRender.cpp
// Purpose: generate slice planes and crosshair for 3d render view
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// vtk includes
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkAxesActor.h>
#include <vtkRenderWindow.h>

// project includes
#include "AxesRender.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
AxesRender::AxesRender(vtkSmartPointer<vtkRenderer> renderer, const Coordinates &coords)
{
  m_spacing = coords.GetImageSpacing();
  auto size = coords.GetTransformedSize();

  for(auto i: {0,1,2})
  {
    m_max[i] = (size[i]-1) * m_spacing[i];
  }

  m_visible = true;

  GenerateSlicePlanes(renderer);
  GenerateVoxelCrosshair(renderer);
  CreateOrientationWidget(renderer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
AxesRender::~AxesRender()
{
  if(m_visible)
  {
    for(auto i: {0,1,2})
    {
      m_renderer->RemoveActor(m_planesActors[i]);
      m_renderer->RemoveActor(m_crossActors[i]);
    }
  }

  m_axesWidget->SetEnabled(false);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void AxesRender::Update(const Vector3ui &crosshair)
{
  UpdateVoxelCrosshair(crosshair);
  UpdateSlicePlanes(crosshair);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void AxesRender::GenerateVoxelCrosshair(vtkSmartPointer<vtkRenderer> renderer)
{
  for (unsigned int i: {0,1,2})
  {
    auto line = vtkSmartPointer<vtkLineSource>::New();
    line->SetResolution(1);
    line->Update();

    auto linemapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    linemapper->SetInputData(line->GetOutput());
    linemapper->SetResolveCoincidentTopologyToPolygonOffset();
    linemapper->Update();

    auto lineactor = vtkSmartPointer<vtkActor>::New();
    lineactor->SetMapper(linemapper);
    lineactor->GetProperty()->SetColor(1, 1, 1);
    lineactor->GetProperty()->SetLineStipplePattern(0x9999);
    lineactor->GetProperty()->SetLineStippleRepeatFactor(1);
    lineactor->GetProperty()->SetPointSize(1);
    lineactor->GetProperty()->SetLineWidth(2);

    renderer->AddActor(lineactor);
    m_crossActors.push_back(lineactor);
    m_POILines.push_back(line);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void AxesRender::UpdateVoxelCrosshair(const Vector3ui &crosshair)
{
  auto point3d = Vector3d(crosshair[0] * m_spacing[0], crosshair[1] * m_spacing[1], crosshair[2] * m_spacing[2]);
  auto iter = m_POILines.begin();

  (*iter)->SetPoint1(0, point3d[1], point3d[2]);
  (*iter)->SetPoint2(m_max[0], point3d[1], point3d[2]);
  (*iter)->Update();
  iter++;

  (*iter)->SetPoint1(point3d[0], 0, point3d[2]);
  (*iter)->SetPoint2(point3d[0], m_max[1], point3d[2]);
  (*iter)->Update();
  iter++;

  (*iter)->SetPoint1(point3d[0], point3d[1], 0);
  (*iter)->SetPoint2(point3d[0], point3d[1], m_max[2]);
  (*iter)->Update();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void AxesRender::GenerateSlicePlanes(vtkSmartPointer<vtkRenderer> renderer)
{
  for (unsigned int idx: {0,1,2})
  {
    auto plane = vtkSmartPointer<vtkPlaneSource>::New();
    plane->Update();

    auto planemapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    planemapper->SetInputConnection(0, plane->GetOutputPort(0));
    planemapper->Update();

    auto planeactor = vtkSmartPointer<vtkActor>::New();
    planeactor->GetProperty()->SetColor(0.5, 0.5, 0.5);
    planeactor->GetProperty()->SetSpecular(0);
    planeactor->GetProperty()->SetOpacity(0.3);
    planeactor->GetProperty()->ShadingOff();
    planeactor->GetProperty()->EdgeVisibilityOff();
    planeactor->GetProperty()->LightingOn();
    planeactor->SetMapper(planemapper);

    renderer->AddActor(planeactor);
    m_planesActors.push_back(planeactor);
    m_planes.push_back(plane);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void AxesRender::UpdateSlicePlanes(const Vector3ui &point)
{
  auto point3d = Vector3d(point[0] * m_spacing[0], point[1] * m_spacing[1], point[2] * m_spacing[2]);
  auto iter = m_planes.begin();

  (*iter)->SetOrigin(0, 0, point3d[2]);
  (*iter)->SetPoint1(m_max[0], 0, point3d[2]);
  (*iter)->SetPoint2(0, m_max[1], point3d[2]);
  (*iter)->SetNormal(0, 0, 1);
  (*iter)->Update();
  iter++;

  (*iter)->SetOrigin(0, point3d[1], 0);
  (*iter)->SetPoint1(m_max[0], point3d[1], 0);
  (*iter)->SetPoint2(0, point3d[1], m_max[2]);
  (*iter)->SetNormal(0, 1, 0);
  (*iter)->Update();
  iter++;

  (*iter)->SetOrigin(point3d[0], 0, 0);
  (*iter)->SetPoint1(point3d[0], m_max[1], 0);
  (*iter)->SetPoint2(point3d[0], 0, m_max[2]);
  (*iter)->SetNormal(1, 0, 0);
  (*iter)->Update();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void AxesRender::CreateOrientationWidget(vtkSmartPointer<vtkRenderer> renderer)
{
  auto axes = vtkSmartPointer<vtkAxesActor>::New();
  axes->DragableOff();
  axes->PickableOff();

  m_axesWidget = vtkSmartPointer<vtkOrientationMarkerWidget>::New();
  m_axesWidget->SetOrientationMarker(axes);
  m_axesWidget->SetInteractor(renderer->GetRenderWindow()->GetInteractor());
  m_axesWidget->SetViewport(0.0, 0.0, 0.3, 0.3);
  m_axesWidget->SetEnabled(true);
  m_axesWidget->InteractiveOff();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool AxesRender::isVisible() const
{
  return m_visible;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void AxesRender::setVisible(bool value)
{
  if (m_visible == value) return;

  m_visible = value;

  for(auto i: {0,1,2})
  {
    m_planesActors[i]->SetVisibility(value);
    m_crossActors[i]->SetVisibility(value);
  }
}
