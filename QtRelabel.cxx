///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtRelabel.cxx
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
QtRelabel::QtRelabel(QWidget *parent_Widget, Qt::WindowFlags f) : QDialog(parent_Widget,f)
{
    setupUi(this); // this sets up GUI
    _modified = false;
    _newlabel = false;
    _maxcolors = 0;
    _selectedLabel = 0;

    // want to make the dialog appear centered
	this->move(parent_Widget->geometry().center() - this->rect().center());
}

QtRelabel::~QtRelabel()
{
    // empty
}

void QtRelabel::SetInitialOptions(std::set<unsigned short> labels, Metadata* data, DataManager* dataManager)
{
    double rgba[4];
    QPixmap icon(16,16);
    QColor color;

    newlabelbox->setSelectionMode(QAbstractItemView::SingleSelection);
    this->_maxcolors = dataManager->GetNumberOfLabels();
    this->_multipleLabels = (labels.size() > 1);

    // add box items. Color 0 is background, so we will start from 1
	newlabelbox->insertItem(0, "Background");

	for (unsigned int i = 1; i < _maxcolors; i++)
	{
		dataManager->GetColorComponents(i, rgba);
		color.setRgbF(rgba[0], rgba[1], rgba[2], 1);
		icon.fill(color);
		std::stringstream out;
		out << data->GetObjectSegmentName(i) << " " << dataManager->GetScalarForLabel(i);
		std::string text = out.str();
		QListWidgetItem *item = new QListWidgetItem(QIcon(icon), QString(text.c_str()));
		newlabelbox->addItem(item);

		// hide already hidden/deleted labels
		if (0LL == dataManager->GetNumberOfVoxelsForLabel(i))
			item->setHidden(true);
	}

	newlabelbox->addItem("New label");
	newlabelbox->setCurrentRow(_maxcolors);

	// fill selection label text and hide label if user selected just one label
    if (labels.size() > 1)
		selectionlabel->setText("Volume with multiple labels");
    else
    {
    	unsigned int tempLabel = 0;

		if (labels.size() == 1)
		{
			tempLabel = labels.begin().operator *();

			dataManager->GetColorComponents(tempLabel, rgba);
			color.setRgbF(rgba[0], rgba[1], rgba[2], 1);
			icon.fill(color);
			std::stringstream out;
			out << data->GetObjectSegmentName(tempLabel) << " " << dataManager->GetScalarForLabel(tempLabel) << " voxels";
			std::string text = out.str();
			colorlabel->setPixmap(icon);
			selectionlabel->setText(QString(text.c_str()));
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

bool QtRelabel::ModifiedData()
{
    return this->_modified;
}

unsigned short QtRelabel::GetSelectedLabel()
{
    return static_cast<unsigned short int>(this->_selectedLabel);
}

void QtRelabel::AcceptedData()
{
    this->_selectedLabel = newlabelbox->currentRow();
    if (this->_selectedLabel == _maxcolors)
    		this->_newlabel = true;

    this->_modified = true;
}

bool QtRelabel::IsNewLabel()
{
    return this->_newlabel;
}
