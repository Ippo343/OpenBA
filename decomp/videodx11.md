# VideoDX11-x64.dll (DX11 Renderer Plugin)

576 functions (~86 rendering code, rest CRT). Plugin-style DLL with minimal export surface.

### Exports

| Export | Address | Purpose |
|--------|---------|---------|
| `CreateDeviceData` | `0x1800011a0` | Factory - creates DX11 device backend |
| `GetDeviceName` | `0x180001230` | Returns renderer name |

### Classes (all `*Data` companion classes for engine abstractions)

| Status | Class | Purpose |
|--------|-------|---------|
| - | `DeviceData` | DX11 device implementation |
| - | `ContextData` | DX11 context bindings |
| - | `VertexBufferData` | DX11 vertex buffer |
| - | `IndexBufferData` | DX11 index buffer |
| - | `ConstantBufferData` | DX11 constant buffer |
| - | `RenderBufferData` | DX11 render target |
| - | `ImageBufferData` | DX11 texture |
| - | `VertexProgramData` | DX11 vertex shader |
| - | `PixelProgramData` | DX11 pixel shader |
| - | `VideoProgramData` | DX11 combined shader program |
| - | `RenderStateData` | DX11 rasterizer/blend/depth state |
| - | `VertexLayoutData` | DX11 input layout |

### Rendering Specs
- Backbuffer: `DXGI_FORMAT_B8G8R8A8_UNORM`
- Depth/Stencil: `DXGI_FORMAT_D24_UNORM_S8_UINT`
- Min feature level: `D3D_FEATURE_LEVEL_10_0`
- MSAA support, device lost/reset handling
- Imports: `d3d11.dll` (D3D11CreateDevice), `dxgi.dll` (CreateDXGIFactory1), DINPUT8
