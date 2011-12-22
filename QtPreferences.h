///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtPreferences.h
// Purpose: modal dialog for configuring preferences
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _QTPREFERENCES_H_
#define _QTPREFERENCES_H_

// Qt includes
#include <QtGui>
#include "ui_QtPreferences.h"

// project includes
#include "EditorOperations.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtPreferences class
//
class QtPreferences: public QDialog, private Ui_Preferences
{
    Q_OBJECT

    public:
        // constructor & destructor
        QtPreferences(QWidget *parent = 0, Qt::WindowFlags f = Qt::Dialog);
        ~QtPreferences();
        
        // set initial options
        void SetInitialOptions(unsigned long int, unsigned long int, unsigned int, double, int, unsigned int, bool);

        // get the capacity
        unsigned long int GetSize(void);
        
        // get the radius
        unsigned int GetRadius(void);
        
        // get the flood level
        double GetLevel(void);
        
        // get the segmentation opacity when a reference image is present
        unsigned int GetSegmentationOpacity(void);
        
        // returns true if the user pushed the Ok button instead the Cancel or Close.
        bool ModifiedData(void);
        
        // enable the visualization options box, that should be disabled if there isn't a reference image loaded
        void EnableVisualizationBox(void);

        // get the save session time
        unsigned int GetSaveSessionTime(void);

        // get if the save session data option is enabled
        bool GetSaveSessionEnabled(void);
    public slots:
        // slots for signals
        virtual void SelectSize(int);
        virtual void SelectRadius(int);
        virtual void SelectLevel(double);
        virtual void SelectOpacity(int);
        virtual void SelectSaveTime(int);
        
    private slots:
        void AcceptedData();
        
    private:
        // undo/redo buffer capacity in bytes
        unsigned long int _undoSize;
        
        // undo/redo buffer system occupation
        unsigned long int _undoCapacity;
        
        // open/close/dilate/erode filters' radius
        unsigned int _filtersRadius;
        
        // watershed filter flood level
        double _watershedLevel;
        
        // segmentation opacity when a reference image is present
        unsigned int _segmentationOpacity;
        
        // save session time
        unsigned int _saveTime;

        // just to know that the data has been modified 
        bool _modified;
};

#endif // _QTPREFERENCES_H_
