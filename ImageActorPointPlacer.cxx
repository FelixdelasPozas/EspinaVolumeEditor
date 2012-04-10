///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: ImageActorPointPlacer.cxx
// Purpose: Custom point placer for lasso and polygon selections
// Notes: Ripper from vtkImageActorPointPlacer class in vtk 5.8.
// 		  Changes over vtk 5.8 class: the imageactor bounds never changes, deleted that part.
//                                    Adapted the returned vales to the center of the voxels.
///////////////////////////////////////////////////////////////////////////////////////////////////

// project includes
#include "ImageActorPointPlacer.h"

// vtk includes
#include <vtkObjectFactory.h>
#include <vtkBoundedPlanePointPlacer.h>
#include <vtkPlane.h>
#include <vtkRenderer.h>
#include <vtkImageActor.h>
#include <vtkImageData.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
// ImageActorPointPlacer class
//
vtkStandardNewMacro(ImageActorPointPlacer);

vtkCxxSetObjectMacro(ImageActorPointPlacer, ImageActor, vtkImageActor);

ImageActorPointPlacer::ImageActorPointPlacer()
{
	this->Placer = vtkBoundedPlanePointPlacer::New();
	this->ImageActor = NULL;
	this->Bounds[0] = this->Bounds[2] = this->Bounds[4] = VTK_DOUBLE_MAX;
	this->Bounds[1] = this->Bounds[3] = this->Bounds[5] = VTK_DOUBLE_MIN;
}

ImageActorPointPlacer::~ImageActorPointPlacer()
{
	this->Placer->Delete();
	this->SetImageActor(NULL);
}

int ImageActorPointPlacer::ComputeWorldPosition(vtkRenderer *ren, double displayPos[2], double *refWorldPos, double worldPos[3], double worldOrient[9])
{
	if (!this->UpdateInternalState())
		return 0;

	if (0 == this->Placer->ComputeWorldPosition(ren, displayPos, refWorldPos, worldPos, worldOrient))
		return 0;

	if ((worldPos[0] < this->Bounds[0]) || (worldPos[0] > this->Bounds[1]) || (worldPos[1] < this->Bounds[2]) || (worldPos[1] > this->Bounds[3]))
		return 0;

	this->TransformToSpacedCoordinates(&worldPos[0], &worldPos[1]);
	return 1;
}

int ImageActorPointPlacer::ComputeWorldPosition(vtkRenderer *ren, double displayPos[2], double worldPos[3], double worldOrient[9])
{
	if (!this->UpdateInternalState())
		return 0;

	if (0 == this->Placer->ComputeWorldPosition(ren, displayPos, worldPos, worldOrient))
		return 0;

	if ((worldPos[0] < this->Bounds[0]) || (worldPos[0] > this->Bounds[1]) || (worldPos[1] < this->Bounds[2]) || (worldPos[1] > this->Bounds[3]))
		return 0;

	this->TransformToSpacedCoordinates(&worldPos[0], &worldPos[1]);
	return 1;
}

int ImageActorPointPlacer::ValidateWorldPosition(double worldPos[3], double *worldOrient)
{
	if (!this->UpdateInternalState())
		return 0;

	return this->Placer->ValidateWorldPosition(worldPos, worldOrient);
}

int ImageActorPointPlacer::ValidateWorldPosition(double worldPos[3])
{
	if (!this->UpdateInternalState())
		return 0;

	return this->Placer->ValidateWorldPosition(worldPos);
}

int ImageActorPointPlacer::UpdateWorldPosition(vtkRenderer *ren, double worldPos[3], double worldOrient[9])
{
	if (!this->UpdateInternalState())
		return 0;

	return this->Placer->UpdateWorldPosition(ren, worldPos, worldOrient);
}

int ImageActorPointPlacer::UpdateInternalState()
{
	if (!this->ImageActor)
		return 0;

	vtkImageData *input = this->ImageActor->GetInput();
	if (!input)
		return 0;

	input->GetSpacing(this->Spacing);

	double origin[3];
	input->GetOrigin(origin);

	double bounds[6];
	this->ImageActor->GetBounds(bounds);
	this->Bounds[0] = bounds[0];
	this->Bounds[1] = bounds[1];
	this->Bounds[2] = bounds[2];
	this->Bounds[3] = bounds[3];
	this->Bounds[4] = bounds[4];
	this->Bounds[5] = bounds[5];

	int displayExtent[6];
	this->ImageActor->GetDisplayExtent(displayExtent);

	// our images always have a 0 Z component (is just a plane) so we will have fixed bounding planes
	// in the X and Y coordinates
	int axis = vtkBoundedPlanePointPlacer::ZAxis;
	double position = 0.0;

	if (axis != this->Placer->GetProjectionNormal() || position != this->Placer->GetProjectionPosition())
	{
		this->Placer->SetProjectionNormal(axis);
		this->Placer->SetProjectionPosition(position);

		this->Placer->RemoveAllBoundingPlanes();

		vtkPlane *plane;

		plane = vtkPlane::New();
		plane->SetOrigin(bounds[0], bounds[2], bounds[4]);
		plane->SetNormal(1.0, 0.0, 0.0);
		this->Placer->AddBoundingPlane(plane);
		plane->Delete();

		plane = vtkPlane::New();
		plane->SetOrigin(bounds[1], bounds[3], bounds[5]);
		plane->SetNormal(-1.0, 0.0, 0.0);
		this->Placer->AddBoundingPlane(plane);
		plane->Delete();

		plane = vtkPlane::New();
		plane->SetOrigin(bounds[0], bounds[2], bounds[4]);
		plane->SetNormal(0.0, 1.0, 0.0);
		this->Placer->AddBoundingPlane(plane);
		plane->Delete();

		plane = vtkPlane::New();
		plane->SetOrigin(bounds[1], bounds[3], bounds[5]);
		plane->SetNormal(0.0, -1.0, 0.0);
		this->Placer->AddBoundingPlane(plane);
		plane->Delete();

		this->Modified();
	}

	return 1;
}

void ImageActorPointPlacer::SetWorldTolerance(double tol)
{
	if (this->WorldTolerance != (tol < 0.0 ? 0.0 : (tol > VTK_DOUBLE_MAX ? VTK_DOUBLE_MAX : tol)))
	{
		this->WorldTolerance = (tol < 0.0 ? 0.0 : (tol > VTK_DOUBLE_MAX ? VTK_DOUBLE_MAX : tol));
		this->Placer->SetWorldTolerance(tol);
		this->Modified();
	}
}

void ImageActorPointPlacer::PrintSelf(ostream& os, vtkIndent indent)
{
	this->Superclass::PrintSelf(os, indent);

	double *bounds = this->GetBounds();
	if (bounds != NULL)
	{
		os << indent << "Bounds: \n";
		os << indent << "  Xmin,Xmax: (" << this->Bounds[0] << ", " << this->Bounds[1] << ")\n";
		os << indent << "  Ymin,Ymax: (" << this->Bounds[2] << ", " << this->Bounds[3] << ")\n";
		os << indent << "  Zmin,Zmax: (" << this->Bounds[4] << ", " << this->Bounds[5] << ")\n";
	}
	else
	{
		os << indent << "Bounds: (not defined)\n";
	}

	os << indent << "Image Actor: " << this->ImageActor << "\n";
}

void ImageActorPointPlacer::TransformToSpacedCoordinates(double *x, double *y)
{
	double xCoord, yCoord;

	if (*x < 0.0)
		xCoord = 0.0;
	else
		xCoord = floor(*x / this->Spacing[0]);

	if (*y < 0.0)
		yCoord = 0.0;
	else
		yCoord = floor(*y / this->Spacing[1]);

	if ( fmod(*x, this->Spacing[0]) >= 0.5)
		xCoord++;

	if ( fmod(*y, this->Spacing[1]) >= 0.5)
		yCoord++;

	*x = xCoord * this->Spacing[0];
	*y = yCoord * this->Spacing[1];
}
