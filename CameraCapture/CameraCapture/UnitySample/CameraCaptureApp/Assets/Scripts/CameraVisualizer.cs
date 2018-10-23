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

        const float smooth = 5f;
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
                Vector3 newPos = Vector3.Lerp(currentPos, offset, Time.deltaTime * smooth);
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
                Vector3 newPos = Vector3.Lerp(currentPos, offset, Time.deltaTime * smooth);
                farPlaneRenderer.SetPosition(i, newPos);

                farBounds.Expand(newPos);
            }
        }

        // update the offset from main camera to spatial camera
        if (Position != null)
        {
            Vector3 mainPos = Camera.main.transform.position;
            Vector3 mainToSpatialOffset = cameraPos - mainPos;

            Position.localPosition = Vector3.Lerp(Position.localPosition, mainToSpatialOffset, Time.deltaTime * smooth);
        }
    }
}
