///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtPreferences.h
// Purpose: modal dialog for configuring preferences
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _QTPREFERENCES_H_
#define _QTPREFERENCES_H_

// Qt includes
#include <QtGui>
#include "ui_QtPreferences.h"

// project includes
#include "EditorOperations.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// QtPreferences class
//
class QtPreferences
: public QDialog
, private Ui_Preferences
{
  Q_OBJECT

  public:
    /** \brief QtPreferences class constructor.
     * \param[in] parent pointer to the QWidget parent of this one.
     * \param[in] f window flags.
     *
     */
    QtPreferences(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Dialog);

    /** \brief Sets the initial options.
     * \param[in] size size of the undo/redo system in bytes.
     * \param[in] capacity capacity of the undo/redo system.
     * \param[in] radius radius of the morphological operations.
     * \param[in] level level value for the watershed operation.
     * \param[in] opacity opacity value for the segmentation representations.
     * \param[in] saveTime auto-save time interval in minutes.
     * \param[in] saveEnabled true to enable auto-save feature.
     * \param[in] paintRadius paint disk radius value.
     *
     */
    void SetInitialOptions(const unsigned long int size,
                           const unsigned long int capacity,
                           const unsigned int      radius,
                           const double            level,
                           const int               opacity,
                           const unsigned int      saveTime,
                           const bool              saveEnabled,
                           const unsigned int      paintRadius);

    /** \brief Returns the size of the undo/redo system.
     *
     */
    const unsigned long int size() const;

    /** \brief Returns the radius value for the morphological operations.
     *
     */
    const unsigned int radius() const;

    /** \brief Returns the level value of the watershed operation.
     *
     */
    const double level() const;

    /** \brief Returns the segmentation opacity when a reference image is present.
     *
     */
    const unsigned int opacity() const;

    /** \brief Returns true if the user pushed the Ok button instead the Cancel or Close.
     *
     */
    bool isModified() const;

    /** \brief Enables the visualization options box, that should be disabled if there isn't a reference image loaded.
     *
     */
    void enableVisualizationBox();

    /** \brief Returns the auto-save interval in minutes.
     *
     */
    const unsigned int autoSaveInterval() const;

    /** \brief Returns true if the auto-save feature is enabled and false otherwise.
     *
     */
    bool isAutoSaveEnabled() const;

    /** \brief Returns the brush radius value
     *
     */
    unsigned int brushRadius() const;
  public slots:
    // slots for signals
    virtual void SelectSize(int);
    virtual void SelectRadius(int);
    virtual void SelectLevel(double);
    virtual void SelectOpacity(int);
    virtual void SelectSaveTime(int);
    virtual void SelectPaintEraseRadius(int);

  private slots:
    void AcceptedData();

  private:
    unsigned long int m_undoSize;       /** undo/redo buffer capacity in bytes.                     */
    unsigned long int m_undoCapacity;   /** undo/redo buffer system occupation.                     */
    unsigned int      m_filtersRadius;  /** open/close/dilate/erode filters' radius.                */
    unsigned int      m_brushRadius;    /** paint/erase operations radius.                          */
    double            m_watershedLevel; /** watershed filter flood level.                           */
    unsigned int      m_opacity;        /** segmentation opacity when a reference image is present. */
    unsigned int      m_saveTime;       /** auto-save time interval.                                */
    bool              m_modified;       /** just to know that the data has been modified.           */
};

#endif // _QTPREFERENCES_H_
