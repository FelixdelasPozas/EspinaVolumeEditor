///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtColorPicker.h
// Purpose: pick a color by selecting it's RGB components 
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

// Qt includes
#include "QtColorPicker.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtColorPicker class
//
QtColorPicker::QtColorPicker(QWidget *p, Qt::WindowFlags f) : QDialog(p,f)
{
    setupUi(this); // this sets up GUI
    _modified = false;
    _data = NULL;
    _rgb = Vector3d(0.5,0.5,0.5);
    
    Rslider->setSliderPosition(static_cast<int>(_rgb[0]*255.0));
    Gslider->setSliderPosition(static_cast<int>(_rgb[1]*255.0));
    Bslider->setSliderPosition(static_cast<int>(_rgb[2]*255.0));
    
    connect(Rslider, SIGNAL(valueChanged(int)), this, SLOT(RValueChanged(int)));
    connect(Gslider, SIGNAL(valueChanged(int)), this, SLOT(GValueChanged(int)));
    connect(Bslider, SIGNAL(valueChanged(int)), this, SLOT(BValueChanged(int)));
    
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(AcceptedData()));
}

QtColorPicker::~QtColorPicker()
{
    // empty, nothing to be freed
}

void QtColorPicker::SetInitialOptions(DataManager *data)
{
    _data = data;
    MakeColor();
}

bool QtColorPicker::ModifiedData()
{
    return _modified;
}

void QtColorPicker::AcceptedData()
{
    _modified = true;
}

void QtColorPicker::RValueChanged(int value)
{
    _rgb[0] = static_cast<double>(value/255.0);
    MakeColor();
}

void QtColorPicker::GValueChanged(int value)
{
    _rgb[1] = static_cast<double>(value/255.0);
    MakeColor();
}

void QtColorPicker::BValueChanged(int value)
{
    _rgb[2] = static_cast<double>(value/255.0);
    MakeColor();
}

void QtColorPicker::MakeColor()
{
    double rgba[4];
    double temprgba[4];
    bool match = false;
    
    QPixmap icon(172,31);
    QColor color;
    color.setRgbF(_rgb[0], _rgb[1], _rgb[2], 1);
    icon.fill(color);
    
    colorlabel->setPixmap(icon);
    
    // make a dummy vtklookuptable with one value and insert the double values and get the double values
    // back, this way we will know the values that the vtk lib will use and we will try to see if there is 
    // a match, doesn't work well if we try to match our doubles with the ones from the table without this
    vtkSmartPointer<vtkLookupTable> temptable = vtkSmartPointer<vtkLookupTable>::New();
    temptable->SetTableValue(0, _rgb[0], _rgb[1], _rgb[2], 1.0);
    temptable->GetTableValue(0, temprgba);
    
    for (int i = 0; i < _data->GetNumberOfColors(); i++)
    {
        _data->GetColorComponents(i, rgba);
        if ((rgba[0] == temprgba[0]) && (rgba[1] == temprgba[1]) && (rgba[2] == temprgba[2])) 
            match = true;
    }
    
    if (match == true)
    {
        buttonBox->setEnabled(false);
        label->setText("Color already in use");
    }
    else
    {
        buttonBox->setEnabled(true);
        label->setText("Color selected");
    }
}

Vector3d QtColorPicker::GetColor()
{
    return _rgb;
}
