///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: SaveSession.cxx
// Purpose: Thread for saving a editor session
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// qt includes
#include <QString>
#include <QDir>
#include <QSettings>

// itk includes
#include <itkSmartPointer.h>
#include <itkImageFileWriter.h>
#include <itkMetaImageIO.h>

// project includes
#include "SaveSession.h"
#include "DataManager.h"
#include "Metadata.h"
#include "QtGui.h"

// c++ includes
#include <fstream>

///////////////////////////////////////////////////////////////////////////////////////////////////
// SaveSessionThread class
//
SaveSessionThread::SaveSessionThread(EspinaVolumeEditor* parent)
{
	moveToThread(this);
	_parent = parent;
	_dataManager = NULL;
	_editorOperations = NULL;
	_metadata = NULL;
	connect(this, SIGNAL(finished()), parent, SLOT(SaveSessionEnd()), Qt::QueuedConnection);
	connect(this, SIGNAL(startedSaving()), parent, SLOT(SaveSessionStart()), Qt::QueuedConnection);
	connect(this, SIGNAL(progress(int)), parent, SLOT(SaveSessionProgress(int)), Qt::QueuedConnection);
}

SaveSessionThread::~SaveSessionThread(void)
{
	// empty
}

void SaveSessionThread::run()
{
	_dataManager = _parent->_dataManager;
	_editorOperations = _parent->_editorOperations;
	_metadata = _parent->_fileMetadata;

	// errors are handled by thread->exec() so we don't really need to check the returning value of exec()
	this->exec();
}

int SaveSessionThread::exec()
{
	std::string homedir = std::string(getenv("HOME"));
	std::string username = std::string(getenv("USER"));
	std::string baseFilename = homedir + std::string("/.espinaeditor-") + username;
	std::string temporalFilename = baseFilename + std::string(".session");
	std::string temporalFilenameMHA = baseFilename + std::string(".mha");

	// needed to save the program state in the right moment so we wait for the lock to be open. Grab the lock and notify of action.
	QMutexLocker locker(_parent->actionLock);
	emit startedSaving();

	QFile file(QString(temporalFilename.c_str()));
	if(true == file.exists())
		if (false == file.remove())
		{
			QMessageBox msgBox;
			msgBox.setCaption("Error saving session");
			msgBox.setIcon(QMessageBox::Critical);
			msgBox.setText("An error occurred saving the editor session file.\nThe operation has been aborted.");
			msgBox.setDetailedText("Previous session file exists but couldn't be removed.");
			msgBox.exec();
			return -1;
		}

	QFile fileMHA(QString(temporalFilenameMHA.c_str()));
	if(true == fileMHA.exists())
		if (false == fileMHA.remove())
		{
			QMessageBox msgBox;
			msgBox.setCaption("Error saving session");
			msgBox.setIcon(QMessageBox::Critical);
			msgBox.setText("An error occurred saving the editor session file.\nThe operation has been aborted.");
			msgBox.setDetailedText("Previous MHA session file exists but couldn't be removed.");
			msgBox.exec();
			return -2;
		}

    itk::SmartPointer<ImageType> image = ImageType::New();
    image = _editorOperations->m_selection->GetItkImage();

    // save as an mha
    typedef itk::ImageFileWriter<ImageType> WriterType;
    itk::SmartPointer<itk::MetaImageIO> io = itk::MetaImageIO::New();
    io->SetFileName(temporalFilenameMHA.c_str());
    itk::SmartPointer<WriterType> writer = WriterType::New();
    writer->SetImageIO(io);
    writer->SetFileName(temporalFilenameMHA.c_str());
    writer->SetInput(image);
    writer->UseCompressionOn();

    try
    {
    	writer->Write();
    }
	catch (itk::ExceptionObject &excp)
	{
	    QMessageBox msgBox;
	    msgBox.setCaption("Error saving session");
	    msgBox.setIcon(QMessageBox::Critical);
		msgBox.setText("An error occurred saving the editor MHA session file.\nThe operation has been aborted.");
		msgBox.setDetailedText(excp.what());
		msgBox.exec();
		return -3;
	}

	emit progress(50);

	std::ofstream outfile;
	outfile.open(temporalFilename.c_str(), std::ofstream::out|std::ofstream::trunc|std::ofstream::binary);

	if (!outfile.is_open())
	{
		QMessageBox msgBox;
		msgBox.setCaption("Error saving session");
		msgBox.setIcon(QMessageBox::Critical);
		msgBox.setText("An error occurred saving the editor session file.\nThe operation has been aborted.");
		msgBox.setDetailedText("Couldn't open session file for writing.");
		msgBox.exec();
		return -4;
	}

	// dump all relevant data and objects to file, first the size of the std::map or std::vector, the objects
	// themselves and also all the relevant data.
	// the order:
	//		- EspinaEditor relevant data: POI and file names...
	//		- metadata ObjectMetadata
	//		- metadata CountingBrickMetadata
	//		- metadata SegmentMetadata
	//		- metadata relevant data: hasUnassignedTag, unassignedTadPosition
	// 		- datamanager ObjectInformation
	// in the editor we must follow the same order while loading this data, obviously...

	// EspinaEditor relevant data
	unsigned short size = _parent->_segmentationFileName.size();
	write(outfile, size);
	outfile << _parent->_segmentationFileName;

	write(outfile, _parent->_hasReferenceImage);
	if (_parent->_hasReferenceImage)
	{
		size = _parent->_referenceFileName.size();
		write(outfile, size);
		outfile << _parent->_referenceFileName;
	}

	write(outfile, _parent->_POI[0]);
	write(outfile, _parent->_POI[1]);
	write(outfile, _parent->_POI[2]);

	// Metadata::ObjectMetadata std::vector dump
	size = _metadata->ObjectVector.size();
	write(outfile, size);

	std::vector<struct Metadata::ObjectMetadata>::iterator objmetait;
	for (objmetait = _metadata->ObjectVector.begin(); objmetait != _metadata->ObjectVector.end(); objmetait++)
	{
		struct Metadata::ObjectMetadata object = (*objmetait);
		write(outfile, object.scalar);
		write(outfile, object.segment);
		write(outfile, object.selected);
		// we could write the whole structure just by doing write(outfile, object) but i don't want to write down the
		// "used" bool field, i already know it's true
	}

	// Metadata::CountingBrickMetadata std::vector dump
	size = _metadata->CountingBrickVector.size();
	write(outfile, size);

	std::vector<struct Metadata::CountingBrickMetadata>::iterator brickit;
	for (brickit = _metadata->CountingBrickVector.begin(); brickit != _metadata->CountingBrickVector.end(); brickit++)
	{
		struct Metadata::CountingBrickMetadata object = (*brickit);
		write(outfile, object.inclusive[0]);
		write(outfile, object.inclusive[1]);
		write(outfile, object.inclusive[2]);
		write(outfile, object.exclusive[0]);
		write(outfile, object.exclusive[1]);
		write(outfile, object.exclusive[2]);
	}

	// Metadata::SegmentMetadata std::vector dump
	size = _metadata->SegmentVector.size();
	write(outfile, size);

	std::vector<struct Metadata::SegmentMetadata>::iterator segit;
	for (segit = _metadata->SegmentVector.begin(); segit != _metadata->SegmentVector.end(); segit++)
	{
		struct Metadata::SegmentMetadata object = (*segit);
		write(outfile, object.color[0]);
		write(outfile, object.color[1]);
		write(outfile, object.color[2]);
		write(outfile, object.value);
		size = object.name.size();
		write(outfile, size);
		outfile << object.name;
	}

	// Metadata relevant data
	write(outfile, _metadata->hasUnassignedTag);
	write(outfile, _metadata->unassignedTagPosition);

	// DataManager::ObjectInformation std::map dump
	size = _dataManager->ObjectVector.size();
	write(outfile, size);

	std::map<unsigned short, struct DataManager::ObjectInformation*>::iterator objit;
	for (objit = _dataManager->ObjectVector.begin(); objit != _dataManager->ObjectVector.end(); objit++)
	{
		unsigned short int position = (*objit).first;
		struct DataManager::ObjectInformation *object = (*objit).second;
		write(outfile, position);
		write(outfile, object->scalar);
		write(outfile, object->sizeInVoxels);
		write(outfile, object->centroid[0]);
		write(outfile, object->centroid[1]);
		write(outfile, object->centroid[2]);
		write(outfile, object->min[0]);
		write(outfile, object->min[1]);
		write(outfile, object->min[2]);
		write(outfile, object->max[0]);
		write(outfile, object->max[1]);
		write(outfile, object->max[2]);
	}

	outfile.close();
	emit progress(100);

	// store labels
	QSettings editorSettings("UPM", "Espina Volume Editor");
	QString filename(temporalFilename.c_str());
	filename.replace(QChar('/'), QChar('\\'));
	editorSettings.beginGroup("Editor");

	std::set<unsigned short> labelIndexes = this->_dataManager->GetSelectedLabelsSet();
	std::set<unsigned short> labelScalars;

	for (std::set<unsigned short>::iterator it = labelIndexes.begin(); it != labelIndexes.end(); it++)
		labelScalars.insert(this->_dataManager->GetScalarForLabel(*it));

	QList<QVariant> labelList;
	for (std::set<unsigned short>::iterator it = labelScalars.begin(); it != labelScalars.end(); it++)
		labelList.append(static_cast<int>(*it));

	QVariant variant;
	variant.setValue(labelList);
	editorSettings.setValue(filename, variant);

	// everything went fine
	return 0;
}
