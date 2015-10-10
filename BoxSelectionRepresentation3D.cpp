///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: BoxSelectionRepresentation3D.cpp
// Purpose: used as representation for the box selection volume in the render view
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// project includes
#include "BoxSelectionRepresentation3D.h"

// vtk includes
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkCellArray.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
// BoxSelectionRepresentation3D class
//
BoxSelectionRepresentation3D::BoxSelectionRepresentation3D()
{
  // Construct initial points
  this->m_points = vtkPoints::New(VTK_DOUBLE);
  this->m_points->SetNumberOfPoints(8);	// 8 corners

  // Create the outline
  auto cells = vtkCellArray::New();
  this->m_polyData = vtkPolyData::New();
  this->m_polyData->SetPoints(this->m_points);

  this->m_mapper = vtkPolyDataMapper::New();
  this->m_mapper->SetInputData(this->m_polyData);

  this->m_property = vtkProperty::New();
  this->m_property->SetAmbient(1.0);
  this->m_property->SetAmbientColor(1.0, 1.0, 1.0);
  this->m_property->SetLineWidth(3.0);

  this->m_actor = vtkActor::New();
  this->m_actor->SetMapper(this->m_mapper);
  this->m_actor->SetProperty(this->m_property);

  cells->Allocate(cells->EstimateSize(12, 2));
  this->m_polyData->SetLines(cells);
  cells->Delete();

  // Create the outline
  this->GenerateOutline();

  // Define the point coordinates
  double bounds[6];
  bounds[0] = -0.5;
  bounds[1] = 0.5;
  bounds[2] = -0.5;
  bounds[3] = 0.5;
  bounds[4] = -0.5;
  bounds[5] = 0.5;
  this->PlaceBox(bounds);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
BoxSelectionRepresentation3D::~BoxSelectionRepresentation3D()
{
  this->m_renderer->RemoveActor(this->m_actor);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionRepresentation3D::SetRenderer(vtkRenderer *renderer)
{
  this->m_renderer = renderer;
  this->m_renderer->AddActor(this->m_actor);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionRepresentation3D::PlaceBox(const double *bounds)
{
  this->m_points->SetPoint(0, bounds[0], bounds[2], bounds[4]);
  this->m_points->SetPoint(1, bounds[1], bounds[2], bounds[4]);
  this->m_points->SetPoint(2, bounds[1], bounds[3], bounds[4]);
  this->m_points->SetPoint(3, bounds[0], bounds[3], bounds[4]);
  this->m_points->SetPoint(4, bounds[0], bounds[2], bounds[5]);
  this->m_points->SetPoint(5, bounds[1], bounds[2], bounds[5]);
  this->m_points->SetPoint(6, bounds[1], bounds[3], bounds[5]);
  this->m_points->SetPoint(7, bounds[0], bounds[3], bounds[5]);
  this->m_points->Modified();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionRepresentation3D::GenerateOutline()
{
  // Whatever the case may be, we have to reset the Lines of the
  // OutlinePolyData (i.e. nuke all current line data)
  auto cells = this->m_polyData->GetLines();
  cells->Reset();

  vtkIdType pts[2];

  pts[0] = 0;
  pts[1] = 1;
  cells->InsertNextCell(2, pts);
  pts[0] = 0;
  pts[1] = 3;
  cells->InsertNextCell(2, pts);
  pts[0] = 0;
  pts[1] = 4;
  cells->InsertNextCell(2, pts);

  pts[0] = 2;
  pts[1] = 3;
  cells->InsertNextCell(2, pts);
  pts[0] = 2;
  pts[1] = 1;
  cells->InsertNextCell(2, pts);
  pts[0] = 2;
  pts[1] = 6;
  cells->InsertNextCell(2, pts);

  pts[0] = 5;
  pts[1] = 4;
  cells->InsertNextCell(2, pts);
  pts[0] = 5;
  pts[1] = 6;
  cells->InsertNextCell(2, pts);
  pts[0] = 5;
  pts[1] = 1;
  cells->InsertNextCell(2, pts);

  pts[0] = 7;
  pts[1] = 6;
  cells->InsertNextCell(2, pts);
  pts[0] = 7;
  pts[1] = 4;
  cells->InsertNextCell(2, pts);
  pts[0] = 7;
  pts[1] = 3;
  cells->InsertNextCell(2, pts);

  this->m_polyData->Modified();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
double *BoxSelectionRepresentation3D::GetBounds() const
{
  return this->m_actor->GetBounds();
}
