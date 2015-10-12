///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtColorPicker.h
// Purpose: pick a color by selecting it's RGB components 
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _QTCOLORPICKER_H_
#define _QTCOLORPICKER_H_

// Qt includes
#include <QtGui>
#include "ui_QtColorPicker.h"

// project includes
#include "DataManager.h"
#include "VectorSpaceAlgebra.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtColorPicker class
//
class QtColorPicker
: public QDialog
, private Ui_ColorPicker
{
  Q_OBJECT
  public:
    /** \brief QtColorPicker class constructor.
     * \param[in] parent pointer to the QWidget parent of this one.
     * \param[in] f window flags.
     *
     */
    QtColorPicker(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Dialog);

    /** \brief Sets the initial options of the dialog.
     * \param[in] data data manager.
     */
    void SetInitialOptions(std::shared_ptr<DataManager> data);

    /** \brief Returns true if the user clicked the Ok button instead the Cancel or Close.
     *
     */
    bool ModifiedData() const;

    /** \brief Return the selected color.
     *
     */
    const QColor GetColor() const;
  public slots:
    void AcceptedData();

  private slots:
    virtual void RValueChanged(int);
    virtual void GValueChanged(int);
    virtual void BValueChanged(int);

  private:
    /** \brief Computes color box and disables buttons if color is already in use.
     *
     */
    void MakeColor();

    bool                         m_modified; /** i prefer this way instead returning data. */
    std::shared_ptr<DataManager> m_data;     /** pointer to the datamanager containing the color table. */
    QColor                       m_color;    /** color to be returned. */
};

#endif // _QTLABELEDITOR_H_
