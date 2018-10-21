Shader "Custom/VertexColor"
{
    Properties
    {
        _Brightness("Brightness", Range(0, 1)) = 1.0
    }

    SubShader
    {
        Tags { "RenderType" = "Opaque" }
        LOD 100

        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            // make fog work
            #pragma multi_compile_fog

            #include "UnityCG.cginc"

            struct appdata
            {
                float3 vertex : POSITION;
                float4 color : COLOR;
            };

            struct v2f
            {
                float4 color : COLOR;
                UNITY_FOG_COORDS(1)
                float4 vertex : SV_POSITION;
            };

            float _Brightness;

            v2f vert(appdata v)
            {
                v2f o;
                o.vertex = UnityObjectToClipPos(v.vertex);
                o.color = v.color;
                UNITY_TRANSFER_FOG(o,o.vertex);
                return o;
            }

            fixed4 frag(v2f i) : SV_Target
            {
                fixed4 col = i.color;

                // apply fog
                UNITY_APPLY_FOG(i.fogCoord, col);

                return col * _Brightness;
            }
        ENDCG
    }
    }
}
