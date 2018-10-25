using System;
using UnityEngine;

using Spatial4x4 = SpatialTranformHelper.Matrix4x4;

public class SpatialCameraTracker : MonoBehaviour
{
    public Vector3 ImageCenter { get; private set; }
    public Vector3[] ImageCorners { get; private set; }

    private Spatial4x4 cameraTransform = Spatial4x4.Zero;
    private Spatial4x4 lastGoodCameraTransform = Spatial4x4.Zero;

    private Spatial4x4 cameraProjection = Spatial4x4.Zero;
    private Spatial4x4 lastGoodCameraProjection = Spatial4x4.Zero;

    private readonly Vector2[] viewport = new Vector2[] { new Vector2(0.0f, 0.0f),  new Vector2(0.0f, 1.0f), new Vector2(1.0f, 1.0f), new Vector2(1.0f, 0.0f) };

    private void Awake()
    {
        if (transform.parent != null)
        {
            Debug.LogError("This gameObject should be on the root of the scene.");

            return;
        }
    }

    private void OnEnable()
    {
        ImageCorners = null;
    }

    // store the matrix values, any updates will happen on the update loop
    public void UpdateCameraMatrices(Spatial4x4 transform, Spatial4x4 projection)
    {
        // store matrix information from the sample
        if (cameraTransform != Spatial4x4.Zero)
        {
            // always track the last known good transform
            lastGoodCameraTransform = cameraTransform;
        }
        cameraTransform = transform;

        if (cameraProjection != Spatial4x4.Zero)
        {
            lastGoodCameraProjection = cameraProjection;
        }
        cameraProjection = projection;

        // get last known good transform
        Matrix4x4? transformMatrix = null;
        if (cameraTransform != Spatial4x4.Zero)
        {
            transformMatrix = cameraTransform.ToUnityTransform();
        }
        else if (lastGoodCameraTransform != Spatial4x4.Zero)
        {
            transformMatrix = lastGoodCameraTransform.ToUnityTransform();
        }

        // swap the projection for the one sent to us
        UnityEngine.Matrix4x4? projectionMatrix = null;
        if (cameraProjection != Spatial4x4.Zero)
        {
            projectionMatrix = cameraProjection.ToUnity();
        }
        else if (lastGoodCameraProjection != Spatial4x4.Zero)
        {
            projectionMatrix = cameraProjection.ToUnity();
        }

        if (transformMatrix == null || projectionMatrix == null)
        {
             return;
        }

        // set the real worl position to PV camera pose
        if (transformMatrix.Value.ValidTRS())
        {
            gameObject.transform.position = transformMatrix.Value.GetColumn(3);
            gameObject.transform.rotation = Quaternion.LookRotation(transformMatrix.Value.GetColumn(2), transformMatrix.Value.GetColumn(1));
        }

        if (ImageCorners == null)
        {
            ImageCorners = new Vector3[4];
        }

        ImageCenter = WorldPoint(new Vector2(.5f, .5f), transformMatrix.Value, projectionMatrix.Value);

        for (int i = 0; i < viewport.Length; i++)
        {
            ImageCorners[i] = WorldPoint(viewport[i], transformMatrix.Value, projectionMatrix.Value);
        }
    }

    // https://docs.microsoft.com/en-us/windows/mixed-reality/locatable-camera 
    // Video        Preview     Still       Horizontal Field of View(H-FOV)     Suggested usage
    // 1280x720     1280x720    1280x720    45deg                               (default mode)
    public static Vector3 WorldPoint(Vector2 uv, Matrix4x4 cameraTransform, Matrix4x4 cameraProjection)
    {
        Vector3 imagePosProj = (uv * 2.0f) - Vector2.one; // -1 to 1 space
        Vector3 cameraSpacePos = UnProjectVector(cameraProjection, imagePosProj);
        return cameraTransform.MultiplyVector(cameraSpacePos);
    }

    public static Vector3 UnProjectVector(Matrix4x4 projectionMatrix, Vector3 imagePosProjected)
    {
        Vector3 from = Vector3.zero;

        var axsX = projectionMatrix.GetRow(0);
        var axsY = projectionMatrix.GetRow(1);
        var axsZ = projectionMatrix.GetRow(2);

        from.z = imagePosProjected.z / axsZ.z;
        from.y = (imagePosProjected.y - (from.z * axsY.z)) / axsY.y;
        from.x = (imagePosProjected.x - (from.z * axsX.z)) / axsX.x;

        return from;
    }
}
