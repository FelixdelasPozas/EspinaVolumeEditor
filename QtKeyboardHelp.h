///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtKeyboardHelp.h
// Purpose: shows the keyboard shortcuts for the editor
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _QTKEYBOARDSHORTCUTSHELP_H_
#define _QTKEYBOARDSHORTCUTSHELP_H_

// qt includes
#include <QtGui>
#include "ui_QtKeyboardHelp.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtAbout class
//
class QtKeyboardHelp
: public QDialog
, private Ui_KeyBoardHelp
{
  Q_OBJECT
  public:
    /** \brief QtKeyboardHelp class constructor.
     * \param[in] parent pointer to the QWidget parent of this one.
     * \param[in] f window flags.
     *
     */
    QtKeyboardHelp(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Dialog);
};

#endif // _QTKEYBOARDSHORTCUTSHELP_H_
