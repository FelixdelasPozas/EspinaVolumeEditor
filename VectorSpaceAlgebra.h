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

class algebra_error: public std::logic_error
{
    public:
        algebra_error(const std::string& arg) :
            std::logic_error(arg)
        {
        }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Vector class
//
template<class T> class Vector3
{
    public:
        // constructors & destructor
        //
        ~Vector3()
        {
        }

        Vector3()
        {
            _v.push_back(T(0));
            _v.push_back(T(0));
            _v.push_back(T(0));
        }

        Vector3(const Vector3<T> &v)
        {
            _v.push_back(v[0]);
            _v.push_back(v[1]);
            _v.push_back(v[2]);
        }

        Vector3(const T& value)
        {
            _v.push_back(value);
            _v.push_back(value);
            _v.push_back(value);
        }
        
        template<class X> Vector3(const X& x, const X& y, const X& z)
        {
            _v.push_back(static_cast<T>(x));
            _v.push_back(static_cast<T>(y));
            _v.push_back(static_cast<T>(z));
        }


        // assignments
        //
        Vector3<T>& operator=(const T scalar) throw ()
        {
            _v[0] = _v[1] = _v[2] = scalar;
            return *this;
        }

        Vector3<T>& operator=(const Vector3<T> &v) throw ()
        {
            _v[0] = v[0];
            _v[1] = v[1];
            _v[2] = v[2];
            return *this;
        }

        // logical
        //
        friend bool operator==(const Vector3<T>& a, const Vector3<T>& b) throw ()
        {
            for (unsigned long i = 0; i < 3; i++)
                if (a[i] != b[i])
                    return false;

            return true;
        }

        friend bool operator!=(const Vector3<T>& a, const Vector3<T>& b) throw ()
        {
            return !(a == b);
        }

        // subscripting
        //
        inline T& operator[](int i) throw ()
        {
            assert(0 <= i);
            assert(i < 3);
            return _v[i];
        }

        inline const T& operator[](int i) const throw ()
        {
            assert(0 <= i);
            assert(i < 3);
            return _v[i];
        }

        // unary operators
        //
        inline const Vector3<T> operator+() throw ()
        {
            Vector3<T> result = *this;
            return result;
        }

        inline const Vector3<T> operator-() throw ()
        {
            Vector3<T> result;
            result._v[0] = -_v[0];
            result._v[1] = -_v[1];
            result._v[2] = -_v[2];
            return result;
        }

        // set values
        //
        template<class X> inline Vector3<T>& Set(X x, X y, X z) throw ()
        {
            _v[0] = static_cast<T>(x);
            _v[1] = static_cast<T>(y);
            _v[2] = static_cast<T>(z);
            return *this;
        }
    private:
        std::vector<T> _v;
};

// operator<<
template<class T> std::ostream& operator<<(std::ostream &stream, const Vector3<T> &a)
{
    stream << "[";
    for (int i = 0; i < 3; i++)
        stream << a[i] << ((i == 2) ? "]" : ",");

    stream << std::endl;

    return stream;
}

// operator* (scalar*vector)
template<class T> inline Vector3<T> operator*(const T c, const Vector3<T> a) throw ()
{
    Vector3<T> tmp(0);

    for (int i = 0; i < 3; i++)
        tmp[i] = a[i] * c;

    return tmp;
}

// operator* (vector*scalar)
template<class T> inline Vector3<T> operator*(const Vector3<T> a, const T c) throw ()
{
    Vector3<T> tmp(0);

    for (int i = 0; i < 3; i++)
        tmp[i] = a[i] * c;

    return tmp;
}

// operator/ (vector/scalar)
template<class T, class X> inline Vector3<T> operator/(const Vector3<T> a, const X c) throw (algebra_error)
{
    if (c == T(0))
        throw algebra_error("Vector3<T>::operator/: division by zero");

    Vector3<T> tmp(0);

    for (int i = 0; i < 3; i++)
        tmp[i] = a[i] / T(c);

    return tmp;
}

// operator+ (binary) 
template<class T> inline Vector3<T> operator+(const Vector3<T> &a, const Vector3<T> &b) throw ()
{
    Vector3<T> ret(0);

    for (int i = 0; i < 3; i++)
        ret[i] = a[i] + b[i];

    return ret;
}

//operator- (binary)
template<class T> inline Vector3<T> operator-(const Vector3<T> &a, const Vector3<T> &b) throw ()
{
    Vector3<T> ret(0);

    for (int i = 0; i < 3; i++)
        ret[i] = a[i] - b[i];

    return ret;
}

//operator* (binary, dot product)
template<class T> inline T operator*(const Vector3<T> &a, const Vector3<T> &b) throw ()
{
    T sum = 0;

    for (int i = 0; i < 3; i++)
        sum += a[i] * b[i];

    return sum;
}

// operator+= (binary)
template<class T> inline Vector3<T> operator+=(Vector3<T> &a, const Vector3<T> &b) throw ()
{
    for (int i = 0; i < 3; i++)
        a[i] = a[i] + b[i];

    return a;
}

// operator-= (binary)
template<class T> inline Vector3<T> operator-=(Vector3<T> &a, const Vector3<T> &b) throw ()
{
    for (int i = 0; i < 3; i++)
        a[i] = a[i] - b[i];

    return a;
}

// operator*= (vector*scalar)
template<class T> inline Vector3<T> operator*=(const Vector3<T> a, const T c) throw ()
{
    for (int i = 0; i < 3; i++)
        a[i] = a[i] * c;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// Matrix class
//
template<class T> class Matrix3
{
    public:
        // constructors and destructor
        //
        Matrix3()
        {
            Vector3<T> a;
            Vector3<T> b;
            Vector3<T> c;
            _m.push_back(a);
            _m.push_back(b);
            _m.push_back(c);
        }

        Matrix3(Matrix3<T> &a)
        {
            Vector3<T> x = a[0];
            Vector3<T> y = a[1];
            Vector3<T> z = a[2];
            _m.push_back(x);
            _m.push_back(y);
            _m.push_back(z);
        }

        Matrix3(const T& value)
        {
            Vector3<T> x(value);
            Vector3<T> y(value);
            Vector3<T> z(value);
            _m.push_back(x);
            _m.push_back(y);
            _m.push_back(z);
        }

        // destructor
        //
        ~Matrix3()
        {
        }

        // assignments
        //
        const Matrix3<T>& operator=(T& scalar) const throw ()
        {
            _m[0] = scalar;
            _m[1] = scalar;
            _m[2] = scalar;
            return *this;
        }

        Matrix3<T>& operator=(const Matrix3<T> &a) throw ()
        {
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++)
                    _m[i][j] = a[i][j];

            return *this;
        }

        // logical
        //
        friend bool operator==(const Matrix3<T>& a, const Matrix3<T>& b) throw ()
        {
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++)
                    if (a[i][j] != b[i][j])
                        return false;

            return true;
        }

        friend bool operator!=(const Matrix3<T>& a, const Matrix3<T>& b) throw ()
        {
            return !(a == b);
        }

        const bool isNull() throw ()
        {
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++)
                    if (_m[i][j] != 0)
                        return false;

            return true;
        }

        // subscript
        //
        inline T& operator()(const int i, const int j) throw ()
        {
            assert(0 <= i);
            assert(i < 3);
            assert(0 <= j);
            assert(j < 3);
            return _m[i][j];
        }

        inline const T& operator()(const int i, const int j) const throw ()
        {
            assert(0 <= i);
            assert(i < 3);
            assert(0 <= j);
            assert(j < 3);
            return _m[i][j];
        }

        inline Vector3<T>& operator[](const int i) throw ()
        {
            assert(0 <= i);
            assert(i < 3);
            return _m[i];
        }

        inline const Vector3<T>& operator[](const int i) const throw ()
        {
            assert(0 <= i);
            assert(i < 3);
            return _m[i];
        }

        // unary operators
        //
        inline const Matrix3<T> operator+() throw ()
        {
            Matrix3<T> result = *this;
            return result;
        }

        inline const Matrix3<T> operator-() throw ()
        {
            Matrix3<T> result;
            result._m[0] = -_m[0];
            result._m[1] = -_m[1];
            result._m[2] = -_m[2];
            return result;
        }

        // transpose operation
        //
        Matrix3<T>& Transpose() throw ()
        {
            Matrix3<T> copy(*this);

            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++)
                    _m[j][i] = copy[i][j];

            return *this;
        }

        // determinant
        //
        float Determinant() throw ()
        {
            return ((_m[0][0] * ((_m[1][1] * _m[2][2]) - (_m[1][2] * _m[2][1]))) - 
                     (_m[0][1] * ((_m[1][0] * _m[2][2]) - (_m[1][2] * _m[2][0]))) + 
                     (_m[0][2] * ((_m[1][0] * _m[2][1]) - (_m[1][1] * _m[2][0]))));
        }

        // inverse
        //
        Matrix3<T>& Inverse() throw (algebra_error)
        {
            float det = Determinant();
            if (det == 0)
                throw algebra_error("Matrix3<T>::Inverse: determinant is zero");

            det = 1 / det;
            Matrix3<T> Inv(*this);

            _m[0][0] = ((Inv[1][1] * Inv[2][2]) - (Inv[1][2] * Inv[2][1])) * det;
            _m[0][1] = ((Inv[0][2] * Inv[2][1]) - (Inv[0][1] * Inv[2][2])) * det;
            _m[0][2] = ((Inv[0][1] * Inv[1][2]) - (Inv[0][2] * Inv[1][1])) * det;
            _m[1][0] = ((Inv[1][2] * Inv[2][0]) - (Inv[1][0] * Inv[2][2])) * det;
            _m[1][1] = ((Inv[0][0] * Inv[2][2]) - (Inv[0][2] * Inv[2][0])) * det;
            _m[1][2] = ((Inv[0][2] * Inv[1][0]) - (Inv[0][0] * Inv[1][2])) * det;
            _m[2][0] = ((Inv[1][0] * Inv[2][1]) - (Inv[1][1] * Inv[2][0])) * det;
            _m[2][1] = ((Inv[0][1] * Inv[2][0]) - (Inv[0][0] * Inv[2][1])) * det;
            _m[2][2] = ((Inv[0][0] * Inv[1][1]) - (Inv[0][1] * Inv[1][0])) * det;

            return *this;
        }

        // identity
        //
        Matrix3<T>& Identity() throw ()
        {
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++)
                    _m[i][j] = ((i == j) ? T(1) : T(0));

            return *this;
        }

        // row & column access
        //
        Vector3<T> GetRow(int i) throw ()
        {
            assert(0 <= i);
            assert(i < 3);
            return _m[i];
        }

        Vector3<T> GetColumn(const int i) const throw ()
        {
            assert(0 <= i);
            assert(i < 3);
            Vector3<T> result;
            result[0] = _m[0][i];
            result[1] = _m[1][i];
            result[2] = _m[2][i];

            return result;
        }
    private:
        std::vector<Vector3<T> > _m;
};

// operator <<
template<class T> std::ostream& operator<<(std::ostream &stream, const Matrix3<T> &a)
{
    for (int i = 0; i < 3; i++)
    {
        stream << "[";
        for (int j = 0; j < 3; j++)
            stream << a[i][j] << ((j == 2) ? "]" : ",");

        stream << std::endl;
    }
    return stream;
}

// operator+ (binary, matrix)
template<class T> inline Matrix3<T> operator+(const Matrix3<T> &a, const Matrix3<T> &b) throw ()
{
    Matrix3<T> tmp(0);

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            tmp(i, j) = a(i, j) + b(i, j);

    return tmp;
}

// operator+ (binary, scalar)
template<class T> inline Matrix3<T> operator+(const Matrix3<T> &a, const T &b) throw ()
{
    Matrix3<T> tmp(0);

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            tmp(i, j) = a(i, j) + b;

    return tmp;
}

// operator- (binary, matrix)
template<class T> inline Matrix3<T> operator-(const Matrix3<T> &a, const Matrix3<T> &b) throw ()
{
    Matrix3<T> tmp;

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            tmp(i, j) = a(i, j) - b(i, j);

    return tmp;
}

// operator- (binary, scalar)
template<class T> inline Matrix3<T> operator-(const Matrix3<T> &a, const T &b) throw ()
{
    Matrix3<T> tmp;

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            tmp(i, j) = a(i, j) - b;

    return tmp;
}

// operator/ (binary, scalar)
template<class T> inline Matrix3<T> operator/(const Matrix3<T> &a, const T &b) throw ()
{
    Matrix3<T> tmp;

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            tmp(i, j) = a(i, j) / b;

    return tmp;
}

// operator* (binary, matrix)
template<class T> inline Matrix3<T> operator*(const Matrix3<T> &a, const Matrix3<T> &b) throw ()
{
    Matrix3<T> tmp;
    T sum;

    for (int i = 0; i < 3; i++)
        for (int k = 0; k < 3; k++)
        {
            sum = 0;
            for (int j = 0; j < 3; j++)
                sum = sum + (a(i,j) * b(j,k));

            tmp(i,k) = sum;
        }

    return tmp;
}

// operator* (binary, scalar)
template<class T> inline Matrix3<T> operator*(const Matrix3<T> &a, const T scalar) throw ()
{
    Matrix3<T> tmp;

    for (int i = 0; i < 3; i++)
        tmp[i] = a[i] * scalar;

    return tmp;
}

// operator* (binary, scalar)
template<class T> inline Matrix3<T> operator*(const T scalar, const Matrix3<T> &a) throw ()
{
    Matrix3<T> tmp;

    for (int i = 0; i < 3; i++)
        tmp[i] = a[i] * scalar;

    return tmp;
}

// operator* (binary, vector)
template<class T> inline Vector3<T> operator*(const Vector3<T> &v, const Matrix3<T> &a) throw ()
{
    Vector3<T> tmp;
    T sum;

    for (int i = 0; i < 3; i++)
    {
        sum = 0;

        for (int j = 0; j < 3; j++)
            sum = sum + (a(j, i) * v[j]);

        tmp[i] = sum;
    }

    return tmp;
}

// operator* (binary, vector)
template<class T> inline Vector3<T> operator*(const Matrix3<T> &a, const Vector3<T> &v) throw ()
{
    Vector3<T> tmp;
    T sum;

    for (int i = 0; i < 3; i++)
    {
        sum = 0;

        for (int j = 0; j < 3; j++)
            sum = sum + (a(j, i) * v[j]);

        tmp[i] = sum;
    }

    return tmp;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Typedefs
//
typedef Vector3<unsigned int> Vector3ui;
typedef Vector3<int> Vector3i;
typedef Vector3<float> Vector3f;
typedef Vector3<double> Vector3d;
typedef Vector3<unsigned long int> Vector3ul;
typedef Vector3<unsigned long long int> Vector3ull;
typedef Vector3<long long int> Vector3ll;

typedef Matrix3<unsigned int> Matrix3ui;
typedef Matrix3<int> Matrix3i;
typedef Matrix3<float> Matrix3f;
typedef Matrix3<double> Matrix3d;

#endif // _VECTORSPACEALGEBRA_H_
