using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;

namespace VideoPlayer
{
    internal static class PlaybackEngineStateCallback
    {
        [AOT.MonoPInvokeCallback(typeof(Wrapper.StateChangedCallback))]
        internal static void StateChangedCallback(IntPtr targetObj, Wrapper.CallbackState args)
        {
            if (targetObj == IntPtr.Zero)
            {
                Debug.LogError("StateChangedCallback: targetObj is null.");

                return;
            }

            GCHandle gcHandle = GCHandle.FromIntPtr(targetObj);

            var thisObject = gcHandle.Target as PlaybackEngine;
            if (thisObject == null)
            {
                Debug.LogError("StateChangedCallback: targetObj is not null, but not of type PlaybackEngine");

                return;
            }

#if UNITY_WSA_10_0
            if (!UnityEngine.WSA.Application.RunningOnAppThread())
            {
                UnityEngine.WSA.Application.InvokeOnAppThread(() =>
                {
                    thisObject.OnStateChanged(args);
                }, false);
            }
            else
            {
                thisObject.OnStateChanged(args);
            }
#else
            // there is still a chance the callback is on a non AppThread(callbacks genereated from WaitForEndOfFrame are not)
            // this will process the callback on AppThread on a FixedUpdate
            thisObject.OnStateChanged(args);
#endif
        }
    }

    internal class PlaybackEngine : BasePlugin<PlaybackEngine>
    {
        public Renderer playbackRenderer;
        public String RootVideoFolder;
        public String VideoPath;

        // texture size
        public Int32 textureWidth = 1920;
        public Int32 textureHeight = 1080;

        private Texture2D playbackTexture = null;

        protected override void Awake()
        {
            base.Awake();

            if (playbackRenderer == null)
            {
                playbackRenderer = GetComponent<Renderer>();
            }

#if UNITY_EDITOR || UNITY_STANDALONE
            if (RootVideoFolder != null && !System.IO.Path.IsPathRooted(RootVideoFolder))
            {
                RootVideoFolder = (Application.persistentDataPath + "/" + RootVideoFolder);
            }
#else
            RootVideoFolder = (Application.persistentDataPath + "/" + RootVideoFolder);
#endif
            RootVideoFolder = RootVideoFolder.Replace("/", "\\");

            if (!System.IO.Path.IsPathRooted(VideoPath))
            {
                VideoPath = (RootVideoFolder + VideoPath).Replace("/", "\\");
            }
        }

        protected override void OnEnable()
        {
            base.OnEnable();

            CreateMediaPlayer(new Wrapper.StateChangedCallback(PlaybackEngineStateCallback.StateChangedCallback));

            // create native texture for playback
            IntPtr nativeTexture = IntPtr.Zero;
            CheckHR(Native.CreatePlaybackTexture(instanceId, textureWidth, textureHeight, out nativeTexture));

            // create the unity texture2d 
            this.playbackTexture = Texture2D.CreateExternalTexture(textureWidth, textureWidth, TextureFormat.BGRA32, false, false, nativeTexture);

            // set texture for the shader
            if (playbackRenderer != null)
            {
                playbackRenderer.material.SetTexture("_MainTex", this.playbackTexture);
                playbackRenderer.material.SetTextureScale("_MainTex", new Vector2(1, -1));
            }

            CheckHR(Native.LoadContent(instanceId, VideoPath));

            CheckHR(Native.Play(instanceId));
        }

        protected override void OnDisable()
        {
            CheckHR(Native.Stop(instanceId));

            base.OnDisable();
        }

        protected override void OnCallback(Wrapper.CallbackType type, Wrapper.CallbackState args)
        {
            Debug.Log(args.PlaybackState);
        }

        private static class Native
        {
            [DllImport(Wrapper.ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "MediaPlayerCreateTexture")]
            internal static extern Int32 CreatePlaybackTexture(Int32 instanceId, Int32 width, Int32 height, out System.IntPtr playbackTexture);

            [DllImport(Wrapper.ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "MediaPlayerLoadContent")]
            internal static extern Int32 LoadContent(Int32 instanceId, [MarshalAs(UnmanagedType.BStr)] String contentLocation);

            [DllImport(Wrapper.ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "MediaPlayerPlay")]
            internal static extern Int32 Play(Int32 instanceId);

            [DllImport(Wrapper.ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "MediaPlayerPause")]
            internal static extern Int32 Pause(Int32 instanceId);

            [DllImport(Wrapper.ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "MediaPlayerStop")]
            internal static extern Int32 Stop(Int32 instanceId);
        }
    }
}