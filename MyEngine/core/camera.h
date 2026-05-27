#pragma once
#include <DirectXMath.h>

class Camera
{
    using namespace DirectX;


public:
    void setPerspective(float fovY, float aspect, float nearZ, float farZ)
    {
        m_proj = DirectX::XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
    }

    void setPosition(float x, float y, float z)
    {
        m_position = DirectX::XMFLOAT3(x, y, z);
    }

    void lookAt(DirectX::XMFLOAT3 target)
    {
        using namespace DirectX;

        XMVECTOR eye = XMLoadFloat3(&m_position);
        XMVECTOR at = XMLoadFloat3(&target);
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);

        m_view = XMMatrixLookAtLH(eye, at, up);
    }

    DirectX::XMMATRIX getView() const { return m_view; }
    DirectX::XMMATRIX getProj() const { return m_proj; }
    DirectX::XMFLOAT3 getPosition() const { return m_position; }

private:
    DirectX::XMFLOAT3 m_position = { 0.0f, 0.0f, -5.0f };
    DirectX::XMMATRIX m_view = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX m_proj = DirectX::XMMatrixIdentity();
};
