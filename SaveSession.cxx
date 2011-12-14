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

// itk includes
#include <itkSmartPointer.h>
#include <itkImageFileWriter.h>
#include <itkMetaImageIO.h>

// project includes
#include "SaveSession.h"
#include "DataManager.h"
#include "QtGui.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// SaveSessionThread class
//
SaveSessionThread::SaveSessionThread(EspinaVolumeEditor* parent)
{
	moveToThread(this);
	_parent = parent;
	_dataManager = parent->_dataManager;
	_editorOperations = parent->_editorOperations;
	_metadata = parent->_fileMetadata;
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
	this->exec();
}

int SaveSessionThread::exec()
{
	QString userTempDir = QDir::tempPath();
	std::string username = std::string(getenv("USER"));
	std::string temporalFilename = userTempDir.toStdString() + std::string("/espinaeditor-") + username + std::string(".session");

	// needed to save the program state in the right moment so we wait for the lock to be open. Grab the lock and notify of action.
	QMutexLocker locker(_parent->actionLock);
	emit startedSaving();

	QFile file(QString(temporalFilename.c_str()));
	if(file.exists())
		if (!file.remove())
		{
			QMessageBox msgBox;
			msgBox.setIcon(QMessageBox::Critical);
			msgBox.setText("An error occurred saving the editor session file.\nThe operation has been aborted.");
			msgBox.setDetailedText("Previous session file exists but couldn't be removed.");
			msgBox.exec();
			return -1;
		}

	if (!file.open(QIODevice::WriteOnly|QIODevice::Truncate))
	{
		QMessageBox msgBox;
		msgBox.setIcon(QMessageBox::Critical);
		msgBox.setText("An error occurred saving the editor session file.\nThe operation has been aborted.");
		msgBox.setDetailedText("Error trying to truncate and open the session file for writing.");
		msgBox.exec();
		return -2;
	}
	file.close();

    itk::SmartPointer<ImageType> image = ImageType::New();
    image = _editorOperations->GetItkImageFromSelection(0,0);

    // save as an mha and rename
    std::string mhatemporalFilename = temporalFilename + std::string(".mha");
    typedef itk::ImageFileWriter<ImageType> WriterType;
    itk::SmartPointer<itk::MetaImageIO> io = itk::MetaImageIO::New();
    io->SetFileName(mhatemporalFilename.c_str());
    itk::SmartPointer<WriterType> writer = WriterType::New();
    writer->SetImageIO(io);
    writer->SetFileName(mhatemporalFilename.c_str());
    writer->SetInput(image);
    writer->UseCompressionOn();

    try
    {
    	writer->Write();
    }
	catch (itk::ExceptionObject &excp)
	{
	    QMessageBox msgBox;
	    msgBox.setIcon(QMessageBox::Critical);
		msgBox.setText("An error occurred saving the editor session file.\nThe operation has been aborted.");
		msgBox.setDetailedText(excp.what());
		msgBox.exec();
		return -3;
	}

	emit progress(33);

	if (0 != (rename(mhatemporalFilename.c_str(), temporalFilename.c_str())))
	{
	    QMessageBox msgBox;
	    msgBox.setIcon(QMessageBox::Critical);
		msgBox.setText("An error occurred saving the editor session file.\nThe operation has been aborted.");
		msgBox.setDetailedText(QString("The temporal file couldn't be renamed."));
		msgBox.exec();

		if (0 != (remove(mhatemporalFilename.c_str())))
		{
		    QMessageBox msgBox;
		    msgBox.setIcon(QMessageBox::Critical);
		    msgBox.setText("An error occurred saving the editor session file.\nThe operation has been aborted.");
			msgBox.setDetailedText(QString("The temporal file couldn't be deleted after an error renaming it."));
			msgBox.exec();
		}
		return -4;
	}

	if (!file.open(QIODevice::Append|QIODevice::Text))
	{
		QMessageBox msgBox;
		msgBox.setIcon(QMessageBox::Critical);
		msgBox.setText("An error occurred saving the editor session file.\nThe operation has been aborted.");
		msgBox.setDetailedText("Error trying to open session file to write metadata.");
		msgBox.exec();
		return -5;
	}

	if (false == file.seek(file.size()))
		return -6;

	QTextStream out(&file);

	out << "\n";

	std::map<unsigned short, struct DataManager::ObjectInformation*>::iterator it;

	for (it = _dataManager->ObjectVector.begin(); it != _dataManager->ObjectVector.end(); it++)
	{
		out << "Object " << (*it).first << " Scalar " << (*it).second->scalar;
		out << " Centroid [" << (*it).second->centroid[0] << ","<< (*it).second->centroid[1] << "," << (*it).second->centroid[2] << "] ";
		out << " BBMax [" << (*it).second->max[0] << "," << (*it).second->max[1] << "," << (*it).second->max[2] << "]";
		out << " BBMin [" << (*it).second->min[0] << "," << (*it).second->min[1] << "," << (*it).second->min[2] << "]";
		out << " Size " << (*it).second->sizeInVoxels << "\n";
	}
	file.close();

	emit progress(66);

	if (!_metadata->Write(QString(temporalFilename.c_str()), _dataManager))
		return -7;

	emit progress(100);

	// everything went fine
	return 0;
}
