///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: SaveSession.h
// Purpose: Thread for saving a editor session
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// qt includes
#include <QtGui>

///////////////////////////////////////////////////////////////////////////////////////////////////
// SaveSession class
//
#ifndef _SAVESESSION_H_
#define _SAVESESSION_H_

// forward declarations for pointers used in class
class EspinaVolumeEditor;
class DataManager;
class EditorOperations;
class Metadata;

class SaveSessionThread : public QThread
{
		Q_OBJECT
	public:
		// usual constructor and destructor
		SaveSessionThread(EspinaVolumeEditor *);
		virtual ~SaveSessionThread(void);

		// default thread execution method
		void run(void);
	signals:
		// allow main window to display the thread updates
		void progress(int);
		// to notify that we are starting the save session operation (we already have the lock at this point)
		void startedSaving();
	private:
		// initiate event loop for this thread
		int exec(void);

		EspinaVolumeEditor 		*_parent;
		DataManager 			*_dataManager;
		EditorOperations		*_editorOperations;
		Metadata                *_metadata;
};

#endif // _SAVESESSION_H_
