///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtRelabel.cpp
// Purpose: modal dialog for configuring relabel options 
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

// c++ includes
#include <sstream>

// Qt includes
#include <QListWidget>
#include <QListWidgetItem>

// project includes
#include "QtRelabel.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtRelabel class
//
QtRelabel::QtRelabel(QWidget *parent, Qt::WindowFlags f)
: QDialog(parent, f)
, m_modified {false}
, m_newlabel {false}
, m_maxcolors{0}
{
  setupUi(this);

  move(parent->geometry().center() - rect().center());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtRelabel::setInitialOptions(const std::set<unsigned short> labels, std::shared_ptr<Metadata> data, std::shared_ptr<DataManager> dataManager)
{
  newlabelbox->setSelectionMode(QAbstractItemView::SingleSelection);
  m_maxcolors = dataManager->GetNumberOfLabels();

  // add box items. Color 0 is background, so we will start from 1
  newlabelbox->insertItem(0, "Background");

  for (unsigned int i = 1; i < m_maxcolors; i++)
  {
    QPixmap icon(16, 16);
    auto color = dataManager->GetColorComponents(i);
    icon.fill(color);
    auto text = QString("%1 %2").arg(QString(data->objectSegmentName(i))).arg(dataManager->GetScalarForLabel(i));
    QListWidgetItem *item = new QListWidgetItem(QIcon(icon), text);
    newlabelbox->addItem(item);

    // hide already hidden/deleted labels
    if (0LL == dataManager->GetNumberOfVoxelsForLabel(i))
    {
      item->setHidden(true);
    }
  }

  newlabelbox->addItem("New label");
  newlabelbox->setCurrentRow(m_maxcolors);

  // fill selection label text and hide label if user selected just one label
  if (labels.size() > 1)
  {
    selectionlabel->setText("Volume with multiple labels");
  }
  else
  {
    unsigned int tempLabel = 0;

    if (labels.size() == 1)
    {
      tempLabel = labels.begin().operator *();

      QPixmap icon(16, 16);
      auto color = dataManager->GetColorComponents(tempLabel);
      icon.fill(color);
      auto text = QString("%1 %2").arg(QString(data->objectSegmentName(tempLabel))).arg(dataManager->GetScalarForLabel(tempLabel));
      colorlabel->setPixmap(icon);
      selectionlabel->setText(text);
      newlabelbox->item(tempLabel)->setHidden(true);
    }
    else
    {
      selectionlabel->setText("Background voxels");
      newlabelbox->item(0)->setHidden(true);
    }
  }

  connect(acceptbutton, SIGNAL(accepted()), this, SLOT(AcceptedData()));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool QtRelabel::isModified() const
{
  return m_modified;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned short QtRelabel::selectedLabel() const
{
  return static_cast<unsigned short int>(newlabelbox->currentRow());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtRelabel::AcceptedData()
{
  if (selectedLabel() == m_maxcolors) m_newlabel = true;

  m_modified = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool QtRelabel::isNewLabel() const
{
  return m_newlabel;
}
