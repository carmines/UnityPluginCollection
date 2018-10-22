using SpatialTranformHelper;
using UnityEngine;

public class CameraVisualizer : MonoBehaviour
{
    public SpatialCameraTracker cameraTracker;

    public LineRenderer nearPlaneRenderer;
    public LineRenderer farPlaneRenderer;
    public Transform[] Corners;
    public Transform Position;

    private void Awake()
    {
        if (cameraTracker == null)
        {
            Debug.LogError("Set the spatial camera tracker property before beginning.");
            return;
        }

        if (cameraTracker.gameObject == gameObject)
        {
            Debug.LogError("Tracker script should not be on the same gameobject or rooted under the Spatial Camera.");

            return;
        }
    }

    private void Update()
    {
        if (cameraTracker.NearCorners == null || cameraTracker.FarCorners == null)
        {
            return;
        }

        if (nearPlaneRenderer != null && nearPlaneRenderer.positionCount != 4)
        {
            nearPlaneRenderer.positionCount = 4;
        }

        if (farPlaneRenderer != null && farPlaneRenderer.positionCount != 4)
        {
            farPlaneRenderer.positionCount = 4;
        }

        const float lerpAmount = 0.2f;
        Vector3 cameraPos = cameraTracker.spatialCamera.transform.position;
        Bounds nearBounds = new Bounds();
        Bounds farBounds = new Bounds();

        // update corner verticies
        for (int i = 0; i < 4; i++)
        {
            if (nearPlaneRenderer != null)
            {
                // calc relative offset
                Vector3 offset = (cameraTracker.NearCorners[i] - cameraPos) * 4;
                Vector3 currentPos = nearPlaneRenderer.GetPosition(i);
                Vector3 newPos = Vector3.Lerp(currentPos, offset, lerpAmount);
                nearPlaneRenderer.SetPosition(i, newPos);

                if (i < Corners.Length)
                {
                    Corners[i].localPosition = newPos;
                }

                nearBounds.Expand(newPos);
            }

            if (farPlaneRenderer != null)
            {
                Vector3 offset = (cameraTracker.FarCorners[i] - cameraPos) * 4;
                Vector3 currentPos = farPlaneRenderer.GetPosition(i);
                Vector3 newPos = Vector3.Lerp(currentPos, offset, lerpAmount);
                farPlaneRenderer.SetPosition(i, newPos);

                farBounds.Expand(newPos);
            }
        }

        // update markers
        if (Position != null)
        {
            Vector3 center = nearBounds.center;
            Vector3 forward = (farBounds.center - nearBounds.center).normalized;

            Position.localPosition = (center - (forward * cameraTracker.spatialCamera.nearClipPlane));
        }
    }


    //private void OnDrawGizmos()
    //{
    //    if (nearCorners == null || farCorners == null || nearCorners.Length != 4 || farCorners.Length != 4)
    //    {
    //        return;
    //    }

    //    for (int i = 0; i < 4; i++)
    //    {
    //        Debug.DrawLine(nearCorners[i], nearCorners[(i + 1) % 4], Color.red, Time.deltaTime, true); // near corners on the created projection matrix
    //        Debug.DrawLine(farCorners[i], farCorners[(i + 1) % 4], Color.blue, Time.deltaTime, true); // far corners on the created projection matrix
    //        Debug.DrawLine(nearCorners[i], farCorners[i], Color.green, Time.deltaTime, true); // sides of the created projection matrix
    //    }
    //}
}
