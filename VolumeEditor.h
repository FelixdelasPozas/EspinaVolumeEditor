///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: VolumeEditor.h  
// Purpose: class for interfacing & dealing with wrapping, making it as simple as possible
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _VOLUMEEDITOR_H_
#define _VOLUMEEDITOR_H_

// qt includes
#include <QApplication>

// itk includes
#include <itkLabelMap.h>
#include <itkLabelObject.h>
#include <itkSmartPointer.h>

// project includes
#include "QtGui.h"

// defines and typedefs
typedef itk::LabelObject < unsigned short,3 >  LabelObjectType;
typedef itk::LabelMap    < LabelObjectType >  LabelMapType;

// NOTA: introducir un método similar a SetInput llamado SetReferenceInput para poder pasar la
//       imagen de referencia de segmentación

///////////////////////////////////////////////////////////////////////////////////////////////////
// VolumeEditor Class
//
class VolumeEditor
{
    public:
        VolumeEditor();
        ~VolumeEditor();
        
        // INTERFACE FUNCTIONS ////////////////////////////////////////////////
        
        // set the label map to edit
        void SetInput(itk::SmartPointer<LabelMapType> labelMap);
        
        // set the initial scalar to be used by the editor to assign to a label.
        // OPTIONAL: if not set the initial value is assumed to be 1
        void SetInitialFreeValue(unsigned short);
        
        // get the last scalar used in the editor to assign a value to a label
        unsigned short GetLastUsedScalarValue();
        
        // must be used to get sure that the user has created new labels, if false
        // the value returned for GetLastUsedScalarValue() is 0
        bool GetUserCreatedNewLabels();
        
        // get the label map resulting from the volume edition
        itk::SmartPointer<LabelMapType> GetOutput();
        
        // get a pointer to the rgba values of the scalar used by the user in
        // the editor, optional, just in case you want to use it in Espina.
        // BEWARE: deallocation must be done by the caller of the function as
        // those values won't be deleted when the editor class is destroyed.
        double* GetRGBAColorFromValue(unsigned short);
        
        // must be called before using any of the "gets" to be sure that the user
        // has accepted the modifications to the volume
        bool VolumeModified();
        
        // execute the editor, must be called after SetInput() and, optionally, 
        // SetInitialFreeValue()
        void Execute();
    private:
        // initial values
        itk::SmartPointer<itk::LabelMap<itk::LabelObject < unsigned short,3 > > >  _labelMap;
        unsigned short                  _freeValue;
        
        // editor application
        EspinaVolumeEditor             *_editor;
        QApplication                   *_app;
        
        // to know if it has been initialized
        bool                            _initialized;
};

#endif
