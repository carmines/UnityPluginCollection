// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using UnityEngine;

namespace CameraCapture
{
    internal static class Wrapper
    {
        internal const string ModuleName = "CameraCapture";

        internal const Int32 InvalidHandle = 0x0bad;

        internal enum CallbackType : Int32
        {
            None = 0,
            Failed,
            Capture,
        };

        internal enum CaptureStateType : Int32
        {
            None = 0,
            PreviewStarted,
            PreviewStopped,
            PreviewAudioFrame,
            PreviewVideoFrame,
            PhotoFrame,
        };

        [StructLayout(LayoutKind.Sequential)]
        internal struct FailedState
        {
            public Int32 HResult;

            public override string ToString()
            {
                StringBuilder sb = new StringBuilder();
                sb.AppendLine("HResult: 0x" + HResult.ToString("X", System.Globalization.NumberFormatInfo.InvariantInfo));
                return sb.ToString();
            }
        };

        [StructLayout(LayoutKind.Sequential)]
        internal struct CaptureState
        {
            public CaptureStateType stateType;
            public Int32 width;
            public Int32 height;
            public IntPtr imgTexture;
            public SpatialTranformHelper.Matrix4x4 cameraWorld;
            public SpatialTranformHelper.Matrix4x4 cameraProjection;

            public override string ToString()
            {
                StringBuilder sb = new StringBuilder();
                sb.AppendLine("state: " + stateType);
                sb.AppendLine("width: " + width);
                sb.AppendLine("height: " + height);
                sb.AppendLine("imgTexture: " + imgTexture);
                return sb.ToString();
            }
        }

        [StructLayout(LayoutKind.Explicit, Pack = 4)]
        internal struct CallbackState
        {
            [FieldOffset(0)]
            public CallbackType Type;

            [FieldOffset(4)]
            public FailedState FailState;

            [FieldOffset(4)]
            public CaptureState CaptureState;
        };

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        internal delegate void StateChangedCallback(IntPtr senderPtr, CallbackState args);

        [DllImport(ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "GetRenderEventFunc")]
        internal static extern IntPtr GetRenderEventFunc();

        [DllImport(ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "ReleaseInstance")]
        internal static extern void ReleaseInstance(Int32 instanceId);

        [DllImport(ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "CreateCapture")]
        internal static extern Int32 CreateCapture([MarshalAs(UnmanagedType.FunctionPtr)]Wrapper.StateChangedCallback callback, IntPtr objectPtr, out Int32 instanceId);
    }

    internal abstract class BasePlugin<T> : MonoBehaviour where T : BasePlugin<T>
    {
        // TODO: il2cpp doesn't support generics for static method callback
        [AOT.MonoPInvokeCallback(typeof(Wrapper.StateChangedCallback))]
        internal static void PInvokeCallbackHandler(IntPtr senderPtr, Wrapper.CallbackState args)
        {
            if (senderPtr == IntPtr.Zero)
            {
                Debug.LogError("OnCallback: senderPtr is null.");

                return;
            }

            GCHandle handle = default(GCHandle);
            try
            {
                handle = GCHandle.FromIntPtr(senderPtr);
            }
            catch (Exception ex)
            {
                Debug.LogError("OnCallback: " + ex.Message);

                return;
            }

            // check if this is of correct type
            if (handle.Target is CameraCapture)
            {
                var thisObject = (CameraCapture)handle.Target;
                thisObject.CompleteCallback(args);
            }
            //else if (handle.Target is OtherClass)
            //{
            //    var thisObject = (OtherClass)handle.Target;
            //    thisObject.CompleteCallback(args);
            //}
            else
            {
                Debug.LogError("OnCallback: senderPtr is not null, but not of correct type.");

                return;
            }
        }

        // callback handler for derived class
        protected abstract void OnCallback(Wrapper.CallbackType type, Wrapper.CallbackState args);

        // instance returned from plugin
        protected Int32 instanceId = Wrapper.InvalidHandle;

        // current frame index
        protected UInt16 currentFrameIndex = 0;

        // delegate function used for callback, uses CallbackWrapper static method
        protected Wrapper.StateChangedCallback stateChangedCallback
            = new Wrapper.StateChangedCallback(PInvokeCallbackHandler);

        // pin GC memory location for the object
        protected GCHandle thisObject = default(GCHandle);

        // pointer of the render function in plugin
        private IntPtr renderFuncPtr = IntPtr.Zero;

        // async queue
        private IEnumerator coroutine = null;
        private IEnumerator callbacksCoroutine = null;
        public bool oneCallbackPerFrame = true;
        private readonly object eventLock = new object();
        private readonly List<Action> callbacks = new List<Action>();
        private readonly List<Action> callbacksToProcess = new List<Action>();

        protected virtual void Awake()
        {
            // pin this object in the GC
            thisObject = GCHandle.Alloc(this, GCHandleType.Normal);

            renderFuncPtr = Wrapper.GetRenderEventFunc();
        }

        protected virtual void OnDestroy()
        {
            renderFuncPtr = IntPtr.Zero;

            if (thisObject.IsAllocated)
            {
                thisObject.Free();
                thisObject = default(GCHandle);
            }
        }

        protected virtual void OnEnable()
        {
            callbacksCoroutine = ProcessCallbacks(null); // yield return null happens after Update()
            StartCoroutine(callbacksCoroutine);

            coroutine = CallPluginAtEndOfFrames();
            StartCoroutine(coroutine);
        }

        protected virtual void OnDisable()
        {
            lock (eventLock)
            {
                callbacks.Clear();
            }

            if (callbacksCoroutine != null)
            {
                StopCoroutine(callbacksCoroutine);

                callbacksCoroutine = null;
            }

            if (coroutine != null)
            {
                StopCoroutine(coroutine);

                coroutine = null;
            }

            if (instanceId != Wrapper.InvalidHandle)
            {
                Wrapper.ReleaseInstance(instanceId);

                instanceId = Wrapper.InvalidHandle;
            }
        }

        protected IEnumerator CallPluginAtEndOfFrames()
        {
            while (coroutine != null)
            {
                // Wait until all frame rendering is done
                // the update callback will be on this thread, which is not the Unity Main thread
                yield return new WaitForEndOfFrame();

                if (instanceId != Wrapper.InvalidHandle && renderFuncPtr != IntPtr.Zero)
                {
                    // hi - lastFrameIndex / low - instanceId
                    int packedValue = ((0xffff & currentFrameIndex) << 16) | (0xffff & instanceId);

                    GL.IssuePluginEvent(renderFuncPtr, packedValue);
                }
            }

            yield return null;
        }

        protected virtual void OnFailed(Wrapper.FailedState args)
        {
            // failed could be called on a non-ui thread, see OnUpdate
            // TODO: queue up special actions that can be processed on the next Update pass
            Debug.LogError(args);
        }

        internal static Int32 CheckHR(Int32 hresult)
        {
            if (hresult != 0)
            {
                Debug.LogError("Failed: HRESULT = 0x" + hresult.ToString("X", System.Globalization.NumberFormatInfo.InvariantInfo));
            }

            return hresult;
        }

        internal void CompleteCallback(Wrapper.CallbackState args)
        {
#if UNITY_WSA_10_0
            if (!UnityEngine.WSA.Application.RunningOnAppThread())
            {
                UnityEngine.WSA.Application.InvokeOnAppThread(() =>
                {
                    OnStateChanged(args);
                }, false);
            }
            else
            {
                OnStateChanged(args);
            }
#else
            // there is still a chance the callback is on a non AppThread(callbacks genereated from WaitForEndOfFrame are not)
            // this will process the callback on AppThread on a FixedUpdate
            OnStateChanged(args);
#endif
        }

        internal void OnStateChanged(Wrapper.CallbackState args)
        {
            // TODO: validate callback is on the right thread
            // if not queue callback action(QueueCallback(() => { OnStateChanged(args); });)
            QueueCallback(() =>
            {
                switch (args.Type)
                {
                    case Wrapper.CallbackType.Failed:
                        OnFailed(args.FailState);
                        break;
                    default:
                        OnCallback(args.Type, args);
                        break;
                }
            });
        }

        internal void QueueCallback(Action action)
        {
            if (action == null)
            {
                return;
            }

            lock (eventLock)
            {
                callbacks.Add(action);
            }
        }

        protected IEnumerator ProcessCallbacks(YieldInstruction yieldInstruction)
        {
            while (callbacksCoroutine != null)
            {
                yield return new WaitUntil(() => callbacks.Count > 0);

                lock (eventLock)
                {
                    for (int i = 0; i < callbacks.Count; ++i)
                    {
                        callbacksToProcess.Add(callbacks[i]);
                    }

                    callbacks.Clear();
                }

                for (int i = 0; i < callbacksToProcess.Count; ++i)
                {
                    if (oneCallbackPerFrame)
                    {
                        yield return yieldInstruction;
                    }

                    var action = callbacksToProcess[i];
                    try
                    {
                        action();
                    }
                    catch (Exception ex)
                    {
                        Debug.LogWarning("Action not able to execute: " + ex);
                    }
                }

                callbacksToProcess.Clear();
            }

            yield return null;
        }
    }
}
