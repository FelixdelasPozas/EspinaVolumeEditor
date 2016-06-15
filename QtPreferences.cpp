///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtPreferences.cpp
// Purpose: modal dialog for configuring preferences
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

// Qt includes
#include "QtPreferences.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtPreferences class
//
QtPreferences::QtPreferences(QWidget *parent, Qt::WindowFlags f)
: QDialog         {parent, f}
, m_undoSize      {0}
, m_undoCapacity  {0}
, m_filtersRadius {1}
, m_brushRadius   {1}
, m_watershedLevel{0.5}
, m_opacity       {100}
, m_saveTime      {0}
, m_modified      {false}
{
  setupUi(this); // this sets up GUI

  // want to make the dialog appear centered
  move(parent->geometry().center() - rect().center());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtPreferences::SetInitialOptions(const unsigned long int size,
                                      const unsigned long int capacity,
                                      const unsigned int      radius,
                                      const double            level,
                                      const int               opacity,
                                      const unsigned int      saveTime,
                                      bool                    saveEnabled,
                                      const unsigned int      paintRadius)
{
  m_undoSize       = size;
  m_undoCapacity   = capacity;
  m_filtersRadius  = radius;
  m_watershedLevel = level;
  m_opacity        = opacity;
  m_saveTime       = saveTime / (60 * 1000);
  m_brushRadius    = paintRadius;

  if (!saveEnabled)
  {
    saveSessionBox->setChecked(false);
  }

  capacityBar   ->setValue(static_cast<int>((static_cast<float>(m_undoCapacity) / static_cast<float>(m_undoSize)) * 100.0));
  sizeBox       ->setValue(static_cast<int>(m_undoSize / (1024 * 1024)));
  radiusBox     ->setValue(m_filtersRadius);
  levelBox      ->setValue(m_watershedLevel);
  opacityBox    ->setValue(m_opacity);
  saveTimeBox   ->setValue(m_saveTime);
  paintRadiusBox->setValue(m_brushRadius);

  // configure widgets
  connect(sizeBox, SIGNAL(valueChanged(int)), this, SLOT(SelectSize(int)));
  connect(radiusBox, SIGNAL(valueChanged(int)), this, SLOT(SelectRadius(int)));
  connect(opacityBox, SIGNAL(valueChanged(int)), this, SLOT(SelectOpacity(int)));
  connect(levelBox, SIGNAL(valueChanged(double)), this, SLOT(SelectLevel(double)));
  connect(saveTimeBox, SIGNAL(valueChanged(int)), this, SLOT(SelectSaveTime(int)));
  connect(paintRadiusBox, SIGNAL(valueChanged(int)), this, SLOT(SelectPaintEraseRadius(int)));

  connect(acceptbutton, SIGNAL(accepted()), this, SLOT(AcceptedData()));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtPreferences::enableVisualizationBox(void)
{
  visualizationGroupBox->setEnabled(true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned long int QtPreferences::size() const
{
  return m_undoSize;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned int QtPreferences::radius() const
{
  return m_filtersRadius;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const double QtPreferences::level() const
{
  return m_watershedLevel;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned int QtPreferences::opacity() const
{
  return m_opacity;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned int QtPreferences::autoSaveInterval() const
{
  return m_saveTime;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtPreferences::SelectSize(int value)
{
  m_undoSize = value * 1024 * 1024;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtPreferences::SelectRadius(int value)
{
  m_filtersRadius = static_cast<unsigned int>(value);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtPreferences::SelectOpacity(int value)
{
  m_opacity = static_cast<unsigned int>(value);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtPreferences::SelectLevel(double value)
{
  m_watershedLevel = value;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtPreferences::SelectSaveTime(int value)
{
  m_saveTime = value;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtPreferences::SelectPaintEraseRadius(int value)
{
  m_brushRadius = static_cast<unsigned int>(value);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtPreferences::AcceptedData()
{
  m_modified = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool QtPreferences::isModified() const
{
  return m_modified;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool QtPreferences::isAutoSaveEnabled() const
{
  return saveSessionBox->isChecked();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
unsigned int QtPreferences::brushRadius() const
{
  return m_brushRadius;
}
