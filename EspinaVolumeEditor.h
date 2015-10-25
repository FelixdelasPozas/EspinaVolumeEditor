///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: EspinaVolumeEditor.h
// Purpose: Volume Editor Qt GUI class
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _QTEDITORESPINA_H_
#define _QTEDITORESPINA_H_

// qt includes
#include <QMutex>
#include <QTimer>
#include "ui_QtGui.h"

// itk includes
#include <itkImage.h>
#include <itkLabelMap.h>
#include <itkShapeLabelObject.h>
#include <itkSmartPointer.h>

// vtk includes 
#include <vtkRenderer.h>
#include <vtkLookupTable.h>
#include <vtkStructuredPoints.h>
#include <vtkSmartPointer.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkCommand.h>

// c++ includes
#include <map>
#include <vector>

// project includes
#include "SliceVisualization.h"
#include "AxesRender.h"
#include "VectorSpaceAlgebra.h"
#include "Coordinates.h"
#include "VoxelVolumeRender.h"
#include "ProgressAccumulator.h"
#include "EditorOperations.h"
#include "DataManager.h"
#include "Metadata.h"
#include "SaveSession.h"

// defines and typedefs
using LabelObjectType = itk::ShapeLabelObject<unsigned short, 3>;
using LabelMapType    = itk::LabelMap<LabelObjectType>;

class EspinaVolumeEditor
: public QMainWindow, private Ui_MainWindow
{
  Q_OBJECT
  public:
  /** \brief EspinaVolumeEditor class constructor.
   * \param[in] app QApplication instance.
   * \param[in] parent Pointer of the QWidget parent of this one.
   *
   */
    EspinaVolumeEditor(QApplication *app, QWidget *parent = 0);

    /** \brief EspinaVolumeEditor class destructor.
     *
     */
    ~EspinaVolumeEditor();

  protected slots:
    /** \brief Show the dialog to open a segmha file, and loads the file if the name is valid and exists.
     *
     */
    virtual void open();

    /** \brief Shows the dialog to open a reference file, and loads the file if the name is valid and exists.
     *
     */
    virtual void referenceOpen();

    /** \brief Opens the dialog to save a segmha file, and saves the data to a segmha file if the name is valid.
     *
     */
    virtual void save();

    /** \brief Exits the application.
     *
     */
    virtual void exit();

    /** \brief Shows a dialog with the information about the current session.
     *
     */
    virtual void sessionInfo();

    /** \brief Shows the keyboard help dialog.
     *
     */
    virtual void keyboardHelp();

    /** \brief Toggles between fullscreen state.
     *
     */
    virtual void fullscreenToggle();

    /** \brief Callback to manage slice view's interactions.
     * \param[in] caller pointer of the caller object.
     * \param[in] event event id.
     * \param[in] unused
     * \param[in] unused
     * \param[in] unused
     *
     */
    virtual void sliceInteraction(vtkObject *caller, unsigned long event, void *unused, void *unused, vtkCommand *unused);

    /** \brief Toggles segmentation visibility.
     *
     */
    virtual void segmentationViewToggle();

    /** \brief Shows the preferences dialog.
     *
     */
    virtual void preferences();

    /** \brief Shows the about dialog.
     *
     */
    virtual void about();

    /** \brief Updates the axial view when the slider value changes.
     * \param[in] value slider value.
     *
     */
    virtual void onAxialSliderModified(int value);

    /** \brief Updates the coronal view when the slider value changes.
     * \param[in] value slider value.
     *
     */
    virtual void onCoronalSliderModified(int value);

    /** \brief Updates the sagittal view when the slider value changes.
     * \param[in] value slider value.
     *
     */
    virtual void onSagittalSliderModified(int value);

    /** \brief Avoids updating the volume view when the slider is pressed.
     *
     */
    virtual void onSliderPressed();

    /** \brief Allow the volume updating again when releasing a slider.
     *
     */
    virtual void onSliderReleased();

    /** \brief Updates the sagittal view when the spinbox changes value.
     * \param[in] value new spinbox value.
     *
     */
    virtual void onSpinBoxXModified(int value);

    /** \brief Updates the coronal view when the spinbox changes value.
     * \param[in] value new spinbox value.
     *
     */
    virtual void onSpinBoxYModified(int value);

    /** \brief Updates the axial view when the spinbox changes value.
     * \param[in] value new spinbox value.
     *
     */
    virtual void onSpinBoxZModified(int value);

    /** \brief Updates the views when the selection of labels changes.
     *
     */
    virtual void onSelectionChanged();

    /** \brief Updates the interface when the user interacts with the label widget.
     * \param[in] unused
     * \param[in] unused
     *
     */
    virtual void onLabelSelectionInteraction(QListWidgetItem *unused, QListWidgetItem *unused);

    /** \brief Resets the view's cameras.
     *
     */
    virtual void resetViews();

    /** \brief Toggles the visibility of the axes in the volume render view.
     *
     */
    virtual void axesViewToggle();

    /** \brief Switches between volumetric and mesh rendering in the volume render view.
     *
     */
    virtual void renderTypeSwitch();

    /** \brief Cuts the selected voxels.
     *
     */
    virtual void cut();

    /** \brief Relabels the currently selected label.
     *
     */
    virtual void relabel();

    /** \brief Erodes the currently selected label.
     *
     */
    virtual void erodeVolumes();

    /** \brief Dilates the currently selected label.
     *
     */
    virtual void dilateVolumes();

    /** \brief Opens the currently selected label.
     *
     */
    virtual void openVolumes();

    /** \brief Closes the currently selected label.
     *
     */
    virtual void closeVolumes();

    /** \brief Performs a watershed algorithm in the selected volumes.
     *
     */
    virtual void watershedVolumes();

    /** \brief Undoes the last operation in the undo buffer.
     *
     */
    virtual void undo();

    /** \brief Redoes the last operation in the redo buffer.
     *
     */
    virtual void redo();

    /** \brief Toggles the maximization of the views.
     *
     */
    virtual void onViewZoom();

    /** \brief Toggles the activation of the volume render view.
     *
     */
    virtual void renderViewToggle();

    /** \brief Saves the current session to disk when the auto save feature kicks in.
     *
     */
    virtual void saveSession();

    /** \brief Updates the progress bar when starting an auto save.
     *
     */
    virtual void saveSessionStart();

    /** \brief Updates the progress bar during an auto save.
     * \param[in] value save session progress value.
     *
     */
    virtual void saveSessionProgress(int value);

    /** \brief Updates the progress bar when ending an auto save.
     *
     */
    virtual void saveSessionEnd();

    /** \brief Updates the selection when using the wand tool.
     * \param[in] status wand button tool status.
     *
     */
    virtual void wandButtonToggle(bool status);

    /** \brief Updates the selection when using the erase/paint button.
     * \param[in] status erase/paint button tool status.
     *
     */
    virtual void eraseOrPaintButtonToggle(bool);

    /** \brief
     *
     */
    virtual void ToggleButtonDefault(bool status);
  private:
    friend class SaveSessionThread;

    enum class ViewPorts: char { All = 0, Slices, Render, Axial, Coronal, Sagittal };

    /** \brief Manages picking events in the slice views.
     * \param[in] event event id.
     * \param[in] view view where the event is coming from.
     *
     */
    void sliceXYPick(const unsigned long event, std::shared_ptr<SliceVisualization> view);

    /** \brief Updates the gui point label.
     *
     */
    void updatePointLabel();

    /** \brief Helper method to build the label color table.
     *
     */
    void fillColorLabels();

    /** \brief Updates the given views.
     * \param[in] views views to update.
     *
     */
    void updateViewports(ViewPorts views);

    /** \brief Updates the undo/redo menu texts.
     *
     */
    void updateUndoRedoMenu();

    /** \brief Restore the session from the auto save file.
     *
     */
    void restoreSavedSession();

    /** \brief Removes the auto save files from disk.
     *
     */
    void removeSessionFiles();

    /** \brief Helper method to load the reference file.
     *
     */
    void loadReferenceFile(const QString &filename);

    /** \brief Helper method to initialize the GUI at the beginning of the session.
     *
     */
    void initializeGUI();

    /** \brief Enable morphological filters and watershed operations.
     * \param[in] value true to enable and false otherwise.
     *
     */
    void enableOperations(bool value);

    /** \brief Restarts the volume render view.
     *
     */
    void restartVoxelRender();

    /** \brief Selects the given labels group.
     *
     */
    void selectLabels(const std::set<unsigned short> labels);

    /** \brief Applies the currently selected operation.
     * \param[in] view view to apply the action if needed.
     *
     */
    void applyUserAction(std::shared_ptr<SliceVisualization> view);

    /** \brief Connects the GUI signals.
     *
     */
    void connectSignals();

    /** \brief Loads the application settings from the ini file.
     *
     */
    void loadSettings();

    bool eventFilter(QObject *, QEvent*);

    bool m_updateVoxelRenderer;  /** bool to minimize drawing in render view. True to update and false otherwise. */
    bool m_updateSliceRenderers; /** bool to minimize drawing in slice views. True to update and false otherwise. */
    bool m_updatePointLabel;     /** bool to minimize point label updating. */

    bool m_renderIsAVolume; /** true if we are doing volume rendering and not mesh rendering. */

    // renderers for four QVTKWidget viewports
    vtkSmartPointer<vtkRenderer> m_axialRenderer;    /** axial view renderer.    */
    vtkSmartPointer<vtkRenderer> m_coronalRenderer;  /** coronal view renderer.  */
    vtkSmartPointer<vtkRenderer> m_sagittalRenderer; /** sagittal view renderer. */
    vtkSmartPointer<vtkRenderer> m_volumeRenderer;   /** volume view renderer.   */

    std::shared_ptr<SliceVisualization> m_axialView;    /** axial   view.  */
    std::shared_ptr<SliceVisualization> m_coronalView;  /** coronal view   */
    std::shared_ptr<SliceVisualization> m_sagittalView; /** sagittal view. */
    std::shared_ptr<VoxelVolumeRender>  m_volumeView;   /** volume view.   */

    std::shared_ptr<AxesRender>          m_axesRender;        /** axes renderer.          */
    std::shared_ptr<Coordinates>         m_orientationData;   /** image orientation data. */
    std::shared_ptr<Metadata>            m_fileMetadata;      /** image metadata.         */
    std::shared_ptr<SaveSessionThread>   m_saveSessionThread; /** auto save thread.       */
    std::shared_ptr<DataManager>         m_dataManager;       /** data manager.           */
    std::shared_ptr<EditorOperations>    m_editorOperations;  /** editor operations.      */
    std::shared_ptr<ProgressAccumulator> m_progress;          /** progress accumulator.   */

    QMutex m_mutex; /** to assure that no editing action is interrupted by the save session operation that can kick in at any time. */

    // point of interest and its label
    Vector3ui      m_POI;         /** point of interest, crosshair point. */
    unsigned short m_pointScalar; /** label of the POI point.             */

    vtkSmartPointer<vtkEventQtSlotConnect> m_connections; /** converts between vtk events and qt slots. */

    // misc boolean values that affect editor
    bool m_hasReferenceImage;    /** true if the session has a reference image and false otherwise. */
    bool m_segmentationsVisible; /** true if the segmentations are visibla and false otherwise.     */

    QTimer       m_sessionTimer;       /** auto save timer.                                       */
    unsigned int m_saveSessionTime;    /** time of the auto save timer, in seconds.               */
    bool         m_saveSessionEnabled; /** true if the auto save is enabled, and false otherwise. */

    QString m_segmentationFileName; /** segmha file name.          */
    QString m_referenceFileName;    /** reference image file name. */

    unsigned int m_brushRadius; /** brush radius. */
};

#endif
