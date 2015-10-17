///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: BoxSelectionWidget.cpp
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

// qt includes
#include <QApplication>

// c++ includes
#include <math.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
// BoxSelectionWidget class
//
vtkStandardNewMacro(BoxSelectionWidget);

///////////////////////////////////////////////////////////////////////////////////////////////////
BoxSelectionWidget::BoxSelectionWidget()
: m_state{WidgetState::Start}
, ManagesCursor{false}
{
  this->CallbackMapper->SetCallbackMethod(vtkCommand::LeftButtonPressEvent, vtkWidgetEvent::Select, this, BoxSelectionWidget::SelectAction);
  this->CallbackMapper->SetCallbackMethod(vtkCommand::LeftButtonReleaseEvent, vtkWidgetEvent::EndSelect, this, BoxSelectionWidget::EndSelectAction);
  this->CallbackMapper->SetCallbackMethod(vtkCommand::MouseMoveEvent, vtkWidgetEvent::Move, this, BoxSelectionWidget::MoveAction);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
BoxSelectionWidget::~BoxSelectionWidget()
{
  // restores default qt cursor if it has been changed at the time of executing destructor
  this->SetEnabled(false);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionWidget::SetCursor(int cState)
{
  if (!this->ManagesCursor && cState != BoxSelectionRepresentation2D::WidgetState::Outside)
  {
    this->ManagesCursor = true;
    QApplication::setOverrideCursor(Qt::CrossCursor);
  }

  switch (cState)
  {
    case BoxSelectionRepresentation2D::WidgetState::AdjustingP0:
      this->RequestCursorShape(VTK_CURSOR_SIZESW);
      break;
    case BoxSelectionRepresentation2D::WidgetState::AdjustingP1:
      this->RequestCursorShape(VTK_CURSOR_SIZESE);
      break;
    case BoxSelectionRepresentation2D::WidgetState::AdjustingP2:
      this->RequestCursorShape(VTK_CURSOR_SIZENE);
      break;
    case BoxSelectionRepresentation2D::WidgetState::AdjustingP3:
      this->RequestCursorShape(VTK_CURSOR_SIZENW);
      break;
    case BoxSelectionRepresentation2D::WidgetState::AdjustingE0:
    case BoxSelectionRepresentation2D::WidgetState::AdjustingE2:
      this->RequestCursorShape(VTK_CURSOR_SIZENS);
      break;
    case BoxSelectionRepresentation2D::WidgetState::AdjustingE1:
    case BoxSelectionRepresentation2D::WidgetState::AdjustingE3:
      this->RequestCursorShape(VTK_CURSOR_SIZEWE);
      break;
    case BoxSelectionRepresentation2D::WidgetState::Inside:
      if (reinterpret_cast<BoxSelectionRepresentation2D*>(this->WidgetRep)->Getm_moving())
        this->RequestCursorShape(VTK_CURSOR_SIZEALL);
      else
        this->RequestCursorShape(VTK_CURSOR_HAND);
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
void BoxSelectionWidget::SelectAction(vtkAbstractWidget *widget)
{
  auto self = dynamic_cast<BoxSelectionWidget*>(widget);
  Q_ASSERT(self);

  if (self->WidgetRep->GetInteractionState() == BoxSelectionRepresentation2D::WidgetState::Outside) return;

  // We are definitely selected
  self->GrabFocus(self->EventCallbackCommand);
  self->m_state = WidgetState::Selected;

  // Picked something inside the widget
  auto X = self->Interactor->GetEventPosition()[0];
  auto Y = self->Interactor->GetEventPosition()[1];

  // This is redundant but necessary on some systems (windows) because the
  // cursor is switched during OS event processing and reverts to the default
  // cursor (i.e., the MoveAction may have set the cursor previously, but this
  // method is necessary to maintain the proper cursor shape)..
  self->SetCursor(self->WidgetRep->GetInteractionState());

  // convert to world coordinates
  double eventPos[2] = { static_cast<double>(X), static_cast<double>(Y) };
  auto rep = dynamic_cast<BoxSelectionRepresentation2D*>(self->WidgetRep);
  Q_ASSERT(rep);
  rep->TransformToWorldCoordinates(&eventPos[0], &eventPos[1]);

  self->WidgetRep->StartWidgetInteraction(eventPos);
  self->EventCallbackCommand->SetAbortFlag(1);
  self->StartInteraction();
  self->InvokeEvent(vtkCommand::StartInteractionEvent, nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionWidget::TranslateAction(vtkAbstractWidget *widget)
{
  auto self = dynamic_cast<BoxSelectionWidget*>(widget);
  Q_ASSERT(self);

  if (self->WidgetRep->GetInteractionState() == BoxSelectionRepresentation2D::WidgetState::Outside) return;

  // We are definitely selected
  self->GrabFocus(self->EventCallbackCommand);
  self->m_state = WidgetState::Selected;
  auto rep = dynamic_cast<BoxSelectionRepresentation2D*>(self->WidgetRep);
  Q_ASSERT(rep);
  rep->m_movingOn();

  // Picked something inside the widget
  auto X = self->Interactor->GetEventPosition()[0];
  auto Y = self->Interactor->GetEventPosition()[1];

  // This is redundant but necessary on some systems (windows) because the
  // cursor is switched during OS event processing and reverts to the default
  // cursor.
  self->SetCursor(self->WidgetRep->GetInteractionState());

  // convert to world coordinates
  double eventPos[2] = { static_cast<double>(X), static_cast<double>(Y) };
  rep->TransformToWorldCoordinates(&eventPos[0], &eventPos[1]);

  self->WidgetRep->StartWidgetInteraction(eventPos);
  self->EventCallbackCommand->SetAbortFlag(1);
  self->StartInteraction();
  self->InvokeEvent(vtkCommand::StartInteractionEvent, nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionWidget::MoveAction(vtkAbstractWidget *widget)
{
  auto self = dynamic_cast<BoxSelectionWidget*>(widget);
  Q_ASSERT(self);

  auto rep = dynamic_cast<BoxSelectionRepresentation2D*>(self->WidgetRep);
  Q_ASSERT(rep);

  // compute some info we need for all cases
  auto X = self->Interactor->GetEventPosition()[0];
  auto Y = self->Interactor->GetEventPosition()[1];

  // Set the cursor appropriately
  if (self->m_state == WidgetState::Start)
  {
    self->WidgetRep->ComputeInteractionState(X, Y);
    int state = self->WidgetRep->GetInteractionState();
    self->SetCursor(state);

    rep->Setm_moving(state == BoxSelectionRepresentation2D::WidgetState::Inside);

    return;
  }

  // Okay, adjust the representation (the widget is currently selected)
  double eventPos[2] = { static_cast<double>(X), static_cast<double>(Y) };
  rep->TransformToWorldCoordinates(&eventPos[0], &eventPos[1]);

  self->WidgetRep->WidgetInteraction(eventPos);
  self->EventCallbackCommand->SetAbortFlag(1);
  self->InvokeEvent(vtkCommand::InteractionEvent, nullptr);
  self->Render();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionWidget::EndSelectAction(vtkAbstractWidget *widget)
{
  auto self = dynamic_cast<BoxSelectionWidget*>(widget);
  Q_ASSERT(self);

  if (self->WidgetRep->GetInteractionState() == BoxSelectionRepresentation2D::WidgetState::Outside || self->m_state != WidgetState::Selected) return;

  // Adjust to a grid specified by the image spacing by rounding the final coordinates of the border
  auto rep = dynamic_cast<BoxSelectionRepresentation2D*>(self->WidgetRep);
  Q_ASSERT(rep);
  double *pos1 = rep->GetPosition();
  double *pos2 = rep->GetPosition2();
  double *spacing = rep->Getm_spacing();

  // to better understand this you should look at BoxSelectionRepresentation2D::SetBoxCoordinates()
  pos1[0] = floor(pos1[0] / spacing[0]) + 1; // plus one because we use Xcoord-0.5 to select voxel Xcoord, thats fine only with the upper right corner.
  pos1[1] = floor(pos1[1] / spacing[1]) + 1;
  pos2[0] = floor(pos2[0] / spacing[0]);
  pos2[1] = floor(pos2[1] / spacing[1]);
  rep->SetBoxCoordinates(static_cast<int>(pos1[0]), static_cast<int>(pos1[1]), static_cast<int>(pos2[0]), static_cast<int>(pos2[1]));

  // Return state to not selected
  self->ReleaseFocus();
  self->m_state = WidgetState::Start;
  rep->m_movingOff();

  // stop adjusting
  self->EventCallbackCommand->SetAbortFlag(1);
  self->EndInteraction();
  self->InvokeEvent(vtkCommand::EndInteractionEvent, nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionWidget::CreateDefaultRepresentation()
{
  if (!this->WidgetRep) this->WidgetRep = BoxSelectionRepresentation2D::New();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionWidget::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Resizable: On\n";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void BoxSelectionWidget::SetEnabled(int value)
{
  this->Superclass::SetEnabled(value);

  if ((false == value) && (this->ManagesCursor))
  {
    QApplication::restoreOverrideCursor();
    this->ManagesCursor = false;
  }
}
