///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: VolumeEditor.cxx 
// Purpose: class for interfacing & dealing with wrapping, making it as simple as possible
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "VolumeEditor.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// VolumeEditor class
//
VolumeEditor::VolumeEditor()
{
    _initialized = false;
    _labelMap = NULL;
    _freeValue = 1;
    _editor = NULL;
    _app = NULL;
}

VolumeEditor::~VolumeEditor()
{
    if (this->_editor)
        delete _editor;
    
    if (this->_app)
        delete _app;
}

void VolumeEditor::SetInput(itk::SmartPointer<LabelMapType> labelMap)
{
    _labelMap = labelMap;
    _initialized = true;
}

void VolumeEditor::SetInitialFreeValue(unsigned short value)
{
    _freeValue = value;
}

unsigned short VolumeEditor::GetLastUsedScalarValue()
{
    if (!_initialized)
    {
        std::cerr << "ERROR: VolumeEditor class has not been initialized" << std::endl;
        return 0;
    }
    
    return _editor->GetLastUsedScalarValue();
}

bool VolumeEditor::GetUserCreatedNewLabels()
{
    if (!_initialized)
    {
        std::cerr << "ERROR: VolumeEditor class has not been initialized" << std::endl;
        return false;
    }
    
    return _editor->GetUserCreatedNewLabels();
}

itk::SmartPointer<LabelMapType> VolumeEditor::GetOutput()
{
    if (!_initialized)
    {
        std::cerr << "ERROR: VolumeEditor class has not been initialized" << std::endl;
        return NULL;
    }
    
    return _editor->GetOutput();
}

double* VolumeEditor::GetRGBAColorFromValue(unsigned short value)
{
    if (!_initialized)
    {
        std::cerr << "ERROR: VolumeEditor class has not been initialized" << std::endl;
        return NULL;
    }
    
    return _editor->GetRGBAColorFromValue(value);
}

bool VolumeEditor::VolumeModified()
{
    if (!_initialized)
    {
        std::cerr << "ERROR: VolumeEditor class has not been initialized" << std::endl;
        return NULL;
    }
    
    return _editor->VolumeModified();
}


void VolumeEditor::Execute()
{
    if (!_initialized)
    {
        std::cerr << "ERROR: VolumeEditor class has not been initialized" << std::endl;
        return;
    }
    
    // lame QApplication init, hope it doesn't crash
    int argc = 1;
    char **argv = NULL;

    _app = new QApplication(argc, argv);
    _editor = new EspinaVolumeEditor(_app);
    _editor->SetInitialFreeValue(_freeValue);
    _editor->SetInput(_labelMap);
    _editor->showMaximized();
    _app->exec();
}
