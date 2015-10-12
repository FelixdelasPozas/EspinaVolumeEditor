///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtAbout.cpp
// Purpose: egocentrical banner
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// qt includes
#include "QtAbout.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtAbout class
//
QtAbout::QtAbout(QWidget *parent_Widget, Qt::WindowFlags f)
: QDialog(parent_Widget, f)
{
  setupUi(this);

  this->move(parent_Widget->geometry().center() - this->rect().center());
  this->resize(this->minimumSizeHint());
  this->layout()->setSizeConstraint(QLayout::SetFixedSize);
}
