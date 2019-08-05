using System;
using System.Runtime.InteropServices;
using UnityEngine;

namespace CameraCapture
{
    /// <summary>
    /// Generic wrapper for handling callbacks for the native plugin
    /// </summary>
    /// <typeparam name="T">base class of the plugin</typeparam>
    internal static class CallbackWrapper//<T> where T : BasePlugin<T> 
    {
        // TODO: il2cpp doesn't support generics for static method callback
        [AOT.MonoPInvokeCallback(typeof(Wrapper.StateChangedCallback))]
        internal static void OnCallback(IntPtr senderPtr, Wrapper.CallbackState args)
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
            //if (!(handle.Target is T))
            if (!(handle.Target is CameraCapture))
            {
                Debug.LogError("OnCallback: senderPtr is not null, but not of correct type.");

                return;
            }

            // cast to type
            //var thisObject = (T)handle.Target;
            var thisObject = (CameraCapture)handle.Target;

            // complete callback
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
}
