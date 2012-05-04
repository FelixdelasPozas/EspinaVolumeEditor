///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: BoxSelectionRepresentation2D.cxx
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

BoxSelectionRepresentation2D::BoxSelectionRepresentation2D()
{
	this->InteractionState = BoxSelectionRepresentation2D::Outside;
	this->_selectionPoint[0] = this->_selectionPoint[1] = 0.0;

	// Initial positioning information
	this->PositionCoordinate = vtkCoordinate::New();
	this->PositionCoordinate->SetCoordinateSystemToWorld();
	this->PositionCoordinate->SetValue(0, 0, 0);
	this->Position2Coordinate = vtkCoordinate::New();
	this->Position2Coordinate->SetCoordinateSystemToWorld();
	this->Position2Coordinate->SetValue(1, 1, 0);

	// Create the geometry
	vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
	points->SetDataTypeToDouble();
	points->SetNumberOfPoints(4);
	points->SetPoint(0, 0.0, 0.0, 0.0);
	points->SetPoint(1, 1.0, 0.0, 0.0);
	points->SetPoint(2, 1.0, 1.0, 0.0);
	points->SetPoint(3, 0.0, 1.0, 0.0);

	vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
	lines->InsertNextCell(5);
	lines->InsertCellPoint(0);
	lines->InsertCellPoint(1);
	lines->InsertCellPoint(2);
	lines->InsertCellPoint(3);
	lines->InsertCellPoint(0);

	this->_widgetPolyData = vtkSmartPointer<vtkPolyData>::New();
	this->_widgetPolyData->SetPoints(points);
	this->_widgetPolyData->SetLines(lines);

	this->_widgetMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
	this->_widgetMapper->SetInput(this->_widgetPolyData);
	this->_widgetActor = vtkSmartPointer<vtkActor>::New();
	this->_widgetActor->SetMapper(this->_widgetMapper);

	this->_widgetActorProperty = vtkProperty::New();
	this->_widgetActor->SetProperty(this->_widgetActorProperty);

	this->MinimumSize[0] = 1;
	this->MinimumSize[1] = 1;
	this->MaximumSize[0] = 1000;
	this->MaximumSize[1] = 1000;

	this->MinimumSelectionSize[0] = 0;
	this->MinimumSelectionSize[1] = 0;
	this->MaximumSelectionSize[0] = 1000;
	this->MaximumSelectionSize[1] = 1000;

	this->Spacing[0] = 1.0;
	this->Spacing[1] = 1.0;

	// this->tolerance should depend on image spacing instead
	this->_tolerance = 2.00;
	this->Moving = 0;
}

BoxSelectionRepresentation2D::~BoxSelectionRepresentation2D()
{
	this->Renderer->RemoveActor(this->_widgetActor);

	// delete the three handled by pointers and not by smartpointers due to the use of vtk macros
	this->PositionCoordinate->Delete();
	this->Position2Coordinate->Delete();
	this->_widgetActorProperty->Delete();

	// the rest are handled by smartpointers
}

void BoxSelectionRepresentation2D::StartWidgetInteraction(double eventPos[2])
{
	this->StartEventPosition[0] = eventPos[0];
	this->StartEventPosition[1] = eventPos[1];
}

void BoxSelectionRepresentation2D::WidgetInteraction(double eventPos[2])
{
	// there are four parameters that can be adjusted fpos1[0], fpos1[1], fpos2[0] & fpos2[1]
	double *fpos1 = this->PositionCoordinate->GetValue();
	double *fpos2 = this->Position2Coordinate->GetValue();

	double delX = 0;
	double delY = 0;

	// handle the case when the event coordinates are negative (happens in world coordinates)
	if (this->StartEventPosition[0] < 0)
		delX = fabs(this->StartEventPosition[0]) + eventPos[0];
	else
		delX = eventPos[0] - this->StartEventPosition[0];

	if (this->StartEventPosition[1] < 0)
		delY = fabs(this->StartEventPosition[1]) + eventPos[1];
	else
		delY = eventPos[1] - this->StartEventPosition[1];

	switch (this->InteractionState)
	{
		case BoxSelectionRepresentation2D::AdjustingP0:
		{
			fpos1[0] += delX;
			fpos1[1] += delY;

			// check for size contraints
			double sizeX = Distance(fpos1[0], fpos2[0]);
			if ((sizeX > this->MaximumSize[0]) || (sizeX < this->MinimumSize[0]))
				fpos1[0] -= delX;

			double sizeY = Distance(fpos1[1], fpos2[1]);
			if ((sizeY > this->MaximumSize[1]) || (sizeY < this->MinimumSize[1]))
				fpos1[1] -= delY;

			break;
		}
		case BoxSelectionRepresentation2D::AdjustingP1:
		{
			fpos2[0] += delX;
			fpos1[1] += delY;

			// check for size contraints
			double sizeX = Distance(fpos1[0], fpos2[0]);
			if ((sizeX > this->MaximumSize[0]) || (sizeX < this->MinimumSize[0]))
				fpos2[0] -= delX;

			double sizeY = Distance(fpos1[1], fpos2[1]);
			if ((sizeY > this->MaximumSize[1]) || (sizeY < this->MinimumSize[1]))
				fpos1[1] -= delY;

			break;
		}
		case BoxSelectionRepresentation2D::AdjustingP2:
		{
			fpos2[0] += delX;
			fpos2[1] += delY;

			// check for size contraints
			double sizeX = Distance(fpos1[0], fpos2[0]);
			if ((sizeX > this->MaximumSize[0]) || (sizeX < this->MinimumSize[0]))
				fpos2[0] -= delX;

			double sizeY = Distance(fpos1[1], fpos2[1]);
			if ((sizeY > this->MaximumSize[1]) || (sizeY < this->MinimumSize[1]))
				fpos2[1] -= delY;
			break;
		}
		case BoxSelectionRepresentation2D::AdjustingP3:
		{
			fpos1[0] += delX;
			fpos2[1] += delY;

			double sizeX = Distance(fpos1[0], fpos2[0]);
			if ((sizeX > this->MaximumSize[0]) || (sizeX < this->MinimumSize[0]))
				fpos1[0] -= delX;

			double sizeY = Distance(fpos1[1], fpos2[1]);
			if ((sizeY > this->MaximumSize[1]) || (sizeY < this->MinimumSize[1]))
				fpos2[1] -= delY;
			break;
		}
		case BoxSelectionRepresentation2D::AdjustingE0:
		{
			fpos1[1] += delY;

			double sizeY = Distance(fpos1[1], fpos2[1]);
			if ((sizeY > this->MaximumSize[1]) || (sizeY < this->MinimumSize[1]))
				fpos1[1] -= delY;
			break;
		}
		case BoxSelectionRepresentation2D::AdjustingE1:
		{
			fpos2[0] += delX;

			double sizeX = Distance(fpos1[0], fpos2[0]);
			if ((sizeX > this->MaximumSize[0]) || (sizeX < this->MinimumSize[0]))
				fpos2[0] -= delX;
			break;
		}
		case BoxSelectionRepresentation2D::AdjustingE2:
		{
			fpos2[1] += delY;

			double sizeY = Distance(fpos1[1], fpos2[1]);
			if ((sizeY > this->MaximumSize[1]) || (sizeY < this->MinimumSize[1]))
				fpos2[1] -= delY;
			break;
		}
		case BoxSelectionRepresentation2D::AdjustingE3:
		{
			fpos1[0] += delX;

			double sizeX = Distance(fpos1[0], fpos2[0]);
			if ((sizeX > this->MaximumSize[0]) || (sizeX < this->MinimumSize[0]))
				fpos1[0] -= delX;
			break;
		}
		case BoxSelectionRepresentation2D::Inside:
			if (this->Moving)
			{
				fpos1[0] += delX;
				fpos1[1] += delY;
				fpos2[0] += delX;
				fpos2[1] += delY;
			}

			// these separate ifs allow movement in one axis while being stopped in the other by the selection limits
			if ((fpos1[0] < this->MinimumSelectionSize[0]) || (fpos2[0] > this->MaximumSelectionSize[0]))
			{
				fpos1[0] -= delX;
				fpos2[0] -= delX;
			}

			if ((fpos1[1] < this->MinimumSelectionSize[1]) || (fpos2[1] > this->MaximumSelectionSize[1]))
			{
				fpos1[1] -= delY;
				fpos2[1] -= delY;
			}
			break;
	}

	// check for limits when we're not translating the selection box
	if (fpos1[0] < MinimumSelectionSize[0])
		fpos1[0] = MinimumSelectionSize[0];
	if (fpos1[1] < MinimumSelectionSize[1])
		fpos1[1] = MinimumSelectionSize[1];
	if (fpos2[0] > MaximumSelectionSize[0])
		fpos2[0] = MaximumSelectionSize[0];
	if (fpos2[1] > MaximumSelectionSize[1])
		fpos2[1] = MaximumSelectionSize[1];

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

int BoxSelectionRepresentation2D::ComputeInteractionState(int X, int Y, int vtkNotUsed(modify))
{
	double *pos1 = this->PositionCoordinate->GetValue();
	double *pos2 = this->Position2Coordinate->GetValue();

	double XF = static_cast<double>(X);
	double YF = static_cast<double>(Y);

	// get world coordinates
	TransformToWorldCoordinates(&XF, &YF);

	// Figure out where we are in the widget. Exclude outside case first.
	if ((XF < (pos1[0] - this->_tolerance)) || ((pos2[0] + this->_tolerance) < XF) || (YF < (pos1[1] - this->_tolerance)) || ((pos2[1] + this->_tolerance) < YF))
		this->InteractionState = BoxSelectionRepresentation2D::Outside;
	else   // we are on the boundary or inside the border
	{
		// Now check for proximinity to edges and points
		int e0 = (YF >= (pos1[1] - this->_tolerance) && YF <= (pos1[1] + this->_tolerance));
		int e1 = (XF >= (pos2[0] - this->_tolerance) && XF <= (pos2[0] + this->_tolerance));
		int e2 = (YF >= (pos2[1] - this->_tolerance) && YF <= (pos2[1] + this->_tolerance));
		int e3 = (XF >= (pos1[0] - this->_tolerance) && XF <= (pos1[0] + this->_tolerance));

		// Corner points
		if (e0 && e1)
			this->InteractionState = BoxSelectionRepresentation2D::AdjustingP1;
		else if (e1 && e2)
			this->InteractionState = BoxSelectionRepresentation2D::AdjustingP2;
		else if (e2 && e3)
			this->InteractionState = BoxSelectionRepresentation2D::AdjustingP3;
		else if (e3 && e0)
			this->InteractionState = BoxSelectionRepresentation2D::AdjustingP0;
		// Edges
		else if (e0 || e1 || e2 || e3)
		{
			if (e0)
				this->InteractionState = BoxSelectionRepresentation2D::AdjustingE0;
			else if (e1)
				this->InteractionState = BoxSelectionRepresentation2D::AdjustingE1;
			else if (e2)
				this->InteractionState = BoxSelectionRepresentation2D::AdjustingE2;
			else if (e3)
				this->InteractionState = BoxSelectionRepresentation2D::AdjustingE3;
		}
		else   // must be interior
				this->InteractionState = BoxSelectionRepresentation2D::Inside;
	}

	return this->InteractionState;
}

void BoxSelectionRepresentation2D::BuildRepresentation()
{
	if ((this->GetMTime() > this->BuildTime) || (this->Renderer && this->Renderer->GetVTKWindow() && this->Renderer->GetVTKWindow()->GetMTime() > this->BuildTime))
	{
		// Set things up
		double *pos1 = this->PositionCoordinate->GetValue();
		double *pos2 = this->Position2Coordinate->GetValue();

		// update the geometry according to new coordinates
		vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
		points->SetDataTypeToDouble();
		points->SetNumberOfPoints(4);
		points->SetPoint(0, pos1[0], pos1[1], 0.0);
		points->SetPoint(1, pos2[0], pos1[1], 0.0);
		points->SetPoint(2, pos2[0], pos2[1], 0.0);
		points->SetPoint(3, pos1[0], pos2[1], 0.0);

		vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
		lines->InsertNextCell(5);
		lines->InsertCellPoint(0);
		lines->InsertCellPoint(1);
		lines->InsertCellPoint(2);
		lines->InsertCellPoint(3);
		lines->InsertCellPoint(0);

		this->_widgetPolyData->Reset();
		this->_widgetPolyData->SetPoints(points);
		this->_widgetPolyData->SetLines(lines);
		this->_widgetPolyData->Update();
	    this->BuildTime.Modified();
	}
}

void BoxSelectionRepresentation2D::GetActors2D(vtkPropCollection *pc)
{
	pc->AddItem(this->_widgetActor);
}

void BoxSelectionRepresentation2D::ReleaseGraphicsResources(vtkWindow *w)
{
	this->_widgetActor->ReleaseGraphicsResources(w);
}

int BoxSelectionRepresentation2D::RenderOverlay(vtkViewport *w)
{
	this->BuildRepresentation();
	if (!this->_widgetActor->GetVisibility())
	{
		return 0;
	}
	return this->_widgetActor->RenderOverlay(w);
}

int BoxSelectionRepresentation2D::RenderOpaqueGeometry(vtkViewport *w)
{
	this->BuildRepresentation();
	if (!this->_widgetActor->GetVisibility())
	{
		return 0;
	}
	return this->_widgetActor->RenderOpaqueGeometry(w);
}

int BoxSelectionRepresentation2D::RenderTranslucentPolygonalGeometry(vtkViewport *w)
{
	this->BuildRepresentation();
	if (!this->_widgetActor->GetVisibility())
	{
		return 0;
	}
	return this->_widgetActor->RenderTranslucentPolygonalGeometry(w);
}

int BoxSelectionRepresentation2D::HasTranslucentPolygonalGeometry()
{
	this->BuildRepresentation();
	if (!this->_widgetActor->GetVisibility())
	{
		return 0;
	}
	return this->_widgetActor->HasTranslucentPolygonalGeometry();
}

void BoxSelectionRepresentation2D::PrintSelf(ostream& os, vtkIndent indent)
{
	this->Superclass::PrintSelf(os, indent);

	os << indent << "Show Border: On\n";

	if (this->_widgetActorProperty)
	{
		os << indent << "Border Property:\n";
		this->_widgetActorProperty->PrintSelf(os, indent.GetNextIndent());
	}
	else
	{
		os << indent << "Border Property: (none)\n";
	}

	os << indent << "Minimum Size: " << this->MinimumSize[0] << " " << this->MinimumSize[1] << endl;
	os << indent << "Maximum Size: " << this->MaximumSize[0] << " " << this->MaximumSize[1] << endl;

	os << indent << "Minimum Selection: " << this->MinimumSelectionSize[0] << " " << this->MinimumSelectionSize[1] << endl;
	os << indent << "Maximum Selection: " << this->MaximumSelectionSize[0] << " " << this->MaximumSelectionSize[1] << endl;

	os << indent << "Moving: " << (this->Moving ? "On\n" : "Off\n");
	os << indent << "Tolerance: " << this->_tolerance << "\n";

	os << indent << "Selection Point: (" << this->_selectionPoint[0] << "," << this->_selectionPoint[1] << "}\n";
}

void BoxSelectionRepresentation2D::TransformToWorldCoordinates(double *x, double *y)
{
    vtkSmartPointer<vtkCoordinate> coords = vtkSmartPointer<vtkCoordinate>::New();

    coords->SetViewport(this->Renderer);
    coords->SetCoordinateSystemToDisplay();

    double xy[3] = { *x, *y, 0.0 };
    double *value;

    coords->SetValue(xy);
    value = coords->GetComputedWorldValue(this->Renderer);

	*x = value[0];
	*y = value[1];
}

// NOTE: we know that x is smaller than y, always
double BoxSelectionRepresentation2D::Distance(double x, double y)
{
	if (x < 0)
		return (fabs(x) + y);

	return (y - x);
}

void BoxSelectionRepresentation2D::SetBoxCoordinates(int x1, int y1, int x2, int y2)
{
	double fpos1[2] = { (static_cast<double>(x1)-0.5) * this->Spacing[0] , (static_cast<double>(y1)-0.5) * this->Spacing[1] };
	double fpos2[2] = { (static_cast<double>(x2)+0.5) * this->Spacing[0] , (static_cast<double>(y2)+0.5) * this->Spacing[1] };

	this->PositionCoordinate->SetValue(fpos1[0], fpos1[1],0);
	this->Position2Coordinate->SetValue(fpos2[0],fpos2[1],0);

	this->BuildRepresentation();
	this->Modified();
}
