///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: FocalPlanePointPlacer.h
// Purpose: Used in conjunction with the contour classes to place points on a plane
// Notes: Modified from vtkFocalPlanePointPlacer class in vtk 5.8
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _FOCALPLANEPOINTPLACER_H_
#define _FOCALPLANEPOINTPLACER_H_

// vtk includes
#include <vtkPointPlacer.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
// FocalPlanePointPlacer class
//
class vtkRenderer;

class FocalPlanePointPlacer
: public vtkPointPlacer
{
  public:
    static FocalPlanePointPlacer *New();

    vtkTypeMacro(FocalPlanePointPlacer,vtkPointPlacer);

    void PrintSelf(ostream& os, vtkIndent indent);

    int ComputeWorldPosition(vtkRenderer *ren, double displayPos[2], double worldPos[3], double worldOrient[9]);

    int ComputeWorldPosition(vtkRenderer *ren, double displayPos[2], double refWorldPos[3], double worldPos[3], double worldOrient[9]);

    int ValidateWorldPosition(double worldPos[3]);

    int ValidateWorldPosition(double worldPos[3], double worldOrient[9]);

    /** \brief Optionally specify a signed offset from the focal plane for the points to
     * be placed at.  If negative, the constraint plane is offset closer to the
     * camera. If positive, its further away from the camera.
     *
     */
    vtkSetMacro(Offset, double);
    vtkGetMacro(Offset, double);

    /** \brief Optionally Restrict the points to a set of bounds. The placer will
     * invalidate points outside these bounds.
     *
     */
    vtkSetVector6Macro(PointBounds, double);
    vtkGetVector6Macro(PointBounds, double);

    /** \brief Spacing needed to place points only on voxel centers.
     *
     */
    vtkSetVector2Macro(Spacing, double);
    vtkGetVector2Macro(Spacing, double);

  protected:
    /** \brief FocalPlanePointPlacer default constructor.
     *
     */
    FocalPlanePointPlacer();

    /** \brief Returns the current orientation.
     *
     */
    void GetCurrentOrientation(double worldOrient[9]) const;

    /** \brief Transforms parameters constrained to spaced world coordinates using spacing data.
     * \param[inout] x x display coordinate.
     * \param[inout] y y display coordinate.
     *
     */
    void TransformToSpacedCoordinates(double *x, double *y);

    double PointBounds[6]; /** bounds of a voxel.                     */
    double Offset;         /** offset from the plane to place points. */
    double Spacing[2];     /** spacing of the image.                  */

  private:
    FocalPlanePointPlacer(const FocalPlanePointPlacer&) = delete;
    void operator=(const FocalPlanePointPlacer&) = delete;
};

#endif // _FOCALPLANEPOINTPLACER_H_
