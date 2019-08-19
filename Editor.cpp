///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: Editor.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////

// c++ includes
#include <EspinaVolumeEditor.h>
#include <cstddef>

// includes qt

int main(int argc, char * argv[])
{
  QApplication app(argc, argv);
  EspinaVolumeEditor editor(&app);
  editor.showMaximized();

  return app.exec();
}
