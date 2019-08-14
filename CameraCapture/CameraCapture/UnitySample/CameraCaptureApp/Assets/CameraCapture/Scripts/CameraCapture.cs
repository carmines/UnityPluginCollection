// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using SpatialTranformHelper;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using UnityEngine;

namespace CameraCapture
{
    internal class CameraCapture : BasePlugin<CameraCapture>
    {
        public Renderer videoRenderer = null;
        public UInt32 width = 1280;
        public UInt32 height = 720;
        public Boolean enableAudio = false;
        public Boolean enableMrc = false;

        public SpatialCameraTracker cameraTracker = null;

        private Texture2D videoTexture = null;

        private IntPtr spatialCoordinateSystemPtr = IntPtr.Zero;

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

            if (enableAudio && Application.HasUserAuthorization(UserAuthorization.Microphone))
            {
                Application.RequestUserAuthorization(UserAuthorization.Microphone);
            }

            CreateCapture();

            if (videoRenderer != null)
            {
                videoRenderer.enabled = true;
            }

            spatialCoordinateSystemPtr = UnityEngine.XR.WSA.WorldManager.GetNativeISpatialCoordinateSystemPtr();

            CheckHR(Native.SetCoordinateSystem(instanceId, spatialCoordinateSystemPtr));

            CheckHR(Native.StartPreview(instanceId, width, height, enableAudio, enableMrc));
        }

        protected override void OnDisable()
        {
            if (videoRenderer != null)
            {
                videoRenderer.material.SetTexture("_MainTex", null);
                videoRenderer.enabled = false;
            }

            videoTexture = null;

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
                }
            }
        }

        private void OnVideoFrame(Wrapper.CaptureState state)
        {
            if (videoTexture == null)
            {
                if (state.width != width || state.height != height)
                {
                    Debug.LogWarningFormat("Video texture does not match the size requested, using {0} x {1}", state.width, state.height);
                }

                videoTexture = Texture2D.CreateExternalTexture(state.width, state.height, TextureFormat.BGRA32, false, false, state.imgTexture);

                if (videoRenderer != null)
                {
                    videoRenderer.enabled = true;
                    videoRenderer.sharedMaterial.SetTexture("_MainTex", videoTexture);
                    videoRenderer.sharedMaterial.SetTextureScale("_MainTex", new Vector2(1, -1)); // flip texture
                }
            }
            else if (videoTexture.width != state.width || videoTexture.height != state.height)
            {
                Debug.LogWarningFormat("Video texture size changed, using {0} x {1}", state.width, state.height);

                videoTexture.UpdateExternalTexture(state.imgTexture);
            }

            // upate object using this information
            if (cameraTracker != null)
            {
                cameraTracker.UpdateCameraMatrices(state.cameraWorld, state.cameraProjection);
            }
        }

        private void CreateCapture()
        {
            IntPtr thisObjectPtr = GCHandle.ToIntPtr(thisObject);
            CheckHR(Wrapper.CreateCapture(stateChangedCallback, thisObjectPtr, out instanceId));
        }

        private static class Native
        {
            [DllImport(Wrapper.ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "CaptureStartPreview")]
            internal static extern Int32 StartPreview(Int32 handle, UInt32 width, UInt32 height, [MarshalAs(UnmanagedType.I1)]Boolean enableAudio, [MarshalAs(UnmanagedType.I1)]Boolean enableMrc);

            [DllImport(Wrapper.ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "CaptureStopPreview")]
            internal static extern Int32 StopPreview(Int32 handle);

            [DllImport(Wrapper.ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "CaptureSetCoordinateSystem")]
            internal static extern Int32 SetCoordinateSystem(Int32 instanceId, IntPtr spatialCoordinateSystemPtr);
        }
    }
}
