///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: SaveSession.h
// Purpose: Thread for saving a editor session
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// c++ includes
#include <vector>
#include <fstream>
#include <algorithm>

// Qt
#include <QThread>

///////////////////////////////////////////////////////////////////////////////////////////////////
// SaveSession class
//
#ifndef _SAVESESSION_H_
#define _SAVESESSION_H_

class EspinaVolumeEditor;
class DataManager;
class EditorOperations;
class Metadata;

class SaveSessionThread
: public QThread
{
		Q_OBJECT
	public:
		/** \brief SaveSessionThread class constructor.
		 * \param[in] editor application instance.
		 *
		 */
		explicit SaveSessionThread(EspinaVolumeEditor *editor);

		void run(void);
	signals:

		void progress(int value);

		void startedSaving();
	private:
		/** \brief Helper method to write to a stream.
		 *
		 */
		template<typename T>void write(std::ofstream& out, T t)
		{
			out.write(reinterpret_cast<char*>(&t), sizeof(T));
		}

    /** \brief Helper method to read from a stream.
     *
     */
		template<typename T>void read(std::ifstream& in, T t)
		{
			in.read(reinterpret_cast<char*>(&t), sizeof(T));
		}

		EspinaVolumeEditor *m_editor; /** editor instance. */
};


#endif // _SAVESESSION_H_
