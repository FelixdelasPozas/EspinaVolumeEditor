///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: ContourWidget.cxx
// Purpose: Manages lasso and polygon selection interaction
// Notes: Modified from vtkContourWidget class in vtk 5.8
///////////////////////////////////////////////////////////////////////////////////////////////////

// project includes
#include "ContourWidget.h"
#include "ContourRepresentation.h"
#include "ContourRepresentationGlyph.h"

// vtk includes
#include <vtkCommand.h>
#include <vtkCallbackCommand.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkObjectFactory.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkWidgetCallbackMapper.h>
#include <vtkSphereSource.h>
#include <vtkProperty.h>
#include <vtkProperty2D.h>
#include <vtkEvent.h>
#include <vtkWidgetEvent.h>
#include <vtkPolyData.h>
#include <vtkInteractorStyle.h>
#include <vtkInteractorStyleImage.h>

// qt includes
#include <QApplication>

///////////////////////////////////////////////////////////////////////////////////////////////////
// ContourWidget class
//
vtkStandardNewMacro(ContourWidget);

ContourWidget::ContourWidget()
{
	this->ManagesCursor = 0;
	this->WidgetState = ContourWidget::Start;
	this->CurrentHandle = 0;
	this->AllowNodePicking = 0;
	this->FollowCursor = 0;
	this->ContinuousDraw = 0;
	this->ContinuousActive = 1;
	this->InteractionType = Unspecified;

	this->CreateDefaultRepresentation();

	// These are the event callbacks supported by this widget
	this->CallbackMapper->SetCallbackMethod(vtkCommand::LeftButtonPressEvent, vtkWidgetEvent::Select, this, ContourWidget::SelectAction);
	this->CallbackMapper->SetCallbackMethod(vtkCommand::LeftButtonReleaseEvent, vtkWidgetEvent::EndSelect, this, ContourWidget::EndSelectAction);
	this->CallbackMapper->SetCallbackMethod(vtkCommand::RightButtonPressEvent, vtkWidgetEvent::AddFinalPoint, this, ContourWidget::AddFinalPointAction);
	this->CallbackMapper->SetCallbackMethod(vtkCommand::MouseMoveEvent, vtkWidgetEvent::Move, this, ContourWidget::MoveAction);
}

ContourWidget::~ContourWidget()
{
}

void ContourWidget::CreateDefaultRepresentation()
{
	if (!this->WidgetRep)
	{
		ContourRepresentationGlyph *rep = ContourRepresentationGlyph::New();

		this->WidgetRep = rep;

		vtkSphereSource *ss = vtkSphereSource::New();
		ss->SetRadius(0.5);
		rep->SetActiveCursorShape(ss->GetOutput());
		ss->Delete();

		rep->GetProperty()->SetColor(0.25, 1.0, 0.25);

		vtkProperty *property = vtkProperty::SafeDownCast(rep->GetActiveProperty());
		if (property)
		{
			property->SetRepresentationToSurface();
			property->SetAmbient(0.1);
			property->SetDiffuse(0.9);
			property->SetSpecular(0.0);
		}
	}
}

void ContourWidget::CloseLoop()
{
	ContourRepresentation *rep = reinterpret_cast<ContourRepresentation*>(this->WidgetRep);
	if (!rep->GetClosedLoop() && rep->GetNumberOfNodes() > 1)
	{
		this->WidgetState = ContourWidget::Manipulate;
		rep->ClosedLoopOn();
		this->Render();
	}
}

void ContourWidget::SetEnabled(int enabling)
{
	// The handle widgets are not actually enabled until they are placed.
	// The handle widgets take their representation from the ContourRepresentation.
	if (enabling)
	{
		if (this->WidgetState == ContourWidget::Start)
			reinterpret_cast<ContourRepresentation*>(this->WidgetRep)->VisibilityOff();
		else
			reinterpret_cast<ContourRepresentation*>(this->WidgetRep)->VisibilityOn();
	}

	this->Superclass::SetEnabled(enabling);
}

// The following methods are the callbacks that the contour widget responds to.
void ContourWidget::SelectAction(vtkAbstractWidget *w)
{
	ContourWidget *self = reinterpret_cast<ContourWidget*>(w);
	ContourRepresentation *rep = reinterpret_cast<ContourRepresentation*>(self->WidgetRep);

	// interactor keypress callbacks don't work, use qapplication keyboard modifiers, only delete and reset
	Qt::KeyboardModifiers pressedKeys = QApplication::keyboardModifiers();

	if (rep->GetRepresentationType() != ContourRepresentation::SecondaryRepresentation)
	{
		if (pressedKeys & Qt::ControlModifier)
		{
			self->ResetAction(w);
			return;
		}

		if (pressedKeys & Qt::ShiftModifier)
		{
			self->DeleteAction(w);
			return;
		}
	}
	else
		self->WidgetState = ContourWidget::Manipulate;

	int X = self->Interactor->GetEventPosition()[0];
	int Y = self->Interactor->GetEventPosition()[1];

	self->WidgetRep->ComputeInteractionState(X, Y);
	int state = self->WidgetRep->GetInteractionState();
	self->SetCursor(state);

	double pos[2];
	pos[0] = X;
	pos[1] = Y;

	if (self->ContinuousDraw)
		self->ContinuousActive = 0;

	switch (self->WidgetState)
	{
		case ContourWidget::Start:
		case ContourWidget::Define:
		{
			// If we are following the cursor, let's add 2 nodes rightaway, on the
			// first click. The second node is the one that follows the cursor
			// around.
			if ((self->FollowCursor || self->ContinuousDraw) && (rep->GetNumberOfNodes() == 0))
				self->AddNode();

			self->AddNode();

			// check if the cursor crosses the actual contour, if so then close the contour and end defining
			if (rep->CheckAndCutContourIntersection())
			{
				// last check
				int numNodes = rep->GetNumberOfNodes();
				if (rep->CheckNodesForDuplicates(numNodes-1, numNodes-2))
					rep->DeleteNthNode(numNodes-2);

				if (self->ContinuousDraw)
					self->ContinuousActive = 0;

				self->WidgetState = ContourWidget::Manipulate;
				self->EventCallbackCommand->SetAbortFlag(1);
				self->InvokeEvent(vtkCommand::EndInteractionEvent, NULL);

				// set the closed loop now
				rep->ClosedLoopOn();
			}
			else
			{
				if (self->ContinuousDraw)
					self->ContinuousActive = 1;
			}

			break;
		}

		case ContourWidget::Manipulate:
		{
			if (self->WidgetRep->GetInteractionState() == ContourRepresentation::Inside)
			{
				self->TranslateContourAction(w);
				break;
			}

			if (rep->GetRepresentationType() == ContourRepresentation::MainRepresentation)
			{
				if (rep->ActivateNode(X, Y))
				{
					self->Superclass::StartInteraction();
					self->InvokeEvent(vtkCommand::StartInteractionEvent, NULL);
					self->StartInteraction();
					rep->SetCurrentOperationToTranslate();
					rep->StartWidgetInteraction(pos);
					self->EventCallbackCommand->SetAbortFlag(1);
				}
				else
					if (rep->AddNodeOnContour(X, Y))
					{
						if (rep->ActivateNode(X, Y))
						{
							rep->SetCurrentOperationToTranslate();
							rep->StartWidgetInteraction(pos);
						}
						self->EventCallbackCommand->SetAbortFlag(1);
					}
					else
						if (!rep->GetNeedToRender())
							rep->SetRebuildLocator(true);
			}
			break;
		}
	}

	if (rep->GetNeedToRender())
	{
		self->Render();
		rep->NeedToRenderOff();
	}
}

void ContourWidget::AddFinalPointAction(vtkAbstractWidget *w)
{
	ContourWidget *self = reinterpret_cast<ContourWidget*>(w);
	ContourRepresentation *rep = reinterpret_cast<ContourRepresentation*>(self->WidgetRep);

	// pass the event forward
	if (rep->GetRepresentationType() == ContourRepresentation::SecondaryRepresentation)
	{
		vtkInteractorStyle *style = reinterpret_cast<vtkInteractorStyle*>(self->GetInteractor()->GetInteractorStyle());
		style->OnRightButtonDown();
		return;
	}

	if (self->WidgetState != ContourWidget::Manipulate && rep->GetNumberOfNodes() >= 1)
	{
		// In follow cursor and continuous draw mode, the "extra" node
		// has already been added for us.
		if (!self->FollowCursor && !self->ContinuousDraw)
			self->AddNode();

		// need to modify the contour if intersects with itself
		rep->CheckAndCutContourIntersectionInFinalPoint();

		// last check
		int numNodes = rep->GetNumberOfNodes();
		if (rep->CheckNodesForDuplicates(numNodes-1, numNodes-2))
			rep->DeleteNthNode(numNodes-2);

		if (self->ContinuousDraw)
			self->ContinuousActive = 0;

		self->WidgetState = ContourWidget::Manipulate;
		self->EventCallbackCommand->SetAbortFlag(1);
		self->InvokeEvent(vtkCommand::EndInteractionEvent, NULL);

		// set the closed loop now
		rep->ClosedLoopOn();
	}

	if (rep->GetNeedToRender())
	{
		self->Render();
		rep->NeedToRenderOff();
	}
}

void ContourWidget::AddNode()
{
	int X = this->Interactor->GetEventPosition()[0];
	int Y = this->Interactor->GetEventPosition()[1];

	// If the rep already has at least 2 nodes, check how close we are to
	// the first
	ContourRepresentation* rep = reinterpret_cast<ContourRepresentation*>(this->WidgetRep);

	int numNodes = rep->GetNumberOfNodes();
	if (numNodes > 1)
	{
		int pixelTolerance = rep->GetPixelTolerance();
		int pixelTolerance2 = pixelTolerance * pixelTolerance;

		double displayPos[2];
		if (!rep->GetNthNodeDisplayPosition(0, displayPos))
		{
			vtkErrorMacro("Can't get first node display position!");
			return;
		}

		// if in continous draw mode, we dont want to cose the loop until we are at least
		// numNodes > pixelTolerance away

		int distance2 = static_cast<int>((X - displayPos[0]) * (X - displayPos[0]) + (Y - displayPos[1]) * (Y - displayPos[1]));

		if ((distance2 < pixelTolerance2 && numNodes > 2) || (this->ContinuousDraw && numNodes > pixelTolerance && distance2 < pixelTolerance2))
		{
			// yes - we have made a loop. Stop defining and switch to
			// manipulate mode
			this->WidgetState = ContourWidget::Manipulate;
			rep->ClosedLoopOn();
			this->Render();
			this->EventCallbackCommand->SetAbortFlag(1);
			this->InvokeEvent(vtkCommand::EndInteractionEvent, NULL);
			return;
		}
	}

	if (rep->AddNodeAtDisplayPosition(X, Y))
	{
		if (this->WidgetState == ContourWidget::Start)
			this->InvokeEvent(vtkCommand::StartInteractionEvent, NULL);

		this->WidgetState = ContourWidget::Define;
		rep->VisibilityOn();
		this->EventCallbackCommand->SetAbortFlag(1);
		this->InvokeEvent(vtkCommand::InteractionEvent, NULL);
	}
}

// Note that if you select the contour at a location that is not moused over
// a control point, the translate action makes the closest contour node
// jump to the current mouse location. Perhaps we should either
// (a) Disable translations when not moused over a control point
// (b) Fix the jumping behaviour by calculating motion vectors from the start
//     of the interaction.
void ContourWidget::TranslateContourAction(vtkAbstractWidget *w)
{
	ContourWidget *self = reinterpret_cast<ContourWidget*>(w);

	if (self->WidgetState != ContourWidget::Manipulate)
		return;

	ContourRepresentation *rep = reinterpret_cast<ContourRepresentation*>(self->WidgetRep);

	int X = self->Interactor->GetEventPosition()[0];
	int Y = self->Interactor->GetEventPosition()[1];
	double pos[2];
	pos[0] = X;
	pos[1] = Y;

	self->Superclass::StartInteraction();
	self->InvokeEvent(vtkCommand::StartInteractionEvent, NULL);
	self->StartInteraction();
	rep->SetCurrentOperationToShift(); // Here
	rep->StartWidgetInteraction(pos);
	self->EventCallbackCommand->SetAbortFlag(1);

	if (rep->GetNeedToRender())
	{
		self->Render();
		rep->NeedToRenderOff();
	}
}

// Note that if you select the contour at a location that is not moused over
// a control point, the scale action makes the closest contour node
// jump to the current mouse location. Perhaps we should either
// (a) Disable scaling when not moused over a control point
// (b) Fix the jumping behaviour by calculating motion vectors from the start
//     of the interaction.
void ContourWidget::ScaleContourAction(vtkAbstractWidget *w)
{
	ContourWidget *self = reinterpret_cast<ContourWidget*>(w);

	if (self->WidgetState != ContourWidget::Manipulate)
		return;

	ContourRepresentation *rep = reinterpret_cast<ContourRepresentation*>(self->WidgetRep);

	int X = self->Interactor->GetEventPosition()[0];
	int Y = self->Interactor->GetEventPosition()[1];
	double pos[2];
	pos[0] = X;
	pos[1] = Y;

	if (rep->ActivateNode(X, Y))
	{
		self->Superclass::StartInteraction();
		self->InvokeEvent(vtkCommand::StartInteractionEvent, NULL);
		self->StartInteraction();
		rep->SetCurrentOperationToScale(); // Here
		rep->StartWidgetInteraction(pos);
		self->EventCallbackCommand->SetAbortFlag(1);
	}
	else
	{
		double p[3];
		int idx;
		if (rep->FindClosestPointOnContour(X, Y, p, &idx))
		{
			rep->GetNthNodeDisplayPosition(idx, pos);
			rep->ActivateNode(pos);
			self->Superclass::StartInteraction();
			self->InvokeEvent(vtkCommand::StartInteractionEvent, NULL);
			self->StartInteraction();
			rep->SetCurrentOperationToScale(); // Here
			rep->StartWidgetInteraction(pos);
			self->EventCallbackCommand->SetAbortFlag(1);
		}
	}

	if (rep->GetNeedToRender())
	{
		self->Render();
		rep->NeedToRenderOff();
	}
}

void ContourWidget::DeleteAction(vtkAbstractWidget *w)
{
	ContourWidget *self = reinterpret_cast<ContourWidget*>(w);

	if (self->WidgetState == ContourWidget::Start)
		return;

	ContourRepresentation *rep = reinterpret_cast<ContourRepresentation*>(self->WidgetRep);

	if (self->WidgetState == ContourWidget::Define)
	{
		if (rep->DeleteLastNode())
			self->InvokeEvent(vtkCommand::InteractionEvent, NULL);
	}
	else
	{
		int X = self->Interactor->GetEventPosition()[0];
		int Y = self->Interactor->GetEventPosition()[1];
		rep->ActivateNode(X, Y);
		if (rep->DeleteActiveNode())
			self->InvokeEvent(vtkCommand::InteractionEvent, NULL);

		rep->ActivateNode(X, Y);
		int numNodes = rep->GetNumberOfNodes();
		if (numNodes < 3)
		{
			rep->ClosedLoopOff();
			if (numNodes < 2)
				self->WidgetState = ContourWidget::Define;
		}
	}

	if (rep->GetNeedToRender())
	{
		self->Render();
		rep->NeedToRenderOff();
	}
}

void ContourWidget::MoveAction(vtkAbstractWidget *w)
{
	ContourWidget *self = reinterpret_cast<ContourWidget*>(w);

	int X = self->Interactor->GetEventPosition()[0];
	int Y = self->Interactor->GetEventPosition()[1];
	ContourRepresentation *rep = reinterpret_cast<ContourRepresentation*>(self->WidgetRep);

	self->WidgetRep->ComputeInteractionState(X, Y);
	int state = self->WidgetRep->GetInteractionState();
	self->SetCursor(state);

	if (self->WidgetState == ContourWidget::Start)
		return;

	if (self->WidgetState == ContourWidget::Define)
	{
		if (self->FollowCursor || self->ContinuousDraw)
		{
			// Have the last node follow the mouse in this case...
			const int numNodes = rep->GetNumberOfNodes();

			// First check if the last node is near the first node, if so, we intend closing the loop.
			if (numNodes > 1)
			{
				double displayPos[2];
				int pixelTolerance = rep->GetPixelTolerance();
				int pixelTolerance2 = pixelTolerance * pixelTolerance;

				rep->GetNthNodeDisplayPosition(0, displayPos);

				int distance2 = static_cast<int>((X - displayPos[0]) * (X - displayPos[0]) + (Y - displayPos[1]) * (Y - displayPos[1]));

				const bool mustCloseLoop = (distance2 < pixelTolerance2 && numNodes > 2) || (self->ContinuousDraw && numNodes > pixelTolerance && distance2 < pixelTolerance2);

				if (mustCloseLoop != (rep->GetClosedLoop() == 1))
				{
					if (rep->GetClosedLoop())
					{
						// We need to open the closed loop. We do this by adding a node at (X,Y). If by chance the
						// point placer says that (X,Y) is invalid, we'll add it at the location of the first control point (which we know is valid).
						if (!rep->AddNodeAtDisplayPosition(X, Y))
						{
							double closedLoopPoint[3];
							rep->GetNthNodeWorldPosition(0, closedLoopPoint);
							rep->AddNodeAtDisplayPosition(closedLoopPoint);
						}
						rep->ClosedLoopOff();
					}
					else
					{
						// We need to close the open loop. Delete the node that's following the mouse cursor and close the loop between the previous node
						// and the first node.
						rep->DeleteLastNode();
						rep->ClosedLoopOn();
					}
				}
				else
					if (rep->GetClosedLoop() == 0)
					{
						if (self->ContinuousDraw && self->ContinuousActive)
						{
							rep->AddNodeAtDisplayPosition(X, Y);
							self->InvokeEvent(vtkCommand::InteractionEvent, NULL);

							// check the contour detect if it intersects with itself
							if (rep->CheckAndCutContourIntersection())
							{
								// last check
								int numNodes = rep->GetNumberOfNodes();
								if (rep->CheckNodesForDuplicates(numNodes-1, numNodes-2))
									rep->DeleteNthNode(numNodes-2);

								if (self->ContinuousDraw)
									self->ContinuousActive = 0;

								self->WidgetState = ContourWidget::Manipulate;
								self->EventCallbackCommand->SetAbortFlag(1);
								self->InvokeEvent(vtkCommand::EndInteractionEvent, NULL);

								// set the closed loop now
								rep->ClosedLoopOn();
								return;
							}
						}
						else
						{
							// If we aren't changing the loop topology, simply update the position of the latest node to follow the mouse cursor position (X,Y).
							rep->SetNthNodeDisplayPosition(numNodes - 1, X, Y);
							self->InvokeEvent(vtkCommand::InteractionEvent, NULL);
						}
					}
			}
		}
		else
			return;
	}

	if (rep->GetCurrentOperation() == ContourRepresentation::Inactive)
	{
		rep->ComputeInteractionState(X, Y);
		rep->ActivateNode(X, Y);
	}
	else
	{
		double pos[2];
		pos[0] = X;
		pos[1] = Y;
		self->WidgetRep->WidgetInteraction(pos);
		self->InvokeEvent(vtkCommand::InteractionEvent, NULL);
	}

	if (self->WidgetRep->GetNeedToRender())
	{
		self->Render();
		self->WidgetRep->NeedToRenderOff();
	}
}

void ContourWidget::EndSelectAction(vtkAbstractWidget *w)
{
	ContourWidget *self = reinterpret_cast<ContourWidget*>(w);
	ContourRepresentation *rep = reinterpret_cast<ContourRepresentation*>(self->WidgetRep);

	if (self->ContinuousDraw)
		self->ContinuousActive = 0;

	// Do nothing if inactive
	if (rep->GetCurrentOperation() == ContourRepresentation::Inactive)
	{
		rep->SetRebuildLocator(true);
		return;
	}

	// place points in it's final destination if we're shifting the contour
	if (rep->GetCurrentOperation() == ContourRepresentation::Shift)
		rep->PlaceFinalPoints();

	rep->SetCurrentOperationToInactive();
	self->EventCallbackCommand->SetAbortFlag(1);
	self->Superclass::EndInteraction();
	self->InvokeEvent(vtkCommand::EndInteractionEvent, NULL);

	// Node picking
	if (self->AllowNodePicking && self->Interactor->GetControlKey() && self->WidgetState == ContourWidget::Manipulate)
		rep->ToggleActiveNodeSelected();

	if (self->WidgetRep->GetNeedToRender())
	{
		self->Render();
		self->WidgetRep->NeedToRenderOff();
	}
}

void ContourWidget::ResetAction(vtkAbstractWidget *w)
{
	ContourWidget *self = reinterpret_cast<ContourWidget*>(w);
	self->Initialize(NULL);
}

void ContourWidget::Initialize(vtkPolyData * pd, int state)
{
	if (!this->GetEnabled())
		vtkErrorMacro(<<"Enable widget before initializing");

	if (this->WidgetRep)
	{
		ContourRepresentation *rep = reinterpret_cast<ContourRepresentation*>(this->WidgetRep);

		if (pd == NULL)
		{
			while (rep->DeleteLastNode())
			{
				;
			}
			rep->ClosedLoopOff();
			this->Render();
			rep->NeedToRenderOff();
			rep->VisibilityOff();
			this->WidgetState = ContourWidget::Start;
		}
		else
		{
			rep->Initialize(pd);
			this->WidgetState = (rep->GetClosedLoop() || state == 1) ? ContourWidget::Manipulate : ContourWidget::Define;
		}
	}
}

void ContourWidget::SetAllowNodePicking(int val)
{
	if (this->AllowNodePicking == val)
		return;

	this->AllowNodePicking = val;
	if (this->AllowNodePicking)
	{
		ContourRepresentation *rep = reinterpret_cast<ContourRepresentation*>(this->WidgetRep);
		rep->SetShowSelectedNodes(this->AllowNodePicking);
	}
}

void ContourWidget::PrintSelf(ostream& os, vtkIndent indent)
{
	//Superclass typedef defined in vtkTypeMacro() found in vtkSetGet.h
	this->Superclass::PrintSelf(os, indent);

	os << indent << "WidgetState: " << this->WidgetState << endl;
	os << indent << "CurrentHandle: " << this->CurrentHandle << endl;
	os << indent << "AllowNodePicking: " << this->AllowNodePicking << endl;
	os << indent << "FollowCursor: " << (this->FollowCursor ? "On" : "Off") << endl;
	os << indent << "ContinuousDraw: " << (this->ContinuousDraw ? "On" : "Off") << endl;
}

void ContourWidget::SetCursor(int cState)
{
	if (!this->ManagesCursor && cState != ContourRepresentation::Outside)
	{
		this->ManagesCursor = true;
		QApplication::setOverrideCursor(Qt::CrossCursor);
	}

	switch (cState)
	{
		case ContourRepresentation::Nearby:
			this->RequestCursorShape(VTK_CURSOR_HAND);
			break;
		case ContourRepresentation::Inside:
			this->RequestCursorShape(VTK_CURSOR_SIZEALL);
			break;
		default:
			if (this->ManagesCursor)
			{
				this->ManagesCursor = false;
				QApplication::restoreOverrideCursor();
			}
			break;
	}
}

void ContourWidget::SetWidgetInteractionType(ContourWidget::WidgetInteractionType type)
{
	if (this->InteractionType != Unspecified)
		return;

	this->InteractionType = type;

	if (this->InteractionType == Secondary)
	{
		if (this->ContinuousDraw)
			this->ContinuousActive = 0;

		this->WidgetState = ContourWidget::Manipulate;
		this->EventCallbackCommand->SetAbortFlag(1);
		this->InvokeEvent(vtkCommand::EndInteractionEvent, NULL);

		// set the closed loop now
		ContourRepresentation *rep = reinterpret_cast<ContourRepresentation*>(this->WidgetRep);
		rep->ClosedLoopOn();
		rep->SetVisibility(true);
	}
}

ContourWidget::WidgetInteractionType ContourWidget::GetWidgetInteractionType(void)
{
	return this->InteractionType;
}
