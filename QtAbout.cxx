///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtAbout.cxx
// Purpose: egocentrical banner
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// qt includes
#include "QtAbout.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtAbout class
//
QtAbout::QtAbout(QWidget *parent_Widget, Qt::WindowFlags f) : QDialog(parent_Widget,f)
{
    setupUi(this); // this sets up GUI

    // want to make the dialog appear centered
    this->move(parent_Widget->geometry().center() - this->rect().center());
    this->resize(this->minimumSizeHint());
    this->layout()->setSizeConstraint( QLayout::SetFixedSize );
}

QtAbout::~QtAbout()
{
}
