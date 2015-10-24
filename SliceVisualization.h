///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: SliceVisualization.h
// Purpose: generate slices & crosshairs for axial, coronal and sagittal views. Also handles
//          pick function and selection of slice pixels.
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _SLICEVISUALIZATION_H_
#define _SLICEVISUALIZATION_H_

// vtk includes
#include <vtkSmartPointer.h>
#include <vtkStructuredPoints.h>
#include <vtkLookupTable.h>
#include <vtkRenderer.h>
#include <vtkMatrix4x4.h>
#include <vtkPolyData.h>
#include <vtkPropPicker.h>
#include <vtkTextActor.h>
#include <vtkImageMapToColors.h>
#include <vtkActor.h>
#include <vtkPlaneSource.h>
#include <vtkImageActor.h>
#include <vtkImageBlend.h>
#include <vtkAbstractWidget.h>

// project includes
#include "Coordinates.h"
#include "VectorSpaceAlgebra.h"
#include "EditorOperations.h"

// forward declarations
class BoxSelectionWidget;
class vtkImageReslice;

///////////////////////////////////////////////////////////////////////////////////////////////////
// SliceVisualization class
//
class SliceVisualization
{
  public:
    /** \class Orientation.
     * \brief Class to define orientation of the visualizations.
     *
     */
    enum class Orientation: char { Sagittal = 0, Coronal = 1, Axial = 2, None = 3 };

    /** \class PickType.
     *
     */
    enum class PickType: char { Thumbnail = 0, Slice = 1, None = 2 };

    /** \brief SliceVisualization class constructor.
     * \param[in] type Orientation of the visualization.
     */
    explicit SliceVisualization(Orientation type);

    /** \brief SliceVisualization class destructor.
     *
     */
    ~SliceVisualization();

    /** \brief Initializes the class with the required data.
     * \param[in] data image data.
     * \param[in] colorTable image color table.
     * \param[in] renderer renderer where the visualization is shown.
     * \param[in] coordinates image orientation data.
     *
     */
    void initialize(vtkSmartPointer<vtkStructuredPoints> data,
                    vtkSmartPointer<vtkLookupTable>      colorTable,
                    vtkSmartPointer<vtkRenderer>         renderer,
                    std::shared_ptr<Coordinates>         coordinates);

    /** \brief Updates the position of the visualization.
     * \param[in] point point coordinates.
     *
     */
    void update(const Vector3ui &point);

    /** \brief Updates the crosshair position.
     * \param[in] point crosshair point coordinates.
     *
     */
    void updateCrosshair(const Vector3ui &point);

    /** \brief Updates the slice actors in the given point.
     * \param[in] point point coordinates.
     *
     */
    void updateSlice(const Vector3ui &point);

    /** \brief Returns the type of pick and the coordinates of the picking point by reference.
     * \param[out] X x coordinate of the pick point.
     * \param[out] Y y coordinate of the pick point.
     *
     */
    PickType pickData(int *X, int *Y) const;

    /** \brief Manages thumbnail visibility.
     *
     */
    void zoomEvent();

    /** \brief Sets the data reference image.
     * \param[in] reference reference image.
     *
     */
    void setReferenceImage(vtkSmartPointer<vtkStructuredPoints> image);

    /** \brief Returns the segmentation opacity.
     *
     */
    double segmentationOpacity() const;

    /** \brief Sets the opacity of the segmentations.
     *
     */
    void setSegmentationOpacity(const double opacity);

    /** \brief Toggles segmentation view (switches from 0 opacity to previously set opacity and viceversa)
     *
     */
    void toggleSegmentationView();

    /** \brief Set selection volume to reslice and create actors.
     * \param[in] selectionBuffer volume to reslice and create actors.
     * \param[in] useActorBounds true to use actors' bounds and false otherwise.
     *
     */
    void setSelectionVolume(const vtkSmartPointer<vtkImageData> selectionBuffer, bool useActorBounds = true);

    /** \brief Clear selection
     *
     */
    void clearSelections();

    /** \brief Returns the orientation of the visualization.
     *
     */
    const Orientation orientationType() const;

    /** \brief Returns the visualization's renderer.
     *
     */
    vtkSmartPointer<vtkRenderer> renderer() const;

    /** \brief Set a pointer to the widget so we can hide and show it, also we need to
     * deactivate/reactivate the widget depending on the slice.
     * \param[in] widget
     *
     */
    void setSliceWidget(vtkSmartPointer<vtkAbstractWidget> widget);

    /** \brief Returns the imageactor of the slice, needed for polygon/lasso widget interaction.
     *
     */
    vtkSmartPointer<vtkImageActor> actor() const;
  private:
    /** \brief Generate imageactor and adds it to renderer.
     * \param[in] data data points.
     * \param[in] colorTable data colors.
     *
     */
    void generateSlice(vtkSmartPointer<vtkStructuredPoints> data, vtkSmartPointer<vtkLookupTable> colorTable);

    /** \brief Generate crosshairs actors and adds them to renderer.
     *
     */
    void generateCrosshair();


    /** \brief Generate thumbnail actors and adds them to thumbnail renderer.
     *
     */
    void generateThumbnail();

    /** actor's data. */
    struct ActorData
    {
        vtkSmartPointer<vtkActor> actor;
        int                       minSlice;
        int                       maxSlice;

        ActorData(): actor{nullptr}, minSlice{0}, maxSlice{0} {};
    };

    /** \brief Hides of shows the actors depending on the visibility.
     * \param[in] actorInformation information of the actor to update.
     *
     */
    void updateActorVisibility(std::shared_ptr<struct ActorData> actorInformation);

    Orientation m_orientation; /** orientation of the visualization. */
    Vector3d    m_spacing;     /** spacing of the data. */
    Vector3d    m_max;         /** maximum bounds of the visualization. */
    Vector3ui   m_size;        /** size in voxels of the visualization. */
    Vector3ui   m_point;       /** crosshair point, to not redraw if slider is already in the correct position. */

    vtkSmartPointer<vtkPropPicker>     m_picker;        /** actor's picker. */
    vtkSmartPointer<vtkRenderer>       m_renderer;      /** view's main renderer. */
    vtkSmartPointer<vtkRenderer>       m_thumbRenderer; /** thumbnail renderer. */
    vtkSmartPointer<vtkAbstractWidget> m_widget;        /** active widget pointer. */
    vtkSmartPointer<vtkTexture>        m_texture;       /** texture of the selection area. */
    vtkSmartPointer<vtkTextActor>      m_textActor;     /** text actor. */

    vtkSmartPointer<vtkMatrix4x4> m_axesMatrix;  /** reslice axes pointer, used for updating slice. */

    vtkSmartPointer<vtkPolyData> m_horizontalCrosshair;      /** horizontal crosshair line data. */
    vtkSmartPointer<vtkActor>    m_horizontalCrosshairActor; /** horizontal crosshair line actor. */
    vtkSmartPointer<vtkPolyData> m_verticalCrosshair;        /** vertical crosshair line data. */
    vtkSmartPointer<vtkActor>    m_verticalCrosshairActor;   /** vertical crosshair line actor. */

    vtkSmartPointer<vtkPolyData> m_focusData;  /** thumbnail focus square data. */
    vtkSmartPointer<vtkActor>    m_focusActor; /** thumbnail focus square actor. */

    vtkSmartPointer<vtkImageReslice>     m_referenceReslice;     /** reslice filter. */
    vtkSmartPointer<vtkImageMapToColors> m_referenceImageMapper; /** reference image color table. */
    vtkSmartPointer<vtkImageBlend>       m_imageBlender;         /** data and refernce images blender. */

    vtkSmartPointer<vtkImageReslice>     m_segmentationReslice;     /** reslice filter. */
    vtkSmartPointer<vtkImageMapToColors> m_segmentationsMapper;  /** segmentations' mapper. */
    vtkSmartPointer<vtkImageActor>       m_segmentationsActor; /** segmentations' actor. */

    double m_segmentationOpacity; /** segmentations' opacity value. */
    bool   m_segmentationHidden;  /** true if the segmentations are hidden and false otherwise. */

    std::vector<std::shared_ptr<struct ActorData>> m_actorList; /** selection volumes actors' vector. */
};

#endif // _SLICEVISUALIZATION_H_
