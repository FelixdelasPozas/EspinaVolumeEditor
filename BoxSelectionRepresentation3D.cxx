///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: BoxSelectionRepresentation3D.cxx
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
	this->Points = vtkPoints::New(VTK_DOUBLE);
	this->Points->SetNumberOfPoints(8);			//8 corners

	// Create the outline
	vtkCellArray *cells = vtkCellArray::New();
	this->OutlinePolyData = vtkPolyData::New();
	this->OutlinePolyData->SetPoints(this->Points);

	this->OutlineMapper = vtkPolyDataMapper::New();
	this->OutlineMapper->SetInputData(this->OutlinePolyData);

	this->OutlineProperty = vtkProperty::New();
	this->OutlineProperty->SetAmbient(1.0);
	this->OutlineProperty->SetAmbientColor(1.0, 1.0, 1.0);
	this->OutlineProperty->SetLineWidth(3.0);

	this->HexOutline = vtkActor::New();
	this->HexOutline->SetMapper(this->OutlineMapper);
	this->HexOutline->SetProperty(this->OutlineProperty);

	cells->Allocate(cells->EstimateSize(12, 2));
	this->OutlinePolyData->SetLines(cells);
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

BoxSelectionRepresentation3D::~BoxSelectionRepresentation3D()
{
	this->Renderer->RemoveActor(this->HexOutline);
	// the rest is handled by smartpointers
}

void BoxSelectionRepresentation3D::SetRenderer(vtkRenderer *renderer)
{
	this->Renderer = renderer;
	this->Renderer->AddActor(this->HexOutline);
}

void BoxSelectionRepresentation3D::PlaceBox(double bounds[6])
{
	this->Points->SetPoint(0, bounds[0], bounds[2], bounds[4]);
	this->Points->SetPoint(1, bounds[1], bounds[2], bounds[4]);
	this->Points->SetPoint(2, bounds[1], bounds[3], bounds[4]);
	this->Points->SetPoint(3, bounds[0], bounds[3], bounds[4]);
	this->Points->SetPoint(4, bounds[0], bounds[2], bounds[5]);
	this->Points->SetPoint(5, bounds[1], bounds[2], bounds[5]);
	this->Points->SetPoint(6, bounds[1], bounds[3], bounds[5]);
	this->Points->SetPoint(7, bounds[0], bounds[3], bounds[5]);
	this->Points->Modified();
}

void BoxSelectionRepresentation3D::GenerateOutline()
{
	// Whatever the case may be, we have to reset the Lines of the
	// OutlinePolyData (i.e. nuke all current line data)
	vtkCellArray *cells = this->OutlinePolyData->GetLines();
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

	this->OutlinePolyData->Modified();
}

double *BoxSelectionRepresentation3D::GetBounds()
{
	return this->HexOutline->GetBounds();
}
