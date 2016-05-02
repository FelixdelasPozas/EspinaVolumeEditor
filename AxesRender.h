///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: AxesRender.h
// Purpose: generate slice planes and crosshair for 3d render view
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _AXESRENDER_H_
#define _AXESRENDER_H_

// vtk includes
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkLineSource.h>
#include <vtkPoints.h>
#include <vtkPlaneSource.h>
#include <vtkOrientationMarkerWidget.h>

// Qt
#include <QObject>

// c++ includes
#include <vector>

// project includes
#include "Coordinates.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// AxesRender class
//
class AxesRender
: public QObject
{
    Q_OBJECT
  public:
    /** \brief Orientation enum class.
     *
     */
    enum class Orientation: unsigned char { Sagittal = 0, Coronal = 1, Axial = 2};

    /** \brief AxesRender class constructor.
     * \param[in] renderer renderer to insert the actors.
     * \param[in] coords   axes coordinates.
     *
     */
    explicit AxesRender(vtkSmartPointer<vtkRenderer> renderer, std::shared_ptr<Coordinates> coords);

    /** \brief AxesRender class virtual destructor.
     *
     */
    ~AxesRender();

    /** \brief Returs true if the axes are visible in the renderer.
     *
     */
    bool isVisible() const;

    /** \brief Set axes and crosshair visibility
     * \param[in] value true to enable and false otherwise.
     *
     */
    void setVisible(bool value);

  public slots:
    /** \brief Update slice planes in 3d render view to the new point
     * \param[in] crosshair crosshair point.
     *
     */
    void onCrosshairChange(const Vector3ui &crosshair);

  private:
    /** \brief Helper method to generate plane actors pointing to the center of the volume.
     * \param[in] renderer renderer to insert the actors.
     *
     */
    void GenerateSlicePlanes(vtkSmartPointer<vtkRenderer> renderer);

    /** \brief Helper method to generate voxel crosshair pointing to the center of the volume.
     * \param[in] renderer renderer to insert the actors.
     *
     */
    void GenerateVoxelCrosshair(vtkSmartPointer<vtkRenderer> renderer);

    /** \brief Update slice planes in 3d render view to the new point
     * \param[in] crosshair crosshair point.
     *
     */
    void UpdateSlicePlanes(const Vector3ui &crosshair);

    /** \brief Update voxel crosshair in 3d render view to the new point
     * \param[in] crosshair crosshair point.
     */
    void UpdateVoxelCrosshair(const Vector3ui &crosshair);

    /** \brief Helper method that creates and maintains a pointer to the orientation marker
     * widget (axes orientation).
     * \param[in] renderer renderer to insert the widget.
     *
     */
    void CreateOrientationWidget(vtkSmartPointer<vtkRenderer> renderer);

    vtkSmartPointer<vtkRenderer>                 m_renderer;     /** renderer to show the actors. */
    std::vector<vtkSmartPointer<vtkLineSource>>  m_POILines;     /** 3d crosshair lines. */
    std::vector<vtkSmartPointer<vtkPlaneSource>> m_planes;       /** 3d slice planes. */
    std::vector<vtkSmartPointer<vtkActor>>       m_planesActors; /** plane actors. */
    std::vector<vtkSmartPointer<vtkActor>>       m_crossActors;  /** crosshairs actors. */

    Vector3d m_spacing; /** spacing of the scene.      */
    Vector3d m_max;     /** maximum size of the scene. */

    bool m_visible;     /** visibility value.          */
    Vector3ui m_crosshair;

    vtkSmartPointer<vtkOrientationMarkerWidget> m_axesWidget; /** orientation marker widget. */
};

#endif
