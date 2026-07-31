#include "o2/stdafx.h"
#include "TokenDelivery/TiltShiftPass.h"

#include "o2/Render/Render.h"
#include "o2/Render/Shader.h"

namespace
{
#if defined(PLATFORM_MAC) || defined(PLATFORM_IOS)
	const char* kVertexSource = R"(
vertex O2RasterizerData vertexShader(uint vertexID [[vertex_id]],
                                     constant O2VertexIn* vertices [[buffer(0)]],
                                     constant O2Uniforms& uniforms [[buffer(1)]])
{
    O2VertexIn inputVertex = vertices[vertexID];

    O2RasterizerData output;
    output.position = uniforms.mvpMatrix * float4(inputVertex.x, inputVertex.y, inputVertex.z, 1.0);
    output.color = o2_unpackColor(inputVertex.color);
    output.texCoords = inputVertex.texCoord0;
    output.texCoords2 = inputVertex.texCoord1;
    output.texCoords3 = inputVertex.texCoord2;
    output.normal = inputVertex.normal;
    return output;
})";

	const char* kFragmentSource = R"(
struct O2TiltShiftParams
{
    float u_texelX;
    float u_texelY;
    float u_band;
    float u_blur;
};

fragment float4 fragmentShader(O2RasterizerData input [[stage_in]],
                               texture2d<float> u_texture [[texture(0)]],
                               sampler textureSampler [[sampler(0)]],
                               constant O2TiltShiftParams& params [[buffer(2)]])
{
    float2 uv = input.texCoords;
    float dist = abs(uv.y - 0.5)*2.0;
    float strength = smoothstep(params.u_band, 1.0, dist);
    float4 center = u_texture.sample(textureSampler, uv);
    if (strength < 0.01)
        return center*input.color;

    float r = params.u_blur*strength;
    float2 texel = float2(params.u_texelX, params.u_texelY);
    float4 acc = center*2.0;
    float2 dirs[8] = { float2(0.0, 1.0), float2(0.0, -1.0), float2(1.0, 0.0), float2(-1.0, 0.0),
                       float2(0.707, 0.707), float2(0.707, -0.707),
                       float2(-0.707, 0.707), float2(-0.707, -0.707) };
    for (int i = 0; i < 8; i++)
    {
        acc += u_texture.sample(textureSampler, uv + dirs[i]*texel*r);
        acc += u_texture.sample(textureSampler, uv + dirs[i]*texel*r*0.5);
    }
    return (acc/18.0)*input.color;
})";
#else
	const char* kVertexSource = R"(
uniform mat4 u_transformMatrix;

attribute vec4 a_position;
attribute vec4 a_color;
attribute vec2 a_texCoords;

varying vec4 v_color;
varying vec2 v_texCoords;

void main()
{
    v_color = a_color;
    v_texCoords = a_texCoords;
    gl_Position = u_transformMatrix * a_position;
})";

	const char* kFragmentSource = R"(
varying vec4 v_color;
varying vec2 v_texCoords;

uniform sampler2D u_texture;
uniform float u_texelX;
uniform float u_texelY;
uniform float u_band;
uniform float u_blur;

void main()
{
    vec2 uv = v_texCoords;
    float dist = abs(uv.y - 0.5)*2.0;
    float strength = smoothstep(u_band, 1.0, dist);
    vec4 center = texture2D(u_texture, uv);
    if (strength < 0.01)
    {
        gl_FragColor = center*v_color;
        return;
    }

    float r = u_blur*strength;
    vec2 texel = vec2(u_texelX, u_texelY);
    vec4 acc = center*2.0;
    vec2 dirs[8];
    dirs[0] = vec2(0.0, 1.0);   dirs[1] = vec2(0.0, -1.0);
    dirs[2] = vec2(1.0, 0.0);   dirs[3] = vec2(-1.0, 0.0);
    dirs[4] = vec2(0.707, 0.707);  dirs[5] = vec2(0.707, -0.707);
    dirs[6] = vec2(-0.707, 0.707); dirs[7] = vec2(-0.707, -0.707);
    for (int i = 0; i < 8; i++)
    {
        acc += texture2D(u_texture, uv + dirs[i]*texel*r);
        acc += texture2D(u_texture, uv + dirs[i]*texel*r*0.5);
    }
    gl_FragColor = (acc/18.0)*v_color;
})";
#endif
}

namespace td
{
bool TiltShiftPass::EnsureResources()
{
	Vec2I resolution = o2Render.GetResolution();
	if (resolution.x <= 0 || resolution.y <= 0)
		return false;

	if (mTarget && mTargetSize == resolution && mMaterial)
		return true;

	mTargetSize = resolution;
	mTarget = TextureRef(resolution, TextureFormat::R8G8B8A8, Texture::Usage::RenderTarget);

	auto vertexShader = mmake<Shader>();
	auto fragmentShader = mmake<Shader>();
	if (!vertexShader->Compile(String(kVertexSource), Shader::Type::Vertex) ||
		!fragmentShader->Compile(String(kFragmentSource), Shader::Type::Fragment))
	{
		mMaterial = nullptr;
		return false;
	}

	mMaterial = mmake<Material>();
	mMaterial->SetVertexShader(vertexShader);
	mMaterial->SetFragmentShader(fragmentShader);
	mMaterial->AddParam(mmake<ShaderParamFloat>("u_texelX", 1.0f/(float)resolution.x));
	mMaterial->AddParam(mmake<ShaderParamFloat>("u_texelY", 1.0f/(float)resolution.y));
	mMaterial->AddParam(mmake<ShaderParamFloat>("u_band", focusBand));
	mMaterial->AddParam(mmake<ShaderParamFloat>("u_blur", blurRadius));

	if (!mScreenQuad)
		mScreenQuad = mmake<Sprite>();
	mScreenQuad->SetTexture(mTarget);
	mScreenQuad->SetTextureSrcRect(RectI(0, 0, resolution.x, resolution.y));
	mScreenQuad->SetMaterial(mMaterial);
	return true;
}

void TiltShiftPass::Execute(RenderPassContext& context)
{
	if (!EnsureResources())
	{
		Scene2DPass::Execute(context);
		return;
	}

	auto prevTarget = o2Render.GetRenderTexture();
	o2Render.BindRenderTexture(mTarget);
	o2Render.Clear(context.fillColor);
	o2Render.SetCamera(context.camera);

	Scene2DPass::Execute(context);

	if (prevTarget)
		o2Render.BindRenderTexture(prevTarget);
	else
		o2Render.UnbindRenderTexture();
	o2Render.SetCamera(context.camera);

	mScreenQuad->SetSize(context.camera.GetSize());
	mScreenQuad->SetPosition(Vec2F(context.camera.GetPosition().x, context.camera.GetPosition().y));
	mScreenQuad->Draw();
}
}
// --- META ---

DECLARE_CLASS(td::TiltShiftPass, td__TiltShiftPass);
// --- END META ---
