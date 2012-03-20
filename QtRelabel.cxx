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

void QtRelabel::SetInitialOptions(std::set<unsigned short> labels, Metadata* data, DataManager* dataManager)
{
    double rgba[4];
    QPixmap icon(16,16);
    QColor color;

    newlabelbox->setSelectionMode(QAbstractItemView::SingleSelection);
    this->_maxcolors = dataManager->GetNumberOfLabels();
    this->_multipleLabels = (labels.size() > 1);

    if (labels.size() > 1)
	{
		selectionlabel->setText("Area with multiple labels");
		newlabelbox->insertItem(0, "Background");

		// color 0 is black, so we will start from 1
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
	}
    else
    {
    	unsigned int tempLabel = 0;

		if (labels.size() == 1)
		{
			tempLabel = labels.begin().operator *();

			newlabelbox->insertItem(0, "Background");
			dataManager->GetColorComponents(tempLabel, rgba);
			color.setRgbF(rgba[0], rgba[1], rgba[2], 1);
			icon.fill(color);
			std::stringstream out;
			out << data->GetObjectSegmentName(tempLabel) << " " << dataManager->GetScalarForLabel(tempLabel) << " voxels";
			std::string text = out.str();
			colorlabel->setPixmap(icon);
			selectionlabel->setText(QString(text.c_str()));
		}
		else
			selectionlabel->setText("Background voxels");

		// color 0 is background, so we will start from 1
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

			// hide already hidden/deleted labels and the selected label
			if (0LL == dataManager->GetNumberOfVoxelsForLabel(i) || (i == tempLabel))
				item->setHidden(true);
		}
		newlabelbox->addItem("New label");
		newlabelbox->setCurrentRow(_maxcolors);
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
