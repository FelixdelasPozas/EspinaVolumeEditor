///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtPreferences.cxx
// Purpose: modal dialog for configuring preferences
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

// Qt includes
#include "QtPreferences.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtPreferences class
//
QtPreferences::QtPreferences(QWidget *p, Qt::WindowFlags f) : QDialog(p,f)
{
    setupUi(this); // this sets up GUI
    _modified = false;
    
    _undoSize = 0;
    _undoCapacity = 0;
    _filtersRadius = 1;
    _watershedLevel = 0.0;
    _segmentationOpacity = 0;
    _saveTime = 0;
}

QtPreferences::~QtPreferences()
{
    // empty, nothing to be freed
}

void QtPreferences::SetInitialOptions(unsigned long int size, unsigned long int capacity, unsigned int radius, double level, int opacity, unsigned int saveTime, bool saveEnabled)
{
    _undoSize = size;
    _undoCapacity = capacity;
    _filtersRadius = radius;
    _watershedLevel = level;
    _segmentationOpacity = opacity;
    _saveTime = saveTime / (60 * 1000);

    if (!saveEnabled)
    	saveSessionBox->setChecked(false);

    capacityBar->setValue(static_cast<int>((static_cast<float>(_undoCapacity)/static_cast<float>(_undoSize))*100.0));
    sizeBox->setValue(static_cast<int>(_undoSize/(1024*1024)));
    radiusBox->setValue(_filtersRadius);
    levelBox->setValue(_watershedLevel);
    opacityBox->setValue(_segmentationOpacity);
    saveTimeBox->setValue(_saveTime);

    // configure widgets
    connect(sizeBox, SIGNAL(valueChanged(int)), this, SLOT(SelectSize(int)));
    connect(radiusBox, SIGNAL(valueChanged(int)), this, SLOT(SelectRadius(int)));
    connect(opacityBox, SIGNAL(valueChanged(int)), this, SLOT(SelectOpacity(int)));
    connect(levelBox, SIGNAL(valueChanged(double)), this, SLOT(SelectLevel(double)));
    connect(saveTimeBox, SIGNAL(valueChanged(int)), this, SLOT(SelectSaveTime(int)));
    
    connect(acceptbutton, SIGNAL(accepted()), this, SLOT(AcceptedData()));
}

void QtPreferences::EnableVisualizationBox(void)
{
	visualizationGroupBox->setEnabled(true);
}

unsigned long int QtPreferences::GetSize()
{
    return _undoSize;
}

unsigned int QtPreferences::GetRadius()
{
    return _filtersRadius;
}

double QtPreferences::GetLevel()
{
    return _watershedLevel;
}

unsigned int QtPreferences::GetSegmentationOpacity()
{
	return _segmentationOpacity;
}

unsigned int QtPreferences::GetSaveSessionTime()
{
	return _saveTime;
}

void QtPreferences::SelectSize(int value)
{
    _undoSize = value * 1024 * 1024;
}

void QtPreferences::SelectRadius(int value)
{
    _filtersRadius = static_cast<unsigned int>(value);
}

void QtPreferences::SelectOpacity(int value)
{
    _segmentationOpacity = static_cast<unsigned int>(value);
}

void QtPreferences::SelectLevel(double value)
{
    _watershedLevel = value;
}

void QtPreferences::SelectSaveTime(int value)
{
	_saveTime = value;
}

void QtPreferences::AcceptedData()
{
    _modified = true;
}

bool QtPreferences::ModifiedData()
{
    return _modified;
}

bool QtPreferences::GetSaveSessionEnabled()
{
	return saveSessionBox->isChecked();
}
