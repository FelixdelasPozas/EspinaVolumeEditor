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
#include <itkImage.h>
#include <itkSmartPointer.h>

// c++ includes
#include <iostream>

// project includes
#include "VectorSpaceAlgebra.h"

// types for labels and label maps
using ImageType = itk::Image<unsigned short, 3>;

///////////////////////////////////////////////////////////////////////////////////////////////////
// CoordinatesTransform Class
//
class CoordinatesTransform
{
  public:
    /** \brief CoordinatesTransform class constructor.
     *
     */
    CoordinatesTransform();

    /** \brief Initialize the transform with new singed coordinate mappings
     * (1-based signed indices).
     * \param[in] map
     * \param[in] size
     *
     */
    void SetTransform(const Vector3i &map, const Vector3ui &size);

    /** \brief Computes the inverse of this transform
     *
     */
    CoordinatesTransform Inverse() const;

    /** \brief Multiply by another transform
     * \param[in] transform transform to multiply by.
     *
     */
    CoordinatesTransform Product(const CoordinatesTransform &transform) const;

    /** \brief Apply transform to a vector
     * \param[in] vector vector to transform.
     *
     */
    Vector3i TransformVector(const Vector3i &vector) const;

    /** \brief Apply transform to a point
     * \param[in] point point to transform.
     *
     */
    Vector3i TransformPoint(const Vector3i &point) const;

    /** \brief Apply to a size vector
     * \param[in] vector vector to transform.
     *
     */
    Vector3ui TransformSize(const Vector3ui &vector) const;

    /** \brief Returns the orientation or the coordinates (-1 or 1)
     * \param[in] index
     *
     */
    int GetCoordinateOrientation(const unsigned int index) const;

    // return the mapping index of the coordinate (0, 1 or 2)
    unsigned int GetCoordinateMapping(const unsigned int index) const;

    /** \brief Print class properties.
     * \param[in] stream stream where the properties will be printed.
     */
    void Print(std::ostream &stream) const;
  private:
    /** \brief Helper method to compute the internal vectors once the matrix and offset have been computed.
     *
     */
    void ComputeAxesVectors();

    Matrix3i  m_transform;     /** transform matrix */
    Vector3i  m_offset;        /** An offset vector */
    Vector3ui m_axesIndex;     /** The operation abs(i) - 1 applied to m_Mapping. */
    Vector3i  m_axesDirection; /** The operation sign(i) applied to m_Mapping. */
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Coordinates Class
//
class Coordinates
{
  public:
    /** \brief Coordinates class constructor.
     * \param[in] image image to get the coordinate properties from.
     *
     */
    explicit Coordinates(itk::SmartPointer<ImageType> image);

    /** \brief Returns the normal transform.
     *
     */
    const CoordinatesTransform & GetNormalTransform() const;

    /** \brief Returns the inverse transform.
     *
     */
    const CoordinatesTransform & GetInverseTransform() const;

    /** \brief Returns the direction cosine matrix that defines trasforms.
     *
     */
    Matrix3d GetImageDirectionCosineMatrix() const;

    /** \brief Returns the transformed image size
     *
     */
    const Vector3ui GetTransformedSize() const;

    /** \brief Returns the image size
     *
     */
    const Vector3ui GetImageSize() const;

    /** \brief Returns the index mapping vector
     *
     */
    const Vector3ui GetCoordinatesMappingVector() const;

    /** \brief Returns the coordinates direction vector (-1 or 1).
     *
     */
    const Vector3i GetCoordinatesOrientation() const;

    /** \brief Returns the mapping vector.
     *
     */
    const Vector3i GetMappingVector() const;

    /** \brief Returns the origin of the image.
     *
     */
    const Vector3d GetImageOrigin() const;

    /** \brief Returns the spacing of the image.
     *
     */
    const Vector3d GetImageSpacing() const;

    /** \brief Index to index transformation.
     * \param[in] index index to transform.
     *
     */
    const Vector3i TransformIndexToIndex(const Vector3i &index) const;

    /** \brief Index to point transformation.
     * \param[in] index index to transform.
     *
     */
    const Vector3d TransformIndexToPoint(const Vector3i &index) const;

    /** \brief Point to index transformation.
     * \param[in] point point to transform.
     *
     */
    const Vector3i TransformPointToIndex(const Vector3d &point) const;

    /** \brief Index to index inverse transform.
     * \param[in] index index to transform.
     *
     */
    const Vector3i TransformIndexToIndexInverse(const Vector3i &index) const;

    /** \brief Prints the class contents.
     * \param[in] stream stream where the properties will be printed.
     *
     */
    void Print(std::ostream &stream) const;
  private:
    /** \brief Helper method to map direction cosines matrix to the closest mapping vector.
     * \param[in] matrix matrix to map.
     *
     */
    static Vector3i ConvertDirectionMatrixToClosestMappingVector(const Matrix3d &matrix);

    /** \brief Helper method to invert a mapping vector
     *
     */
    static Vector3i InvertMappingVector(const Vector3i &vector);

    CoordinatesTransform m_normal;                /** normal transformation. */
    CoordinatesTransform m_inverse;               /** inverse transformation. */
    Matrix3d             m_directionCosineMatrix; /** Image to anatomy direction matrix. */
    Vector3i             m_mappingVector;         /** mapping vector. */
    Vector3ui            m_imageSize;             /** original image size, not transformed. */
    Vector3d             m_imageOrigin;           /** original image origin, not transformed. */
    Vector3d             m_imageSpacing;          /** image spacing transformed to the coordinate mapping. */
};

#endif // _COORDINATES_H_
