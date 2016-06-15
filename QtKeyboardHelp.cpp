///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtKeyboardHelp.cpp
// Purpose: shows the keyboard shortcuts for the editor
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// qt includes
#include "QtKeyboardHelp.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtKeyboardHelp class
//
QtKeyboardHelp::QtKeyboardHelp(QWidget *parent, Qt::WindowFlags f)
: QDialog{parent, f}
{
  setupUi(this);

  this->move(parent->geometry().center() - this->rect().center());
  this->resize(this->minimumSizeHint());
  this->layout()->setSizeConstraint(QLayout::SetFixedSize);
}
