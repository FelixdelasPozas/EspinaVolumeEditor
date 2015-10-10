///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: BoxSelectionRepresentation3D.h
// Purpose: used as representation for the box selection volume in the render view
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _BOXSELECTIONREPRESENTATION3D_H_
#define _BOXSELECTIONREPRESENTATION3D_H_

// vtk includes
#include <vtkSmartPointer.h>

class vtkActor;
class vtkPolyDataMapper;
class vtkProperty;
class vtkPolyData;
class vtkPoints;
class vtkRenderer;

///////////////////////////////////////////////////////////////////////////////////////////////////
// BoxSelectionRepresentation3D class
//
class BoxSelectionRepresentation3D
{
  public:
    /** \brief BoxSelectionRepresentation3D class constructor.
     *
     */
    BoxSelectionRepresentation3D();

    /** \brief BoxSelectionRepresentation3D class destructor.
     *
     */
    ~BoxSelectionRepresentation3D();

    /** \brief Places the box on the 3D view.
     * \param[in] bounds box bounds.
     *
     */
    void PlaceBox(const double *bounds);

    /** \brief Returns the bounds of the box.
     *
     */
    double *GetBounds() const;

    /** \brief Sets the renderer the box will be rendered on.
     *
     */
    void SetRenderer(vtkRenderer *renderer);

  private:
    /** \brief Helper method to generate the box.
     *
     */
    void GenerateOutline();

    vtkSmartPointer<vtkPoints>         m_points;   /** corner points. */
    vtkSmartPointer<vtkPolyData>       m_polyData; /** polydata. */
    vtkSmartPointer<vtkPolyDataMapper> m_mapper;   /** mapper. */
    vtkSmartPointer<vtkActor>          m_actor;    /** actor. */
    vtkSmartPointer<vtkProperty>       m_property; /** actor property. */
    vtkSmartPointer<vtkRenderer>       m_renderer; /** renderer of the box. */
};

#endif // _BOXSELECTIONREPRESENTATION3D_H_
