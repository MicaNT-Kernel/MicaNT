/**
 * @file prismx_directcomposition_spatial_3d_wddm32.cpp
 * @brief Horizon 323 (Kernel Unit #99): Sovereign MicaNT DirectComposition Spatial 3D
 *        Perspective Transformations, Gimbal-Free Quaternions & Holographic Camera Viewport Pipeline
 *        - IDCompositionMatrixTransform3D 4x4 projective perspective matrices
 *        - Gimbal-lock-free quaternion SLERP rotation interpolation
 *        - Holographic camera viewport projection (FOV, Aspect Ratio, Near/Far clipping)
 *        - Screen-space raycasting & 3D quad hit-testing (Möller–Trumbore algorithm)
 *        - Pure ISO C++23. Zero External Dependencies.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_directcomposition_spatial_3d_wddm32.hpp"

namespace micant::um::prismx_spatial3d {

bool VerifyHorizon323DirectCompositionSpatial3DSubsystem() noexcept {
    MicaNtDirectCompositionSpatial3DEngine engine(1920, 1080);

    // ------------------------------------------------------------------------
    // Pillar 1: Matrix4x4 Projection & Perspective Divide
    // ------------------------------------------------------------------------
    MicaNtMatrix4x4 proj = MicaNtMatrix4x4::PerspectiveFovLH(
        60.0f * (std::numbers::pi_v<float> / 180.0f),
        1920.0f / 1080.0f, 0.1f, 1000.0f);

    MicaNtVector3 testPt{ 0.0f, 0.0f, 10.0f };
    MicaNtVector3 projected = proj.TransformPoint(testPt);
    if (std::abs(projected.x) > 0.001f || std::abs(projected.y) > 0.001f) return false;

    // ------------------------------------------------------------------------
    // Pillar 2: Gimbal-Lock-Free Quaternion SLERP Interpolation
    // ------------------------------------------------------------------------
    MicaNtQuaternion q0 = MicaNtQuaternion::Identity();
    MicaNtQuaternion q1 = MicaNtQuaternion::FromAxisAngle({ 0.0f, 1.0f, 0.0f }, std::numbers::pi_v<float> * 0.5f);

    MicaNtQuaternion qMid = MicaNtQuaternion::Slerp(q0, q1, 0.5f);
    MicaNtQuaternion qExpectedMid = MicaNtQuaternion::FromAxisAngle({ 0.0f, 1.0f, 0.0f }, std::numbers::pi_v<float> * 0.25f);
    if (std::abs(qMid.Dot(qExpectedMid) - 1.0f) > 0.001f) return false;

    // ------------------------------------------------------------------------
    // Pillar 3: Spatial Visual Quad Placement & Orientation
    // ------------------------------------------------------------------------
    MicaNtSpatialVisualNode3D nodeCenter{};
    nodeCenter.NodeId = 101;
    nodeCenter.Position = { 0.0f, 0.0f, 5.0f };
    nodeCenter.Size = { 2.0f, 2.0f, 1.0f };
    nodeCenter.Orientation = MicaNtQuaternion::Identity();
    engine.AddVisualNode(nodeCenter);

    // ------------------------------------------------------------------------
    // Pillar 4: Screen-Space Raycasting & 3D Hit-Testing (Möller–Trumbore)
    // ------------------------------------------------------------------------
    auto hitCenter = engine.HitTestScreen(960.0f, 540.0f);
    if (!hitCenter.Hit) return false;
    if (hitCenter.VisualNodeId != 101) return false;
    if (std::abs(hitCenter.Distance - 5.0f) > 0.05f) return false;

    auto hitCorner = engine.HitTestScreen(50.0f, 50.0f);
    if (hitCorner.Hit) return false;

    // ------------------------------------------------------------------------
    // Pillar 5: Depth Clipping & Telemetry Verification
    // ------------------------------------------------------------------------
    MicaNtSpatialVisualNode3D nodeBehindNearPlane{};
    nodeBehindNearPlane.NodeId = 102;
    nodeBehindNearPlane.Position = { 0.0f, 0.0f, 0.05f }; // Z = 0.05 < nearZ 0.1
    nodeBehindNearPlane.Size = { 1.0f, 1.0f, 1.0f };
    engine.AddVisualNode(nodeBehindNearPlane);

    auto hitNearClipped = engine.HitTestScreen(960.0f, 540.0f);
    if (hitNearClipped.VisualNodeId == 102) return false;

    const auto& tel = engine.GetTelemetry();
    if (tel.TransformationsEvaluated < 2u) return false;
    if (tel.RaycastsProcessed < 2u) return false;
    if (tel.HitsDetected < 1u) return false;
    if (tel.AverageRaycastLatencyMicroseconds >= 120.0f) return false;
    if (!tel.GimbalLockImmunityVerified) return false;
    if (!tel.PerspectiveDivideVerified) return false;
    if (!tel.ScreenSpaceRaycastVerified) return false;
    if (!tel.DepthClippingVerified) return false;

    return true;
}

} // namespace micant::um::prismx_spatial3d
