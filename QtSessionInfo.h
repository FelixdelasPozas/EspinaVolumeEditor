///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtSessionInfo.h
// Purpose: session information dialog
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _QTSESSIONINFORMATION_H_
#define _QTSESSIONINFORMATION_H_

// Qt includes
#include <QtGui>
#include "ui_QtSessionInfo.h"

// project includes
#include "VectorSpaceAlgebra.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtSessionInfo class
//
class QtSessionInfo: public QDialog, private Ui_SessionInfo
{
	Q_OBJECT

	public:
		// constructor & destructor
		QtSessionInfo(QWidget *parent = 0, Qt::WindowFlags f = Qt::Dialog);
		~QtSessionInfo();

		// set values
		void SetFileInfo(const QFileInfo *);
		void SetSpacing(const Vector3d);
		void SetDimensions(const Vector3ui);
		void SetNumberOfSegmentations(const unsigned int);
		void SetReferenceFileInfo(const QFileInfo *);
		void SetDirectionCosineMatrix(const Matrix3d);

	public slots:
	private slots:
	private:
};

#endif // _QTSESSIONINFORMATION_H_
