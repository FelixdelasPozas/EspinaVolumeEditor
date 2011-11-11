///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: Coordinates.h
// Purpose: these classes deal with image orientation and transforms image 
//          coordinates to display world coordinates 
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _COORDINATES_H_
#define _COORDINATES_H_

// itk includes
#include <itkLabelMap.h>
#include <itkLabelObject.h>
#include <itkSmartPointer.h>

// c++ includes
#include <iostream>

// project includes
#include "VectorSpaceAlgebra.h"

// types for labels and label maps
typedef itk::LabelObject<unsigned short, 3 > LabelObjectType;
typedef itk::LabelMap< LabelObjectType > LabelMapType;

///////////////////////////////////////////////////////////////////////////////////////////////////
// CoordinatesTransform Class
//
class CoordinatesTransform
{
    public:
        // Default constructor creates an identity transform
        CoordinatesTransform();
        ~CoordinatesTransform();

        // Initialize the transform with new singed coordinate mappings 
        // (1-based signed indices)
        void SetTransform(const Vector3i &, const Vector3ui &);

        // Compute the inverse of this transform
        CoordinatesTransform Inverse() const;

        // Multiply by another transform 
        CoordinatesTransform Product(const CoordinatesTransform &) const;

        // Apply transform to a vector 
        Vector3i TransformVector(const Vector3i &) const;

        // Apply transform to a point
        Vector3i TransformPoint(const Vector3i &) const;

        // Apply to a size vector 
        Vector3ui TransformSize(const Vector3ui &) const;

        // return the orientation or the coordinates (-1 or 1)
        int GetCoordinateOrientation(unsigned int) const;

        // return the mapping index of the coordinate (0, 1 or 2)
        unsigned int GetCoordinateMapping(unsigned int) const;
        
        // Print class contents
        void Print(std::ostream &);
    private:
        // Compute the internal vectors once the matrix and offset have been computed
        void ComputeAxesVectors();

        // Attributes
        //
        // A transform matrix
        Matrix3i _transform;

        // An offset vector
        Vector3i _offset;

        // The operation abs(i) - 1 applied to m_Mapping
        Vector3ui _axesIndex;

        // The operation sign(i) applied to m_Mapping
        Vector3i _axesDirection;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Coordinates Class
//
class Coordinates
{
    public:
        // constructor & destructor. initializes the class with labelmap orientation data
        Coordinates(itk::SmartPointer<LabelMapType> );
        ~Coordinates();
        
        // get the normal transform 
        const CoordinatesTransform & GetNormalTransform();

        // get the normal transform 
        const CoordinatesTransform & GetInverseTransform();

        // get the direction cosine matrix that defines trasforms
        Matrix3d GetImageDirectionCosineMatrix();

        // get the transformed image size
        const Vector3ui GetTransformedSize();
        
        // get the image size
        const Vector3ui GetImageSize();
        
        // get the index mapping vector 
        const Vector3ui GetCoordinatesMappingVector();
        
        // get the coordinates direction vector (-1 or 1)
        const Vector3i GetCoordinatesOrientation();
        
        // get mapping vector
        const Vector3i GetMappingVector();
        
        // get image origin
        const Vector3d GetImageOrigin();
        
        // get image spacing
        const Vector3d GetImageSpacing();
        
        // index to index
        const Vector3i TransformIndexToIndex(Vector3i);
        
        // index to point transform
        const Vector3d TransformIndexToPoint(Vector3i);
        
        // point to index transform
        const Vector3i TransformPointToIndex(Vector3d);
        
        // index to index inverse
        const Vector3i TransformIndexToIndexInverse(Vector3i index);
        
        // Print class contents
        void Print(std::ostream &);
    private:
        // Map direction cosines matrix to the closest mapping vector
        static Vector3i ConvertDirectionMatrixToClosestMappingVector(const Matrix3d);

        // Invert a mapping vector
        static Vector3i InvertMappingVector(const Vector3i &);

        // Transforms
        CoordinatesTransform _normal;
        CoordinatesTransform _inverse;

        // Attributes
        //
        // Image to anatomy direction matrix
        Matrix3d _directionCosineMatrix;
        
        // mapping vector
        Vector3i _mappingVector;
        
        // original image size, not transformed
        Vector3ui _imageSize;
        
        // original image origin, not transformed
        Vector3d _imageOrigin;

        // image spacing transformed to the coordinate mapping
        Vector3d _imageSpacing;
};

#endif // _COORDINATES_H_
