#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstring>

using Microsoft::WRL::ComPtr;

static constexpr uint32_t AlignConstantBufferSize(uint32_t size) {
    return (size + 255) & ~255;
}

