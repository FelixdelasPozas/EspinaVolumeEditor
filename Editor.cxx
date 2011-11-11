///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: Editor.cxx
// Purpose: Example of use for the editor interface.
// Notes: This file loads a GIPL image with the "test.gipl" filename and executes the editor.
//        once the editor has exited it enumerates the changes made to the volume. It doesn't 
//        modify "test.gipl". Messages are in spanish.
///////////////////////////////////////////////////////////////////////////////////////////////////

// c++ includes
#include <cstddef>

// includes qt
#include "QtGui.h"

int main(int argc, char * argv[])
{

    QApplication *app = new QApplication(argc, argv);
    EspinaVolumeEditor *editor = new EspinaVolumeEditor(app);
    editor->showMaximized();
    app->exec();
    
    delete editor;
}
