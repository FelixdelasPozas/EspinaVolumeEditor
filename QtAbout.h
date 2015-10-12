///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtAbout.h
// Purpose: egocentrical banner
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _QTABOUT_H_
#define _QTABOUT_H_

// qt includes
#include <QtGui>
#include "ui_QtAbout.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtAbout class
//
class QtAbout
: public QDialog
, private Ui_About
{
  Q_OBJECT
  public:
    /** \brief QtAbout class constructor.
     * \param[in] parent pointer to the QWidget parent of this one.
     * \param[in] f window flags.
     *
     */
    QtAbout(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Dialog);
};

#endif // _QTABOUT_H_
