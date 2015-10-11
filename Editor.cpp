///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: Editor.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////

// c++ includes
#include <cstddef>

// includes qt
#include "QtGui.h"

int main(int argc, char * argv[])
{
    auto app = new QApplication(argc, argv);
    auto editor = new EspinaVolumeEditor(app);

    editor->showMaximized();
    app->exec();
    
    delete editor;
}
