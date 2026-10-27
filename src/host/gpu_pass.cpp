#include "gpu_pass.hpp"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <map>

namespace host {

using Microsoft::WRL::ComPtr;

struct GpuPasses::Impl {
  SDL_Renderer* renderer = nullptr;
  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<ID3D11VertexShader> vertex;
  ComPtr<ID3D11RasterizerState> rasterizer;
  ComPtr<ID3D11BlendState> blend;
  ComPtr<ID3D11DepthStencilState> depth;
  ComPtr<ID3D11SamplerState> point, linear;
  ComPtr<ID3D11Buffer> constants;
  std::vector<ComPtr<ID3D11PixelShader>> pixel;
  std::map<ID3D11Texture2D*, ComPtr<ID3D11ShaderResourceView>> views;     // one per texture, kept while the texture lives
  std::map<ID3D11Texture2D*, ComPtr<ID3D11RenderTargetView>> targets;
};

GpuPasses::GpuPasses() : impl_(std::make_unique<Impl>()) {}
GpuPasses::~GpuPasses() { close(); }

bool GpuPasses::isOpen() const { return impl_->device != nullptr; }

void GpuPasses::close()
{
  Impl& m = *impl_;
  if(m.context) m.context->ClearState();
  m.views.clear();
  m.targets.clear();
  m.pixel.clear();
  m.constants.Reset(); m.point.Reset(); m.linear.Reset(); m.depth.Reset(); m.blend.Reset(); m.rasterizer.Reset(); m.vertex.Reset();
  m.context.Reset();
  m.device.Reset();
  m.renderer = nullptr;
}

namespace {
// The pieces of Direct3D state a pass changes: taken before, put back after, so that SDL goes on exactly as if nothing had happened.
struct SavedState {
  ComPtr<ID3D11VertexShader> vertex;
  ComPtr<ID3D11PixelShader> pixel;
  ComPtr<ID3D11InputLayout> layout;
  D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
  ComPtr<ID3D11RasterizerState> rasterizer;
  ComPtr<ID3D11BlendState> blend;
  float blendFactor[4] = {0, 0, 0, 0};
  UINT sampleMask = 0xFFFFFFFF;
  ComPtr<ID3D11DepthStencilState> depth;
  UINT stencilRef = 0;
  ComPtr<ID3D11SamplerState> samplers[2];
  ComPtr<ID3D11Buffer> buffer;
  ComPtr<ID3D11ShaderResourceView> view;
  ComPtr<ID3D11RenderTargetView> target;
  ComPtr<ID3D11DepthStencilView> depthView;
  D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
  UINT viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;

  void take(ID3D11DeviceContext* c)
  {
    c->VSGetShader(&vertex, nullptr, nullptr);
    c->PSGetShader(&pixel, nullptr, nullptr);
    c->IAGetInputLayout(&layout);
    c->IAGetPrimitiveTopology(&topology);
    c->RSGetState(&rasterizer);
    c->OMGetBlendState(&blend, blendFactor, &sampleMask);
    c->OMGetDepthStencilState(&depth, &stencilRef);
    ID3D11SamplerState* s[2] = {};
    c->PSGetSamplers(0, 2, s);
    samplers[0].Attach(s[0]);
    samplers[1].Attach(s[1]);
    c->PSGetConstantBuffers(0, 1, &buffer);
    c->PSGetShaderResources(0, 1, &view);
    c->OMGetRenderTargets(1, &target, &depthView);
    c->RSGetViewports(&viewportCount, viewports);
  }
  void give(ID3D11DeviceContext* c)
  {
    c->VSSetShader(vertex.Get(), nullptr, 0);
    c->PSSetShader(pixel.Get(), nullptr, 0);
    c->IASetInputLayout(layout.Get());
    c->IASetPrimitiveTopology(topology);
    c->RSSetState(rasterizer.Get());
    c->OMSetBlendState(blend.Get(), blendFactor, sampleMask);
    c->OMSetDepthStencilState(depth.Get(), stencilRef);
    ID3D11SamplerState* s[2] = {samplers[0].Get(), samplers[1].Get()};
    c->PSSetSamplers(0, 2, s);
    ID3D11Buffer* b[1] = {buffer.Get()};
    c->PSSetConstantBuffers(0, 1, b);
    ID3D11ShaderResourceView* v[1] = {view.Get()};
    c->PSSetShaderResources(0, 1, v);
    ID3D11RenderTargetView* t[1] = {target.Get()};
    c->OMSetRenderTargets(1, t, depthView.Get());
    c->RSSetViewports(viewportCount, viewports);
  }
};

const char* kVertexShader =
    "struct Out { float4 position : SV_Position; float2 uv : TEXCOORD0; };\n"
    "Out main(uint id : SV_VertexID) {\n"
    "  Out o;\n"
    "  float2 p = float2((id << 1) & 2, id & 2);\n"       // a triangle that covers the whole target: (0,0) (2,0) (0,2)
    "  o.uv = p;\n"
    "  o.position = float4(p * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);\n"
    "  return o;\n"
    "}\n";
}

bool GpuPasses::open(SDL_Renderer* renderer)
{
  close();
  Impl& m = *impl_;
  const SDL_PropertiesID props = SDL_GetRendererProperties(renderer);
  auto* device = static_cast<ID3D11Device*>(SDL_GetPointerProperty(props, SDL_PROP_RENDERER_D3D11_DEVICE_POINTER, nullptr));
  if(!device) return false;
  m.renderer = renderer;
  m.device = device;
  m.device->GetImmediateContext(&m.context);
  ComPtr<ID3DBlob> code, errors;
  if(FAILED(D3DCompile(kVertexShader, std::strlen(kVertexShader), "vertex", nullptr, nullptr, "main", "vs_5_0", 0, 0, &code, &errors))) { close(); return false; }
  if(FAILED(m.device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &m.vertex))) { close(); return false; }
  D3D11_RASTERIZER_DESC rs{};
  rs.FillMode = D3D11_FILL_SOLID; rs.CullMode = D3D11_CULL_NONE; rs.DepthClipEnable = TRUE;
  D3D11_BLEND_DESC bs{};
  bs.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  D3D11_DEPTH_STENCIL_DESC ds{};
  D3D11_SAMPLER_DESC sd{};
  sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
  sd.MaxLOD = D3D11_FLOAT32_MAX;
  sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
  const bool ok = SUCCEEDED(m.device->CreateRasterizerState(&rs, &m.rasterizer)) && SUCCEEDED(m.device->CreateBlendState(&bs, &m.blend)) &&
                  SUCCEEDED(m.device->CreateDepthStencilState(&ds, &m.depth)) && SUCCEEDED(m.device->CreateSamplerState(&sd, &m.point)) &&
                  (sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR, SUCCEEDED(m.device->CreateSamplerState(&sd, &m.linear)));
  if(!ok) { close(); return false; }
  D3D11_BUFFER_DESC bd{};
  bd.ByteWidth = 4096;
  bd.Usage = D3D11_USAGE_DYNAMIC;
  bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  if(FAILED(m.device->CreateBuffer(&bd, nullptr, &m.constants))) { close(); return false; }
  return true;
}

bool GpuPasses::readsEightBitAsLinear() const
{
  return impl_->renderer && SDL_GetNumberProperty(SDL_GetRendererProperties(impl_->renderer), SDL_PROP_RENDERER_OUTPUT_COLORSPACE_NUMBER, 0) == SDL_COLORSPACE_SRGB_LINEAR;
}

int GpuPasses::compile(const std::string& hlsl, std::string& error)
{
  Impl& m = *impl_;
  if(!m.device) { error = "not open"; return -1; }
  ComPtr<ID3DBlob> code, errors;
  if(FAILED(D3DCompile(hlsl.data(), hlsl.size(), "pass", nullptr, nullptr, "main", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors))) {
    error = errors ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize()) : "the shader could not be compiled";
    return -1;
  }
  ComPtr<ID3D11PixelShader> shader;
  if(FAILED(m.device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &shader))) { error = "the shader could not be made"; return -1; }
  m.pixel.push_back(shader);
  return static_cast<int>(m.pixel.size()) - 1;
}

bool GpuPasses::run(int shader, SDL_Texture* input, SDL_Texture* output, const void* constants, int constantBytes, std::string& error)
{
  Impl& m = *impl_;
  if(!m.device || shader < 0 || shader >= static_cast<int>(m.pixel.size())) { error = "no such shader"; return false; }
  auto* in = static_cast<ID3D11Texture2D*>(SDL_GetPointerProperty(SDL_GetTextureProperties(input), SDL_PROP_TEXTURE_D3D11_TEXTURE_POINTER, nullptr));
  auto* out = static_cast<ID3D11Texture2D*>(SDL_GetPointerProperty(SDL_GetTextureProperties(output), SDL_PROP_TEXTURE_D3D11_TEXTURE_POINTER, nullptr));
  if(!in || !out) { error = "the textures are not Direct3D 11 textures"; return false; }
  SDL_FlushRenderer(m.renderer);   // what SDL has queued is done first
  SavedState saved;
  saved.take(m.context.Get());
  auto& view = m.views[in];
  if(!view && FAILED(m.device->CreateShaderResourceView(in, nullptr, &view))) { error = "no view of the input"; m.views.erase(in); return false; }
  auto& target = m.targets[out];
  if(!target && FAILED(m.device->CreateRenderTargetView(out, nullptr, &target))) { error = "no view of the output (is it a render target?)"; m.targets.erase(out); return false; }
  D3D11_TEXTURE2D_DESC od{};
  out->GetDesc(&od);
  if(constants && constantBytes > 0) {
    D3D11_MAPPED_SUBRESOURCE map{};
    if(constantBytes > 4096 || FAILED(m.context->Map(m.constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) { error = "the constants could not be set"; return false; }
    std::memcpy(map.pData, constants, static_cast<std::size_t>(constantBytes));
    m.context->Unmap(m.constants.Get(), 0);
  }
  ID3D11RenderTargetView* targets[1] = {target.Get()};
  m.context->OMSetRenderTargets(1, targets, nullptr);
  const D3D11_VIEWPORT vp{0.0f, 0.0f, static_cast<float>(od.Width), static_cast<float>(od.Height), 0.0f, 1.0f};
  m.context->RSSetViewports(1, &vp);
  m.context->RSSetState(m.rasterizer.Get());
  const float factor[4] = {0, 0, 0, 0};
  m.context->OMSetBlendState(m.blend.Get(), factor, 0xFFFFFFFF);
  m.context->OMSetDepthStencilState(m.depth.Get(), 0);
  m.context->IASetInputLayout(nullptr);
  m.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m.context->VSSetShader(m.vertex.Get(), nullptr, 0);
  m.context->PSSetShader(m.pixel[static_cast<std::size_t>(shader)].Get(), nullptr, 0);
  ID3D11ShaderResourceView* views[1] = {view.Get()};
  m.context->PSSetShaderResources(0, 1, views);
  ID3D11SamplerState* samplers[2] = {m.point.Get(), m.linear.Get()};
  m.context->PSSetSamplers(0, 2, samplers);
  ID3D11Buffer* buffers[1] = {m.constants.Get()};
  m.context->PSSetConstantBuffers(0, 1, buffers);
  m.context->Draw(3, 0);
  ID3D11ShaderResourceView* none[1] = {nullptr};
  m.context->PSSetShaderResources(0, 1, none);   // nothing stays bound that SDL might want to use the other way round
  m.context->OMSetRenderTargets(0, nullptr, nullptr);
  saved.give(m.context.Get());   // SDL's state, as it was
  SDL_FlushRenderer(m.renderer);
  return true;
}

}  // namespace host
