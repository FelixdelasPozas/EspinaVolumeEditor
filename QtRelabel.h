///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtRelabel.h
// Purpose: modal dialog for configuring relabel options 
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _QTRELABEL_H_
#define _QTRELABEL_H_

// Qt includes
#include <QtGui>
#include "ui_QtRelabel.h"

// vtk includes
#include <vtkSmartPointer.h>
#include <vtkLookupTable.h>

// project includes
#include "Metadata.h"
#include "DataManager.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtRelabel class
//
class QtRelabel: public QDialog, private Ui_Relabel
{
        Q_OBJECT
    public:
        // constructor & destructor
        QtRelabel(QWidget *parent = 0, Qt::WindowFlags f = Qt::Dialog);
        ~QtRelabel();

        // set initial options
        void SetInitialOptions(std::set<unsigned short>, Metadata*, DataManager*);

        // get the label selected in combobox
        unsigned short GetSelectedLabel();
        
        // returns true if it's a new label
        bool IsNewLabel();
        
        // returns true if the user clicked the Ok button instead the Cancel or Close.
        bool ModifiedData();
    public slots:
        // slots for signals
        virtual void AcceptedData();

    private:
        // i prefer this way instead returning data
        bool _modified;
        bool _newlabel;
        bool _multipleLabels;
        
        // label selected in the combobox
        unsigned short _selectedLabel;
        unsigned int _maxcolors;
};

#endif // _QTRELABEL_H_
