///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: ContourWidget.cpp
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

///////////////////////////////////////////////////////////////////////////////////////////////////
ContourWidget::ContourWidget()
: vtkAbstractWidget()
, WidgetState     {State::Start}
, CurrentHandle   {0}
, AllowNodePicking{0}
, FollowCursor    {0}
, ContinuousDraw  {0}
, ContinuousActive{0}
, Orientation     {0}
{
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

///////////////////////////////////////////////////////////////////////////////////////////////////
ContourWidget::~ContourWidget()
{
	if (this->ManagesCursor)
	{
		QApplication::restoreOverrideCursor();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourWidget::CreateDefaultRepresentation()
{
	if (!this->WidgetRep)
	{
		auto rep = ContourRepresentationGlyph::New();

		this->WidgetRep = rep;

		auto sphereSource = vtkSphereSource::New();
		sphereSource->SetRadius(0.5);
		rep->SetActiveCursorShape(sphereSource->GetOutput());
		sphereSource->Delete();

		rep->GetProperty()->SetColor(0.25, 1.0, 0.25);

		auto property = vtkProperty::SafeDownCast(rep->GetActiveProperty());
		if (property)
		{
			property->SetRepresentationToSurface();
			property->SetAmbient(0.1);
			property->SetDiffuse(0.9);
			property->SetSpecular(0.0);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourWidget::CloseLoop()
{
	auto rep = dynamic_cast<ContourRepresentation*>(this->WidgetRep);
	if(rep && !rep->GetClosedLoop() && rep->GetNumberOfNodes() > 2)
  {
    this->WidgetState = Manipulate;
    rep->ClosedLoopOn();
    this->Render();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourWidget::SetEnabled(int enabling)
{
	// The handle widgets are not actually enabled until they are placed.
	// The handle widgets take their representation from the ContourRepresentation.
  if(this->WidgetRep)
  {
    if (enabling)
    {
      if (this->WidgetState == Start)
      {
        this->WidgetRep->VisibilityOff();
      }
      else
      {
        this->WidgetRep->VisibilityOn();
      }
    }
  }

	this->Superclass::SetEnabled(enabling);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourWidget::SelectAction(vtkAbstractWidget *widget)
{
	auto self = dynamic_cast<ContourWidget*>(widget);
	auto rep = dynamic_cast<ContourRepresentation*>(self->WidgetRep);
	Q_ASSERT(self && rep);

	auto pressedKeys = QApplication::keyboardModifiers();

	auto X = self->Interactor->GetEventPosition()[0];
	auto Y = self->Interactor->GetEventPosition()[1];

	self->WidgetRep->ComputeInteractionState(X, Y);
	auto state = rep->GetInteractionState();
	self->SetCursor(state);

	double pos[2]{static_cast<double>(X), static_cast<double>(Y)};

	if (self->ContinuousDraw)
	{
		self->ContinuousActive = 0;
	}

	switch (self->WidgetState)
	{
		case Start:
		case Define:
		{
			// If we are following the cursor, let's add 2 nodes rightaway, on the
			// first click. The second node is the one that follows the cursor
			// around.
			if ((self->FollowCursor || self->ContinuousDraw) && (rep->GetNumberOfNodes() == 0))
			{
				self->AddNode();
			}

			self->AddNode();

			// check if the cursor crosses the actual contour, if so then close the contour
			// and end defining
			if (rep->CheckAndCutContourIntersection())
			{
				// last check
				int numNodes = rep->GetNumberOfNodes();
				if (rep->CheckNodesForDuplicates(numNodes-1, numNodes-2))
				{
					rep->DeleteNthNode(numNodes-2);
				}

				if (self->ContinuousDraw)
				{
					self->ContinuousActive = 0;
				}

				// set the closed loop now
				rep->ClosedLoopOn();

				self->WidgetState = Manipulate;
				self->EventCallbackCommand->SetAbortFlag(1);
				self->InvokeEvent(vtkCommand::EndInteractionEvent, nullptr);
			}
			else
			{
				if (self->ContinuousDraw)
				{
					self->ContinuousActive = 1;
				}
			}

			break;
		}

		case Manipulate:
		{
			// NOTE: the 'reset' action is in ContourWidget::KeyPressAction() as it happens when
			//       the user presses the backspace or delete key
			if (pressedKeys & Qt::ShiftModifier)
			{
				self->DeleteAction(widget);
				state = rep->GetInteractionState();
				self->SetCursor(state);
				break;
			}

			if (rep->GetInteractionState() == ContourRepresentation::Inside)
			{
				self->TranslateContourAction(widget);
				break;
			}

			if (rep->ActivateNode(X, Y))
			{
				self->Superclass::StartInteraction();
				self->InvokeEvent(vtkCommand::StartInteractionEvent, nullptr);
				self->StartInteraction();
				rep->SetCurrentOperationToTranslate();
				rep->StartWidgetInteraction(pos);
				self->EventCallbackCommand->SetAbortFlag(1);
			}
			else
			{
				if (rep->AddNodeOnContour(X, Y))
				{
					if (rep->ActivateNode(X, Y))
					{
						rep->SetCurrentOperationToTranslate();
						rep->StartWidgetInteraction(pos);
					}
					self->EventCallbackCommand->SetAbortFlag(1);
				}
			}
			break;
		}
		default:
		  break;
	}

	if (rep->GetNeedToRender())
	{
		self->Render();
		rep->NeedToRenderOff();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourWidget::AddFinalPointAction(vtkAbstractWidget *widget)
{
	auto self = dynamic_cast<ContourWidget*>(widget);
	auto rep = dynamic_cast<ContourRepresentation*>(self->WidgetRep);
	Q_ASSERT(self && rep);

	if (self->WidgetState == Manipulate)
	{
		// pass the event forward
		auto style = dynamic_cast<vtkInteractorStyle*>(self->GetInteractor()->GetInteractorStyle());
		Q_ASSERT(style);
		style->OnRightButtonDown();
		return;
	}

	int numnodes = rep->GetNumberOfNodes();

	// the last node is the cursor so we need to check if there are really that amount of unique points in the representation
	if (rep->CheckNodesForDuplicates(numnodes-1, numnodes-2))
	{
	  rep->DeleteNthNode(numnodes-2);
		--numnodes;
	}

	if (numnodes < 3) return;

	if ((self->WidgetState != Manipulate) && (rep->GetNumberOfNodes() >= 1))
	{
		// In follow cursor and continuous draw mode, the "extra" node
		// has already been added for us.
		if (!self->FollowCursor && !self->ContinuousDraw)
		{
			self->AddNode();
		}

		// need to modify the contour if intersects with itself
		rep->CheckAndCutContourIntersectionInFinalPoint();

		// last check
		int numNodes = rep->GetNumberOfNodes();
		if (rep->CheckNodesForDuplicates(numNodes-1, numNodes-2))
		{
			rep->DeleteNthNode(numNodes-2);
		}

		if (self->ContinuousDraw)
		{
			self->ContinuousActive = 0;
		}

		// set the closed loop now
		rep->ClosedLoopOn();

		self->WidgetState = Manipulate;
		self->EventCallbackCommand->SetAbortFlag(1);
		self->InvokeEvent(vtkCommand::EndInteractionEvent, nullptr);

		auto X = self->Interactor->GetEventPosition()[0];
		auto Y = self->Interactor->GetEventPosition()[1];

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

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourWidget::AddNode()
{
	auto X = this->Interactor->GetEventPosition()[0];
	auto Y = this->Interactor->GetEventPosition()[1];

	// If the rep already has at least 2 nodes, check how close we are to the first
	auto rep = dynamic_cast<ContourRepresentation*>(this->WidgetRep);
	Q_ASSERT(rep);

	auto numNodes = rep->GetNumberOfNodes();
	if (numNodes > 1)
	{
		auto pixelTolerance = rep->GetPixelTolerance();
		auto pixelTolerance2 = pixelTolerance * pixelTolerance;

		int displayPos[2];
		if (!rep->GetNthNodeDisplayPosition(0, displayPos))
		{
			vtkErrorMacro("Can't get first node display position!");
			return;
		}

		auto distance2 = static_cast<int>((X - displayPos[0]) * (X - displayPos[0]) + (Y - displayPos[1]) * (Y - displayPos[1]));

		if ((distance2 < pixelTolerance2) && (numNodes > 2))
		{
			// yes - we have made a loop. Stop defining and switch to manipulate mode
			this->WidgetState = Manipulate;
			rep->ClosedLoopOn();
			this->Render();
			this->EventCallbackCommand->SetAbortFlag(1);
			this->InvokeEvent(vtkCommand::EndInteractionEvent, nullptr);
			return;
		}
	}

	if (rep->AddNodeAtDisplayPosition(X, Y))
	{
		if (this->WidgetState == Start)
		{
			this->InvokeEvent(vtkCommand::StartInteractionEvent, nullptr);
		}

		this->WidgetState = Define;
		rep->VisibilityOn();
		this->EventCallbackCommand->SetAbortFlag(1);
		this->InvokeEvent(vtkCommand::InteractionEvent, nullptr);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourWidget::TranslateContourAction(vtkAbstractWidget *widget)
{
	auto self = dynamic_cast<ContourWidget*>(widget);
	Q_ASSERT(self);

	if (self->WidgetState != Manipulate) return;

	auto rep = dynamic_cast<ContourRepresentation*>(self->WidgetRep);
	Q_ASSERT(rep);

	auto X = self->Interactor->GetEventPosition()[0];
	auto Y = self->Interactor->GetEventPosition()[1];
	double pos[2]{static_cast<double>(X), static_cast<double>(Y)};

	self->Superclass::StartInteraction();
	self->InvokeEvent(vtkCommand::StartInteractionEvent, nullptr);
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

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourWidget::DeleteAction(vtkAbstractWidget *widget)
{
	auto self = dynamic_cast<ContourWidget*>(widget);
	Q_ASSERT(self);

	if (self->WidgetState == Start)
	{
	  return;
	}

	auto rep = dynamic_cast<ContourRepresentation*>(self->WidgetRep);
	Q_ASSERT(rep);

  auto X = self->Interactor->GetEventPosition()[0];
  auto Y = self->Interactor->GetEventPosition()[1];

	if (self->WidgetState == Define)
	{
		if (rep->DeleteLastNode())
		{
			self->InvokeEvent(vtkCommand::InteractionEvent, nullptr);
		}
	}
	else
	{
		// do not allow less than three nodes, i don't want to use the old (original vtkContourWidget)
		// solution of opening the contour if we have less than three nodes and put the widget into
		// ::Define state if we have just one node
		if (rep->GetNumberOfNodes() <= 3)
		{
		  return;
		}

		rep->ActivateNode(X, Y);
		if (rep->DeleteActiveNode())
		{
			self->InvokeEvent(vtkCommand::InteractionEvent, nullptr);
		}

		rep->ActivateNode(X, Y);
	}

	self->WidgetRep->ComputeInteractionState(X, Y);
	int state = self->WidgetRep->GetInteractionState();
	self->SetCursor(state);

	if (rep->GetNeedToRender())
	{
		self->Render();
		rep->NeedToRenderOff();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourWidget::MoveAction(vtkAbstractWidget *widget)
{
	auto self = dynamic_cast<ContourWidget*>(widget);
	auto rep = dynamic_cast<ContourRepresentation*>(self->WidgetRep);
	Q_ASSERT(self && rep);

	if (self->WidgetState == Start)
	{
	  return;
	}

	auto X = self->Interactor->GetEventPosition()[0];
	auto Y = self->Interactor->GetEventPosition()[1];

	self->WidgetRep->ComputeInteractionState(X, Y);
	auto state = self->WidgetRep->GetInteractionState();
	self->SetCursor(state);

	if (self->WidgetState == Define)
	{
		if (self->FollowCursor || self->ContinuousDraw)
		{
			// Have the last node follow the mouse in this case...
			const auto numNodes = rep->GetNumberOfNodes();

			// First check if the last node is near the first node, if so, we intend closing the loop.
			if (numNodes > 1)
			{
				int displayPos[2];
				auto pixelTolerance = rep->GetPixelTolerance();
				auto pixelTolerance2 = pixelTolerance * pixelTolerance;

				rep->GetNthNodeDisplayPosition(0, displayPos);

				auto distance2 = static_cast<int>((X - displayPos[0]) * (X - displayPos[0]) + (Y - displayPos[1]) * (Y - displayPos[1]));

				const auto mustCloseLoop = (distance2 < pixelTolerance2 && numNodes > 2) || (self->ContinuousDraw && numNodes > pixelTolerance && distance2 < pixelTolerance2);

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
							rep->AddNodeAtWorldPosition(closedLoopPoint);
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
				{
					if (rep->GetClosedLoop() == 0)
					{
						if (self->ContinuousDraw && self->ContinuousActive)
						{
							rep->AddNodeAtDisplayPosition(X, Y);

							// check the contour detect if it intersects with itself
							if (rep->CheckAndCutContourIntersection())
							{
								// last check
								auto numNodes = rep->GetNumberOfNodes();
								if (rep->CheckNodesForDuplicates(numNodes-1, numNodes-2))
								{
									rep->DeleteNthNode(numNodes-2);
								}

								if (self->ContinuousDraw)
								{
									self->ContinuousActive = 0;
								}

								// set the closed loop now
								rep->ClosedLoopOn();

								self->WidgetState = Manipulate;
								self->EventCallbackCommand->SetAbortFlag(1);
								self->InvokeEvent(vtkCommand::EndInteractionEvent, nullptr);
								return;
							}
						}
						else
						{
							// If we aren't changing the loop topology, simply update the position of the latest node to follow the mouse cursor position (X,Y).
							rep->SetNthNodeDisplayPosition(numNodes - 1, X, Y);
							self->InvokeEvent(vtkCommand::InteractionEvent, nullptr);
						}
					}
				}
			}
		}
		else
		{
			return;
		}
	}

	if (rep->GetCurrentOperation() == ContourRepresentation::Inactive)
	{
		rep->ComputeInteractionState(X, Y);
		rep->ActivateNode(X, Y);
	}
	else
	{
		double pos[2]{static_cast<double>(X), static_cast<double>(Y)};
		self->WidgetRep->WidgetInteraction(pos);
		self->InvokeEvent(vtkCommand::InteractionEvent, nullptr);
	}

	if (self->WidgetRep->GetNeedToRender())
	{
		self->Render();
		self->WidgetRep->NeedToRenderOff();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourWidget::EndSelectAction(vtkAbstractWidget *widget)
{
	auto self = dynamic_cast<ContourWidget*>(widget);
	auto rep = dynamic_cast<ContourRepresentation*>(self->WidgetRep);
	Q_ASSERT(self && rep);

	if (self->ContinuousDraw)
	{
		self->ContinuousActive = 0;
	}

	// Do nothing if inactive
	if (rep->GetCurrentOperation() == ContourRepresentation::Inactive)
	{
		return;
	}

	rep->SetCurrentOperationToInactive();
	self->EventCallbackCommand->SetAbortFlag(1);
	self->Superclass::EndInteraction();
	self->InvokeEvent(vtkCommand::EndInteractionEvent, nullptr);

	// Node picking
	if (self->AllowNodePicking && self->Interactor->GetControlKey() && self->WidgetState == Manipulate)
	{
		rep->ToggleActiveNodeSelected();
	}

	if (self->WidgetRep->GetNeedToRender())
	{
		self->Render();
		self->WidgetRep->NeedToRenderOff();
	}

	auto X = self->Interactor->GetEventPosition()[0];
	auto Y = self->Interactor->GetEventPosition()[1];

	self->WidgetRep->ComputeInteractionState(X, Y);
	auto state = self->WidgetRep->GetInteractionState();
	self->SetCursor(state);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourWidget::ResetAction(vtkAbstractWidget *widget)
{
	auto self = dynamic_cast<ContourWidget*>(widget);
	Q_ASSERT(self);

	self->Initialize(nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourWidget::Initialize(vtkPolyData *polydata, const int state)
{
	if (!this->GetEnabled())
	{
		vtkErrorMacro(<<"Enable widget before initializing");
	}

	if (this->WidgetRep)
	{
		auto rep = dynamic_cast<ContourRepresentation*>(this->WidgetRep);
		Q_ASSERT(rep);

		if (!polydata)
		{
			while (rep->DeleteLastNode())
			{
				;
			}
			rep->ClosedLoopOff();
			this->Render();
			rep->NeedToRenderOff();
			rep->VisibilityOff();
			this->WidgetState = Start;
		}
		else
		{
			rep->Initialize(polydata);
			this->WidgetState = (rep->GetClosedLoop() || state == 1) ? Manipulate : Define;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourWidget::SetAllowNodePicking(int val)
{
	if (this->AllowNodePicking == val) return;

	this->AllowNodePicking = val;

	if (this->AllowNodePicking)
	{
		auto rep = dynamic_cast<ContourRepresentation*>(this->WidgetRep);
		Q_ASSERT(rep);

		rep->SetShowSelectedNodes(this->AllowNodePicking);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourWidget::SetCursor(int cState)
{
	// cursor will only change in manipulate or start mode only
	if ((this->WidgetState != Manipulate) && (this->WidgetState != Start))
	{
	  return;
	}

	// using vtk keypress/keyrelease events is useless when the interactor loses it's focus
	auto pressedKeys = QApplication::keyboardModifiers();

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
		  QApplication::changeOverrideCursor(Qt::SizeAllCursor);
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

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourWidget::KeyPressAction(vtkAbstractWidget *widget)
{
	auto self = dynamic_cast<ContourWidget*>(widget);
	Q_ASSERT(self);

	auto key = std::string(self->Interactor->GetKeySym());

	if (("Delete" == key) || ("BackSpace" == key))
	{
		self->EnabledOff();
		self->ResetAction(widget);
		self->InvokeEvent(vtkCommand::InteractionEvent, nullptr);
		self->SetCursor(ContourRepresentation::Outside);
		self->EnabledOn();
		return;
	}

	auto X = self->Interactor->GetEventPosition()[0];
	auto Y = self->Interactor->GetEventPosition()[1];

	self->WidgetRep->ComputeInteractionState(X, Y);
	int state = self->WidgetRep->GetInteractionState();
	self->SetCursor(state);
}
