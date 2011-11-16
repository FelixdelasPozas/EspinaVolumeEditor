///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: Coordinates.cxx
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
// CoordinatesTransform Class
//
CoordinatesTransform::CoordinatesTransform()
{
    _transform.Identity();
    _offset.Set(0,0,0);
    ComputeAxesVectors();
}

CoordinatesTransform::~CoordinatesTransform()
{
    // empty, nothing to be freed
}

void CoordinatesTransform::SetTransform(const Vector3i &map3i, const Vector3ui &size3ui)
{
    unsigned int i;

    // Make sure it's a legal mapping
    for (i = 0; i < 3; i++)
    {
        assert(abs(map3i[i]) <= 3 && abs(map3i[i]) > 0);
        assert(abs(map3i[i]) != abs(map3i[(i + 1) % 3]));
    }

    // Initialize the transform matrix
    _transform = 0;
    for (i = 0; i < 3; i++)
    {
        if (map3i[i] > 0)
            _transform(map3i[i] - 1, i) = 1.0;
        else
            _transform(-1 - map3i[i], i) = -1.0;
    }

    // Initialize the offset vector
    _offset = _transform * Vector3i(size3ui[0],size3ui[1],size3ui[2]);

    // Update the offset vector to make it right
    for (i = 0; i < 3; i++)
        _offset[i] = ((_offset[i] < 0) ? -_offset[i] : 0);

    ComputeAxesVectors();
}

void CoordinatesTransform::ComputeAxesVectors()
{
    // For this calculation we need the transpose of the matrix
    Matrix3i transpose3i = _transform;
    transpose3i.Transpose();

    Vector3i map3i = transpose3i * Vector3i(0, 1, 2);
    _axesIndex.Set(abs(map3i[0]), abs(map3i[1]), abs(map3i[2]));

    Vector3i axesdir3i = (transpose3i * Vector3i(1,1,1)); 
    _axesDirection.Set(axesdir3i[0], axesdir3i[1], axesdir3i[2]);
}

CoordinatesTransform CoordinatesTransform::Inverse() const
{
    // Compute the new transform's details
    CoordinatesTransform inverse;
    inverse._transform = _transform;
    inverse._transform.Inverse();
    inverse._offset = -inverse._transform * _offset;
    inverse.ComputeAxesVectors();

    return inverse;
}

CoordinatesTransform CoordinatesTransform::Product(const CoordinatesTransform &t) const
{
    // Compute the new transform's details
    CoordinatesTransform prod;
    prod._transform = _transform * t._transform;
    prod._offset = (_transform * t._offset) + _offset;
    prod.ComputeAxesVectors();

    return prod;
}

Vector3i CoordinatesTransform::TransformPoint(const Vector3i &p3i) const
{
    return (_transform * p3i) + _offset;
}

Vector3i CoordinatesTransform::TransformVector(const Vector3i &v3i) const
{
    return (_transform * v3i);
}

Vector3ui CoordinatesTransform::TransformSize(const Vector3ui &size3ui) const
{
    Vector3i sizeSigned3d = _transform * Vector3i(size3ui[0],size3ui[1],size3ui[2]);
    return Vector3ui(abs(sizeSigned3d[0]),abs(sizeSigned3d[1]),abs(sizeSigned3d[2]));
}

// return the index of the coordinates
unsigned int CoordinatesTransform::GetCoordinateMapping(const unsigned int i) const
{
    return _axesIndex[i];
}

// return the orientation or the coordinates (-1 or 1 each)
int CoordinatesTransform::GetCoordinateOrientation(const unsigned int i) const
{
    return _axesDirection[i];
}

// prints class contents
void CoordinatesTransform::Print(std::ostream &stream)
{
    stream << "\tTransform: \n" << _transform;
    stream << "\tOffset: " << _offset;
    stream << "\tAxes Index: " << _axesIndex;
    stream << "\tAxes Direction: " << _axesDirection;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// Coordinates Class
//
Coordinates::Coordinates(itk::SmartPointer<ImageType> image)
{
    // we need the image data for the structured points data
    ImageType::SizeType size = image->GetLargestPossibleRegion().GetSize();
    ImageType::PointType origin = image->GetOrigin();
    ImageType::SpacingType spacing = image->GetSpacing();
    ImageType::DirectionType cosinematrix = image->GetDirection();
    
    // initialize class
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            _directionCosineMatrix(i,j) = cosinematrix(i,j);
    
    // Remap the direction matrix to a mapping vector
    _mappingVector = ConvertDirectionMatrixToClosestMappingVector(_directionCosineMatrix);
    
    _imageSize.Set(size[0], size[1], size[2]);
    _imageOrigin.Set(origin[0], origin[1], origin[2]);
    
    // remap the spacing
    _imageSpacing[0] = spacing[abs(_mappingVector[0])-1];
    _imageSpacing[1] = spacing[abs(_mappingVector[1])-1];
    _imageSpacing[2] = spacing[abs(_mappingVector[2])-1];

    // set transforms
    _normal.SetTransform(_mappingVector, _imageSize);
    _inverse = _normal.Inverse();
}

Coordinates::~Coordinates()
{
    // empty
}

Vector3i Coordinates::ConvertDirectionMatrixToClosestMappingVector(const Matrix3d directionmatrix)
{
    Vector3<int> result;

    for (size_t i = 0; i < 3; i++)
    {
        // Get the direction of the i-th voxel coordinate
        Vector3d row = directionmatrix.GetColumn(i);

        // Get the maximum angle with any axis
        double maxabs_i = 0;

        for (int k = 0; k < 3; k++)
            if (maxabs_i < fabs(row[k]))
                maxabs_i = fabs(row[k]);

        for (size_t off = 0; off < 3; off++)
        {
            // This trick allows us to visit (i,i) first, so that if one of the
            // direction cosines makes the same angle with two of the axes, we 
            // can still assign a valid mapping vector
            size_t j = (i + off) % 3;

            // Is j the best-matching direction?
            if (fabs(row[j]) == maxabs_i)
            {
                result[i] = ((row[j] > 0) ? j + 1 : -(j + 1));
                break;
            }
        }
    }
    return result;
}

Vector3i Coordinates::InvertMappingVector(const Vector3i &mapvector)
{
    Vector3i inverse;

    for (int i = 0; i < 3; i++)
    {
        if (mapvector[i] > 0)
            inverse[mapvector[i] - 1] = i + 1;
        else
            inverse[-1 - mapvector[i]] = -i - 1;
    }

    return inverse;
}

Matrix3d Coordinates::GetImageDirectionCosineMatrix()
{
    return _directionCosineMatrix;
}

const Vector3ui Coordinates::GetTransformedSize()
{
    return _normal.TransformSize(_imageSize);
}

const Vector3ui Coordinates::GetImageSize()
{
    return _imageSize;
}

const CoordinatesTransform & Coordinates::GetNormalTransform()
{
    return _normal;
}

const CoordinatesTransform & Coordinates::GetInverseTransform()
{
    return _inverse;
}

const Vector3i Coordinates::GetMappingVector()
{
    return _mappingVector;
}

const Vector3ui Coordinates::GetCoordinatesMappingVector()
{
    return Vector3ui(_normal.GetCoordinateMapping(0), _normal.GetCoordinateMapping(1), _normal.GetCoordinateMapping(2));
}

const Vector3i Coordinates::GetCoordinatesOrientation()
{
    return Vector3i(_normal.GetCoordinateOrientation(0), _normal.GetCoordinateOrientation(1), _normal.GetCoordinateOrientation(2));
}

const Vector3d Coordinates::GetImageOrigin()
{
    return _imageOrigin;
}

const Vector3d Coordinates::GetImageSpacing()
{
    return _imageSpacing;
}

const Vector3d Coordinates::TransformIndexToPoint(Vector3i index)
{
    Vector3i point3i(_normal.TransformPoint(Vector3i(index[0], index[1], index[2])));
    return Vector3d(point3i[0]*_imageSpacing[0], point3i[1]*_imageSpacing[1], point3i[2]*_imageSpacing[2]);
}

const Vector3i Coordinates::TransformPointToIndex(Vector3d point)
{
    Vector3d point3d(point[0]/_imageSpacing[0], point[1]/_imageSpacing[1], point[2]/_imageSpacing[2]);
    return _inverse.TransformPoint(Vector3i(point3d[0], point3d[1], point3d[2]));
}

const Vector3i Coordinates::TransformIndexToIndex(Vector3i index)
{
    return _normal.TransformPoint(Vector3i(index[0], index[1], index[2]));
}

const Vector3i Coordinates::TransformIndexToIndexInverse(Vector3i index)
{
    return _inverse.TransformPoint(Vector3i(index[0], index[1], index[2]));
}

void Coordinates::Print(std::ostream &stream)
{
    stream << "Direction Cosine Matrix: \n" << _directionCosineMatrix;
    stream << "Mapping Vector: " << _mappingVector;
    stream << "Image size: " << _imageSize;
    stream << "Image Origin: " << _imageOrigin;
    stream << "Image Spacing: " << _imageSpacing;
    stream << "Normal transform: " << std::endl;
    _normal.Print(stream);
    stream << "Inverse transform: " << std::endl;
    _inverse.Print(stream);
}
