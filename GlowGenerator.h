#pragma once
#include <d3d11.h>
#include <vector>
#include <cmath>
#include <algorithm>

class GlowGenerator {
public:
    static ID3D11ShaderResourceView* CreateGlowTexture(ID3D11Device* device) {
        const int width = 128;
        const int height = 128;
        std::vector<unsigned char> data(width * height * 4);

        float centerX = width / 2.0f;
        float centerY = height / 2.0f;
        float maxDist = width / 2.0f;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float dx = x - centerX;
                float dy = y - centerY;
                float dist = sqrtf(dx * dx + dy * dy);

                float t = dist / maxDist;

                float alpha = expf(-4.0f * t * t);

                if (t >= 1.0f) alpha = 0.0f;

                int pixelIndex = (y * width + x) * 4;

                data[pixelIndex + 0] = 255; 
                data[pixelIndex + 1] = 255;
                data[pixelIndex + 2] = 255;
                data[pixelIndex + 3] = (unsigned char)(std::min(1.0f, alpha) * 255.0f); // A
            }
        }

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA subResource = {};
        subResource.pSysMem = data.data();
        subResource.SysMemPitch = width * 4;

        ID3D11Texture2D* pTexture = nullptr;
        if (FAILED(device->CreateTexture2D(&desc, &subResource, &pTexture)))
            return nullptr;

        ID3D11ShaderResourceView* pSRV = nullptr;
        device->CreateShaderResourceView(pTexture, nullptr, &pSRV);
        pTexture->Release();

        return pSRV;
    }
};