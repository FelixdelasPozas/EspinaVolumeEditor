///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: VectorSpaceAlgebra.h
// Purpose: 3x3 matrix and 1x3 vector operations
// Notes: Beware of type conversions (<class X, class T> operations). Usual typedefs at the end.
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _VECTORSPACEALGEBRA_H_
#define _VECTORSPACEALGEBRA_H_

// c++ includes
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

///////////////////////////////////////////////////////////////////////////////////////////////////
class algebra_error: public std::logic_error
{
  public:
    algebra_error(const std::string& arg)
    : std::logic_error(arg)
    {}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Vector class
//
template<class T> class Vector3
{
  public:
    /** \brief Vector3 default constructor.
     *
     */
    Vector3()
    {
      data.push_back(T(0));
      data.push_back(T(0));
      data.push_back(T(0));
    }

    /** \brief Vector3 vector constructor.
     * \param[in] v vector.
     *
     */
    Vector3(const Vector3<T> &v)
    {
      data.push_back(v[0]);
      data.push_back(v[1]);
      data.push_back(v[2]);
    }

    /** \brief Vector3 value constructor.
     * \param[in] value T value.
     *
     */
    Vector3(const T &value)
    {
      data.push_back(value);
      data.push_back(value);
      data.push_back(value);
    }

    /** \brief Vector3 multiple values constructor.
     * \param[in] x T value.
     * \param[in] y T value.
     * \param[in] z T value.
     *
     */
    template<class X> Vector3(const X &x, const X &y, const X &z)
    {
      data.push_back(static_cast<T>(x));
      data.push_back(static_cast<T>(y));
      data.push_back(static_cast<T>(z));
    }

    /** \brief Scalar assignment operator.
     * \param[in] value T value.
     *
     */
    Vector3<T>& operator=(const T &value)
    {
      data[0] = data[1] = data[2] = value;

      return *this;
    }

    /** \brief Vector assignment operator.
     * \param[in] v vector.
     *
     */
    Vector3<T>& operator=(const Vector3<T> &v)
    {
      data[0] = v[0];
      data[1] = v[1];
      data[2] = v[2];

      return *this;
    }

    /** \brief Logical operator ==
     * \param[in] a vector.
     * \param[in] b vector.
     *
     */
    friend bool operator==(const Vector3<T> &a, const Vector3<T> &b) throw ()
    {
      for (auto i: {0,1,2})
      {
        if (a[i] != b[i])
        {
          return false;
        }
      }

      return true;
    }

    /** \brief Logical operator !=
     * \param[in] a vector.
     * \param[in] b vector.
     *
     */
    friend bool operator!=(const Vector3<T> &a, const Vector3<T> &b)
    {
      return !(a == b);
    }

    /** \brief Subscript operator
     * \param[in] i index value.
     *
     */
    inline T& operator[](int index)
    {
      assert((0 <= index) && (index < 3));

      return data[index];
    }

    /** \brief Const subscript operator
     * \param[in] i index value.
     *
     */
    inline const T& operator[](int index) const
    {
      assert((0 <= index) && (index < 3));

      return data[index];
    }

    /** \brief Unary operator +
     *
     */
    inline const Vector3<T> operator+()
    {
      Vector3<T> result = *this;

      return result;
    }

    /** \brief Unary operator -
     *
     */
    inline const Vector3<T> operator-()
    {
      Vector3<T> result;
      result.data[0] = -data[0];
      result.data[1] = -data[1];
      result.data[2] = -data[2];

      return result;
    }

    /** \brief Set values
     * \param[in] x T value.
     * \param[in] y T value.
     * \param[in] z T value.
     *
     */
    template<class X> inline Vector3<T>& Set(const X &x, const X &y, const X &z)
    {
      data[0] = static_cast<T>(x);
      data[1] = static_cast<T>(y);
      data[2] = static_cast<T>(z);
      return *this;
    }
  private:
    std::vector<T> data;
};

/** \brief operator<<
 * \param[in] stream iostream.
 * \param[in] a vector.
 *
 */
template<class T> std::ostream& operator<<(std::ostream &stream, const Vector3<T> &v)
{
  stream << "[";
  for (auto i: {0,1,2})
  {
    stream << v[i] << ((i == 2) ? "]" : ",");
  }

  return stream;
}

/** \brief operator* (scalar*vector)
 * \param[in] c T value.
 * \param[in] a vector.
 */
template<class T> inline Vector3<T> operator*(const T &c, const Vector3<T> &v)
{
  Vector3<T> tmp(0);

  for (auto i: {0,1,2})
  {
    tmp[i] = v[i] * c;
  }

  return tmp;
}

/** \brief operator* (vector*value)
 * \param[in] v vector.
 * \param[in] c T value.
 *
 */
template<class T> inline Vector3<T> operator*(const Vector3<T> &v, const T &c)
{
  Vector3<T> tmp(0);

  for (auto i: {0,1,2})
  {
    tmp[i] = v[i] * c;
  }

  return tmp;
}

/** \brief operator* (vector*value)
 * \param[in] v vector.
 * \param[in] c X value.
 *
 */
template<class T, class X> inline Vector3<T> operator*(const Vector3<T> &v, const X &c)
{
  Vector3<T> tmp(0);

  for (auto i: {0,1,2})
  {
    tmp[i] = v[i] * static_cast<T>(c);
  }

  return tmp;
}

/** \brief operator/ (vector/value)
 * \param[in] v vector.
 * \param[in] c T value.
 *
 */
template<class T, class X> inline Vector3<T> operator/(const Vector3<T> &v, const X &c)
{
  if (c == T(0))
  {
    throw algebra_error("Vector3::operator/: division by zero");
  }

  Vector3<T> tmp(0);

  for (auto i: {0,1,2})
  {
    tmp[i] = v[i] / static_cast<T>(c);
  }

  return tmp;
}

/** \brief operator+ (binary)
 * \param[in] a vector.
 * \param[in] b vector.
 *
 */
template<class T> inline Vector3<T> operator+(const Vector3<T> &a, const Vector3<T> &b)
{
  Vector3<T> ret(0);

  for (auto i: {0,1,2})
  {
    ret[i] = a[i] + b[i];
  }

  return ret;
}

/** \brief operator- (binary)
 * \param[in] a vector.
 * \param[in] b vector.
 *
 */
template<class T> inline Vector3<T> operator-(const Vector3<T> &a, const Vector3<T> &b)
{
  Vector3<T> ret(0);

  for (auto i: {0,1,2})
  {
    ret[i] = a[i] - b[i];
  }

  return ret;
}

/** \brief operator* (binary, dot product)
 * \param[in] a vector.
 * \param[in] b vector.
 *
 */
template<class T> inline T operator*(const Vector3<T> &a, const Vector3<T> &b)
{
  T sum = 0;

  for (auto i: {0,1,2})
  {
    sum += a[i] * b[i];
  }

  return sum;
}

/** \brief operator+= (binary)
 * \param[in] a vector.
 * \param[in] b vector.
 *
 */
template<class T> inline Vector3<T> operator+=(Vector3<T> &a, const Vector3<T> &b)
{
  for (auto i: {0,1,2})
  {
    a[i] = a[i] + b[i];
  }

  return a;
}

/** \brief operator-= (binary)
 * \param[in] a vector.
 * \param[in] b vector.
 *
 */
template<class T> inline Vector3<T> operator-=(Vector3<T> &a, const Vector3<T> &b)
{
  for (auto i: {0,1,2})
  {
    a[i] = a[i] - b[i];
  }

  return a;
}

/** \brief operator*= (vector*value)
 * \param[in] a vector.
 * \param[in] c X value.
 *
 */
template<class T> inline Vector3<T> operator*=(const Vector3<T> &v, const T &c)
{
  for (auto i: {0,1,2})
  {
    v[i] = v[i] * c;
  }
}

/** \brief operator*= (vector*value)
 * \param[in] a vector.
 * \param[in] c X value.
 *
 */
template<class T, class X> inline Vector3<T> operator*=(const Vector3<T> &v, const X &c)
{
  for (auto i: {0,1,2})
  {
    v[i] = v[i] * static_cast<T>(c);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Matrix class
//
template<class T> class Matrix3
{
  public:
    /** \brief Matrix3 default constructor.
     *
     */
    Matrix3()
    {
      Vector3<T> a;
      Vector3<T> b;
      Vector3<T> c;
      data.push_back(a);
      data.push_back(b);
      data.push_back(c);
    }

    /** \brief Matrix3 constructor with a matrix.
     * \param[in] a matrix.
     *
     */
    Matrix3(Matrix3<T> &a)
    {
      Vector3<T> x = a[0];
      Vector3<T> y = a[1];
      Vector3<T> z = a[2];
      data.push_back(x);
      data.push_back(y);
      data.push_back(z);
    }

    /** \brief Matrix3 constructor with a scalar.
     * \param[in] value scalar value.
     *
     */
    Matrix3(const T& scalar)
    {
      Vector3<T> x(scalar);
      Vector3<T> y(scalar);
      Vector3<T> z(scalar);
      data.push_back(x);
      data.push_back(y);
      data.push_back(z);
    }

    /** \brief Operator =(scalar)
     * \param[in] scalar scalar value.
     *
     */
    const Matrix3<T>& operator=(T& scalar)
    {
      data[0] = scalar;
      data[1] = scalar;
      data[2] = scalar;

      return *this;
    }

    /** \brief Operator =(matrix)
     * \param[in] a matrix.
     *
     */
    Matrix3<T>& operator=(const Matrix3<T> &a)
    {
      for (auto i: {0,1,2})
      {
        for (auto j: {0,1,2})
        {
          data[i][j] = a[i][j];
        }
      }

      return *this;
    }

    /** \brief Operator ==
     * \param[in] a matrix.
     * \param[in] b matrix.
     *
     */
    friend bool operator==(const Matrix3<T>& a, const Matrix3<T>& b)
    {
      for (auto i: {0,1,2})
      {
        for (auto j: {0,1,2})
        {
          if (a[i][j] != b[i][j])
          {
            return false;
          }
        }
      }

      return true;
    }

    /** \brief Operator !=
     * \param[in] a matrix.
     * \param[in] b matrix.
     *
     */
    friend bool operator!=(const Matrix3<T>& a, const Matrix3<T>& b)
    {
      return !(a == b);
    }

    /** \brief Returns true if the matrix is null.
     *
     */
    const bool isNull()
    {
      for (auto i: {0,1,2})
      {
        for (auto j: {0,1,2})
        {
          if (data[i][j] != 0)
          {
            return false;
          }
        }
      }

      return true;
    }

    /** \brief Subscript operator.
     * \param[in] i vector index.
     *
     */
    inline T& operator()(const int i, const int j)
    {
      assert((0 <= i) && (i < 3));
      assert((0 <= j) && (j < 3));

      return data[i][j];
    }

    /** \brief Const subscript operator.
     * \param[in] i vector index.
     *
     */
    inline const T& operator()(const int i, const int j) const
    {
      assert((0 <= i) && (i < 3));
      assert((0 <= j) && (j < 3));

      return data[i][j];
    }

    /** \brief Subscript operator.
     * \param[in] i vector index.
     *
     */
    inline Vector3<T>& operator[](const int i)
    {
      assert((0 <= i) && (i < 3));

      return data[i];
    }

    /** \brief Const subscript operator.
     * \param[in] i vector index.
     *
     */
    inline const Vector3<T>& operator[](const int i) const
    {
      assert((0 <= i) && (i < 3));

      return data[i];
    }

    /** \brief Unary operator +
     *
     */
    inline const Matrix3<T> operator+()
    {
      Matrix3<T> result = *this;

      return result;
    }

    /** \brief Unary operator -
     *
     */
    inline const Matrix3<T> operator-()
    {
      Matrix3<T> result;
      result.data[0] = -data[0];
      result.data[1] = -data[1];
      result.data[2] = -data[2];

      return result;
    }

    /** \brief Transpose operator.
     *
     */
    Matrix3<T>& transpose()
    {
      Matrix3<T> copy(*this);

      for (auto i: {0,1,2})
      {
        for (auto j: {0,1,2})
        {
          data[j][i] = copy[i][j];
        }
      }

      return *this;
    }

    /** \brief Determinant operator.
     *
     */
    float determinant()
    {
      return ((data[0][0] * ((data[1][1] * data[2][2]) - (data[1][2] * data[2][1]))) -
              (data[0][1] * ((data[1][0] * data[2][2]) - (data[1][2] * data[2][0]))) +
              (data[0][2] * ((data[1][0] * data[2][1]) - (data[1][1] * data[2][0]))));
    }

    /** \brief Inverse operator.
     *
     */
    Matrix3<T>& inverse()
    {
      auto det = determinant();
      if (det == 0)
      {
        throw algebra_error("Matrix3::Inverse: determinant is zero");
      }

      det = 1 / det;
      Matrix3<T> Inv(*this);

      data[0][0] = ((Inv[1][1] * Inv[2][2]) - (Inv[1][2] * Inv[2][1])) * det;
      data[0][1] = ((Inv[0][2] * Inv[2][1]) - (Inv[0][1] * Inv[2][2])) * det;
      data[0][2] = ((Inv[0][1] * Inv[1][2]) - (Inv[0][2] * Inv[1][1])) * det;
      data[1][0] = ((Inv[1][2] * Inv[2][0]) - (Inv[1][0] * Inv[2][2])) * det;
      data[1][1] = ((Inv[0][0] * Inv[2][2]) - (Inv[0][2] * Inv[2][0])) * det;
      data[1][2] = ((Inv[0][2] * Inv[1][0]) - (Inv[0][0] * Inv[1][2])) * det;
      data[2][0] = ((Inv[1][0] * Inv[2][1]) - (Inv[1][1] * Inv[2][0])) * det;
      data[2][1] = ((Inv[0][1] * Inv[2][0]) - (Inv[0][0] * Inv[2][1])) * det;
      data[2][2] = ((Inv[0][0] * Inv[1][1]) - (Inv[0][1] * Inv[1][0])) * det;

      return *this;
    }

    /** \brief Identity operator.
     *
     */
    Matrix3<T>& Identity()
    {
      for (auto i: {0,1,2})
      {
        for (auto j: {0,1,2})
        {
          data[i][j] = ((i == j) ? T(1) : T(0));
        }
      }

      return *this;
    }

    /** \brief Row access.
     * \param[in] i row index.
     *
     */
    Vector3<T> row(int i)
    {
      assert((0 <= i) && (i < 3));

      return data[i];
    }

    /** \brief Column access.
     * \param[in] i column index.
     *
     */
    Vector3<T> column(const int i)
    {
      assert((0 <= i) && (i < 3));

      Vector3<T> result;
      result[0] = data[0][i];
      result[1] = data[1][i];
      result[2] = data[2][i];

      return result;
    }
  private:
    std::vector<Vector3<T> > data;
};

/** \brief operator <<
 * \param[in] stream iostream.
 * \param[in] m matrix.
 *
 */
template<class T> std::ostream& operator<<(std::ostream &stream, const Matrix3<T> &m)
{
  for (auto i: {0,1,2})
  {
    stream << "[";
    for (auto j: {0,1,2})
    {
      stream << m[i][j] << ((j == 2) ? "]" : ",");
    }

    stream << std::endl;
  }
  return stream;
}

/** \brief operator+ (binary, matrix)
 * \param[in] a matrix.
 * \param[in] b matrix.
 *
 */
template<class T> inline Matrix3<T> operator+(const Matrix3<T> &a, const Matrix3<T> &b)
{
  Matrix3<T> tmp(0);

  for (auto i: {0,1,2})
  {
    for (auto j: {0,1,2})
    {
      tmp(i, j) = a(i, j) + b(i, j);
    }
  }

  return tmp;
}

/** \brief operator+ (binary, value)
 * \param[in] m matrix.
 * \param[in] value T value.
 *
 */
template<class T> inline Matrix3<T> operator+(const Matrix3<T> &m, const T &value)
{
  Matrix3<T> tmp(0);

  for (auto i: {0,1,2})
  {
    for (auto j: {0,1,2})
    {
      tmp(i, j) = m(i, j) + value;
    }
  }

  return tmp;
}

/** \brief operator- (binary, matrix)
 * \param[in] a matrix.
 * \param[in] b matrix.
 *
 */
template<class T> inline Matrix3<T> operator-(const Matrix3<T> &a, const Matrix3<T> &b)
{
  Matrix3<T> tmp;

  for (auto i: {0,1,2})
  {
    for (auto j: {0,1,2})
    {
      tmp(i, j) = a(i, j) - b(i, j);
    }
  }

  return tmp;
}

/** \brief operator- (binary, value)
 * \param[in] m matrix.
 * \param[in] value T value.
 */
template<class T> inline Matrix3<T> operator-(const Matrix3<T> &m, const T &value)
{
  Matrix3<T> tmp;

  for (auto i: {0,1,2})
  {
    for (auto j: {0,1,2})
    {
      tmp(i, j) = m(i, j) - value;
    }
  }

  return tmp;
}

/** \brief operator/ (binary, scalar)
 * \param[in] m matrix.
 * \parma[in] value T value.
 *
 */
template<class T> inline Matrix3<T> operator/(const Matrix3<T> &m, const T &value)
{
  Matrix3<T> tmp;

  for (auto i: {0,1,2})
  {
    for (auto j: {0,1,2})
    {
      tmp(i, j) = m(i, j) / value;
    }
  }

  return tmp;
}

/** \brief operator* (binary, matrix)
 * \param[in] a matrix.
 * \param[in] b matrix.
 *
 */
template<class T> inline Matrix3<T> operator*(const Matrix3<T> &a, const Matrix3<T> &b)
{
  Matrix3<T> tmp;
  T sum;

  for (auto i: {0,1,2})
  {
    for (auto k: {0,1,2})
    {
      sum = 0;
      for (auto j: {0,1,2})
      {
        sum = sum + (a(i, j) * b(j, k));
      }

      tmp(i, k) = sum;
    }
  }

  return tmp;
}

/** \brief operator* (binary, value)
 * \param[in] m matrix.
 * \param[in] value T value.
 *
 */
template<class T> inline Matrix3<T> operator*(const Matrix3<T> &m, const T &value)
{
  Matrix3<T> tmp;

  for (auto i: {0,1,2})
  {
    tmp[i] = m[i] * value;
  }

  return tmp;
}

/** \brief operator* (binary, scalar)
 * \param[in] a matrix.
 * \param[in] b matrix.
 *
 */
template<class T> inline Matrix3<T> operator*(const T scalar, const Matrix3<T> &a)
{
  Matrix3<T> tmp;

  for (auto i: {0,1,2})
  {
    tmp[i] = a[i] * scalar;
  }

  return tmp;
}

/** \brief operator* (binary, vector)
 * \param[in] v vector.
 * \param[in] m matrix.
 *
 */
template<class T> inline Vector3<T> operator*(const Vector3<T> &v, const Matrix3<T> &m)
{
  Vector3<T> tmp;
  T sum;

  for (auto i: {0,1,2})
  {
    sum = 0;

    for (auto j: {0,1,2})
    {
      sum = sum + (m(j, i) * v[j]);
    }

    tmp[i] = sum;
  }

  return tmp;
}

/** \brief operator* (binary, vector)
 * \param[in] m matrix.
 * \param[in] v vector.
 *
 */
template<class T> inline Vector3<T> operator*(const Matrix3<T> &m, const Vector3<T> &v)
{
  Vector3<T> tmp;
  T sum;

  for (auto i: {0,1,2})
  {
    sum = 0;

    for (auto j: {0,1,2})
    {
      sum = sum + (m(j, i) * v[j]);
    }

    tmp[i] = sum;
  }

  return tmp;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
using Vector3ui = Vector3<unsigned int>;
using Vector3i = Vector3<int>;
using Vector3f = Vector3<float>;
using Vector3d = Vector3<double>;
using Vector3ul = Vector3<unsigned long int>;
using Vector3ull = Vector3<unsigned long long int>;
using Vector3ll = Vector3<long long int>;

using Matrix3ui = Matrix3<unsigned int>;
using Matrix3i = Matrix3<int>;
using Matrix3f = Matrix3<float>;
using Matrix3d = Matrix3<double>;

#endif // _VECTORSPACEALGEBRA_H_
