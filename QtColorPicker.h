///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtColorPicker.h
// Purpose: pick a color by selecting it's RGB components 
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _QTCOLORPICKER_H_
#define _QTCOLORPICKER_H_

// vtk includes
#include <vtkLookupTable.h>
#include <vtkSmartPointer.h>

// Qt includes
#include <QtGui>
#include "ui_QtColorPicker.h"

// project includes
#include "DataManager.h"
#include "VectorSpaceAlgebra.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtColorPicker class
//
class QtColorPicker: public QDialog, private Ui_ColorPicker
{
        Q_OBJECT
    public:
        // constructor & destructor
        QtColorPicker(QWidget *parent = 0, Qt::WindowFlags f = Qt::Dialog);
        ~QtColorPicker();

        // set initial options
        void SetInitialOptions(DataManager *);

        // returns true if the user clicked the Ok button instead the Cancel or Close.
        bool ModifiedData();
        
        // return the selected color
        Vector3d GetColor();
    public slots:
        // slots for signals
        void AcceptedData();

    private slots:
        virtual void RValueChanged(int);
        virtual void GValueChanged(int);
        virtual void BValueChanged(int);
        
    private:
        // computes color box and disables buttons if color is already in use
        void MakeColor();
        
        // i prefer this way instead returning data
        bool 			_modified;
        
        // pointer to the datamanager containing the color table
        DataManager 	*_data;
        
        // rgb of selected color to be returned
        Vector3d 		_rgb;
};

#endif // _QTLABELEDITOR_H_
