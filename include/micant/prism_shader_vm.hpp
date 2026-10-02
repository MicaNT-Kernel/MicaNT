// ============================================================================
// MicaNT: PrismShaderVM - Sovereign Programmable Shader Bytecode Virtual Machine
// 
// Strict Clean-Room Implementation
//
// Subsystem Overview:
//   PrismShaderVM provides an independent, register-based RISC SIMD bytecode
//   execution engine for Vertex Shaders (VS) and Pixel Shaders (PS).
//   It executes 4-component single-precision floating point vector instructions,
//   supporting matrix transformations, lighting evaluation, UV texture sampling,
//   and color modulation without external dependencies.
// ============================================================================

#pragma once

#include "prismx.hpp"
#include "prism3d.hpp"
#include <cstdint>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <functional>
#include <string>

namespace micant::prism_vm {

using namespace micant::prism3d;

// ============================================================================
// 1. VM Opcodes & Register Enums
// ============================================================================

enum Opcode : uint8_t {
    OP_NOP   = 0,
    OP_MOV   = 1,   // dst = src0
    OP_ADD   = 2,   // dst = src0 + src1
    OP_SUB   = 3,   // dst = src0 - src1
    OP_MUL   = 4,   // dst = src0 * src1
    OP_MAD   = 5,   // dst = src0 * src1 + src2
    OP_DP3   = 6,   // dst = dot3(src0, src1)
    OP_DP4   = 7,   // dst = dot4(src0, src1)
    OP_MIN   = 8,   // dst = min(src0, src1)
    OP_MAX   = 9,   // dst = max(src0, src1)
    OP_SLT   = 10,  // dst = (src0 < src1) ? 1.0 : 0.0
    OP_SGE   = 11,  // dst = (src0 >= src1) ? 1.0 : 0.0
    OP_RCP   = 12,  // dst = 1.0 / src0
    OP_RSQ   = 13,  // dst = 1.0 / sqrt(src0)
    OP_CLAMP = 14,  // dst = clamp(src0, src1, src2)
    OP_TEX   = 15,  // dst = sample_texture(src0.xy, samplerId)
    OP_RET   = 16   // Return
};

enum RegisterType : uint8_t {
    REG_TEMP   = 0, // r0 - r7 (Temporary working registers)
    REG_INPUT  = 1, // v0 - v3 (Input attributes: Position, Color, UV, Normal)
    REG_CONST  = 2, // c0 - c15 (Constant Buffer float4 parameters)
    REG_OUTPUT = 3  // o0 - o1 (o0 = Position/Clip, o1 = Color)
};

struct RegisterRef {
    RegisterType type;
    uint8_t index;
    uint8_t swizzle[4];        // Component mapping: 0=x, 1=y, 2=z, 3=w
    uint8_t writeMask{ 0x0F }; // Component write mask: bit 0=x, 1=y, 2=z, 3=w

    static RegisterRef Temp(uint8_t idx, uint8_t mask = 0x0F) {
        return { REG_TEMP, idx, { 0, 1, 2, 3 }, mask };
    }
    static RegisterRef Input(uint8_t idx) {
        return { REG_INPUT, idx, { 0, 1, 2, 3 }, 0x0F };
    }
    static RegisterRef Const(uint8_t idx) {
        return { REG_CONST, idx, { 0, 1, 2, 3 }, 0x0F };
    }
    static RegisterRef Output(uint8_t idx, uint8_t mask = 0x0F) {
        return { REG_OUTPUT, idx, { 0, 1, 2, 3 }, mask };
    }

    RegisterRef Mask(uint8_t m) const {
        RegisterRef copy = *this;
        copy.writeMask = m;
        return copy;
    }

    RegisterRef Swizzle(uint8_t x, uint8_t y, uint8_t z, uint8_t w) const {
        return { type, index, { x, y, z, w }, writeMask };
    }
};

struct Instruction {
    Opcode op;
    RegisterRef dst;
    RegisterRef src0;
    RegisterRef src1;
    RegisterRef src2;
    uint8_t samplerSlot{ 0 };
};

// 4-component vector register
struct VectorRegister {
    float v[4]{ 0.0f, 0.0f, 0.0f, 0.0f };

    float& x() { return v[0]; }
    float& y() { return v[1]; }
    float& z() { return v[2]; }
    float& w() { return v[3]; }

    float x() const { return v[0]; }
    float y() const { return v[1]; }
    float z() const { return v[2]; }
    float w() const { return v[3]; }

    VectorRegister() = default;
    VectorRegister(float x, float y, float z, float w) {
        v[0] = x; v[1] = y; v[2] = z; v[3] = w;
    }
};

// ============================================================================
// 2. VM Execution Context & Shader Program
// ============================================================================

using TextureSamplerFn = std::function<VectorRegister(uint8_t slot, float u, float v)>;

class ShaderProgram {
private:
    std::vector<Instruction> m_instructions;

public:
    void Add(const Instruction& inst) {
        m_instructions.push_back(inst);
    }

    const std::vector<Instruction>& GetInstructions() const {
        return m_instructions;
    }

    size_t InstructionCount() const {
        return m_instructions.size();
    }
};

class PrismShaderVM {
public:
    static VectorRegister ReadRegister(
        const RegisterRef& reg,
        const std::array<VectorRegister, 8>& temps,
        const std::array<VectorRegister, 4>& inputs,
        const std::array<VectorRegister, 16>& consts,
        const std::array<VectorRegister, 2>& outputs
    ) {
        VectorRegister src{};
        switch (reg.type) {
            case REG_TEMP:   src = (reg.index < temps.size())   ? temps[reg.index]   : VectorRegister{}; break;
            case REG_INPUT:  src = (reg.index < inputs.size())  ? inputs[reg.index]  : VectorRegister{}; break;
            case REG_CONST:  src = (reg.index < consts.size())  ? consts[reg.index]  : VectorRegister{}; break;
            case REG_OUTPUT: src = (reg.index < outputs.size()) ? outputs[reg.index] : VectorRegister{}; break;
        }

        // Apply swizzle
        return VectorRegister{
            src.v[reg.swizzle[0] & 3],
            src.v[reg.swizzle[1] & 3],
            src.v[reg.swizzle[2] & 3],
            src.v[reg.swizzle[3] & 3]
        };
    }

    static void WriteRegister(
        const RegisterRef& reg,
        const VectorRegister& val,
        std::array<VectorRegister, 8>& temps,
        std::array<VectorRegister, 2>& outputs
    ) {
        VectorRegister* target = nullptr;
        if (reg.type == REG_TEMP && reg.index < temps.size()) {
            target = &temps[reg.index];
        } else if (reg.type == REG_OUTPUT && reg.index < outputs.size()) {
            target = &outputs[reg.index];
        }
        if (!target) return;

        uint8_t mask = reg.writeMask;
        if (mask & 0x1) target->v[0] = val.v[0];
        if (mask & 0x2) target->v[1] = val.v[1];
        if (mask & 0x4) target->v[2] = val.v[2];
        if (mask & 0x8) target->v[3] = val.v[3];
    }

    // Execute Vertex Shader Program
    static void ExecuteVertexShader(
        const ShaderProgram& program,
        const VectorRegister& inPosition,
        const VectorRegister& inColor,
        const VectorRegister& inUV,
        const VectorRegister& inNormal,
        const std::array<VectorRegister, 16>& constBuffers,
        VectorRegister& outPosition,
        VectorRegister& outColor
    ) {
        std::array<VectorRegister, 8> temps{};
        std::array<VectorRegister, 4> inputs{ inPosition, inColor, inUV, inNormal };
        std::array<VectorRegister, 2> outputs{ inPosition, inColor };

        for (const auto& inst : program.GetInstructions()) {
            if (inst.op == OP_RET) break;

            VectorRegister s0 = ReadRegister(inst.src0, temps, inputs, constBuffers, outputs);
            VectorRegister s1 = ReadRegister(inst.src1, temps, inputs, constBuffers, outputs);
            VectorRegister s2 = ReadRegister(inst.src2, temps, inputs, constBuffers, outputs);
            VectorRegister result{};

            switch (inst.op) {
                case OP_MOV:
                    result = s0;
                    break;
                case OP_ADD:
                    result = VectorRegister{ s0.x() + s1.x(), s0.y() + s1.y(), s0.z() + s1.z(), s0.w() + s1.w() };
                    break;
                case OP_SUB:
                    result = VectorRegister{ s0.x() - s1.x(), s0.y() - s1.y(), s0.z() - s1.z(), s0.w() - s1.w() };
                    break;
                case OP_MUL:
                    result = VectorRegister{ s0.x() * s1.x(), s0.y() * s1.y(), s0.z() * s1.z(), s0.w() * s1.w() };
                    break;
                case OP_MAD:
                    result = VectorRegister{
                        s0.x() * s1.x() + s2.x(),
                        s0.y() * s1.y() + s2.y(),
                        s0.z() * s1.z() + s2.z(),
                        s0.w() * s1.w() + s2.w()
                    };
                    break;
                case OP_DP3: {
                    float dot = s0.x() * s1.x() + s0.y() * s1.y() + s0.z() * s1.z();
                    result = VectorRegister{ dot, dot, dot, dot };
                    break;
                }
                case OP_DP4: {
                    float dot = s0.x() * s1.x() + s0.y() * s1.y() + s0.z() * s1.z() + s0.w() * s1.w();
                    result = VectorRegister{ dot, dot, dot, dot };
                    break;
                }
                case OP_MIN:
                    result = VectorRegister{ std::min(s0.x(), s1.x()), std::min(s0.y(), s1.y()), std::min(s0.z(), s1.z()), std::min(s0.w(), s1.w()) };
                    break;
                case OP_MAX:
                    result = VectorRegister{ std::max(s0.x(), s1.x()), std::max(s0.y(), s1.y()), std::max(s0.z(), s1.z()), std::max(s0.w(), s1.w()) };
                    break;
                case OP_SLT:
                    result = VectorRegister{ (s0.x() < s1.x()) ? 1.0f : 0.0f, (s0.y() < s1.y()) ? 1.0f : 0.0f, (s0.z() < s1.z()) ? 1.0f : 0.0f, (s0.w() < s1.w()) ? 1.0f : 0.0f };
                    break;
                case OP_SGE:
                    result = VectorRegister{ (s0.x() >= s1.x()) ? 1.0f : 0.0f, (s0.y() >= s1.y()) ? 1.0f : 0.0f, (s0.z() >= s1.z()) ? 1.0f : 0.0f, (s0.w() >= s1.w()) ? 1.0f : 0.0f };
                    break;
                case OP_RCP:
                    result = VectorRegister{ (std::abs(s0.x()) > 1e-6f) ? (1.0f / s0.x()) : 0.0f,
                                            (std::abs(s0.y()) > 1e-6f) ? (1.0f / s0.y()) : 0.0f,
                                            (std::abs(s0.z()) > 1e-6f) ? (1.0f / s0.z()) : 0.0f,
                                            (std::abs(s0.w()) > 1e-6f) ? (1.0f / s0.w()) : 0.0f };
                    break;
                case OP_RSQ:
                    result = VectorRegister{ (s0.x() > 1e-6f) ? (1.0f / std::sqrt(s0.x())) : 0.0f,
                                            (s0.y() > 1e-6f) ? (1.0f / std::sqrt(s0.y())) : 0.0f,
                                            (s0.z() > 1e-6f) ? (1.0f / std::sqrt(s0.z())) : 0.0f,
                                            (s0.w() > 1e-6f) ? (1.0f / std::sqrt(s0.w())) : 0.0f };
                    break;
                case OP_CLAMP:
                    result = VectorRegister{
                        std::clamp(s0.x(), s1.x(), s2.x()),
                        std::clamp(s0.y(), s1.y(), s2.y()),
                        std::clamp(s0.z(), s1.z(), s2.z()),
                        std::clamp(s0.w(), s1.w(), s2.w())
                    };
                    break;
                default:
                    break;
            }

            WriteRegister(inst.dst, result, temps, outputs);
        }

        outPosition = outputs[0];
        outColor = outputs[1];
    }

    // Execute Pixel Shader Program
    static void ExecutePixelShader(
        const ShaderProgram& program,
        const VectorRegister& inPosition,
        const VectorRegister& inColor,
        const VectorRegister& inUV,
        const VectorRegister& inNormal,
        const std::array<VectorRegister, 16>& constBuffers,
        const TextureSamplerFn& sampler,
        VectorRegister& outColor
    ) {
        std::array<VectorRegister, 8> temps{};
        std::array<VectorRegister, 4> inputs{ inPosition, inColor, inUV, inNormal };
        std::array<VectorRegister, 2> outputs{ inPosition, inColor };

        for (const auto& inst : program.GetInstructions()) {
            if (inst.op == OP_RET) break;

            VectorRegister s0 = ReadRegister(inst.src0, temps, inputs, constBuffers, outputs);
            VectorRegister s1 = ReadRegister(inst.src1, temps, inputs, constBuffers, outputs);
            VectorRegister s2 = ReadRegister(inst.src2, temps, inputs, constBuffers, outputs);
            VectorRegister result{};

            switch (inst.op) {
                case OP_MOV:
                    result = s0;
                    break;
                case OP_ADD:
                    result = VectorRegister{ s0.x() + s1.x(), s0.y() + s1.y(), s0.z() + s1.z(), s0.w() + s1.w() };
                    break;
                case OP_SUB:
                    result = VectorRegister{ s0.x() - s1.x(), s0.y() - s1.y(), s0.z() - s1.z(), s0.w() - s1.w() };
                    break;
                case OP_MUL:
                    result = VectorRegister{ s0.x() * s1.x(), s0.y() * s1.y(), s0.z() * s1.z(), s0.w() * s1.w() };
                    break;
                case OP_MAD:
                    result = VectorRegister{
                        s0.x() * s1.x() + s2.x(),
                        s0.y() * s1.y() + s2.y(),
                        s0.z() * s1.z() + s2.z(),
                        s0.w() * s1.w() + s2.w()
                    };
                    break;
                case OP_DP3: {
                    float dot = s0.x() * s1.x() + s0.y() * s1.y() + s0.z() * s1.z();
                    result = VectorRegister{ dot, dot, dot, dot };
                    break;
                }
                case OP_DP4: {
                    float dot = s0.x() * s1.x() + s0.y() * s1.y() + s0.z() * s1.z() + s0.w() * s1.w();
                    result = VectorRegister{ dot, dot, dot, dot };
                    break;
                }
                case OP_MIN:
                    result = VectorRegister{ std::min(s0.x(), s1.x()), std::min(s0.y(), s1.y()), std::min(s0.z(), s1.z()), std::min(s0.w(), s1.w()) };
                    break;
                case OP_MAX:
                    result = VectorRegister{ std::max(s0.x(), s1.x()), std::max(s0.y(), s1.y()), std::max(s0.z(), s1.z()), std::max(s0.w(), s1.w()) };
                    break;
                case OP_CLAMP:
                    result = VectorRegister{
                        std::clamp(s0.x(), s1.x(), s2.x()),
                        std::clamp(s0.y(), s1.y(), s2.y()),
                        std::clamp(s0.z(), s1.z(), s2.z()),
                        std::clamp(s0.w(), s1.w(), s2.w())
                    };
                    break;
                case OP_TEX:
                    if (sampler) {
                        result = sampler(inst.samplerSlot, s0.x(), s0.y());
                    }
                    break;
                default:
                    break;
            }

            WriteRegister(inst.dst, result, temps, outputs);
        }

        outColor = outputs[1];
    }

    // ========================================================================
    // Standard Factory Shaders
    // ========================================================================

    // Standard MVP Matrix Transform Vertex Shader
    // c0, c1, c2, c3 = Column vectors of 4x4 MVP Matrix
    // o0.x = dot4(v0, c0), o0.y = dot4(v0, c1), o0.z = dot4(v0, c2), o0.w = dot4(v0, c3)
    // o1 = v1 (color pass-through)
    static ShaderProgram BuildMVPTransformVS() {
        ShaderProgram prog;
        Instruction inst{};

        // r0.x = dot4(v0, c0) (mask 0x1)
        inst = { OP_DP4, RegisterRef::Temp(0, 0x1), RegisterRef::Input(0), RegisterRef::Const(0), {} };
        prog.Add(inst);
        // r0.y = dot4(v0, c1) (mask 0x2)
        inst = { OP_DP4, RegisterRef::Temp(0, 0x2), RegisterRef::Input(0), RegisterRef::Const(1), {} };
        prog.Add(inst);
        // r0.z = dot4(v0, c2) (mask 0x4)
        inst = { OP_DP4, RegisterRef::Temp(0, 0x4), RegisterRef::Input(0), RegisterRef::Const(2), {} };
        prog.Add(inst);
        // r0.w = dot4(v0, c3) (mask 0x8)
        inst = { OP_DP4, RegisterRef::Temp(0, 0x8), RegisterRef::Input(0), RegisterRef::Const(3), {} };
        prog.Add(inst);

        // o0 = r0 (output position)
        inst = { OP_MOV, RegisterRef::Output(0), RegisterRef::Temp(0), {}, {} };
        prog.Add(inst);

        // o1 = v1 (output color)
        inst = { OP_MOV, RegisterRef::Output(1), RegisterRef::Input(1), {}, {} };
        prog.Add(inst);

        prog.Add({ OP_RET, {}, {}, {}, {} });
        return prog;
    }

    // Standard Textured Shaded Pixel Shader
    // r0 = sample_texture(v2.xy, slot 0)
    // o1 = r0 * v1 (texture modulate by vertex color)
    static ShaderProgram BuildTexturedModulatePS() {
        ShaderProgram prog;
        Instruction inst{};

        // r0 = sample(slot 0, v2)
        inst = { OP_TEX, RegisterRef::Temp(0), RegisterRef::Input(2), {}, {}, 0 };
        prog.Add(inst);

        // o1 = r0 * v1
        inst = { OP_MUL, RegisterRef::Output(1), RegisterRef::Temp(0), RegisterRef::Input(1), {} };
        prog.Add(inst);

        prog.Add({ OP_RET, {}, {}, {}, {} });
        return prog;
    }

    // Directional Lighting Shaded Pixel Shader
    // c0 = Light Direction (Normalized float4)
    // c1 = Light Ambient (float4)
    // c2 = Light Diffuse (float4)
    // Computes: o1 = v1 * (c1 + c2 * max(dot3(v3, c0), 0.0))
    static ShaderProgram BuildDirectionalLightingPS() {
        ShaderProgram prog;
        Instruction inst{};

        // r0.x = dot3(v3, c0)
        inst = { OP_DP3, RegisterRef::Temp(0), RegisterRef::Input(3), RegisterRef::Const(0), {} };
        prog.Add(inst);

        // r0.x = max(r0.x, 0.0)
        inst = { OP_MAX, RegisterRef::Temp(0), RegisterRef::Temp(0), RegisterRef::Const(4) /* 0,0,0,0 */, {} };
        prog.Add(inst);

        // r1 = c2 * r0.xxxx (Diffuse contribution)
        inst = { OP_MUL, RegisterRef::Temp(1), RegisterRef::Const(2), RegisterRef::Temp(0).Swizzle(0, 0, 0, 0), {} };
        prog.Add(inst);

        // r2 = r1 + c1 (Ambient + Diffuse)
        inst = { OP_ADD, RegisterRef::Temp(2), RegisterRef::Temp(1), RegisterRef::Const(1), {} };
        prog.Add(inst);

        // o1 = v1 * r2 (Surface color modulated by lighting)
        inst = { OP_MUL, RegisterRef::Output(1), RegisterRef::Input(1), RegisterRef::Temp(2), {} };
        prog.Add(inst);

        prog.Add({ OP_RET, {}, {}, {}, {} });
        return prog;
    }
};

} // namespace micant::prism_vm
