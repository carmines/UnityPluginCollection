// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Runtime.InteropServices;
using UnityEngine;

namespace SpatialTranformHelper
{
    [Serializable]
    [StructLayout(LayoutKind.Sequential)]
    public struct Matrix4x4 : IEquatable<Matrix4x4>
    {
        public float M00;
        public float M01;
        public float M02;
        public float M03;
        public float M10;
        public float M11;
        public float M12;
        public float M13;
        public float M20;
        public float M21;
        public float M22;
        public float M23;
        public float M30;
        public float M31;
        public float M32;
        public float M33;

        public static readonly Matrix4x4 Zero = default(Matrix4x4);

        public static Matrix4x4 BaseProjectionMatrix = new Matrix4x4()
        {
            M00 = 2.0f, M01 = 0.0f, M02 = 0.0f, M03 = 0.0f,
            M10 = 0.0f, M11 =-2.0f, M12 = 0.0f, M13 = 0.0f,
            M20 =-1.0f, M21 = 1.0f, M22 = 1.0f, M23 = 1.0f,
            M30 = 0.0f, M31 = 0.0f, M32 = 0.0f, M33 = 0.0f
        };

        public UnityEngine.Matrix4x4 ToUnity()
        {
            return this.ToUnityMatrix();
        }

        public UnityEngine.Matrix4x4 ToUnityTransform()
        {
            var zflip = UnityEngine.Matrix4x4.identity;
            zflip.SetColumn(2, -1 * zflip.GetColumn(2));

            return zflip * this.ToUnityMatrix() * zflip;
        }

        public bool Equals(Matrix4x4 other)
        {
            return this.M00 == other.M00
                && this.M01 == other.M01
                && this.M02 == other.M02
                && this.M03 == other.M03
                && this.M10 == other.M10
                && this.M11 == other.M11
                && this.M12 == other.M12
                && this.M13 == other.M13
                && this.M20 == other.M20
                && this.M21 == other.M21
                && this.M22 == other.M22
                && this.M23 == other.M23;
        }

        public override bool Equals(object obj)
        {
            if (obj is Matrix4x4)
            {
                return this.Equals((Matrix4x4)obj);
            }

            return false;
        }

        public override int GetHashCode()
        {
            return base.GetHashCode();
        }

        public static bool operator ==(Matrix4x4 a, Matrix4x4 b)
        {
            return a.Equals(b);
        }

        public static bool operator !=(Matrix4x4 a, Matrix4x4 b)
        {
            return !a.Equals(b);
        }

        public override string ToString()
        {
            return string.Format("{0} {1} {2} {3}\n{4} {5} {6} {7}\n{8} {9} {10} {11}\n{12} {13} {14} {15}\n"
                , this.M00, this.M01, this.M02, this.M03, this.M10, this.M11, this.M12, this.M13, this.M20, this.M21, this.M22, this.M23, this.M30, this.M31, this.M32, this.M33);
        }
    }

    public static class Functions
    {
        public static Matrix4x4 FromUnity(this UnityEngine.Matrix4x4 unityMatrix)
        {
            return new Matrix4x4
            {
                M00 = unityMatrix.m00,
                M10 = unityMatrix.m01,
                M20 = unityMatrix.m02,
                M30 = unityMatrix.m03,

                M01 = unityMatrix.m10,
                M11 = unityMatrix.m11,
                M21 = unityMatrix.m12,
                M31 = unityMatrix.m13,

                M02 = unityMatrix.m20,
                M12 = unityMatrix.m21,
                M22 = unityMatrix.m22,
                M32 = unityMatrix.m23,

                M03 = unityMatrix.m30,
                M13 = unityMatrix.m31,
                M23 = unityMatrix.m32,
                M33 = unityMatrix.m33,
            };
        }

        /// <summary>
        /// Converts the spaitial matrix to a Unity matrix
        /// </summary>
        public static UnityEngine.Matrix4x4 ToUnityMatrix(this Matrix4x4 rawMatrix)
        {
            return new UnityEngine.Matrix4x4
            {   // to column major
                m00 = rawMatrix.M00,
                m01 = rawMatrix.M10,
                m02 = rawMatrix.M20,
                m03 = rawMatrix.M30,

                m10 = rawMatrix.M01,
                m11 = rawMatrix.M11,
                m12 = rawMatrix.M21,
                m13 = rawMatrix.M31,

                m20 = rawMatrix.M02,
                m21 = rawMatrix.M12,
                m22 = rawMatrix.M22,
                m23 = rawMatrix.M32,

                m30 = rawMatrix.M03,
                m31 = rawMatrix.M13,
                m32 = rawMatrix.M23,
                m33 = rawMatrix.M33,
            };
        }

        /// <summary>
        /// Converts the input matrix to Unity transform matrix.
        /// </summary>
        public static UnityEngine.Matrix4x4 ToUnityTransform(this Matrix4x4 rawMatrix)
        {
            var zflip = UnityEngine.Matrix4x4.identity;
            zflip.SetColumn(2, -1 * zflip.GetColumn(2));

            return zflip * rawMatrix.ToUnityMatrix() * zflip;
        }

        /// <summary>
        /// Converts the input matrix to Unity transform matrix but can return null
        /// </summary>
        public static UnityEngine.Matrix4x4? ConvertToUnityMatrix(this Matrix4x4 rawMatrix)
        {
            if (rawMatrix == Matrix4x4.Zero)
            {
                return null;
            }

            return rawMatrix.ToUnityMatrix();
        }

    }
}
