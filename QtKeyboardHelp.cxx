///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtKeyboardHelp.cxx
// Purpose: shows the keyboard shortcuts for the editor
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// qt includes
#include "QtKeyboardHelp.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtKeyboardHelp class
//
QtKeyboardHelp::QtKeyboardHelp(QWidget *parent_Widget, Qt::WindowFlags f) : QDialog(parent_Widget,f)
{
    setupUi(this); // this sets up GUI

    // want to make the dialog appear centered
    this->move(parent_Widget->geometry().center() - this->rect().center());
    this->resize(this->minimumSizeHint());
    this->layout()->setSizeConstraint( QLayout::SetFixedSize );
}

QtKeyboardHelp::~QtKeyboardHelp()
{
}
