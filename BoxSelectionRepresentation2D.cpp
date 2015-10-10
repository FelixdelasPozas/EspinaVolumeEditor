///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: BoxSelectionRepresentation2D.cpp
// Purpose: used as BoxSelectionWidget representation in one of the planes
// Notes: Ripped from vtkBorderRepresentation class in vtk 5.8. Border always on and cannot change,
// 		  proportional resize off, no negotiation with subclasses.
///////////////////////////////////////////////////////////////////////////////////////////////////

// vtk includes
#include <vtkRenderer.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkObjectFactory.h>
#include <vtkWindow.h>
#include <vtkSmartPointer.h>
#include <vtkCoordinate.h>

// c++ includes (for fabs())
#include <cmath>

// project includes
#include "BoxSelectionRepresentation2D.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// BoxSelectionRepresentation2D class
//
vtkStandardNewMacro(BoxSelectionRepresentation2D);

///////////////////////////////////////////////////////////////////////////////////////////////////
BoxSelectionRepresentation2D::BoxSelectionRepresentation2D()
{
	this->InteractionState = WidgetState::Outside;
	this->m_selectionPoint[0] = this->m_selectionPoint[1] = 0.0;

	// Initial positioning information
	this->PositionCoordinate = vtkCoordinate::New();
	this->PositionCoordinate->SetCoordinateSystemToWorld();
	this->PositionCoordinate->SetValue(0, 0, 0);
	this->Position2Coordinate = vtkCoordinate::New();
	this->Position2Coordinate->SetCoordinateSystemToWorld();
	this->Position2Coordinate->SetValue(1, 1, 0);

	// Create the geometry
	auto points = vtkSmartPointer<vtkPoints>::New();
	points->SetDataTypeToDouble();
	points->SetNumberOfPoints(4);
	points->SetPoint(0, 0.0, 0.0, 0.0);
	points->SetPoint(1, 1.0, 0.0, 0.0);
	points->SetPoint(2, 1.0, 1.0, 0.0);
	points->SetPoint(3, 0.0, 1.0, 0.0);

	auto lines = vtkSmartPointer<vtkCellArray>::New();
	lines->InsertNextCell(5);
	lines->InsertCellPoint(0);
	lines->InsertCellPoint(1);
	lines->InsertCellPoint(2);
	lines->InsertCellPoint(3);
	lines->InsertCellPoint(0);

	this->m_polyData = vtkSmartPointer<vtkPolyData>::New();
	this->m_polyData->SetPoints(points);
	this->m_polyData->SetLines(lines);

	this->m_mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
	this->m_mapper->SetInputData(this->m_polyData);
	this->m_actor = vtkSmartPointer<vtkActor>::New();
	this->m_actor->SetMapper(this->m_mapper);

	this->m_widgetActorProperty = vtkProperty::New();
	this->m_actor->SetProperty(this->m_widgetActorProperty);

	this->m_minimumSize[0] = 1;
	this->m_minimumSize[1] = 1;
	this->m_maximumSize[0] = 1000;
	this->m_maximumSize[1] = 1000;

	this->m_minimumSelectionSize[0] = 0;
	this->m_minimumSelectionSize[1] = 0;
	this->m_maximumSelectionSize[0] = 1000;
	this->m_maximumSelectionSize[1] = 1000;

	this->m_spacing[0] = 1.0;
	this->m_spacing[1] = 1.0;

	// this->tolerance should depend on image spacing instead
	this->m_tolerance = 2.00;
	this->m_moving = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
BoxSelectionRepresentation2D::~BoxSelectionRepresentation2D()
{
	this->Renderer->RemoveActor(this->m_actor);

	// delete the three handled by pointers and not by smartpointers due to the use of vtk macros
	this->PositionCoordinate->Delete();
	this->Position2Coordinate->Delete();
	this->m_widgetActorProperty->Delete();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionRepresentation2D::StartWidgetInteraction(double eventPos[2])
{
	this->StartEventPosition[0] = eventPos[0];
	this->StartEventPosition[1] = eventPos[1];
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionRepresentation2D::WidgetInteraction(double eventPos[2])
{
	// there are four parameters that can be adjusted fpos1[0], fpos1[1], fpos2[0] & fpos2[1]
	auto fpos1 = this->PositionCoordinate->GetValue();
	auto fpos2 = this->Position2Coordinate->GetValue();

	double delX = 0;
	double delY = 0;

	// handle the case when the event coordinates are negative (happens in world coordinates)
	if (this->StartEventPosition[0] < 0)
	{
		delX = fabs(this->StartEventPosition[0]) + eventPos[0];
	}
	else
	{
		delX = eventPos[0] - this->StartEventPosition[0];
	}

	if (this->StartEventPosition[1] < 0)
	{
		delY = fabs(this->StartEventPosition[1]) + eventPos[1];
	}
	else
	{
		delY = eventPos[1] - this->StartEventPosition[1];
	}

	switch (this->InteractionState)
	{
		case WidgetState::AdjustingP0:
		{
			fpos1[0] += delX;
			fpos1[1] += delY;

			// check for size contraints
			auto sizeX = Distance(fpos1[0], fpos2[0]);
			if ((sizeX > this->m_maximumSize[0]) || (sizeX < this->m_minimumSize[0]))
			{
				fpos1[0] -= delX;
			}

			auto sizeY = Distance(fpos1[1], fpos2[1]);
			if ((sizeY > this->m_maximumSize[1]) || (sizeY < this->m_minimumSize[1]))
			{
				fpos1[1] -= delY;
			}

			break;
		}
		case WidgetState::AdjustingP1:
		{
			fpos2[0] += delX;
			fpos1[1] += delY;

			// check for size contraints
			auto sizeX = Distance(fpos1[0], fpos2[0]);
			if ((sizeX > this->m_maximumSize[0]) || (sizeX < this->m_minimumSize[0]))
			{
				fpos2[0] -= delX;
			}

			auto sizeY = Distance(fpos1[1], fpos2[1]);
			if ((sizeY > this->m_maximumSize[1]) || (sizeY < this->m_minimumSize[1]))
			{
				fpos1[1] -= delY;
			}

			break;
		}
		case WidgetState::AdjustingP2:
		{
			fpos2[0] += delX;
			fpos2[1] += delY;

			// check for size contraints
			auto sizeX = Distance(fpos1[0], fpos2[0]);
			if ((sizeX > this->m_maximumSize[0]) || (sizeX < this->m_minimumSize[0]))
			{
				fpos2[0] -= delX;
			}

			auto sizeY = Distance(fpos1[1], fpos2[1]);
			if ((sizeY > this->m_maximumSize[1]) || (sizeY < this->m_minimumSize[1]))
			{
				fpos2[1] -= delY;
			}

			break;
		}
		case WidgetState::AdjustingP3:
		{
			fpos1[0] += delX;
			fpos2[1] += delY;

			auto sizeX = Distance(fpos1[0], fpos2[0]);
			if ((sizeX > this->m_maximumSize[0]) || (sizeX < this->m_minimumSize[0]))
			{
				fpos1[0] -= delX;
			}

			auto sizeY = Distance(fpos1[1], fpos2[1]);
			if ((sizeY > this->m_maximumSize[1]) || (sizeY < this->m_minimumSize[1]))
			{
				fpos2[1] -= delY;
			}

			break;
		}
		case WidgetState::AdjustingE0:
		{
			fpos1[1] += delY;

			auto sizeY = Distance(fpos1[1], fpos2[1]);
			if ((sizeY > this->m_maximumSize[1]) || (sizeY < this->m_minimumSize[1]))
			{
				fpos1[1] -= delY;
			}

			break;
		}
		case WidgetState::AdjustingE1:
		{
			fpos2[0] += delX;

			auto sizeX = Distance(fpos1[0], fpos2[0]);
			if ((sizeX > this->m_maximumSize[0]) || (sizeX < this->m_minimumSize[0]))
			{
				fpos2[0] -= delX;
			}

			break;
		}
		case WidgetState::AdjustingE2:
		{
			fpos2[1] += delY;

			auto sizeY = Distance(fpos1[1], fpos2[1]);
			if ((sizeY > this->m_maximumSize[1]) || (sizeY < this->m_minimumSize[1]))
			{
				fpos2[1] -= delY;
			}

			break;
		}
		case WidgetState::AdjustingE3:
		{
			fpos1[0] += delX;

			auto sizeX = Distance(fpos1[0], fpos2[0]);
			if ((sizeX > this->m_maximumSize[0]) || (sizeX < this->m_minimumSize[0]))
			{
				fpos1[0] -= delX;
			}

			break;
		}
		case WidgetState::Inside:
			if (this->m_moving)
			{
				fpos1[0] += delX;
				fpos1[1] += delY;
				fpos2[0] += delX;
				fpos2[1] += delY;
			}

			// these separate ifs allow movement in one axis while being stopped in the other by the selection limits
			if ((fpos1[0] < this->m_minimumSelectionSize[0]) || (fpos2[0] > this->m_maximumSelectionSize[0]))
			{
				fpos1[0] -= delX;
				fpos2[0] -= delX;
			}

			if ((fpos1[1] < this->m_minimumSelectionSize[1]) || (fpos2[1] > this->m_maximumSelectionSize[1]))
			{
				fpos1[1] -= delY;
				fpos2[1] -= delY;
			}

			break;
	}

	// check for limits when we're not translating the selection box
	for(auto i: {0,1})
	{
	  if (fpos1[i] < m_minimumSelectionSize[i]) fpos1[i] = m_minimumSelectionSize[i];
	  if (fpos2[i] > m_maximumSelectionSize[i]) fpos2[i] = m_maximumSelectionSize[i];
	}

	// Modify the representation
	if (fpos2[0] > fpos1[0] && fpos2[1] > fpos1[1])
	{
		this->PositionCoordinate->SetValue(fpos1[0], fpos1[1],0);
		this->Position2Coordinate->SetValue(fpos2[0],fpos2[1],0);

		this->StartEventPosition[0] = eventPos[0];
		this->StartEventPosition[1] = eventPos[1];
	}

	this->BuildRepresentation();
	this->Modified();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int BoxSelectionRepresentation2D::ComputeInteractionState(int X, int Y, int vtkNotUsed(modify))
{
	auto pos1 = this->PositionCoordinate->GetValue();
	auto pos2 = this->Position2Coordinate->GetValue();

	auto fx = static_cast<double>(X);
	auto fy = static_cast<double>(Y);

	// get world coordinates
	TransformToWorldCoordinates(&fx, &fy);

	// Figure out where we are in the widget. Exclude outside case first.
	if ((fx < (pos1[0] - this->m_tolerance)) || ((pos2[0] + this->m_tolerance) < fx) || (fy < (pos1[1] - this->m_tolerance)) || ((pos2[1] + this->m_tolerance) < fy))
	{
		this->InteractionState = WidgetState::Outside;
	}
	else   // we are on the boundary or inside the border
	{
		// Now check for proximinity to edges and points
		auto e0 = (fy >= (pos1[1] - this->m_tolerance) && fy <= (pos1[1] + this->m_tolerance));
		auto e1 = (fx >= (pos2[0] - this->m_tolerance) && fx <= (pos2[0] + this->m_tolerance));
		auto e2 = (fy >= (pos2[1] - this->m_tolerance) && fy <= (pos2[1] + this->m_tolerance));
		auto e3 = (fx >= (pos1[0] - this->m_tolerance) && fx <= (pos1[0] + this->m_tolerance));

		// Corner points
		if (e0 && e1)
		{
			this->InteractionState = WidgetState::AdjustingP1;
		}
		else if (e1 && e2)
		{
			this->InteractionState = WidgetState::AdjustingP2;
		}
		else if (e2 && e3)
		{
			this->InteractionState = WidgetState::AdjustingP3;
		}
		else if (e3 && e0)
		{
			this->InteractionState = WidgetState::AdjustingP0;
		}
		else if (e0 || e1 || e2 || e3) // Edges
		{
			if (e0)
			{
				this->InteractionState = WidgetState::AdjustingE0;
			}
			else if (e1)
			{
				this->InteractionState = WidgetState::AdjustingE1;
			}
			else if (e2)
			{
				this->InteractionState = WidgetState::AdjustingE2;
			}
			else if (e3)
			{
				this->InteractionState = WidgetState::AdjustingE3;
			}
		}
		else // must be interior
		{
			this->InteractionState = WidgetState::Inside;
		}
	}

	return this->InteractionState;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionRepresentation2D::BuildRepresentation()
{
	if ((this->GetMTime() > this->BuildTime) || (this->Renderer && this->Renderer->GetVTKWindow() && this->Renderer->GetVTKWindow()->GetMTime() > this->BuildTime))
	{
		// Set things up
		auto pos1 = this->PositionCoordinate->GetValue();
		auto pos2 = this->Position2Coordinate->GetValue();

		// update the geometry according to new coordinates
		auto points = vtkSmartPointer<vtkPoints>::New();
		points->SetDataTypeToDouble();
		points->SetNumberOfPoints(4);
		points->SetPoint(0, pos1[0], pos1[1], 0.0);
		points->SetPoint(1, pos2[0], pos1[1], 0.0);
		points->SetPoint(2, pos2[0], pos2[1], 0.0);
		points->SetPoint(3, pos1[0], pos2[1], 0.0);

		auto lines = vtkSmartPointer<vtkCellArray>::New();
		lines->InsertNextCell(5);
		lines->InsertCellPoint(0);
		lines->InsertCellPoint(1);
		lines->InsertCellPoint(2);
		lines->InsertCellPoint(3);
		lines->InsertCellPoint(0);

		this->m_polyData->Reset();
		this->m_polyData->SetPoints(points);
		this->m_polyData->SetLines(lines);
		this->m_polyData->Modified();
	  this->BuildTime.Modified();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionRepresentation2D::GetActors2D(vtkPropCollection *pc)
{
	pc->AddItem(this->m_actor);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionRepresentation2D::ReleaseGraphicsResources(vtkWindow *w)
{
	this->m_actor->ReleaseGraphicsResources(w);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int BoxSelectionRepresentation2D::RenderOverlay(vtkViewport *w)
{
	this->BuildRepresentation();
	if (!this->m_actor->GetVisibility())
	{
		return 0;
	}
	return this->m_actor->RenderOverlay(w);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int BoxSelectionRepresentation2D::RenderOpaqueGeometry(vtkViewport *w)
{
	this->BuildRepresentation();
	if (!this->m_actor->GetVisibility())
	{
		return 0;
	}
	return this->m_actor->RenderOpaqueGeometry(w);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int BoxSelectionRepresentation2D::RenderTranslucentPolygonalGeometry(vtkViewport *w)
{
	this->BuildRepresentation();
	if (!this->m_actor->GetVisibility())
	{
		return 0;
	}
	return this->m_actor->RenderTranslucentPolygonalGeometry(w);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int BoxSelectionRepresentation2D::HasTranslucentPolygonalGeometry()
{
	this->BuildRepresentation();
	if (!this->m_actor->GetVisibility())
	{
		return 0;
	}
	return this->m_actor->HasTranslucentPolygonalGeometry();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionRepresentation2D::PrintSelf(ostream& os, vtkIndent indent)
{
	this->Superclass::PrintSelf(os, indent);

	os << indent << "Show Border: On\n";

	if (this->m_widgetActorProperty)
	{
		os << indent << "Border Property:\n";
		this->m_widgetActorProperty->PrintSelf(os, indent.GetNextIndent());
	}
	else
	{
		os << indent << "Border Property: (none)\n";
	}

	os << indent << "Minimum Size: " << this->m_minimumSize[0] << " " << this->m_minimumSize[1] << endl;
	os << indent << "Maximum Size: " << this->m_maximumSize[0] << " " << this->m_maximumSize[1] << endl;

	os << indent << "Minimum Selection: " << this->m_minimumSelectionSize[0] << " " << this->m_minimumSelectionSize[1] << endl;
	os << indent << "Maximum Selection: " << this->m_maximumSelectionSize[0] << " " << this->m_maximumSelectionSize[1] << endl;

	os << indent << "Moving: " << (this->m_moving ? "On\n" : "Off\n");
	os << indent << "Tolerance: " << this->m_tolerance << "\n";

	os << indent << "Selection Point: (" << this->m_selectionPoint[0] << "," << this->m_selectionPoint[1] << "}\n";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionRepresentation2D::TransformToWorldCoordinates(double *x, double *y)
{
  auto coords = vtkSmartPointer<vtkCoordinate>::New();

  coords->SetViewport(this->Renderer);
  coords->SetCoordinateSystemToDisplay();

  double xy[3] = { *x, *y, 0.0 };
  double *value;

  coords->SetValue(xy);
  value = coords->GetComputedWorldValue(this->Renderer);

  *x = value[0];
  *y = value[1];
}

///////////////////////////////////////////////////////////////////////////////////////////////////
double BoxSelectionRepresentation2D::Distance(double x, double y)
{
  // NOTE: we know that x is smaller than y, always
	if (x < 0)
	{
		return (fabs(x) + y);
	}

	return (y - x);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionRepresentation2D::SetBoxCoordinates(int x1, int y1, int x2, int y2)
{
	double fpos1[3] = { (static_cast<double>(x1)-0.5) * this->m_spacing[0] , (static_cast<double>(y1)-0.5) * this->m_spacing[1], 0.0 };
	double fpos2[3] = { (static_cast<double>(x2)+0.5) * this->m_spacing[0] , (static_cast<double>(y2)+0.5) * this->m_spacing[1], 0.0 };

	this->PositionCoordinate->SetValue(fpos1);
	this->Position2Coordinate->SetValue(fpos2);

	this->BuildRepresentation();
	this->Modified();
}
