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

// project includes
#include "SaveSession.h"
#include "DataManager.h"
#include "QtGui.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// SaveSessionThread class
//
SaveSessionThread::SaveSessionThread(EspinaVolumeEditor* parent, DataManager* data)
{
	moveToThread(this);
	_parent = parent;
	_dataManager = data;
	connect(this, SIGNAL(finished()), parent, SLOT(SaveSessionEnd()), Qt::QueuedConnection);
	connect(this, SIGNAL(started()), parent, SLOT(SaveSessionStart()), Qt::QueuedConnection);
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
	std::string temporalFilename = userTempDir.toStdString() + std::string("/espinaeditorsession-") + username + std::string(".temp");

	// needed to save the program state in the right moment
	QMutexLocker locker(_parent->actionLock);

	std::cout << "dormido\n";
	sleep(2);
	std::cout << "tempfilename: " << temporalFilename << std::endl;
	emit progress(50);
	sleep(3);
	std::cout << "despierto\n";
	return 0;
}



