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
SaveSessionThread::SaveSessionThread(EspinaVolumeEditor* parent, DataManager* data, EditorOperations* editor)
{
	moveToThread(this);
	_parent = parent;
	_dataManager = data;
	_editorOperations = editor;
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
			msgBox.setDetailedText("Previous session file exists but couldn't be removed for the new one.");
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
			msgBox.setDetailedText(QString("The temporal file couldn't be deleted."));
			msgBox.exec();
		}
		return -4;
	}

	if (!file.open(QIODevice::WriteOnly|QIODevice::Text))
	{
		QMessageBox msgBox;
		msgBox.setIcon(QMessageBox::Critical);
		msgBox.setText("An error occurred saving the editor session file.\nThe operation has been aborted.");
		msgBox.setDetailedText("Error trying to open session file to write metadata.");
		msgBox.exec();
		return -5;
	}

	if (false == file.seek(file.size()))
		return false;

	// TODO: continuar y terminar con el save session
	// TODO: aumentar la opacidad por defecto al cargar las segmentaciones
	// TODO: quitar/poner las segmentaciones con la barra espaciadora

	std::cout << "dormido\n";
	sleep(2);
	std::cout << "tempfilename: " << temporalFilename << std::endl;
	emit progress(50);
	sleep(3);
	std::cout << "despierto\n";
	return 0;
}



