///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtRelabel.h
// Purpose: modal dialog for configuring relabel options 
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _QTRELABEL_H_
#define _QTRELABEL_H_

// Qt includes
#include <QtGui>
#include "ui_QtRelabel.h"

// vtk includes
#include <vtkSmartPointer.h>
#include <vtkLookupTable.h>

// project includes
#include "Metadata.h"
#include "DataManager.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtRelabel class
//
class QtRelabel
: public QDialog
, private Ui_Relabel
{
  Q_OBJECT
  public:
    /** \brief QtRelabel class constructor.
     * \param[in] parent pointer to the QWidget parent of this one.
     * \param[in] f window flags.
     *
     */
    QtRelabel(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Dialog);

    /** \brief Sets initial options.
     * \param[in] labels group of labels in the image.
     * \param[in] data session data accessor.
     * \param[in] dataManager session data manager.
     *
     */
    void setInitialOptions(const std::set<unsigned short> labels, std::shared_ptr<Metadata> data, std::shared_ptr<DataManager> dataManager);

    /** \brief Returns the selected label in the combobox.
     *
     */
    const unsigned short selectedLabel() const;

    /** \brief Returns true if it's a new label.
     *
     */
    bool isNewLabel() const;

    /** \brief Returns true if the user clicked the Ok button instead the Cancel or Close.
     *
     */
    bool isModified() const;
  public slots:
    virtual void AcceptedData();

  private:
    bool           m_modified;
    bool           m_newlabel;
    unsigned int   m_maxcolors;
};

#endif // _QTRELABEL_H_
