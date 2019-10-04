// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using SpatialTranformHelper;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Windows.Speech;

namespace CameraCapture
{
    internal class CameraCapture : BasePlugin<CameraCapture>
    {
        public Renderer VideoRenderer = null;
        public Renderer PhotoRenderer = null;
        public Int32 Width = 1280;
        public Int32 Height = 720;
        public Boolean EnableAudio = false;
        public Boolean EnableMrc = false;
        public SpatialCameraTracker CameraTracker = null;

        private Texture2D videoTexture = null;
        private Texture2D photoTexture = null;
        private IntPtr spatialCoordinateSystemPtr = IntPtr.Zero;

        private KeywordRecognizer keywordRecognizer;
        private Dictionary<string, Action> keywords = new Dictionary<string, Action>();

        private void OnGUI()
        {
            if (GUI.Button(new Rect(50, 50, 200, 50), "Take Photo"))
            {
                TakePhoto();
            }
        }

        private void KeywordRecognizer_OnPhraseRecognized(PhraseRecognizedEventArgs args)
        {
            if (args.confidence <= ConfidenceLevel.Medium)
            {
                System.Action keywordAction;

                if (keywords.TryGetValue(args.text, out keywordAction))
                {
                    keywordAction.Invoke();
                }
            }
        }

        protected override void Awake()
        {
            base.Awake();

            UnityEngine.XR.WSA.WorldManager.OnPositionalLocatorStateChanged += (oldState, newState) =>
            {
                Debug.Log("WorldManager.OnPositionalLocatorStateChanged: " + newState + ", updating any capture in progress");
                spatialCoordinateSystemPtr = UnityEngine.XR.WSA.WorldManager.GetNativeISpatialCoordinateSystemPtr();
                if (instanceId != Wrapper.InvalidHandle)
                {
                    CheckHR(Native.SetCoordinateSystem(instanceId, spatialCoordinateSystemPtr));
                }
            };
        }

        protected override void OnEnable()
        {
            base.OnEnable();

            if (Application.HasUserAuthorization(UserAuthorization.WebCam))
            {
                Application.RequestUserAuthorization(UserAuthorization.WebCam);
            }

            if (EnableAudio && Application.HasUserAuthorization(UserAuthorization.Microphone))
            {
                Application.RequestUserAuthorization(UserAuthorization.Microphone);
            }

            CreateCapture();

            if (VideoRenderer != null)
            {
                VideoRenderer.enabled = true;
            }

            spatialCoordinateSystemPtr = UnityEngine.XR.WSA.WorldManager.GetNativeISpatialCoordinateSystemPtr();

            CheckHR(Native.SetCoordinateSystem(instanceId, spatialCoordinateSystemPtr));

            CheckHR(Native.StartPreview(instanceId, (UInt32)Width, (UInt32)Height, EnableAudio, EnableMrc));

            keywords.Add("take photo", () =>
            {
                TakePhoto();
            });

            keywordRecognizer = new KeywordRecognizer(keywords.Keys.ToArray());
            keywordRecognizer.OnPhraseRecognized += KeywordRecognizer_OnPhraseRecognized;
            keywordRecognizer.Start();
        }

        protected override void OnDisable()
        {
            if (VideoRenderer != null)
            {
                VideoRenderer.material.SetTexture("_MainTex", null);
                VideoRenderer.enabled = false;
            }

            if (PhotoRenderer != null)
            {
                PhotoRenderer.material.SetTexture("_MainTex", null);
                PhotoRenderer.enabled = false;
            }

            videoTexture = null;

            photoTexture = null;

            CheckHR(Native.StopPreview(instanceId));

            base.OnDisable();
        }

        protected override void OnCallback(Wrapper.CallbackType type, Wrapper.CallbackState args)
        {
            if (type == Wrapper.CallbackType.Capture)
            {
                switch (args.CaptureState.stateType)
                {
                    case Wrapper.CaptureStateType.PreviewStarted:
                        Debug.Log("PreviewStarted");
                        break;
                    case Wrapper.CaptureStateType.PreviewStopped:
                        Debug.Log("PreviewStopped");
                        break;
                    case Wrapper.CaptureStateType.PreviewAudioFrame:
                        //Debug.Log("PreviewAudioFrame");
                        break;
                    case Wrapper.CaptureStateType.PreviewVideoFrame:
                        OnVideoFrame(args.CaptureState);
                        break;
                    case Wrapper.CaptureStateType.PhotoFrame:
                        OnPhotoFrame(args.CaptureState);
                        break;
                }
            }
        }

        private void OnVideoFrame(Wrapper.CaptureState state)
        {
            if (videoTexture == null)
            {
                if (state.width != Width || state.height != Height)
                {
                    Debug.LogWarningFormat("Video texture does not match the size requested, using {0} x {1}", state.width, state.height);
                }

                videoTexture = Texture2D.CreateExternalTexture(state.width, state.height, TextureFormat.BGRA32, false, false, state.imgTexture);

                if (VideoRenderer != null)
                {
                    VideoRenderer.enabled = true;
                    VideoRenderer.sharedMaterial.SetTexture("_MainTex", videoTexture);
                    VideoRenderer.sharedMaterial.SetTextureScale("_MainTex", new Vector2(1, -1)); // flip texture
                }
            }
            else if (videoTexture.width != state.width || videoTexture.height != state.height)
            {
                Debug.LogWarningFormat("Video texture size changed, using {0} x {1}", state.width, state.height);

                videoTexture.UpdateExternalTexture(state.imgTexture);
            }

            // upate object using this information
            if (CameraTracker != null)
            {
                CameraTracker.UpdateCameraMatrices(state.cameraWorld, state.cameraProjection);
            }
        }

        private void OnPhotoFrame(Wrapper.CaptureState state)
        {
            if (photoTexture == null)
            {
                if (state.width != Width || state.height != Height)
                {
                    Debug.LogWarningFormat("Photo texture does not match the size requested, using {0} x {1}", state.width, state.height);
                }
                Width = state.width;

                Height = state.height;

                photoTexture = Texture2D.CreateExternalTexture(state.width, state.height, TextureFormat.BGRA32, false, false, state.imgTexture);

                if (PhotoRenderer != null)
                {
                    PhotoRenderer.enabled = true;
                    PhotoRenderer.sharedMaterial.SetTexture("_MainTex", photoTexture);
                    PhotoRenderer.sharedMaterial.SetTextureScale("_MainTex", new Vector2(1, -1)); // flip texture
                }
            }
            else if (photoTexture.width != state.width || photoTexture.height != state.height)
            {
                Debug.LogWarningFormat("Photo texture size changed, using {0} x {1}", state.width, state.height);

                photoTexture.UpdateExternalTexture(state.imgTexture);
            }
        }

        private void CreateCapture()
        {
            IntPtr thisObjectPtr = GCHandle.ToIntPtr(thisObject);
            CheckHR(Wrapper.CreateCapture(stateChangedCallback, thisObjectPtr, out instanceId));
        }

        public void TakePhoto()
        {
            Boolean useMrc = false;

#if !UNITY_EDITOR
            useMrc = true;
#endif

            CheckHR(Native.TakePhoto(instanceId, (UInt32)Width, (UInt32)Height, useMrc));
        }

        private static class Native
        {
            [DllImport(Wrapper.ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "CaptureStartPreview")]
            internal static extern Int32 StartPreview(Int32 handle, UInt32 width, UInt32 height, [MarshalAs(UnmanagedType.I1)]Boolean enableAudio, [MarshalAs(UnmanagedType.I1)]Boolean enableMrc);

            [DllImport(Wrapper.ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "CaptureStopPreview")]
            internal static extern Int32 StopPreview(Int32 handle);

            [DllImport(Wrapper.ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "CaptureSetCoordinateSystem")]
            internal static extern Int32 SetCoordinateSystem(Int32 instanceId, IntPtr spatialCoordinateSystemPtr);

            [DllImport(Wrapper.ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "CaptureTakePhoto")]
            internal static extern Int32 TakePhoto(Int32 instanceId, UInt32 width, UInt32 height, [MarshalAs(UnmanagedType.I1)]Boolean enableMrc);
        }
    }
}
