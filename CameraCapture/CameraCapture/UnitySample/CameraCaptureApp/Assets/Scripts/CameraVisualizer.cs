using SpatialTranformHelper;
using UnityEngine;

public class CameraVisualizer : MonoBehaviour
{
    public SpatialCameraTracker cameraTracker;

    public LineRenderer imgbounds;
    public Transform[] corners;
    public Transform position;

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
        if (cameraTracker.ImageCorners == null)
        {
            return;
        }

        if (imgbounds != null && imgbounds.positionCount != 4)
        {
            imgbounds.positionCount = 4;
        }

        // increase nearplane size
        const float scaleAmount = 4f;
        float smooth = Time.deltaTime * 5f;
        Vector3 cameraPos = cameraTracker.transform.position;

        // update the offset from main camera to spatial camera
        if (position != null)
        {
            Vector3 mainPos = Camera.main.transform.position;
            Vector3 mainToSpatialOffset = cameraPos - mainPos;

            position.localPosition = Vector3.Lerp(position.localPosition, mainToSpatialOffset, smooth);
        }

        // update corner verticies
        for (int i = 0; i < 4; i++)
        {
            imgbounds.SetPosition(i, position.localPosition + cameraTracker.ImageCorners[i]);

            if (i < corners.Length)
            {
                corners[i].position = imgbounds.GetPosition(i);
            }
        }
    }
}
