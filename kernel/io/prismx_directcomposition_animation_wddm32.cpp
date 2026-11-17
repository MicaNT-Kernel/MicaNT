/**
 * @file prismx_directcomposition_animation_wddm32.cpp
 * @brief Horizon 320 (Kernel Unit #96): Sovereign MicaNT DirectComposition Visual Layer
 *        Animation & Transition Storyboard Engine with Cubic Bézier Spring Physics
 *        - DirectComposition Visual Node Animation Storyboards (Translate, Scale, Opacity)
 *        - Analytical Cubic Bézier Easing Curves with Newton-Raphson Root Solving
 *        - Harmonic Damped Spring Physics Simulator (Critically Damped & Underdamped)
 *        - 540Hz Ultra-High-Refresh Frame Interpolation (1.85ms Tick / <150us Latency)
 *        - Multi-Channel Timeline Synchronization & Zero-Jitter Compositor Scheduling.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_directcomposition_animation_wddm32.hpp"

namespace micant::um::prismx_animation {

bool VerifyHorizon320DirectCompositionAnimationSubsystem() noexcept {
    // ------------------------------------------------------------------------
    // Pillar 1: Analytical Cubic Bézier Easing Curves
    // ------------------------------------------------------------------------
    MicaNtDirectCompositionAnimationEngine engine;

    MicaNtCubicBezierControlPoints dummyCp{};
    float in0 = engine.EvaluateEasingProgress(0.0f, MicaNtEasingMode::EaseIn, dummyCp);
    float inMid = engine.EvaluateEasingProgress(0.5f, MicaNtEasingMode::EaseIn, dummyCp);
    float in1 = engine.EvaluateEasingProgress(1.0f, MicaNtEasingMode::EaseIn, dummyCp);
    if (in0 != 0.0f || in1 != 1.0f) return false;
    if (inMid >= 0.5f) return false; // Slow start

    float outMid = engine.EvaluateEasingProgress(0.5f, MicaNtEasingMode::EaseOut, dummyCp);
    if (outMid <= 0.5f) return false; // Fast start, decelerating

    // ------------------------------------------------------------------------
    // Pillar 2: Harmonic Spring Physics (Critically Damped)
    // ------------------------------------------------------------------------
    MicaNtSpringParameters critSpring{};
    critSpring.Stiffness = 180.0f;
    critSpring.Mass = 1.0f;
    critSpring.Damping = 2.0f * std::sqrt(critSpring.Stiffness * critSpring.Mass); // zeta = 1.0
    critSpring.InitialVelocity = 0.0f;

    float s0 = engine.EvaluateSpringPhysics(0.0f, 0.0f, 100.0f, critSpring);
    if (s0 != 0.0f) return false;

    float sMid = engine.EvaluateSpringPhysics(0.15f, 0.0f, 100.0f, critSpring);
    if (sMid <= 0.0f || sMid >= 100.0f) return false;

    float sEnd = engine.EvaluateSpringPhysics(1.0f, 0.0f, 100.0f, critSpring);
    if (std::abs(sEnd - 100.0f) >= 0.05f) return false; // Converged smoothly

    // ------------------------------------------------------------------------
    // Pillar 3: Harmonic Spring Physics (Underdamped with Bouncy Overshoot)
    // ------------------------------------------------------------------------
    MicaNtSpringParameters underSpring{};
    underSpring.Stiffness = 200.0f;
    underSpring.Mass = 1.0f;
    underSpring.Damping = 10.0f; // zeta ~ 0.35 (underdamped)
    underSpring.InitialVelocity = 0.0f;

    bool foundOvershoot = false;
    for (int step = 1; step <= 50; ++step) {
        float t = step * 0.01f;
        float val = engine.EvaluateSpringPhysics(t, 0.0f, 100.0f, underSpring);
        if (val > 100.0f) {
            foundOvershoot = true;
            break;
        }
    }
    if (!foundOvershoot) return false;

    // ------------------------------------------------------------------------
    // Pillar 4: Multi-Channel 540Hz Timeline Storyboard
    // ------------------------------------------------------------------------
    std::vector<MicaNtKeyframeDesc> posXKeyframes = {
        { 0.0f, 0.0f, MicaNtEasingMode::Spring, {}, critSpring },
        { 0.5f, 800.0f, MicaNtEasingMode::Spring, {}, critSpring }
    };
    engine.ScheduleAnimation(MicaNtAnimatedProperty::OffsetX, posXKeyframes);

    std::vector<MicaNtKeyframeDesc> opacityKeyframes = {
        { 0.0f, 0.0f, MicaNtEasingMode::EaseInOut, {}, {} },
        { 0.3f, 1.0f, MicaNtEasingMode::EaseInOut, {}, {} }
    };
    engine.ScheduleAnimation(MicaNtAnimatedProperty::Opacity, opacityKeyframes);

    constexpr float dt540Hz = 1.0f / 540.0f;
    float currentT = 0.0f;
    float lastPosX = 0.0f;
    float lastOpacity = 0.0f;

    for (int frame = 0; frame < 300; ++frame) {
        currentT += dt540Hz;
        float posX = engine.StepAnimation(MicaNtAnimatedProperty::OffsetX, currentT);
        float opacity = engine.StepAnimation(MicaNtAnimatedProperty::Opacity, currentT);

        if (posX < lastPosX) return false;
        if (opacity < lastOpacity) return false;
        lastPosX = posX;
        lastOpacity = opacity;
    }

    if (lastOpacity != 1.0f) return false;
    if (lastPosX <= 750.0f) return false;

    // ------------------------------------------------------------------------
    // Pillar 5: Telemetry Verification
    // ------------------------------------------------------------------------
    const auto& tel = engine.GetTelemetry();
    if (tel.TotalAnimationsScheduled != 2u) return false;
    if (tel.FramesInterpolated540Hz < 300u) return false;
    if (tel.SpringPhysicsEvaluations < 100u) return false;
    if (tel.NewtonRaphsonIterations < 10u) return false;
    if (tel.AverageInterpolationLatencyMicroseconds >= 150.0f) return false;
    if (!tel.CriticallyDampedConvergenceVerified) return false;
    if (!tel.CubicBezierContinuityVerified) return false;
    if (!tel.ZeroJitter540HzSyncVerified) return false;

    return true;
}

} // namespace micant::um::prismx_animation
