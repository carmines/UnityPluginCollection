// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using SpatialTranformHelper;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.Windows.Speech;

namespace CameraCapture
{
    internal class CameraCapture : BasePlugin<CameraCapture>
    {
        public Int32 Width = 1280;
        public Int32 Height = 720;
        public Boolean EnableAudio = false;
        public Boolean EnableMrc = false;
        public SpatialCameraTracker CameraTracker = null;

        public Renderer VideoRenderer = null;
        public Renderer PhotoRenderer = null;

        private Texture2D videoTexture = null;
        private Texture2D photoTexture = null;

        private IntPtr spatialCoordinateSystemPtr = IntPtr.Zero;

        TaskCompletionSource<Wrapper.CaptureState> startPreviewCompletionSource = null;
        TaskCompletionSource<Wrapper.CaptureState> stopCompletionSource = null;
        TaskCompletionSource<Wrapper.CaptureState> photoCompletionSource = null;

        private KeywordRecognizer keywordRecognizer;
        private Dictionary<string, Action> keywords = new Dictionary<string, Action>();

        private void OnGUI()
        {
            int y = 50;
            if (GUI.Button(new Rect(50, y, 200, 50), "Start Preview"))
            {
                StartPreview();
            }

            y += 55;

            if (GUI.Button(new Rect(50, y, 200, 50), "Stop Preview"))
            {
                StopPreview();
            }

            y += 55;

            if (GUI.Button(new Rect(50, y, 200, 50), "Take Photo"))
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

                SetSpatialCoordinateSystem();
            };
        }

        private void SetSpatialCoordinateSystem()
        {
            spatialCoordinateSystemPtr = UnityEngine.XR.WSA.WorldManager.GetNativeISpatialCoordinateSystemPtr();
            if (instanceId != Wrapper.InvalidHandle)
            {
                CheckHR(Native.SetCoordinateSystem(instanceId, spatialCoordinateSystemPtr));
            }
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

            if (PhotoRenderer != null)
            {
                PhotoRenderer.enabled = true;
            }

            keywords.Add("take photo", () =>
            {
                TakePhoto();
            });

            keywords.Add("start preview", () =>
            {
                SetSpatialCoordinateSystem();

                StartPreview();
            });

            keywords.Add("stop preview", () =>
            {
                StopPreview();
            });

            keywordRecognizer = new KeywordRecognizer(keywords.Keys.ToArray());
            keywordRecognizer.OnPhraseRecognized += KeywordRecognizer_OnPhraseRecognized;
            keywordRecognizer.Start();
        }

        protected override void OnDisable()
        {
            startPreviewCompletionSource?.TrySetCanceled();
            stopCompletionSource?.TrySetCanceled();
            photoCompletionSource?.TrySetCanceled();

            if (VideoRenderer != null)
            {
                VideoRenderer.material.SetTexture("_MainTex", null);
                VideoRenderer.enabled = false;
            }

            videoTexture = null;

            if (PhotoRenderer != null)
            {
                PhotoRenderer.material.SetTexture("_MainTex", null);
                PhotoRenderer.enabled = false;
            }

            photoTexture = null;

            StopPreview();

            base.OnDisable();
        }

        protected override void OnCallback(Wrapper.CallbackType type, Wrapper.CallbackState args)
        {
            if (type == Wrapper.CallbackType.Capture)
            {
                switch (args.CaptureState.stateType)
                {
                    case Wrapper.CaptureStateType.PreviewStarted:
                        startPreviewCompletionSource?.TrySetResult(args.CaptureState);
                        break;
                    case Wrapper.CaptureStateType.PreviewStopped:
                        stopCompletionSource?.TrySetResult(args.CaptureState);
                        break;
                    case Wrapper.CaptureStateType.PreviewAudioFrame:
                        //Debug.Log("PreviewAudioFrame");
                        break;
                    case Wrapper.CaptureStateType.PreviewVideoFrame:
                        OnPreviewFrameChanged(args.CaptureState);
                        break;
                    case Wrapper.CaptureStateType.PhotoFrame:
                        photoCompletionSource?.TrySetResult(args.CaptureState);
                        break;
                }
            }
        }

        private void OnPreviewFrameChanged(Wrapper.CaptureState state)
        {
            var sizeChanged = false;

            if (videoTexture == null)
            {
                if (state.width != Width || state.height != Height)
                {
                    Debug.Log("Video texture does not match the size requested, using " + state.width + " x " + state.height);
                }

                videoTexture = Texture2D.CreateExternalTexture(state.width, state.height, TextureFormat.BGRA32, false, false, state.imgTexture);

                sizeChanged = true;

                if (VideoRenderer != null)
                {
                    VideoRenderer.enabled = true;
                    VideoRenderer.sharedMaterial.SetTexture("_MainTex", videoTexture);
                    VideoRenderer.sharedMaterial.SetTextureScale("_MainTex", new Vector2(1, -1)); // flip texture
                }
            }
            else if (videoTexture.width != state.width || videoTexture.height != state.height)
            {
                Debug.Log("Video texture size changed, using " + state.width + " x " + state.height);

                videoTexture.UpdateExternalTexture(state.imgTexture);

                sizeChanged = true;
            }

            if (sizeChanged)
            {
                Debug.Log($"Size Changed = {state.width} x {state.height}");

                Width = state.width;
                Height = state.height;
            }

            if (CameraTracker != null)
            {
                CameraTracker.UpdateCameraMatrices(state.cameraWorld, state.cameraProjection);
            }
        }

        protected override void OnFailed(Wrapper.FailedState args)
        {
            base.OnFailed(args);

            startPreviewCompletionSource?.TrySetCanceled();

            stopCompletionSource?.TrySetCanceled();

            photoCompletionSource?.TrySetCanceled();
        }

        private void CreateCapture()
        {
            IntPtr thisObjectPtr = GCHandle.ToIntPtr(thisObject);
            CheckHR(Wrapper.CreateCapture(stateChangedCallback, thisObjectPtr, out instanceId));
        }

        public async void StartPreview()
        {
            await StartPreviewAsync(Width, Height, EnableAudio, EnableMrc);
        }

        public async void StopPreview()
        {
            if (!await StopPreviewAsync())
            {
                StopPreview();
            }
        }

        public async void TakePhoto()
        {
            await TakePhotoAsync(Width, Height, true, true);
        }

        public async Task<bool> StartPreviewAsync(int width, int height, bool enableAudio, bool useMrc)
        {
            startPreviewCompletionSource?.TrySetCanceled();

            var hr = Native.StartPreview(instanceId, (UInt32)width, (UInt32)height, enableAudio, useMrc);
            if (hr == 0)
            {
                startPreviewCompletionSource = new TaskCompletionSource<Wrapper.CaptureState>();

                try
                {
                    await startPreviewCompletionSource.Task;
                }
                catch (Exception ex)
                {
                    // task could have been cancelled
                    Debug.LogError(ex.Message);
                    hr = ex.HResult;
                }

                startPreviewCompletionSource = null;
            }
            else
            {
                await Task.Yield();
            }

            return (hr == 0);
        }

        public async Task<bool> StopPreviewAsync()
        {
            stopCompletionSource?.TrySetCanceled();

            var hr = Native.StopPreview(instanceId);
            if (hr == 0)
            {
                stopCompletionSource = new TaskCompletionSource<Wrapper.CaptureState>();

                try
                {
                    var state = await stopCompletionSource.Task;
                }
                catch (Exception ex)
                {
                    Debug.LogError(ex.Message);

                    hr = ex.HResult;
                }
            }
            else
            {
                await Task.Yield();
            }

            stopCompletionSource = null;

            videoTexture = null;

            return CheckHR(hr) == 0;
        }

        public async Task<Texture2D> TakePhotoAsync(int width, int height, bool useMrc, bool flipImage = false)
        {
            photoCompletionSource?.TrySetCanceled();

            Texture2D copyTexture = null;

            var hr = Native.TakePhoto(instanceId, (UInt32)width, (UInt32)height, useMrc);
            if (hr == 0)
            {
                photoCompletionSource = new TaskCompletionSource<Wrapper.CaptureState>();

                try
                {
                    var state = await photoCompletionSource.Task;

                    if (state.width != width || state.height != height)
                    {
                        Debug.Log("Video texture does not match the size requested, using " + state.width + " x " + state.height);
                    }

                    if (photoTexture == null)
                    {
                        photoTexture = Texture2D.CreateExternalTexture(state.width, state.height, TextureFormat.BGRA32, false, false, state.imgTexture);
                    }

                    photoTexture.UpdateExternalTexture(state.imgTexture);

                    copyTexture = CopyTexture(photoTexture, flipImage);

                    if (PhotoRenderer != null)
                    {
                        PhotoRenderer.enabled = true;
                        PhotoRenderer.sharedMaterial.SetTexture("_MainTex", photoTexture);
                        PhotoRenderer.sharedMaterial.SetTextureScale("_MainTex", new Vector2(1, -1)); // flip texture
                    }
                }
                catch (Exception ex)
                {
                    Debug.LogError(ex.Message);
                }
            }
            else
            {
                await Task.Yield();

                CheckHR(hr);
            }

            return copyTexture;
        }

        private Texture2D CopyTexture(Texture2D sourceTexture, bool flipImage = false)
        {
            Texture2D texture2D = null;

            if (sourceTexture != null)
            {
                var tempRenderTexture = RenderTexture.GetTemporary(
                        sourceTexture.width,
                        sourceTexture.height,
                        0,
                        RenderTextureFormat.BGRA32,
                        RenderTextureReadWrite.Linear);

                // Blit the pixels on texture to the RenderTexture
                Graphics.Blit(sourceTexture, tempRenderTexture);

                // Backup the currently set RenderTexture
                RenderTexture previous = RenderTexture.active;

                // Set the current RenderTexture to the temporary one we created
                RenderTexture.active = tempRenderTexture;

                // Create a new readable Texture2D to copy the pixels to it
                texture2D = new Texture2D(sourceTexture.width, sourceTexture.height, TextureFormat.RGB24, false, true);
                if (flipImage)
                {
                    for (int y = 0; y < sourceTexture.height; y++)
                    {
                        texture2D.ReadPixels(new Rect(0, y, tempRenderTexture.width, 1), 0, y);
                    }
                }
                else
                {
                    texture2D.ReadPixels(new Rect(0, 0, tempRenderTexture.width, tempRenderTexture.height), 0, 0);
                }
                texture2D.Apply();

                // Reset the active RenderTexture
                RenderTexture.active = previous;

                // Release the temporary RenderTexture
                RenderTexture.ReleaseTemporary(tempRenderTexture);
            }

            return texture2D;
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
