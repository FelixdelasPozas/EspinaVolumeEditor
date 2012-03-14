///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: BoxSelectionWidget.cxx
// Purpose: widget that manages interaction of a box in one of the planes
// Notes: Ripped from vtkBorderWidget class
///////////////////////////////////////////////////////////////////////////////////////////////////

// vtk includes
#include <vtkCommand.h>
#include <vtkCallbackCommand.h>
#include <vtkObjectFactory.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkWidgetEventTranslator.h>
#include <vtkWidgetCallbackMapper.h>
#include <vtkEvent.h>
#include <vtkWidgetEvent.h>

// project includes
#include "BoxSelectionWidget.h"
#include "BoxSelectionRepresentation2D.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// BoxSelectionWidget class
//
vtkStandardNewMacro(BoxSelectionWidget);

BoxSelectionWidget::BoxSelectionWidget()
{
	this->WidgetState = BoxSelectionWidget::Start;
	this->Resizable = 1;

	this->CallbackMapper->SetCallbackMethod(vtkCommand::LeftButtonPressEvent, vtkWidgetEvent::Select, this, BoxSelectionWidget::SelectAction);
	this->CallbackMapper->SetCallbackMethod(vtkCommand::LeftButtonReleaseEvent, vtkWidgetEvent::EndSelect, this, BoxSelectionWidget::EndSelectAction);
	this->CallbackMapper->SetCallbackMethod(vtkCommand::MiddleButtonPressEvent, vtkWidgetEvent::Translate, this, BoxSelectionWidget::TranslateAction);
	this->CallbackMapper->SetCallbackMethod(vtkCommand::MiddleButtonReleaseEvent, vtkWidgetEvent::EndSelect, this, BoxSelectionWidget::EndSelectAction);
	this->CallbackMapper->SetCallbackMethod(vtkCommand::MouseMoveEvent, vtkWidgetEvent::Move, this, BoxSelectionWidget::MoveAction);
}

BoxSelectionWidget::~BoxSelectionWidget()
{
}

void BoxSelectionWidget::SetCursor(int cState)
{
	if (!this->Resizable && cState != BoxSelectionRepresentation2D::Inside)
	{
		this->RequestCursorShape(VTK_CURSOR_DEFAULT);
		return;
	}

	switch (cState)
	{
		case BoxSelectionRepresentation2D::AdjustingP0:
			this->RequestCursorShape(VTK_CURSOR_SIZESW);
			break;
		case BoxSelectionRepresentation2D::AdjustingP1:
			this->RequestCursorShape(VTK_CURSOR_SIZESE);
			break;
		case BoxSelectionRepresentation2D::AdjustingP2:
			this->RequestCursorShape(VTK_CURSOR_SIZENE);
			break;
		case BoxSelectionRepresentation2D::AdjustingP3:
			this->RequestCursorShape(VTK_CURSOR_SIZENW);
			break;
		case BoxSelectionRepresentation2D::AdjustingE0:
		case BoxSelectionRepresentation2D::AdjustingE2:
			this->RequestCursorShape(VTK_CURSOR_SIZENS);
			break;
		case BoxSelectionRepresentation2D::AdjustingE1:
		case BoxSelectionRepresentation2D::AdjustingE3:
			this->RequestCursorShape(VTK_CURSOR_SIZEWE);
			break;
		case BoxSelectionRepresentation2D::Inside:
			if (reinterpret_cast<BoxSelectionRepresentation2D*>(this->WidgetRep)->GetMoving())
			{
				this->RequestCursorShape(VTK_CURSOR_SIZEALL);
			}
			else
			{
				this->RequestCursorShape(VTK_CURSOR_HAND);
			}
			break;
		default:
			this->RequestCursorShape(VTK_CURSOR_DEFAULT);
			break;
	}
}

void BoxSelectionWidget::SelectAction(vtkAbstractWidget *w)
{
	BoxSelectionWidget *self = reinterpret_cast<BoxSelectionWidget*>(w);

	if (self->WidgetRep->GetInteractionState() == BoxSelectionRepresentation2D::Outside)
		return;

	// We are definitely selected
	self->GrabFocus(self->EventCallbackCommand);
	self->WidgetState = BoxSelectionWidget::Selected;

	// Picked something inside the widget
	int X = self->Interactor->GetEventPosition()[0];
	int Y = self->Interactor->GetEventPosition()[1];

	// This is redundant but necessary on some systems (windows) because the
	// cursor is switched during OS event processing and reverts to the default
	// cursor (i.e., the MoveAction may have set the cursor previously, but this
	// method is necessary to maintain the proper cursor shape)..
	self->SetCursor(self->WidgetRep->GetInteractionState());

	// convert to world coordinates
	double eventPos[2] = { static_cast<double>(X), static_cast<double>(Y) };
	reinterpret_cast<BoxSelectionRepresentation2D*>(self->WidgetRep)->TransformToWorldCoordinates(&eventPos[0],&eventPos[1]);

	self->WidgetRep->StartWidgetInteraction(eventPos);
	self->EventCallbackCommand->SetAbortFlag(1);
	self->StartInteraction();
	self->InvokeEvent(vtkCommand::StartInteractionEvent, NULL);
}

void BoxSelectionWidget::TranslateAction(vtkAbstractWidget *w)
{
	BoxSelectionWidget *self = reinterpret_cast<BoxSelectionWidget*>(w);

	if (self->WidgetRep->GetInteractionState() == BoxSelectionRepresentation2D::Outside)
		return;

	// We are definitely selected
	self->GrabFocus(self->EventCallbackCommand);
	self->WidgetState = BoxSelectionWidget::Selected;
	reinterpret_cast<BoxSelectionRepresentation2D*>(self->WidgetRep)->MovingOn();

	// Picked something inside the widget
	int X = self->Interactor->GetEventPosition()[0];
	int Y = self->Interactor->GetEventPosition()[1];

	// This is redundant but necessary on some systems (windows) because the
	// cursor is switched during OS event processing and reverts to the default
	// cursor.
	self->SetCursor(self->WidgetRep->GetInteractionState());

	// convert to world coordinates
	double eventPos[2] = { static_cast<double>(X), static_cast<double>(Y) };
	reinterpret_cast<BoxSelectionRepresentation2D*>(self->WidgetRep)->TransformToWorldCoordinates(&eventPos[0],&eventPos[1]);

	self->WidgetRep->StartWidgetInteraction(eventPos);
	self->EventCallbackCommand->SetAbortFlag(1);
	self->StartInteraction();
	self->InvokeEvent(vtkCommand::StartInteractionEvent, NULL);
}

void BoxSelectionWidget::MoveAction(vtkAbstractWidget *w)
{
	BoxSelectionWidget *self = reinterpret_cast<BoxSelectionWidget*>(w);

	// compute some info we need for all cases
	int X = self->Interactor->GetEventPosition()[0];
	int Y = self->Interactor->GetEventPosition()[1];

	// Set the cursor appropriately
	if (self->WidgetState == BoxSelectionWidget::Start)
	{
		int stateBefore = self->WidgetRep->GetInteractionState();
		self->WidgetRep->ComputeInteractionState(X, Y);
		int stateAfter = self->WidgetRep->GetInteractionState();
		self->SetCursor(stateAfter);

		if (stateAfter != BoxSelectionRepresentation2D::Inside)
			reinterpret_cast<BoxSelectionRepresentation2D*>(self->WidgetRep)->MovingOff();
		else
			reinterpret_cast<BoxSelectionRepresentation2D*>(self->WidgetRep)->MovingOn();

		if (reinterpret_cast<BoxSelectionRepresentation2D*>(self->WidgetRep)->GetShowBorder() == BoxSelectionRepresentation2D::BORDER_ACTIVE && stateBefore != stateAfter
				&& (stateBefore == BoxSelectionRepresentation2D::Outside || stateAfter == BoxSelectionRepresentation2D::Outside))
		{
			self->Render();
		}
		return;
	}

	if (!self->Resizable && self->WidgetRep->GetInteractionState() != BoxSelectionRepresentation2D::Inside)
		return;

	// Okay, adjust the representation (the widget is currently selected)
	double eventPos[2] = { static_cast<double>(X), static_cast<double>(Y)};
	reinterpret_cast<BoxSelectionRepresentation2D*>(self->WidgetRep)->TransformToWorldCoordinates(&eventPos[0],&eventPos[1]);

	self->WidgetRep->WidgetInteraction(eventPos);
	self->EventCallbackCommand->SetAbortFlag(1);
	self->InvokeEvent(vtkCommand::InteractionEvent, NULL);
	self->Render();
}

void BoxSelectionWidget::EndSelectAction(vtkAbstractWidget *w)
{
	BoxSelectionWidget *self = reinterpret_cast<BoxSelectionWidget*>(w);

	if (self->WidgetRep->GetInteractionState() == BoxSelectionRepresentation2D::Outside || self->WidgetState != BoxSelectionWidget::Selected)
		return;

	// Return state to not selected
	self->ReleaseFocus();
	self->WidgetState = BoxSelectionWidget::Start;
	reinterpret_cast<BoxSelectionRepresentation2D*>(self->WidgetRep)->MovingOff();

	// stop adjusting
	self->EventCallbackCommand->SetAbortFlag(1);
	self->EndInteraction();
	self->InvokeEvent(vtkCommand::EndInteractionEvent, NULL);
}

void BoxSelectionWidget::CreateDefaultRepresentation()
{
	if (!this->WidgetRep)
		this->WidgetRep = BoxSelectionRepresentation2D::New();
}

void BoxSelectionWidget::PrintSelf(ostream& os, vtkIndent indent)
{
	this->Superclass::PrintSelf(os, indent);

	os << indent << "Resizable: " << (this->Resizable ? "On\n" : "Off\n");
}
