#include "gpu_graph.hpp"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <cstring>

namespace host {

using Microsoft::WRL::ComPtr;

namespace {
constexpr UINT kSlots = 10;   // textures (and samplers) a pass can read; the largest pass reads seven

struct Image {
  ComPtr<ID3D11Texture2D> texture;
  ComPtr<ID3D11ShaderResourceView> view;
  ComPtr<ID3D11RenderTargetView> target;
  int width = 0, height = 0;
  bool used = false;
};

struct Shader {
  ComPtr<ID3D11VertexShader> vertex;
  ComPtr<ID3D11PixelShader> pixel;
};

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
  ComPtr<ID3D11SamplerState> samplers[kSlots];
  ComPtr<ID3D11ShaderResourceView> views[kSlots];
  ComPtr<ID3D11Buffer> pixelBuffers[2], vertexBuffers[2];
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
    ID3D11SamplerState* s[kSlots] = {};
    c->PSGetSamplers(0, kSlots, s);
    ID3D11ShaderResourceView* v[kSlots] = {};
    c->PSGetShaderResources(0, kSlots, v);
    for(UINT i = 0; i < kSlots; ++i) { samplers[i].Attach(s[i]); views[i].Attach(v[i]); }
    ID3D11Buffer* p[2] = {}; ID3D11Buffer* q[2] = {};
    c->PSGetConstantBuffers(0, 2, p);
    c->VSGetConstantBuffers(0, 2, q);
    for(int i = 0; i < 2; ++i) { pixelBuffers[i].Attach(p[i]); vertexBuffers[i].Attach(q[i]); }
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
    ID3D11SamplerState* s[kSlots]; ID3D11ShaderResourceView* v[kSlots];
    for(UINT i = 0; i < kSlots; ++i) { s[i] = samplers[i].Get(); v[i] = views[i].Get(); }
    c->PSSetSamplers(0, kSlots, s);
    c->PSSetShaderResources(0, kSlots, v);
    ID3D11Buffer* p[2] = {pixelBuffers[0].Get(), pixelBuffers[1].Get()};
    ID3D11Buffer* q[2] = {vertexBuffers[0].Get(), vertexBuffers[1].Get()};
    c->PSSetConstantBuffers(0, 2, p);
    c->VSSetConstantBuffers(0, 2, q);
    ID3D11RenderTargetView* t[1] = {target.Get()};
    c->OMSetRenderTargets(1, t, depthView.Get());
    c->RSSetViewports(viewportCount, viewports);
  }
};

const char* kPlainVertex =
    "struct Out { float4 position : SV_Position; float2 uv : TEXCOORD0; };\n"
    "Out main(uint id : SV_VertexID) {\n"
    "  Out o;\n"
    "  float2 p = float2((id << 1) & 2, id & 2);\n"       // a triangle that covers the whole target: (0,0) (2,0) (0,2)
    "  o.uv = p;\n"
    "  o.position = float4(p * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);\n"
    "  return o;\n"
    "}\n";

std::string compileError(ID3DBlob* errors) { return errors ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize()) : "the shader could not be compiled"; }
}  // namespace

struct GpuGraph::Impl {
  SDL_Renderer* renderer = nullptr;
  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<ID3D11RasterizerState> rasterizer;
  ComPtr<ID3D11BlendState> blend;
  ComPtr<ID3D11DepthStencilState> depth;
  ComPtr<ID3D11SamplerState> sampler[2];   // nearest, linear (both read black outside the image)
  ComPtr<ID3D11Buffer> buffer[2];
  std::vector<Image> images;
  std::vector<Shader> shaders;

  int add(Image&& image)
  {
    for(std::size_t i = 0; i < images.size(); ++i)
      if(!images[i].used) { images[i] = std::move(image); images[i].used = true; return static_cast<int>(i); }
    images.push_back(std::move(image));
    images.back().used = true;
    return static_cast<int>(images.size()) - 1;
  }
  Image* get(int id) { return id >= 0 && id < static_cast<int>(images.size()) && images[static_cast<std::size_t>(id)].used ? &images[static_cast<std::size_t>(id)] : nullptr; }
};

GpuGraph::GpuGraph() : impl_(std::make_unique<Impl>()) {}
GpuGraph::~GpuGraph() { close(); }
bool GpuGraph::isOpen() const { return impl_->device != nullptr; }

void GpuGraph::close()
{
  Impl& m = *impl_;
  if(m.context) m.context->ClearState();
  m.images.clear();
  m.shaders.clear();
  for(auto& b : m.buffer) b.Reset();
  for(auto& s : m.sampler) s.Reset();
  m.depth.Reset(); m.blend.Reset(); m.rasterizer.Reset(); m.context.Reset(); m.device.Reset();
  m.renderer = nullptr;
}

bool GpuGraph::open(SDL_Renderer* renderer)
{
  close();
  Impl& m = *impl_;
  auto* device = static_cast<ID3D11Device*>(SDL_GetPointerProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_D3D11_DEVICE_POINTER, nullptr));
  if(!device) return false;
  m.renderer = renderer;
  m.device = device;
  m.device->GetImmediateContext(&m.context);
  D3D11_RASTERIZER_DESC rs{};
  rs.FillMode = D3D11_FILL_SOLID; rs.CullMode = D3D11_CULL_NONE; rs.DepthClipEnable = TRUE;
  D3D11_BLEND_DESC bs{};
  bs.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  D3D11_DEPTH_STENCIL_DESC ds{};
  D3D11_SAMPLER_DESC sd{};
  sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
  sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
  sd.MaxLOD = D3D11_FLOAT32_MAX;
  bool ok = SUCCEEDED(m.device->CreateRasterizerState(&rs, &m.rasterizer)) && SUCCEEDED(m.device->CreateBlendState(&bs, &m.blend)) &&
            SUCCEEDED(m.device->CreateDepthStencilState(&ds, &m.depth));
  sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
  ok = ok && SUCCEEDED(m.device->CreateSamplerState(&sd, &m.sampler[0]));
  sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  ok = ok && SUCCEEDED(m.device->CreateSamplerState(&sd, &m.sampler[1]));
  D3D11_BUFFER_DESC bd{};
  bd.ByteWidth = 4096;
  bd.Usage = D3D11_USAGE_DYNAMIC;
  bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  for(auto& b : m.buffer) ok = ok && SUCCEEDED(m.device->CreateBuffer(&bd, nullptr, &b));
  if(!ok) { close(); return false; }
  return true;
}

int GpuGraph::makeImage(int width, int height, Format format, bool mips)
{
  Impl& m = *impl_;
  if(!m.device || width < 1 || height < 1) return -1;
  D3D11_TEXTURE2D_DESC td{};
  td.Width = static_cast<UINT>(width);
  td.Height = static_cast<UINT>(height);
  td.MipLevels = mips ? 0 : 1;
  td.ArraySize = 1;
  td.Format = format == Format::Float16 ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM;
  td.SampleDesc.Count = 1;
  td.Usage = D3D11_USAGE_DEFAULT;
  td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  td.MiscFlags = mips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
  Image image;
  image.width = width; image.height = height;
  if(FAILED(m.device->CreateTexture2D(&td, nullptr, &image.texture)) || FAILED(m.device->CreateShaderResourceView(image.texture.Get(), nullptr, &image.view)) ||
     FAILED(m.device->CreateRenderTargetView(image.texture.Get(), nullptr, &image.target)))
    return -1;
  return m.add(std::move(image));
}

int GpuGraph::wrapTarget(SDL_Texture* texture)
{
  Impl& m = *impl_;
  auto* d3d = static_cast<ID3D11Texture2D*>(SDL_GetPointerProperty(SDL_GetTextureProperties(texture), SDL_PROP_TEXTURE_D3D11_TEXTURE_POINTER, nullptr));
  if(!m.device || !d3d) return -1;
  Image image;
  image.texture = d3d;
  D3D11_TEXTURE2D_DESC td{};
  d3d->GetDesc(&td);
  image.width = static_cast<int>(td.Width); image.height = static_cast<int>(td.Height);
  if(FAILED(m.device->CreateRenderTargetView(d3d, nullptr, &image.target))) return -1;
  return m.add(std::move(image));
}

void GpuGraph::freeImage(int id)
{
  if(Image* image = impl_->get(id)) *image = Image{};
}

int GpuGraph::width(int id) const { const Image* i = impl_->get(id); return i ? i->width : 0; }
int GpuGraph::height(int id) const { const Image* i = impl_->get(id); return i ? i->height : 0; }

bool GpuGraph::upload(int id, const void* bgra, int pitch)
{
  Impl& m = *impl_;
  Image* image = m.get(id);
  if(!image || !m.context) return false;
  m.context->UpdateSubresource(image->texture.Get(), 0, nullptr, bgra, static_cast<UINT>(pitch), 0);
  return true;
}

int GpuGraph::makeShader(const std::string& vertex, const std::string& pixel, std::string& error)
{
  Impl& m = *impl_;
  if(!m.device) { error = "not open"; return -1; }
  const std::string& vs = vertex.empty() ? std::string(kPlainVertex) : vertex;
  ComPtr<ID3DBlob> vsCode, psCode, errors;
  if(FAILED(D3DCompile(vs.data(), vs.size(), "vertex", nullptr, nullptr, "main", "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL1, 0, &vsCode, &errors))) { error = "vertex shader: " + compileError(errors.Get()); return -1; }
  errors.Reset();
  if(FAILED(D3DCompile(pixel.data(), pixel.size(), "pixel", nullptr, nullptr, "main", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL1, 0, &psCode, &errors))) { error = "pixel shader: " + compileError(errors.Get()); return -1; }
  Shader shader;
  if(FAILED(m.device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, &shader.vertex)) ||
     FAILED(m.device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, &shader.pixel))) { error = "the shaders could not be made"; return -1; }
  m.shaders.push_back(shader);
  return static_cast<int>(m.shaders.size()) - 1;
}

bool GpuGraph::draw(int shader, const std::vector<Input>& inputs, int target, const void* b0, int b0Bytes, const void* b1, int b1Bytes, std::string& error)
{
  Impl& m = *impl_;
  Image* out = m.get(target);
  if(!m.device || !out || !out->target || shader < 0 || shader >= static_cast<int>(m.shaders.size()) || inputs.size() > kSlots) { error = "no such shader or image"; return false; }
  ID3D11ShaderResourceView* views[kSlots] = {};
  ID3D11SamplerState* samplers[kSlots] = {};
  SDL_FlushRenderer(m.renderer);   // what SDL has queued is done first
  for(std::size_t i = 0; i < inputs.size(); ++i) {
    Image* in = m.get(inputs[i].image);
    if(!in || !in->view || in == out) { error = "a pass reads an image that is not there (or the one it draws into)"; return false; }
    if(inputs[i].mips) m.context->GenerateMips(in->view.Get());
    views[i] = in->view.Get();
    samplers[i] = m.sampler[inputs[i].linear ? 1 : 0].Get();
  }
  const void* data[2] = {b0, b1};
  const int bytes[2] = {b0Bytes, b1Bytes};
  for(int i = 0; i < 2; ++i) {
    if(!data[i] || bytes[i] <= 0) continue;
    D3D11_MAPPED_SUBRESOURCE map{};
    if(bytes[i] > 4096 || FAILED(m.context->Map(m.buffer[i].Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) { error = "the constants could not be set"; return false; }
    std::memcpy(map.pData, data[i], static_cast<std::size_t>(bytes[i]));
    m.context->Unmap(m.buffer[i].Get(), 0);
  }
  SavedState saved;
  saved.take(m.context.Get());
  ID3D11RenderTargetView* targets[1] = {out->target.Get()};
  m.context->OMSetRenderTargets(1, targets, nullptr);
  const D3D11_VIEWPORT vp{0.0f, 0.0f, static_cast<float>(out->width), static_cast<float>(out->height), 0.0f, 1.0f};
  m.context->RSSetViewports(1, &vp);
  m.context->RSSetState(m.rasterizer.Get());
  const float factor[4] = {0, 0, 0, 0};
  m.context->OMSetBlendState(m.blend.Get(), factor, 0xFFFFFFFF);
  m.context->OMSetDepthStencilState(m.depth.Get(), 0);
  m.context->IASetInputLayout(nullptr);
  m.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  const Shader& s = m.shaders[static_cast<std::size_t>(shader)];
  m.context->VSSetShader(s.vertex.Get(), nullptr, 0);
  m.context->PSSetShader(s.pixel.Get(), nullptr, 0);
  ID3D11Buffer* buffers[2] = {m.buffer[0].Get(), m.buffer[1].Get()};
  m.context->VSSetConstantBuffers(0, 2, buffers);
  m.context->PSSetConstantBuffers(0, 2, buffers);
  m.context->PSSetShaderResources(0, kSlots, views);
  m.context->PSSetSamplers(0, kSlots, samplers);
  m.context->Draw(3, 0);
  ID3D11ShaderResourceView* none[kSlots] = {};
  m.context->PSSetShaderResources(0, kSlots, none);   // nothing stays bound that the next pass or SDL might want to use the other way round
  m.context->OMSetRenderTargets(0, nullptr, nullptr);
  saved.give(m.context.Get());   // SDL's state, as it was
  SDL_FlushRenderer(m.renderer);
  return true;
}

}  // namespace host
