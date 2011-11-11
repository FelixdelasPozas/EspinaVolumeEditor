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
class QtAbout: public QDialog, private Ui_About
{
        Q_OBJECT
    public:
        // constructor & destructor
        QtAbout(QWidget *parent = 0, Qt::WindowFlags f = Qt::Dialog);
        ~QtAbout();

    public slots:

    private:
};

#endif // _QTABOUT_H_