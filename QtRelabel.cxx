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
    _maxcolors = dataManager->GetNumberOfLabels();
    _multipleLabels = (labels.size() > 1);

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
		}

		newlabelbox->addItem("New label");
		newlabelbox->setCurrentRow(_maxcolors);
	}
    else
    {
    	unsigned int tempLabel = 0;

		if (labels.size() == 1)
		{
			std::set<unsigned short>::iterator it = labels.begin();
			tempLabel = static_cast<unsigned int>(*it);

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

		// color 0 is black, so we will start from 1
		for (unsigned int i = 1; i < _maxcolors; i++)
		{
			// avoid showing the selected color
			if (i == tempLabel)
				continue;

			dataManager->GetColorComponents(i, rgba);
			color.setRgbF(rgba[0], rgba[1], rgba[2], 1);
			icon.fill(color);
			std::stringstream out;
			out << data->GetObjectSegmentName(i) << " " << dataManager->GetScalarForLabel(i);
			std::string text = out.str();
			QListWidgetItem *item = new QListWidgetItem(QIcon(icon), QString(text.c_str()));
			newlabelbox->addItem(item);
		}
		newlabelbox->addItem("New label");
		newlabelbox->setCurrentRow(_maxcolors-1);
	}
    
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
    if (_multipleLabels)
    {
    	_selectedLabel = newlabelbox->currentRow();
    	if (_selectedLabel == _maxcolors)
    		_newlabel = true;
    }
    else
    {
		if (newlabelbox->currentRow() < _selectedLabel)
			_selectedLabel = newlabelbox->currentRow();
		else
			_selectedLabel = newlabelbox->currentRow() + 1;

	    if (_selectedLabel == _maxcolors)
	        _newlabel = true;
    }

    _modified = true;
}

bool QtRelabel::IsNewLabel()
{
    return _newlabel;
}
