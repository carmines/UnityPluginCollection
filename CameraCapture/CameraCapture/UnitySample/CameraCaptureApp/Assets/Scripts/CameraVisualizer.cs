using SpatialTranformHelper;
using UnityEngine;

public class CameraVisualizer : MonoBehaviour
{
    public SpatialCameraTracker cameraTracker = null;

    public Transform cameraOffsetRoot = null;
    public LineRenderer cameraBorder = null;
    public Transform[] cornerMarkers = null;

    private Camera cameraCache = null;
    private Vector3 startPosition;

    private Vector3 defaultPosition;
    private bool alignToCamera = false;

    private const string alignTracker = "align tracker";
    private const string resetTracker = "reset tracker";

    private void Awake()
    {
        cameraCache = Camera.main;

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

        if (cameraBorder != null && cameraBorder.positionCount != 4)
        {
            cameraBorder.positionCount = 4;
        }
    }

    private void OnEnable()
    {
        defaultPosition = transform.position;
        startPosition = cameraCache.transform.localPosition;
    }

    private void Update()
    {
        // increase nearplane size
        float smooth = Time.deltaTime * 5.0f;

        // update the offset from main camera to pv camera
        if (cameraTracker != null && cameraOffsetRoot != null)
        {
            if (alignToCamera)
            {
                transform.position = cameraTracker.transform.position;
            }

            cameraOffsetRoot.localPosition = Vector3.Lerp(cameraOffsetRoot.localPosition, cameraTracker.transform.forward + startPosition, smooth);
        }

        // update corner verticies
        if (cameraTracker.ImageCorners == null)
        {
            return;
        }

        for (int i = 0; i < 4; i++)
        {
            cameraBorder.SetPosition(i, cameraTracker.ImageCorners[i]);

            if (i < cornerMarkers.Length)
            {
                cornerMarkers[i].localPosition = cameraBorder.GetPosition(i);
            }
        }
    }

    public void PhraseRecognized(string keywords)
    {
        var toLower = keywords.ToLower();
        switch (toLower)
        {
            case alignTracker:
                alignToCamera = true;
                break;
            case resetTracker:
                transform.position = defaultPosition;
                alignToCamera = false;
                break;
        };
    }
}
