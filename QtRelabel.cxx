///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtRelabel.cxx
// Purpose: modal dialog for configuring relabel options 
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

// Qt includes
#include <QListWidget>
#include <QListWidgetItem>

// project includes
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

void QtRelabel::SetInitialOptions(unsigned short label,vtkSmartPointer<vtkLookupTable> colors, Metadata* data)
{
    double rgba[4];
    char text[100];
    QPixmap icon(16,16);
    QColor color;

    _selectedLabel = label;
    _maxcolors = colors->GetNumberOfTableValues();

    // generate items
    if (label != 0)
    {
        newlabelbox->insertItem(0, "Background");
        std::string name = data->GetObjectSegmentName(label);
        colors->GetTableValue(label, rgba);
        color.setRgbF(rgba[0], rgba[1], rgba[2], 1);
        icon.fill(color);
        sprintf(text, "%s %d voxels",name.c_str(), static_cast<int>(label));
        colorlabel->setPixmap(icon);
        selectionlabel->setText(QString(text));
    }
    else
        selectionlabel->setText("Background voxels");

    // color 0 is black, so we will start from 1
    int j = 0;
    for (unsigned int i = 1; i < _maxcolors; i++)
    {
        if (i == static_cast<unsigned int>(label))
            continue;
        else
            j++;
        
        colors->GetTableValue(i, rgba);
        color.setRgbF(rgba[0], rgba[1], rgba[2], 1);
        icon.fill(color);
        std::string name = data->GetObjectSegmentName(i);
        sprintf(text, "%s %d", name.c_str(), i);
        QListWidgetItem *item = new QListWidgetItem(QIcon(icon), QString(text));
        newlabelbox->addItem(item);
    }
    newlabelbox->addItem("New label");

    // configure widgets
    newlabelbox->setCurrentRow(_maxcolors-1);
    
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
    int label = newlabelbox->currentRow();
    
    if (label < _selectedLabel)
        _selectedLabel = newlabelbox->currentRow();
    else
        _selectedLabel = newlabelbox->currentRow() + 1;
    
    if (_selectedLabel == _maxcolors)
        _newlabel = true;

    _modified = true;
}

bool QtRelabel::IsNewLabel()
{
    return _newlabel;
}
