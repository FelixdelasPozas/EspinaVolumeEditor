///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: ContourRepresentation.cpp
// Purpose: Manages lasso and polygon manual selection representations
// Notes: Modified from vtkOrientedGlyphContourRepresentation class in vtk 5.8
///////////////////////////////////////////////////////////////////////////////////////////////////

// project includes
#include "ContourRepresentationGlyph.h"

// vtk includes
#include <vtkCleanPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkObjectFactory.h>
#include <vtkProperty.h>
#include <vtkAssemblyPath.h>
#include <vtkMath.h>
#include <vtkInteractorObserver.h>
#include <vtkLine.h>
#include <vtkCoordinate.h>
#include <vtkGlyph3D.h>
#include <vtkCursor2D.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkTransform.h>
#include <vtkCamera.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkFocalPlanePointPlacer.h>
#include <vtkBezierContourLineInterpolator.h>
#include <vtkOpenGL.h>
#include <vtkSphereSource.h>
#include <vtkIncrementalOctreePointLocator.h>
#include <vtkPolygon.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
// ContourRepresentation class
//
vtkStandardNewMacro(ContourRepresentationGlyph);

///////////////////////////////////////////////////////////////////////////////////////////////////
ContourRepresentationGlyph::ContourRepresentationGlyph()
: Glypher                 {vtkSmartPointer<vtkGlyph3D>::New()}
, Mapper                  {vtkSmartPointer<vtkPolyDataMapper>::New()}
, Actor                   {vtkSmartPointer<vtkActor>::New()}
, ActiveGlypher           {vtkSmartPointer<vtkGlyph3D>::New()}
, ActiveMapper            {vtkSmartPointer<vtkPolyDataMapper>::New()}
, ActiveActor             {vtkSmartPointer<vtkActor>::New()}
, CursorShape             {nullptr}
, ActiveCursorShape       {nullptr}
, FocalPoint              {vtkSmartPointer<vtkPoints>::New()}
, FocalData               {vtkSmartPointer<vtkPolyData>::New()}
, ActiveFocalPoint        {vtkSmartPointer<vtkPoints>::New()}
, ActiveFocalData         {vtkSmartPointer<vtkPolyData>::New()}
, SelectedNodesData       {nullptr}
, SelectedNodesPoints     {nullptr}
, SelectedNodesActor      {nullptr}
, SelectedNodesMapper     {nullptr}
, SelectedNodesGlypher    {nullptr}
, SelectedNodesCursorShape{nullptr}
, Lines                   {vtkSmartPointer<vtkPolyData>::New()}
, LinesMapper             {vtkSmartPointer<vtkPolyDataMapper>::New()}
, LinesActor              {vtkSmartPointer<vtkActor>::New()}
, Property                {vtkSmartPointer<vtkProperty>::New()}
, ActiveProperty          {vtkSmartPointer<vtkProperty>::New()}
, LinesProperty           {vtkSmartPointer<vtkProperty>::New()}
, AlwaysOnTop             {0}
{
  this->InteractionState = ContourRepresentation::Outside;
  this->SetPointPlacer(vtkFocalPlanePointPlacer::New());
  this->SetLineInterpolator(vtkBezierContourLineInterpolator::New());
  this->HandleSize = 0.01;

	this->Spacing[0] = this->Spacing[1] = 1.0;
	this->LastPickPosition[0] = this->LastPickPosition[1] = 0;
	this->LastEventPosition[0] = this->LastEventPosition[1] = 0;
	this->InteractionOffset[0] = this->InteractionOffset[1] = 0;

	// Represent the position of the cursor
	this->FocalPoint->SetNumberOfPoints(100);
	this->FocalPoint->SetNumberOfPoints(1);
	this->FocalPoint->SetPoint(0, 0.0, 0.0, 0.0);

	auto normals = vtkDoubleArray::New();
	normals->SetNumberOfComponents(3);
	normals->SetNumberOfTuples(100);
	normals->SetNumberOfTuples(1);
	double n[3]{ 0, 0, 0 };
	normals->SetTuple(0, n);

	// Represent the position of the cursor
	this->ActiveFocalPoint->SetNumberOfPoints(100);
	this->ActiveFocalPoint->SetNumberOfPoints(1);
	this->ActiveFocalPoint->SetPoint(0, 0.0, 0.0, 0.0);

	auto activeNormals = vtkDoubleArray::New();
	activeNormals->SetNumberOfComponents(3);
	activeNormals->SetNumberOfTuples(100);
	activeNormals->SetNumberOfTuples(1);
	activeNormals->SetTuple(0, n);

	this->FocalData->SetPoints(this->FocalPoint);
	this->FocalData->GetPointData()->SetNormals(normals);
	normals->Delete();

	this->ActiveFocalData->SetPoints(this->ActiveFocalPoint);
	this->ActiveFocalData->GetPointData()->SetNormals(activeNormals);
	activeNormals->Delete();

	this->Glypher->SetInputData(this->FocalData);
	this->Glypher->SetVectorModeToUseNormal();
	this->Glypher->OrientOn();
	this->Glypher->ScalingOn();
	this->Glypher->SetScaleModeToDataScalingOff();
	this->Glypher->SetScaleFactor(1.0);

	this->ActiveGlypher->SetInputData(this->ActiveFocalData);
	this->ActiveGlypher->SetVectorModeToUseNormal();
	this->ActiveGlypher->OrientOn();
	this->ActiveGlypher->ScalingOn();
	this->ActiveGlypher->SetScaleModeToDataScalingOff();
	this->ActiveGlypher->SetScaleFactor(1.0);

	// The transformation of the cursor will be done via vtkGlyph3D
	// By default a vtkCursor2D will be used to define the cursor shape
	auto cursor2D = vtkSmartPointer<vtkCursor2D>::New();
	cursor2D->AllOff();
	cursor2D->PointOn();
	cursor2D->Update();
	this->SetCursorShape(cursor2D->GetOutput());

	auto sphere = vtkSmartPointer<vtkSphereSource>::New();
	sphere->SetThetaResolution(12);
	sphere->SetRadius(0.5);
	sphere->SetCenter(0, 0, 0);

	auto clean = vtkSmartPointer<vtkCleanPolyData>::New();
	clean->PointMergingOn();
	clean->CreateDefaultLocator();
	clean->SetInputConnection(0, sphere->GetOutputPort(0));

	auto transform = vtkSmartPointer<vtkTransform>::New();
	transform->RotateZ(90.0);

	auto tpd = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
	tpd->SetInputConnection(0, clean->GetOutputPort(0));
	tpd->SetTransform(transform);
	tpd->Update();

	auto shape = vtkSmartPointer<vtkPolyData>::New();
	shape->DeepCopy(tpd->GetOutput());
	this->SetActiveCursorShape(shape);

	this->Glypher->SetSourceData(this->CursorShape);
	this->ActiveGlypher->SetSourceData(this->ActiveCursorShape);

	this->Mapper->SetInputData(this->Glypher->GetOutput());
	this->Mapper->SetResolveCoincidentTopologyToPolygonOffset();
	this->Mapper->ScalarVisibilityOff();
	this->Mapper->ImmediateModeRenderingOn();

	this->ActiveMapper->SetInputData(this->ActiveGlypher->GetOutput());
	this->ActiveMapper->SetResolveCoincidentTopologyToPolygonOffset();
	this->ActiveMapper->ScalarVisibilityOff();
	this->ActiveMapper->ImmediateModeRenderingOn();

	// Set up the initial properties
	this->SetDefaultProperties();

	this->Actor->SetMapper(this->Mapper);
	this->Actor->SetProperty(this->Property);

	this->ActiveActor->SetMapper(this->ActiveMapper);
	this->ActiveActor->SetProperty(this->ActiveProperty);

	this->LinesMapper->SetInputData(this->Lines);

	this->LinesActor->SetMapper(this->LinesMapper);
	this->LinesActor->SetProperty(this->LinesProperty);

	this->InteractionOffset[0] = 0.0;
	this->InteractionOffset[1] = 0.0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::SetCursorShape(vtkSmartPointer<vtkPolyData> shape)
{
	if (shape != this->CursorShape)
	{
		this->CursorShape = shape;

		if (this->CursorShape)
		{
			this->CursorShape->Register(this);
		}

		if (this->CursorShape)
		{
			this->Glypher->SetSourceData(this->CursorShape);
		}

		this->Modified();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
vtkSmartPointer<vtkPolyData> ContourRepresentationGlyph::GetCursorShape() const
{
	return this->CursorShape;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::SetActiveCursorShape(vtkSmartPointer<vtkPolyData> shape)
{
	if (shape != this->ActiveCursorShape)
	{
		this->ActiveCursorShape = shape;

		if (this->ActiveCursorShape)
		{
			this->ActiveCursorShape->Register(this);
		}

		if (this->ActiveCursorShape)
		{
			this->ActiveGlypher->SetSourceData(this->ActiveCursorShape);
		}

		this->Modified();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
vtkSmartPointer<vtkPolyData> ContourRepresentationGlyph::GetActiveCursorShape() const
{
	return this->ActiveCursorShape;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::SetRenderer(vtkRenderer *ren)
{
	this->Superclass::SetRenderer(ren);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentationGlyph::ComputeInteractionState(int X, int Y, int vtkNotUsed(modified))
{
	double pos[4];
	this->FocalPoint->GetPoint(0, pos);
	pos[3] = 1.0;
	this->Renderer->SetWorldPoint(pos);
	this->Renderer->WorldToDisplay();
	this->Renderer->GetDisplayPoint(pos);

	double xyz[3]{ static_cast<double>(X), static_cast<double>(Y), pos[2]};

	this->VisibilityOn();
	auto tolerance2 = this->PixelTolerance * this->PixelTolerance;
	if (vtkMath::Distance2BetweenPoints(xyz, pos) <= tolerance2)
	{
		this->InteractionState = ContourRepresentation::Nearby;
		if (!this->ActiveCursorShape)
		{
			this->VisibilityOff();
		}
	}
	else
	{
		if (this->ActiveNode != -1)
		{
			this->InteractionState = ContourRepresentation::NearPoint;
			if (!this->ActiveCursorShape)
			{
				this->VisibilityOff();
			}
		}
		else
		{
			if (this->FindClosestDistanceToContour(X,Y) <= this->PixelTolerance)
			{
				this->InteractionState = ContourRepresentation::NearContour;
				if (!this->ActiveCursorShape)
				{
					this->VisibilityOff();
				}
			}
			else
			{
				if (!this->ClosedLoop)
				{
					this->InteractionState = ContourRepresentation::Outside;
					if (!this->CursorShape)
					{
						this->VisibilityOff();
					}
				}
				else
				{
					if (!this->ShootingAlgorithm(X, Y))
					{
						this->InteractionState = ContourRepresentation::Outside;
						if (!this->CursorShape)
						{
							this->VisibilityOff();
						}
					}
					else
					{
						// checking the active node allow better node picking, even being inside the polygon
						if (-1 == this->ActiveNode)
						{
							this->InteractionState = ContourRepresentation::Inside;
						}
						else
						{
							this->InteractionState = ContourRepresentation::Outside;
						}
					}
				}
			}
		}
	}

	return this->InteractionState;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::StartWidgetInteraction(double startEventPos[2])
{
	this->StartEventPosition[0] = startEventPos[0];
	this->StartEventPosition[1] = startEventPos[1];
	this->StartEventPosition[2] = 0.0;

	this->LastEventPosition[0] = startEventPos[0];
	this->LastEventPosition[1] = startEventPos[1];

	// How far is this in pixels from the position of this widget?
	// Maintain this during interaction such as translating (don't
	// force center of widget to snap to mouse position)

	// convert position to display coordinates
	double pos[2];
	this->GetNthNodeDisplayPosition(this->ActiveNode, pos);

	this->InteractionOffset[0] = pos[0] - startEventPos[0];
	this->InteractionOffset[1] = pos[1] - startEventPos[1];
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::WidgetInteraction(double eventPos[2])
{
	// Process the motion
  switch(this->CurrentOperation)
  {
    case ContourRepresentation::Translate:
      this->Translate(eventPos);
      break;
    case ContourRepresentation::Shift:
      this->ShiftContour(eventPos);
      break;
    case ContourRepresentation::Scale:
      this->ScaleContour(eventPos);
      break;
    default:
      break;
  }

	// Book keeping
	this->LastEventPosition[0] = eventPos[0];
	this->LastEventPosition[1] = eventPos[1];
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::Translate(double eventPos[2])
{
	double ref[3];

	if (!this->GetActiveNodeWorldPosition(ref))	return;

	double displayPos[2]{ eventPos[0] + this->InteractionOffset[0], eventPos[1] + this->InteractionOffset[1] };
	double worldPos[3];
	double worldOrient[9]{ 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };

	if (this->PointPlacer->ComputeWorldPosition(this->Renderer, displayPos, ref, worldPos, worldOrient))
	{
		this->GetActiveNodeWorldPosition(ref);
		this->SetActiveNodeToWorldPosition(worldPos, worldOrient);

		// take back movement if it breaks the contour
		if (this->CheckContourIntersection(this->ActiveNode))
		{
			this->SetActiveNodeToWorldPosition(ref, worldOrient);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::ShiftContour(double eventPos[2])
{
	double vector[3]{ eventPos[0], eventPos[1], 0.0 };
	double worldPos[3], worldPos2[3];
	double worldOrient[9]{ 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };

	this->Renderer->SetDisplayPoint(vector);
	this->Renderer->DisplayToWorld();
	this->Renderer->GetWorldPoint(worldPos);

	vector[0] = this->LastEventPosition[0];
	vector[1] = this->LastEventPosition[1];
	vector[2] = 0.0;

	this->Renderer->SetDisplayPoint(vector);
	this->Renderer->DisplayToWorld();
	this->Renderer->GetWorldPoint(worldPos2);

	vector[0] = worldPos[0] - worldPos2[0];
	vector[1] = worldPos[1] - worldPos2[1];
	vector[2] = worldPos[2] - worldPos2[2];

	for (int i = 0; i < this->GetNumberOfNodes(); i++)
	{
		this->GetNthNodeWorldPosition(i, worldPos);
		worldPos[0] += vector[0];
		worldPos[1] += vector[1];
		worldPos[2] += vector[2];
		this->SetNthNodeWorldPosition(i, worldPos, worldOrient);
	}

	this->NeedToRenderOn();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::ScaleContour(double eventPos[2])
{
	double ref[3];

	if (!this->GetActiveNodeWorldPosition(ref)) return;

	double centroid[3];
	ComputeCentroid(centroid);

	auto r2 = vtkMath::Distance2BetweenPoints(ref, centroid);

	double displayPos[2]{ eventPos[0] + this->InteractionOffset[0], eventPos[1] + this->InteractionOffset[1]};
	double worldPos[3];
	double worldOrient[9] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };

	if (this->PointPlacer->ComputeWorldPosition(this->Renderer, displayPos, ref, worldPos, worldOrient))
	{
		auto d2 = vtkMath::Distance2BetweenPoints(worldPos, centroid);
		if (d2 != 0.)
		{
			double ratio = sqrt(d2 / r2);

			for (int i = 0; i < this->GetNumberOfNodes(); i++)
			{
				this->GetNthNodeWorldPosition(i, ref);
				worldPos[0] = centroid[0] + ratio * (ref[0] - centroid[0]);
				worldPos[1] = centroid[0] + ratio * (ref[1] - centroid[1]);
				worldPos[2] = centroid[0] + ratio * (ref[2] - centroid[2]);
				this->SetNthNodeWorldPosition(i, worldPos, worldOrient);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::ComputeCentroid(double* centroid)
{
	double p[3];
	centroid[0] = 0.;
	centroid[1] = 0.;
	centroid[2] = 0.;

	for (int i = 0; i < this->GetNumberOfNodes(); i++)
	{
		this->GetNthNodeWorldPosition(i, p);
		centroid[0] += p[0];
		centroid[1] += p[1];
		centroid[2] += p[2];
	}

	auto inv_N = 1. / static_cast<double>(this->GetNumberOfNodes());
	centroid[0] *= inv_N;
	centroid[1] *= inv_N;
	centroid[2] *= inv_N;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::Scale(double eventPos[2])
{
	// Get the current scale factor
	auto sf = this->Glypher->GetScaleFactor();

	// Compute the scale factor
	auto size = this->Renderer->GetSize();
	auto dPos = static_cast<double>(eventPos[1] - this->LastEventPosition[1]);
	sf *= (1.0 + 2.0 * (dPos / size[1])); //scale factor of 2.0 is arbitrary

	// Scale the handle
	this->Glypher->SetScaleFactor(sf);
	if (this->ShowSelectedNodes && this->SelectedNodesGlypher)
	{
		this->SelectedNodesGlypher->SetScaleFactor(sf);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::SetDefaultProperties()
{
	this->Property->SetColor(1.0, 1.0, 1.0);
	this->Property->SetLineWidth(0.5);
	this->Property->SetPointSize(4);

	this->ActiveProperty->SetColor(1.0, 1.0, 1.0);
	this->ActiveProperty->SetRepresentationToSurface();
	this->ActiveProperty->SetAmbient(1.0);
	this->ActiveProperty->SetDiffuse(0.0);
	this->ActiveProperty->SetSpecular(0.0);
	this->ActiveProperty->SetLineWidth(1.0);

	this->LinesProperty->SetAmbient(1.0);
	this->LinesProperty->SetDiffuse(0.0);
	this->LinesProperty->SetSpecular(0.0);
	this->LinesProperty->SetColor(1, 1, 1);
	this->LinesProperty->SetLineWidth(1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::BuildLines()
{
	auto points = vtkPoints::New();
	auto lines = vtkCellArray::New();

	vtkIdType index = 0;

	auto count = this->GetNumberOfNodes();
	for (int i = 0; i < this->GetNumberOfNodes(); i++)
	{
		count += this->GetNumberOfIntermediatePoints(i);
	}

	points->SetNumberOfPoints(count);
	vtkIdType numLines;

	numLines = count + 1;

	if (numLines > 0)
	{
		auto lineIndices = new vtkIdType[numLines];

		double pos[3];
		for (int i = 0; i < this->GetNumberOfNodes(); i++)
		{
			// Add the node
			this->GetNthNodeWorldPosition(i, pos);
			pos[2] = 0.0;
			points->InsertPoint(index, pos);
			lineIndices[index] = index;
			index++;

			auto numIntermediatePoints = this->GetNumberOfIntermediatePoints(i);

			for (int j = 0; j < numIntermediatePoints; j++)
			{
				this->GetIntermediatePointWorldPosition(i, j, pos);
				pos[2] = 0.0;
				points->InsertPoint(index, pos);
				lineIndices[index] = index;
				index++;
			}
		}

		lineIndices[index] = 0;

		lines->InsertNextCell(numLines, lineIndices);
		delete[] lineIndices;
	}

	this->Lines->SetPoints(points);
	this->Lines->SetLines(lines);

	points->Delete();
	lines->Delete();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
vtkPolyData *ContourRepresentationGlyph::GetContourRepresentationAsPolyData()
{
	// Get the points in this contour as a vtkPolyData.
	return this->Lines;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::BuildRepresentation()
{
	// Make sure we are up to date with any changes made in the placer
	this->UpdateContour();

	double p1[4], p2[4];
	this->Renderer->GetActiveCamera()->GetFocalPoint(p1);
	p1[3] = 1.0;
	this->Renderer->SetWorldPoint(p1);
	this->Renderer->WorldToView();
	this->Renderer->GetViewPoint(p1);

	auto depth = p1[2];
	double aspect[2];
	this->Renderer->ComputeAspect();
	this->Renderer->GetAspect(aspect);

	p1[0] = -aspect[0];
	p1[1] = -aspect[1];
	this->Renderer->SetViewPoint(p1);
	this->Renderer->ViewToWorld();
	this->Renderer->GetWorldPoint(p1);

	p2[0] = aspect[0];
	p2[1] = aspect[1];
	p2[2] = depth;
	p2[3] = 1.0;
	this->Renderer->SetViewPoint(p2);
	this->Renderer->ViewToWorld();
	this->Renderer->GetWorldPoint(p2);

	auto distance = sqrt(vtkMath::Distance2BetweenPoints(p1, p2));

	auto size = this->Renderer->GetRenderWindow()->GetSize();
	double viewport[4];
	this->Renderer->GetViewport(viewport);

	double x, y, scale;

	x = size[0] * (viewport[2] - viewport[0]);
	y = size[1] * (viewport[3] - viewport[1]);

	scale = sqrt(x * x + y * y);

	distance = 1000 * distance / scale;

	this->Glypher->SetScaleFactor(distance * this->HandleSize);
	this->ActiveGlypher->SetScaleFactor(distance * this->HandleSize);
	auto numPoints = this->GetNumberOfNodes();

	if (this->ShowSelectedNodes && this->SelectedNodesGlypher)
	{
		this->SelectedNodesGlypher->SetScaleFactor(distance * this->HandleSize);
		this->FocalPoint->Reset();
		this->FocalPoint->SetNumberOfPoints(0);
		this->FocalData->GetPointData()->GetNormals()->SetNumberOfTuples(0);
		this->SelectedNodesPoints->Reset();
		this->SelectedNodesPoints->SetNumberOfPoints(0);
		this->SelectedNodesData->GetPointData()->GetNormals()->SetNumberOfTuples(0);
		for (int i = 0; i < numPoints; i++)
		{
			if (i != this->ActiveNode)
			{
				double worldPos[3];
				double worldOrient[9];
				this->GetNthNodeWorldPosition(i, worldPos);
				this->GetNthNodeWorldOrientation(i, worldOrient);
				if (this->GetNthNodeSelected(i))
				{
					this->SelectedNodesPoints->InsertNextPoint(worldPos);
					this->SelectedNodesData->GetPointData()->GetNormals()->InsertNextTuple(worldOrient + 6);
				}
				else
				{
					this->FocalPoint->InsertNextPoint(worldPos);
					this->FocalData->GetPointData()->GetNormals()->InsertNextTuple(worldOrient + 6);
				}
			}
		}
		this->SelectedNodesPoints->Modified();
		this->SelectedNodesData->GetPointData()->GetNormals()->Modified();
		this->SelectedNodesData->Modified();
	}
	else
	{
		if (this->ActiveNode >= 0 && this->ActiveNode < this->GetNumberOfNodes())
		{
			this->FocalPoint->SetNumberOfPoints(numPoints - 1);
			this->FocalData->GetPointData()->GetNormals()->SetNumberOfTuples(numPoints - 1);
		}
		else
		{
			this->FocalPoint->SetNumberOfPoints(numPoints);
			this->FocalData->GetPointData()->GetNormals()->SetNumberOfTuples(numPoints);
		}
		auto idx = 0;
		for (int i = 0; i < numPoints; i++)
		{
			if (i != this->ActiveNode)
			{
				double worldPos[3];
				double worldOrient[9];
				this->GetNthNodeWorldPosition(i, worldPos);
				this->GetNthNodeWorldOrientation(i, worldOrient);
				this->FocalPoint->SetPoint(idx, worldPos);
				this->FocalData->GetPointData()->GetNormals()->SetTuple(idx, worldOrient + 6);
				idx++;
			}
		}
	}

	this->FocalPoint->Modified();
	this->FocalData->GetPointData()->GetNormals()->Modified();
	this->FocalData->Modified();

	if (this->ActiveNode >= 0 && this->ActiveNode < this->GetNumberOfNodes())
	{
		double worldPos[3];
		double worldOrient[9];
		this->GetNthNodeWorldPosition(this->ActiveNode, worldPos);
		this->GetNthNodeWorldOrientation(this->ActiveNode, worldOrient);
		this->ActiveFocalPoint->SetPoint(0, worldPos);
		this->ActiveFocalData->GetPointData()->GetNormals()->SetTuple(0, worldOrient + 6);

		this->ActiveFocalPoint->Modified();
		this->ActiveFocalData->GetPointData()->GetNormals()->Modified();
		this->ActiveFocalData->Modified();
		this->ActiveActor->VisibilityOn();
	}
	else
	{
		this->ActiveActor->VisibilityOff();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::GetActors(vtkPropCollection *pc)
{
	this->Actor->GetActors(pc);
	this->ActiveActor->GetActors(pc);
	this->LinesActor->GetActors(pc);
	if (this->ShowSelectedNodes && this->SelectedNodesActor)
	{
		this->SelectedNodesActor->GetActors(pc);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::ReleaseGraphicsResources(vtkWindow *win)
{
	this->Actor->ReleaseGraphicsResources(win);
	this->ActiveActor->ReleaseGraphicsResources(win);
	this->LinesActor->ReleaseGraphicsResources(win);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentationGlyph::RenderOverlay(vtkViewport *viewport)
{
	auto count = 0;
	count += this->LinesActor->RenderOverlay(viewport);

	if (this->Actor->GetVisibility())
	{
		count += this->Actor->RenderOverlay(viewport);
	}

	if (this->ActiveActor->GetVisibility())
	{
		count += this->ActiveActor->RenderOverlay(viewport);
	}

	return count;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentationGlyph::RenderOpaqueGeometry(vtkViewport *viewport)
{
	// Since we know RenderOpaqueGeometry gets called first, will do the
	// build here
	this->BuildRepresentation();

	GLboolean flag = GL_FALSE;
	if (this->AlwaysOnTop && (this->ActiveActor->GetVisibility() || this->LinesActor->GetVisibility()))
	{
		glGetBooleanv(GL_DEPTH_TEST, &flag);
		if (flag)
			glDisable(GL_DEPTH_TEST);
	}

	auto count = 0;
	count += this->LinesActor->RenderOpaqueGeometry(viewport);

	if (this->Actor->GetVisibility())
	{
		count += this->Actor->RenderOpaqueGeometry(viewport);
	}

	if (this->ActiveActor->GetVisibility())
	{
		count += this->ActiveActor->RenderOpaqueGeometry(viewport);
	}

	if (this->ShowSelectedNodes && this->SelectedNodesActor && this->SelectedNodesActor->GetVisibility())
	{
		count += this->SelectedNodesActor->RenderOpaqueGeometry(viewport);
	}

	if (flag && this->AlwaysOnTop && (this->ActiveActor->GetVisibility() || this->LinesActor->GetVisibility()))
	{
		glEnable(GL_DEPTH_TEST);
	}

	return count;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentationGlyph::RenderTranslucentPolygonalGeometry(vtkViewport *viewport)
{
	auto count = 0;
	count += this->LinesActor->RenderTranslucentPolygonalGeometry(viewport);

	if (this->Actor->GetVisibility())
	{
		count += this->Actor->RenderTranslucentPolygonalGeometry(viewport);
	}

	if (this->ActiveActor->GetVisibility())
	{
		count += this->ActiveActor->RenderTranslucentPolygonalGeometry(viewport);
	}

	return count;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentationGlyph::HasTranslucentPolygonalGeometry()
{
	auto result = 0;
	result |= this->LinesActor->HasTranslucentPolygonalGeometry();

	if (this->Actor->GetVisibility())
	{
		result |= this->Actor->HasTranslucentPolygonalGeometry();
	}

	if (this->ActiveActor->GetVisibility())
	{
		result |= this->ActiveActor->HasTranslucentPolygonalGeometry();
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::SetLineColor(double r, double g, double b)
{
	if (this->GetLinesProperty())
	{
		this->GetLinesProperty()->SetColor(r, g, b);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::SetShowSelectedNodes(int flag)
{
	vtkDebugMacro(<< this->GetClassName() << " (" << this << "): setting ShowSelectedNodes to " << flag);
	if (this->ShowSelectedNodes != flag)
	{
		this->ShowSelectedNodes = flag;
		this->Modified();

		if (this->ShowSelectedNodes)
		{
			if (!this->SelectedNodesActor)
			{
				this->CreateSelectedNodesRepresentation();
			}
			else
			{
				this->SelectedNodesActor->SetVisibility(1);
			}
		}
		else
		{
			if (this->SelectedNodesActor)
			{
				this->SelectedNodesActor->SetVisibility(0);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
double* ContourRepresentationGlyph::GetBounds() const
{
	return this->Lines->GetPoints() ? this->Lines->GetPoints()->GetBounds() : nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::CreateSelectedNodesRepresentation()
{
	auto sphere = vtkSphereSource::New();
	sphere->SetThetaResolution(12);
	sphere->SetRadius(0.3);
	this->SelectedNodesCursorShape = sphere->GetOutput();
	this->SelectedNodesCursorShape->Register(this);
	sphere->Delete();

	// Represent the position of the cursor
	this->SelectedNodesPoints = vtkPoints::New();
	this->SelectedNodesPoints->SetNumberOfPoints(100);

	auto normals = vtkDoubleArray::New();
	normals->SetNumberOfComponents(3);
	normals->SetNumberOfTuples(100);
	normals->SetNumberOfTuples(1);
	double n[3]{ 0, 0, 0 };
	normals->SetTuple(0, n);

	this->SelectedNodesData = vtkSmartPointer<vtkPolyData>::New();
	this->SelectedNodesData->SetPoints(this->SelectedNodesPoints);
	this->SelectedNodesData->GetPointData()->SetNormals(normals);
	normals->Delete();

	this->SelectedNodesGlypher = vtkSmartPointer<vtkGlyph3D>::New();
	this->SelectedNodesGlypher->SetInputData(this->SelectedNodesData);
	this->SelectedNodesGlypher->SetVectorModeToUseNormal();
	this->SelectedNodesGlypher->OrientOn();
	this->SelectedNodesGlypher->ScalingOn();
	this->SelectedNodesGlypher->SetScaleModeToDataScalingOff();
	this->SelectedNodesGlypher->SetScaleFactor(1.0);
	this->SelectedNodesGlypher->SetSourceData(this->SelectedNodesCursorShape);

	this->SelectedNodesMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
	this->SelectedNodesMapper->SetInputData(this->SelectedNodesGlypher->GetOutput());
	this->SelectedNodesMapper->SetResolveCoincidentTopologyToPolygonOffset();
	this->SelectedNodesMapper->ScalarVisibilityOff();
	this->SelectedNodesMapper->ImmediateModeRenderingOn();

	auto selectionProperty = vtkProperty::New();
	selectionProperty->SetColor(0.0, 1.0, 0.0);
	selectionProperty->SetLineWidth(0.5);
	selectionProperty->SetPointSize(3);

	this->SelectedNodesActor = vtkSmartPointer<vtkActor>::New();
	this->SelectedNodesActor->SetMapper(this->SelectedNodesMapper);
	this->SelectedNodesActor->SetProperty(selectionProperty);
	selectionProperty->Delete();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentationGlyph::PrintSelf(ostream& os, vtkIndent indent)
{
	//Superclass typedef defined in vtkTypeMacro() found in vtkSetGet.h
	this->Superclass::PrintSelf(os, indent);

	os << indent << "Always On Top: " << (this->AlwaysOnTop ? "On\n" : "Off\n");
	os << indent << "ShowSelectedNodes: " << this->ShowSelectedNodes << endl;

	if (this->Property)
		os << indent << "Property: " << this->Property << "\n";
	else
		os << indent << "Property: (none)\n";

	if (this->ActiveProperty)
		os << indent << "Active Property: " << this->ActiveProperty << "\n";
	else
		os << indent << "Active Property: (none)\n";

	if (this->LinesProperty)
		os << indent << "Lines Property: " << this->LinesProperty << "\n";
	else
		os << indent << "Lines Property: (none)\n";
}
