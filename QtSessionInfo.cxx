///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtSessionInfo.h
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
QtSessionInfo::QtSessionInfo(QWidget *parent_Widget, Qt::WindowFlags f) : QDialog(parent_Widget, f)
{
	this->
	setupUi(this); // this sets up GUI

	this->referencePathTitleLabel->hide();
	this->referencePathValueLabel->hide();
	this->referenceTitleLabel->hide();
	this->referenceValueLabel->hide();

	// want to make the dialog appear centered
    this->move(parent_Widget->geometry().center() - this->rect().center());
    this->resize(this->minimumSizeHint());
    this->layout()->setSizeConstraint( QLayout::SetFixedSize );
}

QtSessionInfo::~QtSessionInfo()
{
	// empty, nothing to be freed
}

void QtSessionInfo::SetFileInfo(const QFileInfo *fileInfo)
{
	this->nameLabel->setText(fileInfo->fileName());
	this->pathLabel->setText(fileInfo->path());
}

void QtSessionInfo::SetSpacing(const Vector3d spacing)
{
    std::stringstream out;
    out << "[ " << spacing[0] << " , " << spacing[1] << " , " << spacing[2] << " ] ";
    this->spacingLabel->setText(out.str().c_str());
}

void QtSessionInfo::SetDimensions(const Vector3ui dimensions)
{
    std::stringstream out;
    out << "[ " << dimensions[0] << " , " << dimensions[1] << " , " << dimensions[2] << " ] ";
    this->dimensionsLabel->setText(out.str().c_str());
}

void QtSessionInfo::SetNumberOfSegmentations(const unsigned int value)
{
    std::stringstream out;
    out << value;
    this->numberLabel->setText(out.str().c_str());
}

void QtSessionInfo::SetReferenceFileInfo(const QFileInfo *fileInfo)
{
	this->referencePathTitleLabel->show();
	this->referencePathValueLabel->show();
	this->referenceTitleLabel->show();
	this->referenceValueLabel->show();

	this->referenceValueLabel->setText(fileInfo->fileName());
	this->referencePathValueLabel->setText(fileInfo->path());

	// dialog size has changed because of the labels shown now, recalculate center
	QRect rect = QApplication::activeWindow()->geometry();
	this->move(rect.center() - this->contentsRect().center());
	this->resize(this->minimumSizeHint());
	this->layout()->setSizeConstraint( QLayout::SetFixedSize );
}

void QtSessionInfo::SetDirectionCosineMatrix(const Matrix3d matrix)
{
	Vector3d vector = matrix[0];

    std::stringstream out;
    out << std::setprecision(3) << std::fixed << std::noshowpoint;
    out << vector[0] << "   " << vector[1] << "   " << vector[2];
    this->vector1Label->setText(out.str().c_str());
    out.str(std::string());

    vector = matrix[1];
    out << vector[0] << "   " << vector[1] << "   " << vector[2];
    this->vector2Label->setText(out.str().c_str());
    out.str(std::string());

    vector = matrix[2];
    out << vector[0] << "   " << vector[1] << "   " << vector[2];
    this->vector3Label->setText(out.str().c_str());
    out.str(std::string());
}
