using SpatialTranformHelper;
using UnityEngine;

[RequireComponent(typeof(Camera))]
public class SpatialCameraTracker : MonoBehaviour
{
    public Camera spatialCamera { get; private set; }

    public Vector3[] NearCorners { get; private set; }

    public Vector3[] FarCorners { get; private set; }

    private SpatialTranformHelper.Matrix4x4 cameraTransform;
    private SpatialTranformHelper.Matrix4x4 lastGoodCameraTransform;

    private SpatialTranformHelper.Matrix4x4 cameraProjection;
    private SpatialTranformHelper.Matrix4x4 lastGoodCameraProjection;

    public float fov, aspectRatio, nearPlane, farPlane;
    private UnityEngine.Matrix4x4 defaultCameraProjection;

    private bool wasUpdated = false;

    // frustum points
    private Plane temp;
    private Plane[] cameraPlanes = null;

    private void Awake()
    {
        if (transform.parent != null)
        {
            Debug.LogError("This gameObject should be on the roor of the scene.");

            return;
        }
    }

    private void OnEnable()
    {
        spatialCamera = GetComponent<Camera>();

        fov = Camera.main.fieldOfView;
        aspectRatio = Camera.main.aspect;
        nearPlane = Camera.main.nearClipPlane;
        farPlane = Camera.main.farClipPlane;
        defaultCameraProjection =
            UnityEngine.Matrix4x4.Perspective(fov, aspectRatio, nearPlane, farPlane);

        spatialCamera.stereoTargetEye = StereoTargetEyeMask.None;
        spatialCamera.clearFlags = CameraClearFlags.SolidColor;
        spatialCamera.renderingPath = RenderingPath.Forward;
        spatialCamera.backgroundColor = Color.magenta * 0.25f; // color that is noticeable if not set correctly
        spatialCamera.allowMSAA = false;
        spatialCamera.allowHDR = false;
        spatialCamera.forceIntoRenderTexture = true;
        spatialCamera.targetTexture = new RenderTexture(1, 1, 0);

        spatialCamera.projectionMatrix = defaultCameraProjection;
    }

    private void Update()
    {
        if (cameraPlanes == null || NearCorners == null || FarCorners == null || !wasUpdated)
        {
            return;
        }
        //wasUpdated = false;

        // get the latest planes
        GeometryUtility.CalculateFrustumPlanes(spatialCamera, cameraPlanes);

        // re-arrange planes for looping
        temp = cameraPlanes[1]; cameraPlanes[1] = cameraPlanes[2]; cameraPlanes[2] = temp;

        // update corner verticies
        for (int i = 0; i < 4; i++)
        {
            NearCorners[i] = GetPlaneIntersection(cameraPlanes[4], cameraPlanes[i], cameraPlanes[(i + 1) % 4]);
            FarCorners[i] = GetPlaneIntersection(cameraPlanes[5], cameraPlanes[i], cameraPlanes[(i + 1) % 4]);
        }
    }

    // store the matrix values, any updates will happen on the update loop
    public void UpdateCameraMatrices(SpatialTranformHelper.Matrix4x4 transform, SpatialTranformHelper.Matrix4x4 projection)
    {
        // store matrix information from the sample
        if (cameraTransform != SpatialTranformHelper.Matrix4x4.Zero)
        {
            // always track the last known good transform
            lastGoodCameraTransform = cameraTransform;
        }
        cameraTransform = transform;

        if (cameraProjection != SpatialTranformHelper.Matrix4x4.Zero)
        {
            lastGoodCameraProjection = cameraProjection;
        }
        cameraProjection = projection;

        // update gameobject transform based on the matricies
        CameraPose? currentCameraPose = cameraTransform.ConvertWorldViewMatrix();
        if (currentCameraPose == null)
        {
            currentCameraPose = lastGoodCameraTransform.ConvertWorldViewMatrix();
        }

        // set the real worl position to the matrix
        if (currentCameraPose != null)
        {
            spatialCamera.transform.position = currentCameraPose.Value.Position;
            spatialCamera.transform.rotation = currentCameraPose.Value.Rotation;
        }

        // swap the projection for the one sent to us
        UnityEngine.Matrix4x4? currentCameraProjection
            = cameraProjection.ConvertCameraProjectionMatrix(nearPlane, farPlane);
        if (currentCameraProjection == null)
        {
            currentCameraProjection
                = lastGoodCameraProjection.ConvertCameraProjectionMatrix(nearPlane, farPlane);
        }

        if (currentCameraProjection != null)
        {
            spatialCamera.projectionMatrix = currentCameraProjection.Value;
        }

        //if (currentCameraPose == null)
        //{
        //     return;
        //}

        if (cameraPlanes == null)
        {
            cameraPlanes = new Plane[6];
        }

        if (NearCorners == null)
        {
            NearCorners = new Vector3[4];
        }

        if (FarCorners == null)
        {
            FarCorners = new Vector3[4];
        }

        wasUpdated = true;
    }

    // get the intersection point of 3 planes
    private Vector3 GetPlaneIntersection(Plane p1, Plane p2, Plane p3)
    {
        return ((-p1.distance * Vector3.Cross(p2.normal, p3.normal)) +
                (-p2.distance * Vector3.Cross(p3.normal, p1.normal)) +
                (-p3.distance * Vector3.Cross(p1.normal, p2.normal))) /
                (Vector3.Dot(p1.normal, Vector3.Cross(p2.normal, p3.normal)));
    }
}
