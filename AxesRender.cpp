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
#include <vtkRenderWindow.h>

// project includes
#include "AxesRender.h"

// Qt
#include <QList>
#include <QColor>

///////////////////////////////////////////////////////////////////////////////////////////////////
AxesRender::AxesRender(vtkSmartPointer<vtkRenderer> renderer, std::shared_ptr<Coordinates> coords)
: m_crosshair{-1,-1,-1}
{
  m_spacing = coords->GetImageSpacing();
  auto size = coords->GetTransformedSize();

  for(auto i: {0,1,2})
  {
    m_max[i] = (size[i]-1) * m_spacing[i];
  }

  m_visible = true;

  GenerateSlicePlanes(renderer);
  GenerateVoxelCrosshair(renderer);
  onCrosshairChange(Vector3ui{0,0,0});
}

///////////////////////////////////////////////////////////////////////////////////////////////////
AxesRender::~AxesRender()
{
  if(m_visible && m_renderer)
  {
    for(auto i: {0,1,2})
    {
      m_renderer->RemoveActor(m_planesActors[i]);
      m_renderer->RemoveActor(m_crossActors[i]);
    }

    m_crossActors.clear();
    m_planesActors.clear();
    m_POILines.clear();
    m_planes.clear();
    m_renderer = nullptr;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void AxesRender::onCrosshairChange(const Vector3ui &crosshair)
{
  if(m_crosshair != crosshair)
  {
    if(m_visible)
    {
      UpdateVoxelCrosshair(crosshair);
      UpdateSlicePlanes(crosshair);
    }

    m_crosshair = crosshair;
  }
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
  QList<QColor> colors = { Qt::blue, Qt::green, Qt::red };
  for (unsigned int idx: {0,1,2})
  {
    auto plane = vtkSmartPointer<vtkPlaneSource>::New();
    plane->Update();

    auto planemapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    planemapper->SetInputConnection(0, plane->GetOutputPort(0));
    planemapper->Update();

    auto color = colors[idx];

    auto planeactor = vtkSmartPointer<vtkActor>::New();
    planeactor->GetProperty()->SetColor(color.redF(), color.greenF(), color.blueF());
    planeactor->GetProperty()->SetSpecular(0);
    planeactor->GetProperty()->SetOpacity(0.25);
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
bool AxesRender::isVisible() const
{
  return m_visible;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void AxesRender::setVisible(bool value)
{
  if (m_visible != value)
  {
    m_visible = value;

    UpdateSlicePlanes(m_crosshair);
    UpdateVoxelCrosshair(m_crosshair);

    for(auto i: {0,1,2})
    {
      m_planesActors[i]->SetVisibility(value);
      m_crossActors[i]->SetVisibility(value);
    }
  }
}
