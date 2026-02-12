#include "camera.h"

using namespace DirectX;

void ash::cam_update_view_mat(camera &cam)
{
    XMMATRIX rotation_matrix = XMMatrixRotationRollPitchYaw(cam.rotation.x, cam.rotation.y, cam.rotation.z);

    XMVECTOR forward = XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), rotation_matrix);

    XMVECTOR eye = XMLoadFloat3(&cam.position);
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    XMStoreFloat4x4(&cam.mat_view, XMMatrixLookToLH(eye, forward, up));
}

void ash::cam_update_proj_mat(camera &cam, float fov_y, float screen_aspect, float screen_near, float screen_far)
{
    XMStoreFloat4x4(&cam.mat_proj, XMMatrixPerspectiveFovLH(fov_y, screen_aspect, screen_near, screen_far));
}

DirectX::XMMATRIX ash::cam_get_view_proj_mat(camera &cam)
{
    return (XMLoadFloat4x4(&cam.mat_view) * XMLoadFloat4x4(&cam.mat_proj));
}
