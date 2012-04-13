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

class VTK_WIDGETS_EXPORT FocalPlanePointPlacer: public vtkPointPlacer
{
	public:
		// Description:
		// Instantiate this class.
		static FocalPlanePointPlacer *New();

		// Description:
		// Standard methods for instances of this class.
		vtkTypeMacro(FocalPlanePointPlacer,vtkPointPlacer);
		void PrintSelf(ostream& os, vtkIndent indent);

		// Descirption:
		// Given a renderer and a display position, compute
		// the world position and orientation. The orientation
		// computed by the placer will always line up with the
		// standard coordinate axes. The world position will be
		// computed by projecting the display position onto the
		// focal plane. This method is typically used to place a
		// point for the first time.
		int ComputeWorldPosition(vtkRenderer *ren, double displayPos[2], double worldPos[3], double worldOrient[9]);

		// Description:
		// Given a renderer, a display position, and a reference
		// world position, compute a new world position. The
		// orientation will be the standard coordinate axes, and the
		// computed world position will be created by projecting
		// the display point onto a plane that is parallel to
		// the focal plane and runs through the reference world
		// position. This method is typically used to move existing
		// points.
		int ComputeWorldPosition(vtkRenderer *ren, double displayPos[2], double refWorldPos[3], double worldPos[3], double worldOrient[9]);

		// Description:
		// Validate a world position. All world positions
		// are valid so these methods always return 1.
		int ValidateWorldPosition(double worldPos[3]);
		int ValidateWorldPosition(double worldPos[3], double worldOrient[9]);

		// Description:
		// Optionally specify a signed offset from the focal plane for the points to
		// be placed at.  If negative, the constraint plane is offset closer to the
		// camera. If positive, its further away from the camera.
		vtkSetMacro( Offset, double );
		vtkGetMacro( Offset, double );

		// Description:
		// Optionally Restrict the points to a set of bounds. The placer will
		// invalidate points outside these bounds.
		vtkSetVector6Macro( PointBounds, double );
		vtkGetVector6Macro( PointBounds, double );

		// Spacing needed to place points only on voxel centers
		vtkSetVector2Macro( Spacing, double);
		vtkGetVector2Macro( Spacing, double);

	protected:
		FocalPlanePointPlacer();
		~FocalPlanePointPlacer();

		void GetCurrentOrientation(double worldOrient[9]);

		// transform parameters constrained to spaced world coordinates using spacing data
		void TransformToSpacedCoordinates(double *, double *);

		double PointBounds[6];
		double Offset;
		double Spacing[2];

	private:
		FocalPlanePointPlacer(const FocalPlanePointPlacer&);   //Not implemented
		void operator=(const FocalPlanePointPlacer&);   //Not implemented
};

#endif // _FOCALPLANEPOINTPLACER_H_
