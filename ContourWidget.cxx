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
#include "Selection.h"

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
#include <QPixmap>

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
	this->ContinuousActive = 0;
	this->Orientation = 0;

	this->CreateDefaultRepresentation();

	// These are the event callbacks supported by this widget
	this->CallbackMapper->SetCallbackMethod(vtkCommand::LeftButtonPressEvent, vtkWidgetEvent::Select, this, ContourWidget::SelectAction);
	this->CallbackMapper->SetCallbackMethod(vtkCommand::LeftButtonReleaseEvent, vtkWidgetEvent::EndSelect, this, ContourWidget::EndSelectAction);
	this->CallbackMapper->SetCallbackMethod(vtkCommand::RightButtonPressEvent, vtkWidgetEvent::AddFinalPoint, this, ContourWidget::AddFinalPointAction);
	this->CallbackMapper->SetCallbackMethod(vtkCommand::MouseMoveEvent, vtkWidgetEvent::Move, this, ContourWidget::MoveAction);
	this->CallbackMapper->SetCallbackMethod(vtkCommand::KeyPressEvent, vtkWidgetEvent::ModifyEvent, this, ContourWidget::KeyPressAction);
	this->CallbackMapper->SetCallbackMethod(vtkCommand::KeyReleaseEvent, vtkWidgetEvent::ModifyEvent, this, ContourWidget::KeyPressAction);

	QPixmap crossMinusPixmap, crossPlusPixmap;
	crossMinusPixmap.load(":newPrefix/icons/cross-minus.png", "PNG", Qt::ColorOnly);
	crossPlusPixmap.load(":newPrefix/icons/cross-plus.png", "PNG", Qt::ColorOnly);

	this->crossMinusCursor = QCursor(crossMinusPixmap, -1, -1);
	this->crossPlusCursor = QCursor(crossPlusPixmap, -1, -1);
}

ContourWidget::~ContourWidget()
{
	// restore the pointer if the widget has changed it
	if (this->ManagesCursor == true)
		QApplication::restoreOverrideCursor();
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

	Qt::KeyboardModifiers pressedKeys = QApplication::keyboardModifiers();

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

			// check if the cursor crosses the actual contour, if so then close the contour
			// and end defining
			if (rep->CheckAndCutContourIntersection())
			{
				// last check
				int numNodes = rep->GetNumberOfNodes();
				if (rep->CheckNodesForDuplicates(numNodes-1, numNodes-2))
					rep->DeleteNthNode(numNodes-2);

				if (self->ContinuousDraw)
					self->ContinuousActive = 0;

				// set the closed loop now
				rep->ClosedLoopOn();

				self->WidgetState = ContourWidget::Manipulate;
				self->EventCallbackCommand->SetAbortFlag(1);
				self->InvokeEvent(vtkCommand::EndInteractionEvent, NULL);
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
			// NOTE: the 'reset' action is in ContourWidget::KeyPressAction() as it happens when
			//       the user presses the backspace or delete key
			if (pressedKeys & Qt::ShiftModifier)
			{
				self->DeleteAction(w);
				state = self->WidgetRep->GetInteractionState();
				self->SetCursor(state);
				break;
			}

			if (self->WidgetRep->GetInteractionState() == ContourRepresentation::Inside)
			{
				self->TranslateContourAction(w);
				break;
			}

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

	if (self->WidgetState == ContourWidget::Manipulate)
	{
		// pass the event forward
		vtkInteractorStyle *style = reinterpret_cast<vtkInteractorStyle*>(self->GetInteractor()->GetInteractorStyle());
		style->OnRightButtonDown();
		return;
	}

	int numnodes = rep->GetNumberOfNodes();

	// the last node is the cursor so we need to check if there are really that amount of unique points in the representation
	if (rep->CheckNodesForDuplicates(numnodes-1, numnodes-2))
		numnodes--;

	if (numnodes < 3)
		return;

	if ((self->WidgetState != ContourWidget::Manipulate) && (rep->GetNumberOfNodes() >= 1))
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

		// set the closed loop now
		rep->ClosedLoopOn();

		self->WidgetState = ContourWidget::Manipulate;
		self->EventCallbackCommand->SetAbortFlag(1);
		self->InvokeEvent(vtkCommand::EndInteractionEvent, NULL);

		int X = self->Interactor->GetEventPosition()[0];
		int Y = self->Interactor->GetEventPosition()[1];

		self->WidgetRep->ComputeInteractionState(X, Y);
		int state = self->WidgetRep->GetInteractionState();
		self->SetCursor(state);
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

	// If the rep already has at least 2 nodes, check how close we are to the first
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
			// yes - we have made a loop. Stop defining and switch to manipulate mode
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
		// do not allow less than three nodes, i don't want to use the old (original vtkContourWidget)
		// solution of opening the contour if we have less than three nodes and put the widget into
		// ::Define state if we have just one node
		if (rep->GetNumberOfNodes() <= 3)
			return;

		int X = self->Interactor->GetEventPosition()[0];
		int Y = self->Interactor->GetEventPosition()[1];
		rep->ActivateNode(X, Y);
		if (rep->DeleteActiveNode())
			self->InvokeEvent(vtkCommand::InteractionEvent, NULL);

		rep->ActivateNode(X, Y);
	}

	int X = self->Interactor->GetEventPosition()[0];
	int Y = self->Interactor->GetEventPosition()[1];

	self->WidgetRep->ComputeInteractionState(X, Y);
	int state = self->WidgetRep->GetInteractionState();
	self->SetCursor(state);

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

	if (self->WidgetState == ContourWidget::Start)
		return;

	self->WidgetRep->ComputeInteractionState(X, Y);
	int state = self->WidgetRep->GetInteractionState();
	self->SetCursor(state);

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

								// set the closed loop now
								rep->ClosedLoopOn();

								self->WidgetState = ContourWidget::Manipulate;
								self->EventCallbackCommand->SetAbortFlag(1);
								self->InvokeEvent(vtkCommand::EndInteractionEvent, NULL);
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

	int X = self->Interactor->GetEventPosition()[0];
	int Y = self->Interactor->GetEventPosition()[1];

	self->WidgetRep->ComputeInteractionState(X, Y);
	int state = self->WidgetRep->GetInteractionState();
	self->SetCursor(state);
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
	// cursor will only change in manipulate or start mode only
	if ((this->WidgetState != ContourWidget::Manipulate) && (this->WidgetState != ContourWidget::Start))
		return;

	// using vtk keypress/keyrelease events is useless when the interactor loses it's focus
	Qt::KeyboardModifiers pressedKeys = QApplication::keyboardModifiers();

	if (!this->ManagesCursor && cState != ContourRepresentation::Outside)
	{
		this->ManagesCursor = true;
		QApplication::setOverrideCursor(Qt::CrossCursor);
	}

	switch (cState)
	{
		case ContourRepresentation::Nearby:
		case ContourRepresentation::NearPoint:
			if (pressedKeys & Qt::ShiftModifier)
				QApplication::changeOverrideCursor(crossMinusCursor);
			else
				QApplication::changeOverrideCursor(Qt::PointingHandCursor);
			break;
		case ContourRepresentation::NearContour:
			if (pressedKeys & Qt::ShiftModifier)
				QApplication::changeOverrideCursor(Qt::CrossCursor);
			else
				QApplication::changeOverrideCursor(crossPlusCursor);
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

// NOTE: keypress/keyrelease vtk events are mostly useless. when the interactor loses focus and then user
// 		 presses the keys we are watching outside the interactor it really fucks the widget state as it
//		 still has the keyboard focus. the solution is a mixed vtk/qt key events watching and using a
//		 Qt event filter to the classes that use the contour widget (SliceVisualization) to take away or
//		 put the keyboard focus in the interactor associated with the widgets when the mouse leaves or
//		 enters those widgets.
void ContourWidget::KeyPressAction(vtkAbstractWidget *w)
{
	ContourWidget *self = reinterpret_cast<ContourWidget*>(w);

	std::string key = std::string(self->Interactor->GetKeySym());

	if (("Delete" == key) || ("BackSpace" == key))
	{
		self->EnabledOff();
		self->ResetAction(w);
		self->InvokeEvent(vtkCommand::InteractionEvent, NULL);
		self->SetCursor(ContourRepresentation::Outside);
		self->EnabledOn();
		return;
	}

	int X = self->Interactor->GetEventPosition()[0];
	int Y = self->Interactor->GetEventPosition()[1];

	self->WidgetRep->ComputeInteractionState(X, Y);
	int state = self->WidgetRep->GetInteractionState();
	self->SetCursor(state);
}
