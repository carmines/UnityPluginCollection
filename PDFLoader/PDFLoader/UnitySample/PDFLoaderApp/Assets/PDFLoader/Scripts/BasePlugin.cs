// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using UnityEngine;

namespace PDFLoader
{
    internal static class Wrapper
    {
        internal const string ModuleName = "PDFLoader";

        internal const Int32 InvalidHandle = 0x0bad;

        internal enum CallbackType : Int32
        {
            None = 0,
            Failed,
            Pdf
        };

        internal enum PdfStateType : Int32
        {
            Loading = 0,
            Loaded,
            Opened,
            Selected
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
        internal struct PdfState
        {
            public PdfStateType Type;
            public UInt32 PageNumber;
            public Int32 Width;
            public Int32 Height;
            public IntPtr TexturePtr;
        }

        [StructLayout(LayoutKind.Explicit, Pack = 4)]
        internal struct CallbackState
        {
            [FieldOffset(0)]
            public CallbackType Type;

            [FieldOffset(4)]
            public FailedState FailState;

            [FieldOffset(4)]
            public PdfState PdfState;
        };

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        internal delegate void StateChangedCallback(CallbackState args);

        [DllImport(ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "GetRenderEventFunc")]
        internal static extern IntPtr GetRenderEventFunc();

        [DllImport(ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "ReleaseInstance")]
        internal static extern void ReleaseInstance(Int32 instanceId);

        [DllImport(ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "CreatePdf")]
        internal static extern Int32 CreatePdf([MarshalAs(UnmanagedType.FunctionPtr)]Wrapper.StateChangedCallback callback, out Int32 instanceId);
    }

    internal abstract class BasePlugin<T> : MonoBehaviour where T : BasePlugin<T>
    {

        protected void CreatePdf()
        {
            CheckHR(Wrapper.CreatePdf(stateChangedCallback, out instanceId));
        }

        protected abstract void OnCallback(Wrapper.CallbackType type, Wrapper.CallbackState args);

        protected Int32 instanceId = Wrapper.InvalidHandle;

        protected UInt16 currentFrameIndex = 0;

        private IntPtr renderFuncPtr = IntPtr.Zero;
        private Wrapper.StateChangedCallback stateChangedCallback = null;
        private GCHandle callbackHandle = default(GCHandle);

        // async queue
        private IEnumerator coroutine = null;
        private IEnumerator callbacksCoroutine = null;
        public bool oneCallbackPerFrame = true;
        private readonly object eventLock = new object();
        private readonly List<Action> callbacks = new List<Action>();
        private readonly List<Action> callbacksToProcess = new List<Action>();

        protected void QueueCallback(Action action)
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

        protected virtual void Awake()
        {
            // pin callback
            stateChangedCallback = (args) =>
            {
#if UNITY_WSA_10_0
                if (!UnityEngine.WSA.Application.RunningOnAppThread())
                {
                    UnityEngine.WSA.Application.InvokeOnAppThread(() =>
                    {
                        QueueCallback(() => { OnStateChanged(args); });
                    }, false);
                }
                else
                {
                    QueueCallback(() => { OnStateChanged(args); });
                }
#else
                // there is still a chance the callback is on a non AppThread(callbacks genereated from WaitForEndOfFrame are not)
                // this will process the callback on AppThread on a FixedUpdate
                QueueCallback(() => { OnStateChanged(args); });
#endif
            };

            renderFuncPtr = Wrapper.GetRenderEventFunc();
            callbackHandle = GCHandle.Alloc(stateChangedCallback);
        }

        protected virtual void OnDestroy()
        {
            renderFuncPtr = IntPtr.Zero;

            if (callbackHandle.IsAllocated)
            {
                callbackHandle.Free();
                callbackHandle = default(GCHandle);
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

        private void OnStateChanged(Wrapper.CallbackState args)
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
        }

        private void OnFailed(Wrapper.FailedState args)
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
    }
}
