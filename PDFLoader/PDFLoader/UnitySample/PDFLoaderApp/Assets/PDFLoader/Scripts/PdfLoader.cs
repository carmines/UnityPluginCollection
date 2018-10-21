using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.UI;

namespace PDFLoader
{
    [RequireComponent(typeof(Canvas), typeof(CanvasScaler))]
    internal class PdfLoader : BasePlugin<PdfLoader>
    {
        public string RootFolder = null;
        public string FileName = null;

        public bool LoadComplete
        {
            get; private set;
        }

        [SerializeField]
        private Canvas canvas = null;

        [SerializeField]
        private Image image = null;

        [SerializeField]
        private Text pageinfo = null;

        private UInt32 currentPage = 0;
        private UInt32 pageCount = 0;
        private Texture2D pageTexture;

        private Vector3 defaultScale = Vector3.one;

        protected override void Awake()
        {
            base.Awake();

            defaultScale = transform.localScale;

            if (canvas == null)
            {
                canvas = GetComponent<Canvas>();
            }

            if (image == null)
            {
                image = canvas.gameObject.GetComponentInChildren<Image>();
            }

#if UNITY_EDITOR || UNITY_STANDALONE
            if (RootFolder != null && !System.IO.Path.IsPathRooted(RootFolder))
            {
                RootFolder = (Application.dataPath + "/" + RootFolder + "/");
            }
#endif
            RootFolder = RootFolder.Replace("/", "\\");

        }

        protected override void OnEnable()
        {
            base.OnEnable();

            CreatePdf();

            LoadPDF(FileName);
        }

        protected override void OnDisable()
        {
            base.OnDisable();

        }

        private void Update()
        {
            if (Input.GetKeyDown(KeyCode.RightArrow))
            {
                NextPage();
            }
            else if (Input.GetKeyDown(KeyCode.LeftArrow))
            {
                PreviousPage();
            }
        }

        protected override void OnCallback(Wrapper.CallbackType type, Wrapper.CallbackState args)
        {
            if (type == Wrapper.CallbackType.Pdf)
            {
                switch(args.PdfState.Type)
                {
                    case Wrapper.PdfStateType.Loading:
                    case Wrapper.PdfStateType.Loaded:
                    case Wrapper.PdfStateType.Opened:
                        OnLoaded(args.PdfState);
                        break;
                    case Wrapper.PdfStateType.Selected:
                        OnPageSelected(args.PdfState);
                        break;

                }
            }
        }

        private void OnLoaded(Wrapper.PdfState state)
        {
            LoadComplete = (state.Type == Wrapper.PdfStateType.Opened);

            if (LoadComplete)
            {
                CheckHR(Native.GetPageCount(instanceId, ref pageCount));

                if (pageCount > 0)
                {
                    CheckHR(Native.SelectPage(instanceId, 0));
                }
            }
        }

        private void OnPageSelected(Wrapper.PdfState pageState)
        {
            if (pageState.Width > 0 && pageState.Height > 0 && pageState.TexturePtr != IntPtr.Zero)
            {
                currentPage = pageState.PageNumber;
                pageTexture = Texture2D.CreateExternalTexture(pageState.Width, pageState.Height, TextureFormat.BGRA32, false, false, pageState.TexturePtr);
                pageTexture.filterMode = FilterMode.Bilinear;
                pageTexture.wrapMode = TextureWrapMode.Clamp;
            }

            if (canvas == null)
            {
                return;
            }

            Vector3 scale = defaultScale;
            if (pageTexture.height > pageTexture.width)
            {
                scale.x *= (float)pageTexture.width / pageTexture.height;
                scale.y *= 1.0f;
            }
            else
            {
                scale.x *= 1.0f;
                scale.x *= (float)pageTexture.height / pageTexture.width;
            }

            var trans = canvas.GetComponent<RectTransform>();
            if (trans != null)
            {
                trans.transform.localScale = scale;
            }

            if (image != null)
            {
                Sprite sprite = Sprite.Create(pageTexture, new Rect(0, 0, pageTexture.width, pageTexture.height), new Vector2(0.5f, 0.0f), 1.0f);
                image.sprite = sprite;
            }

            if (pageinfo != null)
            {
                pageinfo.text = currentPage + " / " + (pageCount - 1);
            }
        }

        public void LoadPDF(string filename)
        {
            CheckHR(Native.LoadFile(instanceId, RootFolder, filename));
        }

        internal void GetPage(Int32 newPageIndex)
        {
            if (newPageIndex == currentPage)
                return;

            if (newPageIndex < 0)
            {
                newPageIndex = (Int32)(pageCount - 1);
            }
            else if (newPageIndex >= pageCount)
            {
                newPageIndex = 0;
            }

            CheckHR(Native.SelectPage(instanceId, (UInt32)newPageIndex));
        }

        public void FirstPage()
        {
            GetPage(0);
        }

        public void PreviousPage()
        {
            GetPage((Int32)currentPage - 1);
        }

        public void NextPage()
        {
            GetPage((Int32)currentPage + 1);
        }

        public void LastPage()
        {
            GetPage((Int32)pageCount - 1);
        }

        private static class Native
        {
            [DllImport(Wrapper.ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "LoadFile")]
            public static extern Int32 LoadFile(Int32 handle, [MarshalAs(UnmanagedType.BStr)] string baseFolder, [MarshalAs(UnmanagedType.BStr)] string fileName);

            [DllImport(Wrapper.ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "GetPageCount")]
            public static extern Int32 GetPageCount(Int32 handle, ref UInt32 pageCount);

            [DllImport(Wrapper.ModuleName, CallingConvention = CallingConvention.StdCall, EntryPoint = "SelectPage")]
            public static extern Int32 SelectPage(Int32 handle, UInt32 pageIndex);
        }
    }
}
