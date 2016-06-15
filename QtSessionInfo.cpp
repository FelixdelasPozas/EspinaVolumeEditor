///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtSessionInfo.cpp
// Purpose: session information dialog
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

// Qt includes
#include "QtSessionInfo.h"

// std includes
#include <sstream>
#include <iomanip>

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtSessionInfo class
//
QtSessionInfo::QtSessionInfo(QWidget *parent, Qt::WindowFlags f)
: QDialog{parent, f}
{
  setupUi(this);

  referencePathTitleLabel->hide();
  referencePathValueLabel->hide();
  referenceTitleLabel->hide();
  referenceValueLabel->hide();

  // want to make the dialog appear centered
  move(parent->geometry().center() - rect().center());
  resize(minimumSizeHint());
  layout()->setSizeConstraint(QLayout::SetFixedSize);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtSessionInfo::SetFileInfo(const QFileInfo &fileInfo)
{
  nameLabel->setText(fileInfo.fileName());
  pathLabel->setText(fileInfo.path());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtSessionInfo::SetSpacing(const Vector3d &spacing)
{
  auto text = QString("[%1 , %2 , %3]").arg(spacing[0]).arg(spacing[1]).arg(spacing[2]);
  spacingLabel->setText(text);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtSessionInfo::SetDimensions(const Vector3ui &dimensions)
{
  auto text = QString("[%1 , %2 , %3]").arg(dimensions[0]).arg(dimensions[1]).arg(dimensions[2]);
  dimensionsLabel->setText(text);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtSessionInfo::SetNumberOfSegmentations(const unsigned int segNum)
{
  numberLabel->setText(QString().number(segNum));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtSessionInfo::SetReferenceFileInfo(const QFileInfo &fileInfo)
{
  referencePathTitleLabel->show();
  referencePathValueLabel->show();
  referenceTitleLabel->show();
  referenceValueLabel->show();

  referenceValueLabel->setText(fileInfo.fileName());
  referencePathValueLabel->setText(fileInfo.path());

  // dialog size has changed because of the labels shown now, recalculate center
  QRect rect = QApplication::activeWindow()->geometry();
  move(rect.center() - contentsRect().center());
  resize(minimumSizeHint());
  layout()->setSizeConstraint(QLayout::SetFixedSize);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void QtSessionInfo::SetDirectionCosineMatrix(const Matrix3d &matrix)
{
  auto vector = matrix[0];
  auto text = QString("%1  %2  %3").arg(vector[0]).arg(vector[1]).arg(vector[2]);
  vector1Label->setText(text);

  vector = matrix[1];
  text = QString("%1  %2  %3").arg(vector[0]).arg(vector[1]).arg(vector[2]);
  vector2Label->setText(text);

  vector = matrix[2];
  text = QString("%1  %2  %3").arg(vector[0]).arg(vector[1]).arg(vector[2]);
  vector3Label->setText(text);
}
