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
class QtSessionInfo
: public QDialog
, private Ui_SessionInfo
{
  Q_OBJECT

  public:
    /** \brief QtSessionInfo class constructor.
     * \param[in] parent pointer to the QWidget parent of this one.
     * \param[in] f window flags.
     *
     */
    QtSessionInfo(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Dialog);

    /** \brief Sets file information.
     * \param[in] file QFilInfo object.
     *
     */
    void SetFileInfo(const QFileInfo &file);

    /** \brief Sets the spacing of the image.
     * \param[in] spacing image spacing.
     *
     */
    void SetSpacing(const Vector3d &spacing);

    /** \brief Sets the dimensions of the image.
     * \param[in] dimensions image dimensions.
     *
     */
    void SetDimensions(const Vector3ui &dimensions);

    /** \brief Sets the number of segmentations.
     * \param[in] segNum number of segmentations in the session.
     *
     */
    void SetNumberOfSegmentations(const unsigned int segNum);

    /** \brief Sets reference image information.
     * \param[in] file QFileInfo object.
     *
     */
    void SetReferenceFileInfo(const QFileInfo &file);

    /** \brief Sets the direction cosine matrix.
     * \param[in] matrix image direction cosine matrix.
     *
     */
    void SetDirectionCosineMatrix(const Matrix3d &matrix);
};

#endif // _QTSESSIONINFORMATION_H_
