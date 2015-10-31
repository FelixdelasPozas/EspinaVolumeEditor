///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: Coordinates.cpp
// Purpose: these classes deal with image orientation and transforms image 
//          coordinates to display world coordinates 
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// c++ includes
#include <cmath>
#include <cstdlib>

// project includes
#include "Coordinates.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
CoordinatesTransform::CoordinatesTransform()
{
  m_transform.Identity();
  m_offset.Set(0, 0, 0);
  ComputeAxesVectors();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void CoordinatesTransform::SetTransform(const Vector3i &map, const Vector3ui &size)
{
  // Make sure it's a legal mapping
  for (auto i: {0,1,2})
  {
    assert(abs(map[i]) <= 3 && abs(map[i]) > 0);
    assert(abs(map[i]) != abs(map[(i + 1) % 3]));
  }

  // Initialize the transform matrix
  m_transform = 0;
  for (auto i: {0,1,2})
  {
    if (map[i] > 0)
    {
      m_transform(map[i] - 1, i) = 1.0;
    }
    else
    {
      m_transform(-1 - map[i], i) = -1.0;
    }
  }

  // Initialize the offset vector
  m_offset = m_transform * Vector3i(size[0], size[1], size[2]);

  // Update the offset vector to make it right
  for (auto i: {0,1,2})
  {
    m_offset[i] = ((m_offset[i] < 0) ? -m_offset[i] : 0);
  }

  ComputeAxesVectors();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void CoordinatesTransform::ComputeAxesVectors()
{
  // For this calculation we need the transpose of the matrix
  auto transpose3i = m_transform;
  transpose3i.transpose();

  auto map3i = transpose3i * Vector3i(0, 1, 2);
  m_axesIndex.Set(abs(map3i[0]), abs(map3i[1]), abs(map3i[2]));

  auto axesdir3i = (transpose3i * Vector3i(1, 1, 1));
  m_axesDirection.Set(axesdir3i[0], axesdir3i[1], axesdir3i[2]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
CoordinatesTransform CoordinatesTransform::Inverse() const
{
  // Compute the new transform's details
  CoordinatesTransform inverse;
  inverse.m_transform = m_transform;
  inverse.m_transform.inverse();
  inverse.m_offset = -inverse.m_transform * m_offset;
  inverse.ComputeAxesVectors();

  return inverse;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
CoordinatesTransform CoordinatesTransform::Product(const CoordinatesTransform &transform) const
{
  // Compute the new transform's details
  CoordinatesTransform prod;
  prod.m_transform = m_transform * transform.m_transform;
  prod.m_offset = (m_transform * transform.m_offset) + m_offset;
  prod.ComputeAxesVectors();

  return prod;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
Vector3i CoordinatesTransform::TransformVector(const Vector3i &vector) const
{
  return (m_transform * vector);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
Vector3i CoordinatesTransform::TransformPoint(const Vector3i &point) const
{
  return (m_transform * point) + m_offset;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
Vector3ui CoordinatesTransform::TransformSize(const Vector3ui &size) const
{
  Vector3i sizeSigned3d = m_transform * Vector3i(size[0], size[1], size[2]);

  return Vector3ui(abs(sizeSigned3d[0]), abs(sizeSigned3d[1]), abs(sizeSigned3d[2]));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
unsigned int CoordinatesTransform::GetCoordinateMapping(const unsigned int i) const
{
  return m_axesIndex[i];
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int CoordinatesTransform::GetCoordinateOrientation(const unsigned int i) const
{
  return m_axesDirection[i];
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void CoordinatesTransform::Print(std::ostream &stream) const
{
  stream << "\tTransform: \n" << m_transform;
  stream << "\tOffset: " << m_offset;
  stream << "\tAxes Index: " << m_axesIndex;
  stream << "\tAxes Direction: " << m_axesDirection;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Coordinates Class
//
Coordinates::Coordinates(itk::SmartPointer<ImageType> image)
{
  auto size         = image->GetLargestPossibleRegion().GetSize();
  auto origin       = image->GetOrigin();
  auto spacing      = image->GetSpacing();
  auto cosinematrix = image->GetDirection();

  // initialize class
  for (auto i: {0,1,2})
  {
    for (auto j: {0,1,2})
    {
      m_directionCosineMatrix(i, j) = cosinematrix(i, j);
    }
  }

  // Remap the direction matrix to a mapping vector
  m_mappingVector = ConvertDirectionMatrixToClosestMappingVector(m_directionCosineMatrix);

  m_imageSize.Set(size[0], size[1], size[2]);
  m_imageOrigin.Set(origin[0], origin[1], origin[2]);

  // remap the spacing
  m_imageSpacing[0] = spacing[abs(m_mappingVector[0]) - 1];
  m_imageSpacing[1] = spacing[abs(m_mappingVector[1]) - 1];
  m_imageSpacing[2] = spacing[abs(m_mappingVector[2]) - 1];

  // set transforms
  m_normal.SetTransform(m_mappingVector, m_imageSize);
  m_inverse = m_normal.Inverse();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
Vector3i Coordinates::ConvertDirectionMatrixToClosestMappingVector(const Matrix3d &matrix)
{
  Vector3i result;

  for (int i: {0,1,2})
  {
    // Get the direction of the i-th voxel coordinate
    auto row = matrix.column(i);

    // Get the maximum angle with any axis
    auto maxabs_i = 0;

    for (auto k: {0,1,2})
    {
      if (maxabs_i < std::fabs(row[k])) maxabs_i = std::fabs(row[k]);
    }

    for (auto off: {0,1,2})
    {
      // This trick allows us to visit (i,i) first, so that if one of the
      // direction cosines makes the same angle with two of the axes, we
      // can still assign a valid mapping vector
      auto j = (i + off) % 3;

      // Is j the best-matching direction?
      if (std::fabs(row[j]) == maxabs_i)
      {
        result[i] = ((row[j] > 0) ? j + 1 : -(j + 1));
        break;
      }
    }
  }

  return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
Vector3i Coordinates::InvertMappingVector(const Vector3i &mapvector)
{
  Vector3i inverse;

  for (auto i: {0,1,2})
  {
    if (mapvector[i] > 0)
    {
      inverse[mapvector[i] - 1] = i + 1;
    }
    else
    {
      inverse[-1 - mapvector[i]] = -i - 1;
    }
  }

  return inverse;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
Matrix3d Coordinates::GetImageDirectionCosineMatrix()
{
  Matrix3d matrix{m_directionCosineMatrix};
  return matrix;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const Vector3ui Coordinates::GetTransformedSize() const
{
  return m_normal.TransformSize(m_imageSize);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const Vector3ui Coordinates::GetImageSize() const
{
  return m_imageSize;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const CoordinatesTransform & Coordinates::GetNormalTransform() const
{
  return m_normal;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const CoordinatesTransform & Coordinates::GetInverseTransform() const
{
  return m_inverse;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const Vector3i Coordinates::GetMappingVector() const
{
  return m_mappingVector;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const Vector3ui Coordinates::GetCoordinatesMappingVector() const
{
  return Vector3ui(m_normal.GetCoordinateMapping(0), m_normal.GetCoordinateMapping(1), m_normal.GetCoordinateMapping(2));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const Vector3i Coordinates::GetCoordinatesOrientation() const
{
  return Vector3i(m_normal.GetCoordinateOrientation(0), m_normal.GetCoordinateOrientation(1), m_normal.GetCoordinateOrientation(2));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const Vector3d Coordinates::GetImageOrigin() const
{
  return m_imageOrigin;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const Vector3d Coordinates::GetImageSpacing() const
{
  return m_imageSpacing;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const Vector3d Coordinates::TransformIndexToPoint(const Vector3i &index) const
{
  Vector3i point3i(m_normal.TransformPoint(Vector3i(index[0], index[1], index[2])));

  return Vector3d(point3i[0] * m_imageSpacing[0], point3i[1] * m_imageSpacing[1], point3i[2] * m_imageSpacing[2]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const Vector3i Coordinates::TransformPointToIndex(const Vector3d &point) const
{
  Vector3d point3d(point[0] / m_imageSpacing[0], point[1] / m_imageSpacing[1], point[2] / m_imageSpacing[2]);
  return m_inverse.TransformPoint(Vector3i(point3d[0], point3d[1], point3d[2]));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const Vector3i Coordinates::TransformIndexToIndex(const Vector3i &index) const
{
  return m_normal.TransformPoint(Vector3i(index[0], index[1], index[2]));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const Vector3i Coordinates::TransformIndexToIndexInverse(const Vector3i &index) const
{
  return m_inverse.TransformPoint(Vector3i(index[0], index[1], index[2]));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Coordinates::Print(std::ostream &stream) const
{
  stream << "Direction Cosine Matrix: \n" << m_directionCosineMatrix;
  stream << "Mapping Vector: " << m_mappingVector;
  stream << "Image size: " << m_imageSize;
  stream << "Image Origin: " << m_imageOrigin;
  stream << "Image Spacing: " << m_imageSpacing;
  stream << "Normal transform: " << std::endl;
  m_normal.Print(stream);
  stream << "Inverse transform: " << std::endl;
  m_inverse.Print(stream);
}
