using SpatialTranformHelper;
using UnityEngine;

public class CameraVisualizer : MonoBehaviour
{
    public SpatialCameraTracker cameraTracker = null;

    public LineRenderer imgbounds = null;
    public Transform[] corners = null;
    public Transform targetTransform = null;

    private Camera cameraCache = null;

    private void Awake()
    {
        if (cameraCache == null)
        {
            cameraCache = Camera.main;
        }

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

        if (imgbounds != null && imgbounds.positionCount != 4)
        {
            imgbounds.positionCount = 4;
        }
    }

    private void Update()
    {
        if (cameraTracker.ImageCorners == null)
        {
            return;
        }

        // increase nearplane size
        float smooth = Time.deltaTime * 10.0f;
        Vector3 cameraPos = cameraTracker.transform.position;

        // update the offset from main camera to spatial camera
        if (targetTransform != null)
        {
            Vector3 mainCameraPos = cameraCache.transform.position;
            Vector3 mainToSpatialOffset = cameraPos - mainCameraPos;

            targetTransform.localPosition = Vector3.Lerp(targetTransform.localPosition, mainToSpatialOffset, smooth);
        }

        // update corner verticies
        for (int i = 0; i < 4; i++)
        {
            imgbounds.SetPosition(i, targetTransform.localPosition + cameraTracker.ImageCorners[i]);

            if (i < corners.Length)
            {
                corners[i].localPosition = imgbounds.GetPosition(i);
            }
        }
    }
}
