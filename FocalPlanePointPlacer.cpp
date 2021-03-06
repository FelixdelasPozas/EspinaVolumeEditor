///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: FocalPlanePointPlacer.cpp
// Purpose: Used in conjunction with the contour classes to place points on a plane
// Notes: Modified from vtkFocalPlanePointPlacer class in vtk 5.8
///////////////////////////////////////////////////////////////////////////////////////////////////

// project includes
#include "FocalPlanePointPlacer.h"

// vtk includes
#include <vtkCamera.h>
#include <vtkObjectFactory.h>
#include <vtkMath.h>
#include <vtkPlane.h>
#include <vtkPlanes.h>
#include <vtkPlaneCollection.h>
#include <vtkRenderer.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
// FocalPlanePointPlacer class
//
vtkStandardNewMacro(FocalPlanePointPlacer);

///////////////////////////////////////////////////////////////////////////////////////////////////
FocalPlanePointPlacer::FocalPlanePointPlacer()
: Offset{0}
{
  PointBounds[0] = PointBounds[2] = PointBounds[4] = 0;
  PointBounds[1] = PointBounds[3] = PointBounds[5] = -1;

  Spacing[0] = 1;
  Spacing[1] = 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int FocalPlanePointPlacer::ComputeWorldPosition(vtkRenderer *ren, double displayPos[2], double worldPos[3], double worldOrient[9])
{
  double fp[4];
  ren->GetActiveCamera()->GetFocalPoint(fp);
  fp[3] = 1.0;

  ren->SetWorldPoint(fp);
  ren->WorldToDisplay();
  ren->GetDisplayPoint(fp);

  double tmp[4]{ displayPos[0], displayPos[1], fp[2], 0.0 };
  ren->SetDisplayPoint(tmp);
  ren->DisplayToWorld();
  ren->GetWorldPoint(tmp);

  // Translate the focal point by "Offset" from the focal plane along the viewing direction.
  double focalPlaneNormal[3];
  ren->GetActiveCamera()->GetDirectionOfProjection(focalPlaneNormal);
  if (ren->GetActiveCamera()->GetParallelProjection())
  {
    tmp[0] += (focalPlaneNormal[0] * Offset);
    tmp[1] += (focalPlaneNormal[1] * Offset);
    tmp[2] += (focalPlaneNormal[2] * Offset);
  }
  else
  {
    double camPos[3], viewDirection[3];
    ren->GetActiveCamera()->GetPosition(camPos);
    viewDirection[0] = tmp[0] - camPos[0];
    viewDirection[1] = tmp[1] - camPos[1];
    viewDirection[2] = tmp[2] - camPos[2];
    vtkMath::Normalize(viewDirection);
    auto costheta = vtkMath::Dot(viewDirection, focalPlaneNormal) / (vtkMath::Norm(viewDirection) * vtkMath::Norm(focalPlaneNormal));
    if (costheta != 0.0)   // 0.0 Impossible in a perspective projection
    {
      tmp[0] += (viewDirection[0] * Offset / costheta);
      tmp[1] += (viewDirection[1] * Offset / costheta);
      tmp[2] += (viewDirection[2] * Offset / costheta);
    }
  }

  double tolerance[3]{ 1e-12, 1e-12, 1e-12 };
  if (PointBounds[0] < PointBounds[1] && !(vtkMath::PointIsWithinBounds(tmp, PointBounds, tolerance)))
  {
    return 0;
  }

  TransformToSpacedCoordinates(&tmp[0], &tmp[1]);
  worldPos[0] = tmp[0];
  worldPos[1] = tmp[1];
  worldPos[2] = tmp[2];

  ren->SetWorldPoint(worldPos);
  ren->WorldToDisplay();
  ren->GetDisplayPoint(displayPos);

  GetCurrentOrientation(worldOrient);

  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int FocalPlanePointPlacer::ComputeWorldPosition(vtkRenderer *ren, double displayPos[2], double refWorldPos[3], double worldPos[3], double worldOrient[9])
{
  double tmp[4]{ refWorldPos[0], refWorldPos[1], refWorldPos[2], 1.0 };

  ren->SetWorldPoint(tmp);
  ren->WorldToDisplay();
  ren->GetDisplayPoint(tmp);

  tmp[0] = displayPos[0];
  tmp[1] = displayPos[1];
  tmp[3] = 1.0;

  ren->SetDisplayPoint(tmp);
  ren->DisplayToWorld();
  ren->GetWorldPoint(tmp);

  // Translate the focal point by "Offset" from the focal plane along the
  // viewing direction.
  double focalPlaneNormal[3];
  ren->GetActiveCamera()->GetDirectionOfProjection(focalPlaneNormal);
  if (ren->GetActiveCamera()->GetParallelProjection())
  {
    tmp[0] += (focalPlaneNormal[0] * Offset);
    tmp[1] += (focalPlaneNormal[1] * Offset);
    tmp[2] += (focalPlaneNormal[2] * Offset);
  }
  else
  {
    double camPos[3], viewDirection[3];
    ren->GetActiveCamera()->GetPosition(camPos);
    viewDirection[0] = tmp[0] - camPos[0];
    viewDirection[1] = tmp[1] - camPos[1];
    viewDirection[2] = tmp[2] - camPos[2];
    vtkMath::Normalize(viewDirection);
    auto costheta = vtkMath::Dot(viewDirection, focalPlaneNormal) / (vtkMath::Norm(viewDirection) * vtkMath::Norm(focalPlaneNormal));
    if (costheta != 0.0)   // 0.0 Impossible in a perspective projection
    {
      tmp[0] += (viewDirection[0] * Offset / costheta);
      tmp[1] += (viewDirection[1] * Offset / costheta);
      tmp[2] += (viewDirection[2] * Offset / costheta);
    }
  }

  double tolerance[3]{ 1e-12, 1e-12, 1e-12 };
  if (PointBounds[0] < PointBounds[1] && !(vtkMath::PointIsWithinBounds(tmp, PointBounds, tolerance)))
  {
    return 0;
  }

  TransformToSpacedCoordinates(&tmp[0], &tmp[1]);
  worldPos[0] = tmp[0];
  worldPos[1] = tmp[1];
  worldPos[2] = tmp[2];

  // now that we've changed world position to spaced coordinates, update display position too
  double displayPosition[3];
  ren->SetWorldPoint(worldPos);
  ren->WorldToDisplay();
  ren->GetDisplayPoint(displayPosition);
  displayPos[0] = displayPosition[0];
  displayPos[1] = displayPosition[1];

  GetCurrentOrientation(worldOrient);

  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int FocalPlanePointPlacer::ValidateWorldPosition(double* worldPos)
{
  double tolerance[3]{ 1e-12, 1e-12, 1e-12 };
  if (PointBounds[0] < PointBounds[1] && !(vtkMath::PointIsWithinBounds(worldPos, PointBounds, tolerance)))
  {
    return 0;
  }

  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int FocalPlanePointPlacer::ValidateWorldPosition(double* worldPos, double* vtkNotUsed(worldOrient))
{
  double tolerance[3]{ 1e-12, 1e-12, 1e-12 };
  if (PointBounds[0] < PointBounds[1] && !(vtkMath::PointIsWithinBounds(worldPos, PointBounds, tolerance)))
  {
    return 0;
  }

  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void FocalPlanePointPlacer::GetCurrentOrientation(double worldOrient[9]) const
{
  double *x = worldOrient;
  double *y = worldOrient + 3;
  double *z = worldOrient + 6;

  x[0] = 1.0;
  x[1] = 0.0;
  x[2] = 0.0;

  y[0] = 0.0;
  y[1] = 1.0;
  y[2] = 0.0;

  z[0] = 0.0;
  z[1] = 0.0;
  z[2] = 1.0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void FocalPlanePointPlacer::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);

  os << indent << "PointBounds: \n";
  os << indent << "  Xmin,Xmax: (" << PointBounds[0] << ", " << PointBounds[1] << ")\n";
  os << indent << "  Ymin,Ymax: (" << PointBounds[2] << ", " << PointBounds[3] << ")\n";
  os << indent << "  Zmin,Zmax: (" << PointBounds[4] << ", " << PointBounds[5] << ")\n";
  os << indent << "Offset: " << Offset << endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void FocalPlanePointPlacer::TransformToSpacedCoordinates(double *x, double *y)
{
  double xCoord, yCoord;
  auto X = *x;
  auto Y = *y;

  if (*x < 0.0)
  {
    X = -X;
  }

  if (*y < 0.0)
  {
    Y = -Y;
  }

  xCoord = floor(X / Spacing[0]);
  yCoord = floor(Y / Spacing[1]);

  if (fmod(X, Spacing[0]) > (0.5 * Spacing[0]))
  {
    xCoord++;
  }

  if (fmod(Y, Spacing[1]) > (0.5 * Spacing[1]))
  {
    yCoord++;
  }

  xCoord *= Spacing[0];
  yCoord *= Spacing[1];

  if (*x < 0.0)
  {
    *x = -xCoord;
  }
  else
  {
    *x = xCoord;
  }

  if (*y < 0.0)
  {
    *y = -yCoord;
  }
  else
  {
    *y = yCoord;
  }
}
