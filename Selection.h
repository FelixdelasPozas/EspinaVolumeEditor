///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: Selection.h
// Purpose: Manages selection areas
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _SELECTION_H_
#define _SELECTION_H_

// vtk includes
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkActor.h>
#include <vtkStructuredPoints.h>
#include <vtkImageChangeInformation.h>
#include <vtkImageClip.h>
#include <vtkBoxRepresentation.h>
#include <vtkBoxWidget2.h>
#include <vtkImageStencilToImage.h>
#include <vtkPolyDataToImageStencil.h>

// ITK
#include <itkImage.h>

// project includes
#include "VectorSpaceAlgebra.h"
#include "Coordinates.h"
#include "DataManager.h"
#include "BoxSelectionWidget.h"
#include "BoxSelectionRepresentation2D.h"
#include "BoxSelectionRepresentation3D.h"
#include "ContourWidget.h"

// c++ includes
#include <vector>

// image typedefs
using ImageType   = itk::Image<unsigned short, 3>;
using ImageTypeUC = itk::Image<unsigned char, 3>;

// forward declarations
class SliceVisualization;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Selection class
//
class Selection
{
  public:
    /** \brief Selection class constructor.
     *
     */
    Selection();

    /** \brief Selection class destructor.
     *
     */
    ~Selection();

    /** \brief Type of selections.
     *
     */
    enum class Type: char { EMPTY = 0, CUBE, VOLUME, DISC, CONTOUR };

    /** \brief Initializes the class.
     * \param[in] coordinates image coordinates.
     * \param[in] renderer renderer of the view displaying the selection.
     * \param[in] dataManager application data manager.
     *
     */
    void initialize(std::shared_ptr<Coordinates> coordinates, vtkSmartPointer<vtkRenderer> renderer, std::shared_ptr<DataManager> dataManager);

    /** \brief Modifies selection area adding the given point.
     * \param[in] point point coordinates.
     */
    void addSelectionPoint(const Vector3ui &point);

    /** \brief Contiguous area selection with the wand.
     * \param[in] point wand seed point.
     *
     */
    void addArea(const Vector3ui &point);

    /** \brief Seletes points and hides actor (clears buffer only between [_min, _max] bounds).
     *
     */
    void clear(void);

    /** \brief Returns the selection type.
     *
     */
    const Type type() const;

    /** \brief Returns the minimum selected bounds.
     *
     */
    const Vector3ui minimumBouds() const;

    /** \brief Returns the maximum selected bounds.
     *
     */
    const Vector3ui maximumBouds() const;

    /** \brief Returns true if the voxel is inside the selection.
     * \param[in] point voxel coordinates.
     *
     */
    bool isInsideSelection(const Vector3ui &point) const;

    /** \brief Returns a itk image from the selection, or the segmentation if there is nothing selected.
     * The image bounds are adjusted for filter radius (the selection grows with boundsGrow voxels in
     * each side). Label must be specified always, but it's only used when there's nothing selected.
     * \param[in] label segmentation label.
     * \param[in] boundsGrow number of voxels to grow the selection on each side.
     *
     */
    itk::SmartPointer<ImageType> itkImage(const unsigned short label, const unsigned int boundsGrow = 0) const;

    /** \brief Returns a itk image from the segmentation.
     * \param[in] label segmentation label.
     *
     */
    itk::SmartPointer<ImageType> segmentationItkImage(const unsigned short label) const;

    /** \brief Returns a itk image of the whole data.
     *
     */
    itk::SmartPointer<ImageType> itkImage() const;

    /** \brief Sets the views to pass selection volumes.
     * \param[in] axial axial slice view.
     * \param[in] coronal coronal slice view.
     * \param[in] sagittal sagittal slice view.
     *
     */
    void setSliceViews(std::shared_ptr<SliceVisualization> axial, std::shared_ptr<SliceVisualization> coronal, std::shared_ptr<SliceVisualization> sagittal);

    /** \brief Sets the selection disk in the given point of the view with the given radius.
     * \param[in] point point coordinates.
     * \param[in] radius disc radius.
     * \param[in] view selection view.
     *
     */
    void setSelectionDisc(const Vector3i &point , const unsigned  int radius, std::shared_ptr<SliceVisualization> view);

    /** \brief Values used in the selection buffer.
     *
     */
    enum SelectionValues { VOXEL_UNSELECTED = 0, SELECTION_UNUSED_VALUE, VOXEL_SELECTED };

    /** \brief Callbacks for widget interaction
     *
     */
    static void boxSelectionWidgetCallback(vtkObject *caller, unsigned long event, void *clientdata, void *calldata);
    static void contourSelectionWidgetCallback(vtkObject *caller, unsigned long event, void *clientdata, void *calldata);

    /** \brief Starts lasso/polygon selection.
     * \param[in] point point coordinates to start the selection.
     * \param[in] view selection view.
     *
     */
    void addContourInitialPoint(const Vector3ui &point, std::shared_ptr<SliceVisualization> view);

    /** \brief Moves the contour selection volume if there is a change in slice.
     *
     */
    void updateContourSlice(const Vector3ui &point);
  private:
    /** \brief Helper method to compute box selection area actor and parameters.
     *
     */
    void computeSelectionCube();
    void computeSelectionBounds();

    /** \brief Computes lasso bounds for min/max selection determination and actor visibility (clipper).
     * \param[out] bounds lasso bounds.
     *
     */
    void computeLassoBounds(int *bounds);

    /** \brief Generates a rotated volume according to orientation (0 = axial, 1 = coronal, 2 = sagittal).
     * \param[in] bounds of the contour selection.
     *
     */
    void computeContourSelectionVolume(const int *bounds);

    /** \brief Computes actor from selected volume.
     *
     */
    void computeActor(vtkSmartPointer<vtkImageData> volume);

    /** \brief Deletes all selection volumes.
     *
     */
    void deleteSelectionVolumes();

    /** \brief Deletes all selection actors for render view.
     *
     */
    void deleteSelectionActors();

    /** \brief Returns true if the voxel coordinates refer to a selected voxel.
     * \param[in] subVolume selection volume.
     * \param[in] point voxel coordinates.
     *
     */
    bool isInsideSelectionSubvolume(vtkSmartPointer<vtkImageData> subVolume, const Vector3ui &point) const;

    vtkSmartPointer<vtkRenderer> m_renderer; /** view's renderer. */
    Vector3ui                    m_min;      /** min selection bounds. */
    Vector3ui                    m_max;      /** max selection bounds. */
    Vector3ui                    m_size;     /** Maximum possible bounds ([0,0,0] to _size). */
    Vector3d                     m_spacing;  /** Image spacing (needed for subvolume creation). */

    std::vector<Vector3ui> m_selectedPoints; /** selection points for box selection. */

    Type m_selectionType; /** Selection type */

    std::shared_ptr<DataManager> m_dataManager; /** data manager. */

    vtkSmartPointer<vtkTexture> m_texture; /**  selection actor texture for render view. */


    std::shared_ptr<SliceVisualization> m_axial;     /** axial view.    */
    std::shared_ptr<SliceVisualization> m_coronal;   /** coronal view.  */
    std::shared_ptr<SliceVisualization> m_sagittal;  /** sagittal view. */

    std::vector<vtkSmartPointer<vtkActor>> m_selectionActorsList;   /** list of selection actor for render view. */

    std::vector<vtkSmartPointer<vtkImageData>> m_selectionVolumesList; /** list of selection volumes (unsigned char scalar size). */

    vtkSmartPointer<vtkImageChangeInformation> m_changer; /** to change origin for erase/paint volume on the fly. */
    vtkSmartPointer<vtkImageClip>              m_clipper; /** selection image clip.                               */

    vtkSmartPointer<BoxSelectionWidget>        m_axialBoxWidget;    /** axial view selection box widget.    */
    vtkSmartPointer<BoxSelectionWidget>        m_coronalBoxWidget;  /** coronal view selection box widget.  */
    vtkSmartPointer<BoxSelectionWidget>        m_sagittalBoxWidget; /** sagittal view selection box widget. */

    std::shared_ptr<BoxSelectionRepresentation3D> m_boxRender;      /** box selection representation for 3d render.       */

    vtkSmartPointer<vtkCallbackCommand>   m_widgetsCallbackCommand; /** callback for box selection interaction.           */

    vtkSmartPointer<ContourWidget>             m_contourWidget;     /** contour selection widget.                         */

    vtkSmartPointer<vtkPolyDataToImageStencil> m_polyDataToStencil; /** converts VtkPolyData from a contour to a stencil. */
    vtkSmartPointer<vtkImageStencilToImage>    m_stencilToImage;    /** converts a stencil to a itk image.                */
    vtkSmartPointer<vtkImageData>              m_rotatedImage;      /** rotated contour image.                            */
    bool                                       m_selectionIsValid;  /** true if selection is valid, false otherwise.      */
};

#endif // _SELECTION_H_
