///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: EditorOperations.h
// Purpose: Manages selection area and editor operations & filters
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _EDITOROPERATIONS_H_
#define _EDITOROPERATIONS_H_

// itk includes
#include <itkImage.h>
#include <itkSmartPointer.h>

// vtk includes 
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkCubeSource.h>
#include <vtkStructuredPoints.h>

// project includes 
#include "VectorSpaceAlgebra.h"
#include "Coordinates.h"
#include "ProgressAccumulator.h"
#include "DataManager.h"
#include "Metadata.h"
#include "Selection.h"

// qt includes
#include <QtGui>

// forward declarations
class SliceVisualization;

using ImageType = itk::Image<unsigned short, 3>;

///////////////////////////////////////////////////////////////////////////////////////////////////
// EditorOperations class
//
class EditorOperations
{
  public:
    /** \brief EditorOperations class constructor.
     * \param[in] dataManager application data manager.
     *
     */
    explicit EditorOperations(std::shared_ptr<DataManager> dataManager);

    /** \brief EditorOperations class destructor.
     *
     */
    ~EditorOperations();

    /** \brief Initializes the class.
     * \param[in] renderer view's renderer.
     * \param[in] coordinates image orientation data.
     * \param[in] progress progress coordinator.
     *
     */
    void Initialize(vtkSmartPointer<vtkRenderer> renderer, std::shared_ptr<Coordinates> coordinates, std::shared_ptr<ProgressAccumulator> progress);

    /** \brief Cuts the voxels of the given labels.
     * \param[in] labelsGroup selected objects labels.
     *
     */
    void Cut(std::set<unsigned short> labelsGroup);

    /** \brief Changes label of selected voxels to a different one
     * \param[in] parent QWidget parent pointer.
     * \param[in] metadada image metadata.
     * \param[inout] labelsGroup group of labels to change/final label.
     * \param[out] newColor true if is a new label and false otherwise.
     *
     */
    bool Relabel(QWidget *parent, std::shared_ptr<Metadata> metadata, std::set<unsigned short> *labelsGroup, bool *newColor);

    /** \brief Saves the volume to disk in MHD format.
     * \param[in] filename file name.
     *
     */
    void SaveImage(const std::string &filename) const;

    /** \brief Pains the current selection with the given label.
     * \param[in] label label to chenge the voxels to.
     *
     */
    void Paint(const unsigned short label);

    /** \brief Erases the voxels in the current selection of the given labels.
     * \param[in] labelsGroup labels of the voxels to erase.
     *
     */
    void Erase(const std::set<unsigned short> labelsGroup);

    /** \brief Returns the labelmap representation of the volume.
     *
     */
    itk::SmartPointer<LabelMapType> GetImageLabelMap() const;

    /** \brief Sets the first scalar value that is free to assign a label (it's NOT the label number).
     * \param[in] value scalar value.
     */
    void SetFirstFreeValue(const unsigned short value);

    /** \brief Returns the radius used in the morphological operations.
     *
     */
    const unsigned int GetFiltersRadius() const;

    /** \brief Sets the radius used in the morphological operations.
     * \param[in] radius radius integer value.
     */
    void SetFiltersRadius(const unsigned int radius);

    /** \brief Returns the watershed level value for the watershed operation.
     *
     */
    const double GetWatershedLevel(void) const;

    /** \brief Sets the watershed level value for the watershed operation.
     * \param[in] leveValue watershed level value.
     *
     */
    void SetWatershedLevel(const double levelValue);

    /** \brief Applies an erode filter in the selected area for the voxels of the given label.
     * If there is not a selection the filter operates on all the voxels of the given label in the image.
     * \param[in] label object label.
     *
     */
    void Erode(const unsigned short label);

    /** \brief Applies a dilate filter in the selected area for the voxels of the given label.
     * If there is not a selection the filter operates on all the voxels of the given label in the image.
     * \param[in] label object label.
     *
     */
    void Dilate(const unsigned short label);

    /** \brief Applies a close filter in the selected area for the voxels of the given label.
     * If there is not a selection the filter operates on all the voxels of the given label in the image.
     * \param[in] label object label.
     *
     */
    void Close(const unsigned short label);

    /** \brief Applies an open filter in the selected area for the voxels of the given label.
     * If there is not a selection the filter operates on all the voxels of the given label in the image.
     * \param[in] label object label.
     *
     */
    void Open(const unsigned short label);

    /** \brief Applies a watershed filter in the selected area for the voxels of the given label.
     * If there is not a selection the filter operates on all the voxels of the given label in the image.
     * \param[in] label object label.
     *
     */
    std::set<unsigned short> Watershed(const unsigned short label);

    /** \brief Adds a point to the selection area.
     * \param[in] point point coordinates.
     *
     */
    void AddSelectionPoint(const Vector3ui &point);

    /** \brief Adds a contiguous area to the selection using a connection algorithm.
     * \param[in] seed coordinates of the seed point.
     *
     */
    void ContiguousAreaSelection(const Vector3ui &seed);

    /** \brief Returns the minimum values of the selection bounding box.
     *
     */
    const Vector3ui GetSelectedMinimumBouds() const;

    /** \brief Returns the maximum values of the selection bounding box.
     *
     */
    const Vector3ui GetSelectedMaximumBouds() const;

    /** \brief Clears the selecion area.
     *
     */
    void ClearSelection();

    /** \brief Returns the type of the selecion area.
     *
     */
    Selection::Type GetSelectionType(void) const;

    /** \brief Returns true if the selection is empty.
     *
     */
    const bool IsSelectionEmpty(void) const;

    /** \brief Sets the slice visualization objects for each plane.
     * \param[in] axial axial plane slice visualization.
     * \param[in] coronal coronal plane slice visualization.
     * \param[in] sagittal sagittal plane slice visualization.
     *
     */
    void SetSliceViews(std::shared_ptr<SliceVisualization> axial,
                       std::shared_ptr<SliceVisualization> coronal,
                       std::shared_ptr<SliceVisualization> sagittal);

    /** \brief Updates the disc in the given point, radius and slice visualization.
     * \param[in] point point coordinates.
     * \param[in] radius radius size.
     * \param[in] sliceView slice visualization.
     *
     */
    void UpdatePaintEraseActors(const Vector3i &point, const int radius, std::shared_ptr<SliceVisualization> sliceView);

    /** \brief Adds a point to the contour selection in the given slice visualization.
     * \param[in] point point coordinates.
     * \param[in] sliceView slice visualization.
     */
    void AddContourPoint(const Vector3ui &point, std::shared_ptr<SliceVisualization> sliceView);

    /** \brief Updates the contour slice in the given point.
     * \param[in] point point coordinates.
     *
     */
    void UpdateContourSlice(const Vector3ui &point);

    friend class SaveSessionThread;
  private:
    /** \brief Helper method to show a message and gives the details of the exception error.
     *
     */
    void EditorError(itk::ExceptionObject &excp) const;

    /** \brief Helper method that erases all points in the image that are not from the given label.
     * \param[in] label object label value.
     *
     */
    void CleanImage(itk::SmartPointer<ImageType> image, const unsigned short label) const;

    /** \brief Dumps the values of the image to the vtkstructuredpoints structure.
     *
     */
    void ItkImageToPoints(itk::SmartPointer<ImageType> image);

    std::shared_ptr<Coordinates>         m_orientation;    /** image orientation data. */
    std::shared_ptr<DataManager>         m_dataManager;    /** image data. */
    std::shared_ptr<Selection>           m_selection;      /** selection area. */
    std::shared_ptr<ProgressAccumulator> m_progress;       /** filter's progress accumulator. */
    unsigned int                         m_radius;         /** configuration option for erode/dilate/open/close filters (structuring element radius). */
    double                               m_watershedLevel; /** configuration option for watershed filter. */
};

#endif // _EDITOROPERATIONS_H_
