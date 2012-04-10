///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: ImageActorPointPlacer.h
// Purpose: Custom point placer for lasso and polygon selections
// Notes: Ripper from vtkImageActorPointPlacer class in vtk 5.8
// 		  Changes over vtk 5.8 class: the imageactor bounds never changes, deleted that part.
//                                    Adapted the returned vales to the center of the voxels.
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _IMAGEACTORPOINTPLACER_H_
#define _IMAGEACTORPOINTPLACER_H_

// vtk includes
#include <vtkPointPlacer.h>

// forward declarations
class vtkBoundedPlanePointPlacer;
class vtkImageActor;
class vtkRenderer;

///////////////////////////////////////////////////////////////////////////////////////////////////
// ImageActorPointPlacer class
//
class VTK_WIDGETS_EXPORT ImageActorPointPlacer: public vtkPointPlacer
{
	public:
		// Description:
		// Instantiate this class.
		static ImageActorPointPlacer *New();

		// Description:
		// Standard methods for instances of this class.
		vtkTypeMacro(ImageActorPointPlacer,vtkPointPlacer);
		void PrintSelf(ostream& os, vtkIndent indent);

		// Description:
		// Given and renderer and a display position in pixels,
		// find a world position and orientation. In this class
		// an internal vtkBoundedPlanePointPlacer is used to compute
		// the world position and orientation. The internal placer
		// is set to use the plane of the image actor and the bounds
		// of the image actor as the constraints for placing points.
		int ComputeWorldPosition(vtkRenderer *ren, double displayPos[2], double worldPos[3], double worldOrient[9]);

		// Description:
		// This method is identical to the one above since the
		// reference position is ignored by the bounded plane
		// point placer.
		int ComputeWorldPosition(vtkRenderer *ren, double displayPos[2], double refWorldPos[2], double worldPos[3], double worldOrient[9]);

		// Description:
		// This method validates a world position by checking to see
		// if the world position is valid according to the constraints
		// of the internal placer (essentially - is this world position
		// on the image?)
		int ValidateWorldPosition(double worldPos[3]);

		// Description:
		// This method is identical to the one above since the bounded
		// plane point placer ignores orientation
		int ValidateWorldPosition(double worldPos[3], double worldOrient[9]);

		// Description:
		// Update the world position and orientation according the
		// the current constraints of the placer. Will be called
		// by the representation when it notices that this placer
		// has been modified.
		int UpdateWorldPosition(vtkRenderer *ren, double worldPos[3], double worldOrient[9]);

		// Description:
		// A method for configuring the internal placer according
		// to the constraints of the image actor.
		// Called by the representation to give the placer a chance
		// to update itself, which may cause the MTime to change,
		// which would then cause the representation to update
		// all of its points
		int UpdateInternalState();

		// Description:
		// Set / get the reference vtkImageActor used to place the points.
		// An image actor must be set for this placer to work. An internal
		// bounded plane point placer is created and set to match the bounds
		// of the displayed image.
		void SetImageActor(vtkImageActor *);
		vtkGetObjectMacro( ImageActor, vtkImageActor );

		// Description:
		// Optionally, you may set bounds to restrict the placement of the points.
		// The placement of points will then be constrained to lie not only on
		// the ImageActor but also within the bounds specified. If no bounds are
		// specified, they may lie anywhere on the supplied ImageActor.
		vtkSetVector6Macro( Bounds, double );
		vtkGetVector6Macro( Bounds, double );

		// Description:
		// Set the world tolerance. This propagates it to the internal
		// BoundedPlanePointPlacer.
		virtual void SetWorldTolerance(double s);
	protected:
		ImageActorPointPlacer();
		~ImageActorPointPlacer();

		// auxiliary methods to get the world/actor coordinates from a point
		void TransformToSpacedCoordinates(double*, double*);

		// The reference image actor. Must be configured before this placer
		// is used.
		vtkImageActor *ImageActor;

		// The internal placer.
		vtkBoundedPlanePointPlacer *Placer;

		// See the SetBounds method
		double Bounds[6];

		// image spacing
		double Spacing[3];

	private:
		ImageActorPointPlacer(const ImageActorPointPlacer&);   //Not implemented
		void operator=(const ImageActorPointPlacer&);   //Not implemented
};

#endif // _IMAGEACTORPOINTPLACER_H_
