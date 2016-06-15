///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtColorPicker.cpp
// Purpose: pick a color by selecting it's RGB components 
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

// Qt includes
#include "QtColorPicker.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtColorPicker class
//
QtColorPicker::QtColorPicker(QWidget *parent, Qt::WindowFlags f)
: QDialog   {parent, f}
, m_modified{false}
, m_data    {nullptr}
, m_color   {QColor::fromRgbF(0.5,0.5,0.5)}
{
  setupUi(this); // this sets up GUI

  Rslider->setSliderPosition(m_color.red());
  Gslider->setSliderPosition(m_color.green());
  Bslider->setSliderPosition(m_color.blue());

  connect(Rslider, SIGNAL(valueChanged(int)), this, SLOT(RValueChanged(int)));
  connect(Gslider, SIGNAL(valueChanged(int)), this, SLOT(GValueChanged(int)));
  connect(Bslider, SIGNAL(valueChanged(int)), this, SLOT(BValueChanged(int)));

  connect(buttonBox, SIGNAL(accepted()), this, SLOT(AcceptedData()));

  // want to make the dialog appear centered
  move(parent->geometry().center() - rect().center());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtColorPicker::SetInitialOptions(std::shared_ptr<DataManager> data)
{
  m_data = data;
  MakeColor();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool QtColorPicker::ModifiedData() const
{
  return m_modified;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtColorPicker::AcceptedData()
{
  m_modified = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtColorPicker::RValueChanged(int value)
{
  m_color.setRed(value);
  MakeColor();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtColorPicker::GValueChanged(int value)
{
  m_color.setGreen(value);
  MakeColor();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtColorPicker::BValueChanged(int value)
{
  m_color.setBlue(value);
  MakeColor();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtColorPicker::MakeColor()
{
  double rgba[4];
  double temprgba[4];
  bool match = false;

  QPixmap icon(172, 31);
  icon.fill(m_color);

  colorlabel->setPixmap(icon);

  for (unsigned int i = 0; i < m_data->GetNumberOfColors(); i++)
  {
    if(m_color == m_data->GetColorComponents(i))
    {
      match = true;
      break;
    }
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

///////////////////////////////////////////////////////////////////////////////////////////////////
const QColor QtColorPicker::GetColor() const
{
  return m_color;
}
