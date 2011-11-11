///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtRelabel.cxx
// Purpose: modal dialog for configuring relabel options 
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

// Qt includes
#include "QtRelabel.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtRelabel class
//
QtRelabel::QtRelabel(QWidget *p, Qt::WindowFlags f) : QDialog(p,f)
{
    setupUi(this); // this sets up GUI
    _modified = false;
    _newlabel = false;
    _maxcolors = 0;
    _selectedLabel = 0;
}

QtRelabel::~QtRelabel()
{
    // empty
}

void QtRelabel::SetInitialOptions(unsigned short label,vtkSmartPointer<vtkLookupTable> colors)
{
    double rgba[4];
    char text[20];
    QPixmap icon(16,16);
    QColor color;
    
    _selectedLabel = label;
    _maxcolors = colors->GetNumberOfTableValues();

    // generate combobox items
    newlabelbox->setMaxCount(_maxcolors);
    
    if (label != 0)
    {
        newlabelbox->insertItem(0, "Background");

        colors->GetTableValue(label, rgba);
        color.setRgbF(rgba[0], rgba[1], rgba[2], 1);
        icon.fill(color);
        sprintf(text, "Label %d voxels", static_cast<int>(label));
        colorlabel->setPixmap(icon);
        selectionlabel->setText(QString(text));
    }
    else
        selectionlabel->setText("Background voxels");
    
    // color 0 is black, so we will start from 1
    int j = 0;
    for (int i = 1; i < _maxcolors; i++)
    {
        if (i == static_cast<unsigned int>(label))
            continue;
        else
            j++;
        
        colors->GetTableValue(i, rgba);
        color.setRgbF(rgba[0], rgba[1], rgba[2], 1);
        icon.fill(color);
        sprintf(text, "Label %d", i);
        newlabelbox->insertItem(j,QIcon(icon),text);
    }
    newlabelbox->insertItem(j+1,"New label");

    // configure widgets
    newlabelbox->setEditable(false);
    newlabelbox->setMaxVisibleItems(10);
    newlabelbox->setCurrentIndex(0);
    
    connect(acceptbutton, SIGNAL(accepted()), this, SLOT(AcceptedData()));
}

bool QtRelabel::ModifiedData()
{
    return _modified;
}

unsigned short QtRelabel::GetSelectedLabel()
{
    return static_cast<unsigned short>(_selectedLabel);
}

void QtRelabel::AcceptedData()
{
    int label = newlabelbox->currentIndex();
    
    if (label < _selectedLabel)
        _selectedLabel = newlabelbox->currentIndex();
    else
        _selectedLabel = newlabelbox->currentIndex() + 1;
    
    if (_selectedLabel == _maxcolors)
        _newlabel = true;

    _modified = true;
}

bool QtRelabel::IsNewLabel()
{
    return _newlabel;
}
