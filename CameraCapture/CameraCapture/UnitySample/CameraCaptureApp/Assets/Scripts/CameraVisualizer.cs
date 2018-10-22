using SpatialTranformHelper;
using UnityEngine;

[RequireComponent(typeof(Camera))]
public class CameraVisualizer : MonoBehaviour
{
    public LineRenderer nearPlaneRenderer;
    public LineRenderer farPlaneRenderer;

    private Camera displayCamera;

    private bool transformUpdated = false;
    private SpatialTranformHelper.Matrix4x4 cameraTransform;
    private SpatialTranformHelper.Matrix4x4 lastGoodCameraTransform;

    private SpatialTranformHelper.Matrix4x4 cameraProjection;
    private SpatialTranformHelper.Matrix4x4 lastGoodCameraProjection;

    private float fov, aspectRatio, nearPlane, farPlane;
    private UnityEngine.Matrix4x4 defaultCameraProjection;

    // frustum points
    private Plane temp;
    private Plane[] cameraPlanes = null;
    private Vector3[] nearCorners = null;   // near planes corner position
    private Vector3[] farCorners = null;    // far planes corner position

    private void Awake()
    {
        displayCamera = GetComponent<Camera>();

        fov = Camera.main.fieldOfView;
        aspectRatio = Camera.main.aspect;
        nearPlane = Camera.main.nearClipPlane;
        farPlane = Camera.main.farClipPlane;
        defaultCameraProjection =
            UnityEngine.Matrix4x4.Perspective(fov, aspectRatio, nearPlane, farPlane);
    }

    private void Update()
    {
        if (nearCorners == null || farCorners == null || !transformUpdated)
        {
            return;
        }

        transformUpdated = false;

        // update gameobject transform based on the matricies
        CameraPose? currentCameraPose = cameraTransform.ConvertWorldViewMatrix();
        if (currentCameraPose == null)
        {
            currentCameraPose = lastGoodCameraTransform.ConvertWorldViewMatrix();
        }

        if (currentCameraPose != null)
        {
            gameObject.transform.position = currentCameraPose.Value.Position;
            gameObject.transform.rotation = currentCameraPose.Value.Rotation;
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
            displayCamera.projectionMatrix = currentCameraProjection.Value;
        }

        if (currentCameraPose == null)
        {
            return;
        }

        // update the frustum corners positions
        UpdatePlanes();

        //for (int i = 0; i < 4; i++)
        //{
        //    Debug.DrawLine(nearCorners[i], nearCorners[(i + 1) % 4], Color.red, Time.deltaTime, true); // near corners on the created projection matrix
        //    Debug.DrawLine(farCorners[i], farCorners[(i + 1) % 4], Color.blue, Time.deltaTime, true); // far corners on the created projection matrix
        //    Debug.DrawLine(nearCorners[i], farCorners[i], Color.green, Time.deltaTime, true); // sides of the created projection matrix
        //}
    }

    private void UpdatePlanes()
    {
        if (cameraPlanes == null)
        {
            cameraPlanes = new Plane[6];
        }

        if (nearCorners == null)
        {
            nearCorners = new Vector3[4];
        }

        if (farCorners == null)
        {
            farCorners = new Vector3[4];
        }

        // get the latest planes
        GeometryUtility.CalculateFrustumPlanes(displayCamera, cameraPlanes); //TODO: check if this gets updated during Update only

        // re-arrange planes for looping
        temp = cameraPlanes[1]; cameraPlanes[1] = cameraPlanes[2]; cameraPlanes[2] = temp;

        if (nearPlaneRenderer != null && nearPlaneRenderer.positionCount != 4)
        {
            nearPlaneRenderer.positionCount = 4;
        }

        if (farPlaneRenderer != null && farPlaneRenderer.positionCount != 4)
        {
            farPlaneRenderer.positionCount = 4;
        }

        // update corner verticies
        for (int i = 0; i < 4; i++)
        {
            nearCorners[i] = GetPlaneIntersection(cameraPlanes[4], cameraPlanes[i], cameraPlanes[(i + 1) % 4]); // near corners on the created projection matrix
            farCorners[i] = GetPlaneIntersection(cameraPlanes[5], cameraPlanes[i], cameraPlanes[(i + 1) % 4]); // far corners on the created projection matrix

            if (nearPlaneRenderer != null)
            {
                nearPlaneRenderer.SetPosition(i, nearCorners[i]);
            }

            if (farPlaneRenderer != null)
            {
                farPlaneRenderer.SetPosition(i, farCorners[i]);
            }
        }
    }

    // get the intersection point of 3 planes
    private Vector3 GetPlaneIntersection(Plane p1, Plane p2, Plane p3)
    {
        return ((-p1.distance * Vector3.Cross(p2.normal, p3.normal)) +
                (-p2.distance * Vector3.Cross(p3.normal, p1.normal)) +
                (-p3.distance * Vector3.Cross(p1.normal, p2.normal))) / 
                (Vector3.Dot(p1.normal, Vector3.Cross(p2.normal, p3.normal)));
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

        transformUpdated = true;
    }
}
