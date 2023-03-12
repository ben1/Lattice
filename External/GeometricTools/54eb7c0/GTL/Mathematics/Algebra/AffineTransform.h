// Geometric Tools Library
// https://www.geometrictools.com
// Copyright (c) 2022 David Eberly
// Distributed under the Boost Software License, Version 1.0
// https://www.boost.org/LICENSE_1_0.txt
// File Version: 2022.05.16

#pragma once

// An affine transform is Y = M * X + T, where M is a 3x3 matrix and T is a
// 3x1 translation. In some cases, M = R, a rotation matrix, or M = R * S,
// where R is a rotation matrix and S is a diagonal matrix whose diagonal
// entries are nonzeo scales. Generally, M is typicaly invertible and not a
// rotation matrix or a rotation-scale matrix. The inverse of the affine
// transformation is X = M^{-1} * (Y - T). The affine transformation can be
// represented by a 4x4 homogeneous matrix H = {{M,T},{Z,1}}, where M is 3x3,
// T is 3x1, Z is the 1x3 zero vector and 1 is a scalar stored as a 1x1 block.
// The affine transformation is {Y,1} = H * {X,1} and the inverse affine
// transformation is {X,1} = H^{-1} * {Y,1}.
//
// In an application that has a hierarchy of affine transformations, such as
// scene graphs (tree of nodes), updating the transformation state amounts to
// multiplying transformations along each path from the root node to the child
// nodes. Let a path have nodes N[0] through N[k] with node N[j] managing an
// affine transformation,
//   H[j] = {{R[j] * u[j], T[j]}, {Z, 1}} = <R[j], u[j], T[j]>
// with R[j] a rotation, u[j] uniform scale (S[j] = u[j] * I) and T[j] a
// translation. The product of two such transformations is
//   H[j0] * H[j1] = <R[j0], u[j0], T[j0]> * <R[j1], u[j1], T[j1]>
//   = <R[j0] * R[j1], u[j0] * u[j1], R[j0] * u[j0] * T[j1] + T[j0]>
// which is of the same format. In this special case, it is simple to manage
// the affine transformations by manipulating their rotation, uniform scale
// and translation channels. In the event nonuniform scaling is required, the
// hierarchy has interior nodes with uniform scales but the leaf nodes have
// nonuniform scales. Let N[j] have rotation R[j], uniform scale u[j] and
// translation T[j] for 0 <= j < k. Let N[k] have rotation R[k], nonuniform
// scale S[k] and translation T[k]. The product of the affine transformations
// along the path are
//   H' = H[0] * H[1] * ... * H[k-1] * H[k]
//      = <R[0],u[0],T[0]> * <R[1],u[1],T[1]> * ... *<R[k-1],u[k-1],T[k-1]>
//        * <R[k], S[k], T[k]>
//      = <R',u',T'> * <R[k],S[k],T[k]>
//      = <R' * R[k], u' * S[k], R' * u' * T[k] + T'>

#include <GTL/Mathematics/Algebra/RigidMotion.h>
#include <algorithm>
#include <cmath>

namespace gtl
{
    template <typename T>
    class AffineTransform
    {
    public:
        using value_type = T;

        // The default constructor produces the identity transformation. The
        // default copy constructor and assignment operator are generated by
        // the compiler.
        AffineTransform()
            :
            mHMatrix{},
            mInvHMatrix{},
            mMatrix{},
            mTranslate{},
            mScale{},
            mIsIdentity(true),
            mIsRSMatrix(true),
            mIsUniformScale(true),
            mInverseNeedsUpdate(false)
        {
            gtl::MakeIdentity(mHMatrix);
            gtl::MakeIdentity(mInvHMatrix);
            gtl::MakeIdentity(mMatrix);
            MakeZero(mTranslate);
            MakeOne(mScale);
        }

        // Implicit conversion to access H = {{M,T},{Z,1}}.
        inline operator Matrix4x4<T> const& () const
        {
            return GetH();
        }

        // Set the transformation to the identity matrix.
        void MakeIdentity()
        {
            gtl::MakeIdentity(mHMatrix);
            gtl::MakeIdentity(mInvHMatrix);
            gtl::MakeIdentity(mMatrix);
            MakeZero(mTranslate);
            MakeOne(mScale);
            mIsIdentity = true;
            mIsRSMatrix = true;
            mIsUniformScale = true;
            mInverseNeedsUpdate = false;
        }

        // Set the transformation to have scales of 1.
        void MakeUnitScale()
        {
            GTL_RUNTIME_ASSERT(
                mIsRSMatrix,
                "AffineTransform is not rotation-scale.");

            MakeOne(mScale);
            mIsUniformScale = true;
            UpdateHMatrix();
        }

        // Hints about the structure of the transformation.

        // M = I
        inline bool IsIdentity() const
        {
            return mIsIdentity;
        }

        // M = R * S
        inline bool IsRSMatrix() const
        {
            return mIsRSMatrix;
        }

        // RS-matrix with S = c * I, I is the identity matrix.
        inline bool IsUniformScale() const
        {
            return mIsRSMatrix && mIsUniformScale;
        }

        // RS-matrix with S not a multiple of the identity matrix.
        inline bool IsNonuniformScale() const
        {
            return mIsRSMatrix && !mIsUniformScale;
        }

        // General (invertible) matrix M.
        inline bool IsGeneralMatrix() const
        {
            return !mIsRSMatrix;
        }

        // Member access. The three channels are M, S and T and are set
        // independently. M is either a general invertible matrix or a
        // rotation matrix. S is a diagonal matrix with nonzero diagonal
        // elements. if the diagonal elements are the same, S represents
        // a uniform scale. T is a translation.
        // 
        // The Set* functions set the is-identity hint to false. For each
        // Set* function, the appropriate channel is assigned and the H matrix
        // is computed immediately. However, the computation of the H^{-1}
        // matrix is deferred until GetInverseH is called. The idea is that
        // there is no reason to compute the inverse until it is needed (if
        // ever) by the application.
        // 
        // The SetMatrix function sets the is-rsmatrix and is-uniform-scale
        // hints to false. By calling this function, the caller acknowledges
        // that M stores a general invertible matrix and that the scale
        // channel is to be ignored. Internally, the scales are set to 1. A
        // subsequent call to SetUniformScale or SetScale throws an exception.
        // SetTranslation(...) is allowed. SetRotation(...) is also allowed,
        // restoring the ability to set the scales. The GetMatrix function
        // returns the M-channel, whether or not it is a general matrix or a
        // a rotation matrix.
        //    
        // The SetRotation functions set the is-rsmatrix hint to true. If the
        // hint is false, GetRotation throws an exception. You can set the
        // rotation using 3x3 rotation matrices, quaternions, axis-angle pairs
        // or Euler angles.
        // 
        // The SetScale function sets the scales and the is-uniform-scale hint
        // is set to false. This function is allowed as long as M is a
        // rotation matrix. However, it will throw an exception if M is a
        // general matrix. GetScale returns the scales regardless of the
        // is-uniform-scale hint. If the hint is false, the uniform scale is
        // returned for all 3 scales.
        // 
        // The SetUniformScale function sets all scales to a common value and
        // the is-uniform-scale hint is set to true. This function is allowed
        // as long as M is a rotation matrix. However, it will throw an
        // exception if M is a general matrix. GetUniformScale retrieves the
        // common scale as long as the is-uniform-scale hint is true but
        // throws an exception when the hint is false.
        //
        // SetTranslation and GetTranslation have no restrictions.

        // Set the M-channel to be a general (invertible) matrix.
        void SetMatrix(Matrix3x3<T> const& matrix)
        {
            mMatrix = matrix;
            mIsIdentity = false;
            mIsRSMatrix = false;
            mIsUniformScale = false;
            UpdateHMatrix();
        }

        // Get the M-channel whether it is a general matrix or rotation
        // matrix.
        inline Matrix3x3<T> const& GetMatrix() const
        {
            return mMatrix;
        }

        // Set the M-channel to be a rotation matrix. No validation is
        // performed to verify 'rotate' is in fact a rotation matrix.
        void SetRotation(Matrix3x3<T> const& rotate)
        {
            mMatrix = rotate;
            mIsIdentity = false;
            mIsRSMatrix = true;
            UpdateHMatrix();
        }

        // Get the M-channel as long as it is a rotation matrix.
        Matrix3x3<T> const& GetRotation() const
        {
            GTL_RUNTIME_ASSERT(
                mIsRSMatrix,
                "AffineTransform is not rotation-scale.");

            return mMatrix;
        }

        // Set the M-channel to be a rotation matrix corresponding to the
        // unit-length quaternion q. No validation is performed to verify
        // that q is unit length.
        void SetRotation(Quaternion<T> const& q)
        {
            RigidMotion<T>::Convert(q, mMatrix);
            mIsIdentity = false;
            mIsRSMatrix = true;
            UpdateHMatrix();
        }

        // Get the M-channel, a rotation matrix represented by the unit-length
        // quaternion q.
        void GetRotation(Quaternion<T>& q) const
        {
            GTL_RUNTIME_ASSERT(
                mIsRSMatrix,
                "AffineTransform is not rotation-scale.");

            RigidMotion<T>::Convert(mMatrix, q);
        }

        // Set the M-channel to be a rotation matrix corresponding to the
        // axis-angle pair where the axis is unit length and the angle is
        // in radians. No validation is performed to verify that the axis
        // is unit length.
        void SetRotation(AxisAngle<T> const& axisAngle)
        {
            RigidMotion<T>::Convert(axisAngle, mMatrix);
            mIsIdentity = false;
            mIsRSMatrix = true;
            UpdateHMatrix();
        }

        // Get the M-channel, a rotation matrix represented by the axis-angle
        // pair axisAngle. The angles are in radians.
        void GetRotation(AxisAngle<T>& axisAngle) const
        {
            GTL_RUNTIME_ASSERT(
                mIsRSMatrix,
                "AffineTransform is not rotation-scale.");

            RigidMotion<T>::Convert(mMatrix, axisAngle);
        }

        // Set the M-channel to be a rotation matrix corresponding to the
        // Euler angles which are in radians. The rotation order must be
        // chosen by setting eulerAngles.axis[] before the call.
        void SetRotation(EulerAngles<T> const& eulerAngles)
        {
            RigidMotion<T>::Convert(eulerAngles, mMatrix);
            mIsIdentity = false;
            mIsRSMatrix = true;
            UpdateHMatrix();
        }

        // Get the M-channel, a rotation matrix represented by the Euler
        // angles eulerAngles. The angles are in radians. The rotation order
        // must be chosen by setting eulerAngles.axis[] before the call.
        void GetRotation(EulerAngles<T>& eulerAngles) const
        {
            GTL_RUNTIME_ASSERT(
                mIsRSMatrix,
                "AffineTransform is not rotation-scale.");

            RigidMotion<T>::Convert(mMatrix, eulerAngles);
        }

        // Set the T-channel, a 3-tuple translation.
        void SetTranslation(T const& x0, T const& x1, T const& x2)
        {
            mTranslate = { x0, x1, x2 };
            mIsIdentity = false;
            UpdateHMatrix();
        }

        inline void SetTranslation(Vector3<T> const& translate)
        {
            SetTranslation(translate[0], translate[1], translate[2]);
        }

        // Get the T-channel, a 3-tuple translation.
        inline Vector3<T> GetTranslation() const
        {
            return Vector3<T>{mTranslate[0], mTranslate[1], mTranslate[2] };
        }

        // Set the S-channel, a 3-tuple of positive scales.
        void SetScale(T const& s0, T const& s1, T const& s2)
        {
            GTL_RUNTIME_ASSERT(
                mIsRSMatrix,
                "AffineTransform is not rotation-scale.");

            GTL_RUNTIME_ASSERT(
                s0 != C_<T>(0) && s1 != C_<T>(0) && s2 != C_<T>(0),
                "Scales must be nonzero.");

            mScale = { s0, s1, s2 };
            mIsIdentity = false;
            mIsUniformScale = false;
            UpdateHMatrix();
        }

        inline void SetScale(Vector3<T> const& scale)
        {
            SetScale(scale[0], scale[1], scale[2]);
        }

        Vector3<T> GetScale() const
        {
            GTL_RUNTIME_ASSERT(
                mIsRSMatrix,
                "AffineTransform is not rotation-scale.");

            return Vector3<T>{ mScale[0], mScale[1], mScale[2] };
        }

        void SetUniformScale(T const& scale)
        {
            GTL_RUNTIME_ASSERT(
                mIsRSMatrix,
                "AffineTransform is not rotation-scale.");

            GTL_RUNTIME_ASSERT(
                scale != C_<T>(0),
                "Scale must be nonzero.");

            mScale = { scale, scale, scale };
            mIsIdentity = false;
            mIsUniformScale = true;
            UpdateHMatrix();
        }

        T const& GetUniformScale() const
        {
            GTL_RUNTIME_ASSERT(
                mIsRSMatrix,
                "AffineTransform is not rotation-scale.");

            GTL_RUNTIME_ASSERT(
                mIsUniformScale,
                "AffineTransform is not uniform scale.");

            return mScale[0];
        }

        // For M = R*S, the largest absolute value of S is returned. For
        // general M, the max-row-sum norm is returned, which is a reasonable
        // measure of maximum scale of the transformation.
        T GetNorm() const
        {
            T sum0, sum1, sum2;

            if (mIsRSMatrix)
            {
                // M = R * S
                sum0 = std::fabs(mScale[0]);
                sum1 = std::fabs(mScale[1]);
                sum2 = std::fabs(mScale[2]);
            }
            else
            {
                // Use the max-row-sum matrix norm. The spectral norm, which
                // is the maximum absolute value of the eigenvalues, is
                // smaller or equal to this norm. Therefore, this function
                // returns an approximation to the maximum scale.
                sum0 = std::fabs(mMatrix(0, 0)) + std::fabs(mMatrix(0, 1)) + std::fabs(mMatrix(0, 2));
                sum1 = std::fabs(mMatrix(1, 0)) + std::fabs(mMatrix(1, 1)) + std::fabs(mMatrix(1, 2));
                sum2 = std::fabs(mMatrix(2, 0)) + std::fabs(mMatrix(2, 1)) + std::fabs(mMatrix(2, 2));
            }

            return std::max(std::max(sum0, sum1), sum2);
        }


        // Get the homogeneous matrix H which is the composite of the M-, S-
        // and T-channels.
        inline Matrix4x4<T> const& GetH() const
        {
            return mHMatrix;
        }

        // Get the inverse homogeneous matrix H^{-1}, recomputing it when
        // necessary.
        Matrix4x4<T> const& GetInverseH() const
        {
            if (mInverseNeedsUpdate)
            {
                if (mIsIdentity)
                {
                    gtl::MakeIdentity(mInvHMatrix);
                }
                else
                {
                    if (mIsRSMatrix)
                    {
                        if (mIsUniformScale)
                        {
                            T invScale = C_<T>(1) / mScale[0];
                            mInvHMatrix(0, 0) = invScale * mMatrix(0, 0);
                            mInvHMatrix(0, 1) = invScale * mMatrix(1, 0);
                            mInvHMatrix(0, 2) = invScale * mMatrix(2, 0);
                            mInvHMatrix(1, 0) = invScale * mMatrix(0, 1);
                            mInvHMatrix(1, 1) = invScale * mMatrix(1, 1);
                            mInvHMatrix(1, 2) = invScale * mMatrix(2, 1);
                            mInvHMatrix(2, 0) = invScale * mMatrix(0, 2);
                            mInvHMatrix(2, 1) = invScale * mMatrix(1, 2);
                            mInvHMatrix(2, 2) = invScale * mMatrix(2, 2);
                        }
                        else
                        {
                            // Replace 3 reciprocals by 6 multiplies and
                            // 1 reciprocal.
                            T s01 = mScale[0] * mScale[1];
                            T s02 = mScale[0] * mScale[2];
                            T s12 = mScale[1] * mScale[2];
                            T invs012 = C_<T>(1) / (s01 * mScale[2]);
                            T invS0 = s12 * invs012;
                            T invS1 = s02 * invs012;
                            T invS2 = s01 * invs012;
                            mInvHMatrix(0, 0) = invS0 * mMatrix(0, 0);
                            mInvHMatrix(0, 1) = invS0 * mMatrix(1, 0);
                            mInvHMatrix(0, 2) = invS0 * mMatrix(2, 0);
                            mInvHMatrix(1, 0) = invS1 * mMatrix(0, 1);
                            mInvHMatrix(1, 1) = invS1 * mMatrix(1, 1);
                            mInvHMatrix(1, 2) = invS1 * mMatrix(2, 1);
                            mInvHMatrix(2, 0) = invS2 * mMatrix(0, 2);
                            mInvHMatrix(2, 1) = invS2 * mMatrix(1, 2);
                            mInvHMatrix(2, 2) = invS2 * mMatrix(2, 2);
                        }
                    }
                    else
                    {
                        mInvHMatrix = GetInverse(mHMatrix);
                    }

                    mInvHMatrix(0, 3) = -(
                        mInvHMatrix(0, 0) * mTranslate[0] +
                        mInvHMatrix(0, 1) * mTranslate[1] +
                        mInvHMatrix(0, 2) * mTranslate[2]);

                    mInvHMatrix(1, 3) = -(
                        mInvHMatrix(1, 0) * mTranslate[0] +
                        mInvHMatrix(1, 1) * mTranslate[1] +
                        mInvHMatrix(1, 2) * mTranslate[2]);

                    mInvHMatrix(2, 3) = -(
                        mInvHMatrix(2, 0) * mTranslate[0] +
                        mInvHMatrix(2, 1) * mTranslate[1] +
                        mInvHMatrix(2, 2) * mTranslate[2]);

                    // The last row of mHMatrix is always (0,0,0,1) for an
                    // affine transformation, so it is set once in the
                    // constructor. It is not necessary to reset it here.
                }

                mInverseNeedsUpdate = false;
            }

            return mInvHMatrix;
        }

        // Invert the transform. If possible, the channels are properly
        // assigned. For example, if the input has mIsRSMatrix equal to
        // 'true', then the inverse also has mIsRSMatrix equal to 'true'
        // and the inverse's mMatrix is a rotation matrix and mScale is
        // set accordingly.
        AffineTransform Inverse() const
        {
            AffineTransform inverse{}; // = the identity

            if (!mIsIdentity)
            {
                if (mIsRSMatrix && mIsUniformScale)
                {
                    Matrix3x3<T> invRotate = Transpose(GetRotation());
                    T invScale = C_<T>(1) / GetUniformScale();
                    Vector3<T> invTranslate = -invScale * (invRotate * GetTranslation());
                    inverse.SetRotation(invRotate);
                    inverse.SetUniformScale(invScale);
                    inverse.SetTranslation(invTranslate);
                }
                else
                {
                    Matrix3x3<T> invMatrix = GetInverse(HProject(GetH()));
                    Vector3<T> invTranslate = -invMatrix * GetTranslation();
                    inverse.SetMatrix(invMatrix);
                    inverse.SetTranslation(invTranslate);
                }
            }

            return inverse;
        }

        // The identity transformation.
        static AffineTransform Identity()
        {
            return AffineTransform{};
        }

    private:
        // Fill in the entries of mHMatrix whenever one of the components
        // mMatrix, mTranslate or mScale changes.
        void UpdateHMatrix()
        {
            if (mIsIdentity)
            {
                gtl::MakeIdentity(mHMatrix);
            }
            else
            {
                if (mIsRSMatrix)
                {
                    mHMatrix(0, 0) = mMatrix(0, 0) * mScale[0];
                    mHMatrix(0, 1) = mMatrix(0, 1) * mScale[1];
                    mHMatrix(0, 2) = mMatrix(0, 2) * mScale[2];
                    mHMatrix(1, 0) = mMatrix(1, 0) * mScale[0];
                    mHMatrix(1, 1) = mMatrix(1, 1) * mScale[1];
                    mHMatrix(1, 2) = mMatrix(1, 2) * mScale[2];
                    mHMatrix(2, 0) = mMatrix(2, 0) * mScale[0];
                    mHMatrix(2, 1) = mMatrix(2, 1) * mScale[1];
                    mHMatrix(2, 2) = mMatrix(2, 2) * mScale[2];
                }
                else
                {
                    mHMatrix(0, 0) = mMatrix(0, 0);
                    mHMatrix(0, 1) = mMatrix(0, 1);
                    mHMatrix(0, 2) = mMatrix(0, 2);
                    mHMatrix(1, 0) = mMatrix(1, 0);
                    mHMatrix(1, 1) = mMatrix(1, 1);
                    mHMatrix(1, 2) = mMatrix(1, 2);
                    mHMatrix(2, 0) = mMatrix(2, 0);
                    mHMatrix(2, 1) = mMatrix(2, 1);
                    mHMatrix(2, 2) = mMatrix(2, 2);
                }

                mHMatrix(0, 3) = mTranslate[0];
                mHMatrix(1, 3) = mTranslate[1];
                mHMatrix(2, 3) = mTranslate[2];

                // The last row of mHMatrix is always (0,0,0,1) for an affine
                // transformation, so it is set once in the constructor. It
                // is not necessary to reset it here.
            }

            mInverseNeedsUpdate = true;
        }

        // The full 4x4 homogeneous matrix H and its inverse H^{-1}, stored
        // according to the conventions (see GetInverseH description). The
        // inverse is computed only on demand.
        Matrix4x4<T> mHMatrix;
        mutable Matrix4x4<T> mInvHMatrix;

        Matrix3x3<T> mMatrix;   // M (general) or R (rotation)
        Vector3<T> mTranslate;  // T
        Vector3<T> mScale;      // S
        bool mIsIdentity, mIsRSMatrix, mIsUniformScale;
        mutable bool mInverseNeedsUpdate;

        friend class UnitTestAffineTransform;
    };

    // Compute M * V.
    template <typename T>
    Vector4<T> operator*(AffineTransform<T> const& M, Vector4<T> const& V)
    {
        return M.GetH() * V;
    }

    // Compute V^T * M.
    template <typename T>
    Vector4<T> operator*(Vector4<T> const& V, AffineTransform<T> const& M)
    {
        return V * M.GetHMatrix();
    }

    // Compute A * B.
    template <typename T>
    AffineTransform<T> operator*(AffineTransform<T> const& A, AffineTransform<T> const& B)
    {
        if (A.IsIdentity())
        {
            return B;
        }

        if (B.IsIdentity())
        {
            return A;
        }

        AffineTransform<T> product;

        if (A.IsRSMatrix() && B.IsRSMatrix())
        {
            if (A.IsUniformScale())
            {
                product.SetRotation(A.GetRotation() * B.GetRotation());

                product.SetTranslation(A.GetUniformScale() * (
                    A.GetRotation() * B.GetTranslation()) +
                    A.GetTranslation());

                if (B.IsUniformScale())
                {
                    product.SetUniformScale(A.GetUniformScale() * B.GetUniformScale());
                }
                else
                {
                    product.SetScale(A.GetUniformScale() * B.GetScale());
                }

                return product;
            }
        }

        // In all remaining cases, the matrix cannot be written as R*S*X+T.
        Matrix3x3<T> matMA;
        if (A.IsRSMatrix())
        {
            auto const& s = A.GetScale();
            matMA = MultiplyMD(A.GetRotation(), { s[0], s[1], s[2] });
        }
        else
        {
            matMA = A.GetMatrix();
        }

        Matrix3x3<T> matMB;
        if (B.IsRSMatrix())
        {
            auto const& s = B.GetScale();
            matMB = MultiplyMD(B.GetRotation(), { s[0], s[1], s[2] });
        }
        else
        {
            matMB = B.GetMatrix();
        }

        product.SetMatrix(matMA * matMB);
        product.SetTranslation(matMA * B.GetTranslation() + A.GetTranslation());
        return product;
    }

    template <typename T>
    inline Matrix4x4<T> operator*(Matrix4x4<T> const& A, AffineTransform<T> const& B)
    {
        return A * B.GetH();
    }

    template <typename T>
    inline Matrix4x4<T> operator*(AffineTransform<T> const& A, Matrix4x4<T> const& B)
    {
        return A.GetH()* B;
    }
}
