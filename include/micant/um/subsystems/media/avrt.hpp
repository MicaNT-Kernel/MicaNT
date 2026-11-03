#pragma once

/**
 * @file avrt.hpp
 * @brief Clean-Room Windows Multimedia Class Scheduler Service (MMCSS) Bridge (avrt.dll).
 *
 * Implements high-priority real-time thread characteristics and pacing for AAA game
 * audio pipelines (Wwise, FMOD, XAudio2) and low-latency rendering loops.
 *
 * References: Microsoft Learn Public Win32 MMCSS API Specification.
 */

#include <cstdint>
#include <string>
#include <string_view>
#include <mutex>
#include <unordered_map>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/native/ldr.hpp"

namespace micant::avrt {

using DWORD   = uint32_t;
using LPDWORD = uint32_t*;
using LPCWSTR = const wchar_t*;
using LPCSTR  = const char*;
using HANDLE  = void*;
using BOOL    = int32_t;
using PULONG  = uint32_t*;

enum AVRT_PRIORITY {
    AVRT_PRIORITY_VERYLOW  = -2,
    AVRT_PRIORITY_LOW      = -1,
    AVRT_PRIORITY_NORMAL   = 0,
    AVRT_PRIORITY_HIGH     = 1,
    AVRT_PRIORITY_CRITICAL = 2
};

class MmcssManager {
public:
    static MmcssManager& get() noexcept {
        static MmcssManager instance;
        return instance;
    }

    struct TaskRecord {
        uintptr_t handle;
        std::wstring taskName;
        DWORD taskIndex;
        AVRT_PRIORITY priority;
    };

    HANDLE setThreadCharacteristics(LPCWSTR TaskName, LPDWORD TaskIndex) noexcept {
        if (!TaskName || !TaskName[0]) {
            return nullptr;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        uintptr_t handleVal = ++m_nextHandle;
        DWORD idx = (TaskIndex && *TaskIndex != 0) ? *TaskIndex : 1;
        if (TaskIndex) *TaskIndex = idx;
        m_activeTasks[handleVal] = TaskRecord{ handleVal, TaskName, idx, AVRT_PRIORITY_NORMAL };
        return reinterpret_cast<HANDLE>(handleVal);
    }

    HANDLE setThreadCharacteristicsA(LPCSTR TaskName, LPDWORD TaskIndex) noexcept {
        if (!TaskName || !TaskName[0]) {
            return nullptr;
        }
        std::wstring wTaskName;
        while (*TaskName) {
            wTaskName.push_back(static_cast<wchar_t>(*TaskName++));
        }
        return setThreadCharacteristics(wTaskName.c_str(), TaskIndex);
    }

    BOOL revertThreadCharacteristics(HANDLE AvrtHandle) noexcept {
        if (!AvrtHandle) {
            return FALSE;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        uintptr_t handleVal = reinterpret_cast<uintptr_t>(AvrtHandle);
        auto it = m_activeTasks.find(handleVal);
        if (it == m_activeTasks.end()) {
            return FALSE;
        }
        m_activeTasks.erase(it);
        return TRUE;
    }

    HANDLE setMaxThreadCharacteristics(LPCWSTR FirstTask, LPCWSTR SecondTask, LPDWORD TaskIndex) noexcept {
        if (FirstTask && FirstTask[0]) {
            return setThreadCharacteristics(FirstTask, TaskIndex);
        }
        if (SecondTask && SecondTask[0]) {
            return setThreadCharacteristics(SecondTask, TaskIndex);
        }
        return nullptr;
    }

    HANDLE setMaxThreadCharacteristicsA(LPCSTR FirstTask, LPCSTR SecondTask, LPDWORD TaskIndex) noexcept {
        if (FirstTask && FirstTask[0]) {
            return setThreadCharacteristicsA(FirstTask, TaskIndex);
        }
        if (SecondTask && SecondTask[0]) {
            return setThreadCharacteristicsA(SecondTask, TaskIndex);
        }
        return nullptr;
    }

    BOOL querySystemResponsiveness(HANDLE AvrtHandle, PULONG SystemResponsivenessValue) noexcept {
        if (!SystemResponsivenessValue) return FALSE;
        // Default Windows NT multimedia responsiveness setting is 20% system reserve
        // For game threads with active handle, responsiveness is elevated to 100%
        if (AvrtHandle) {
            *SystemResponsivenessValue = 100;
        } else {
            *SystemResponsivenessValue = 20;
        }
        return TRUE;
    }

    BOOL setThreadPriority(HANDLE AvrtHandle, AVRT_PRIORITY Priority) noexcept {
        if (!AvrtHandle) return FALSE;
        std::lock_guard<std::mutex> lock(m_mutex);
        uintptr_t handleVal = reinterpret_cast<uintptr_t>(AvrtHandle);
        auto it = m_activeTasks.find(handleVal);
        if (it == m_activeTasks.end()) return FALSE;
        it->second.priority = Priority;
        return TRUE;
    }

    [[nodiscard]] size_t getActiveTaskCount() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_activeTasks.size();
    }

    void clear() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeTasks.clear();
    }

private:
    mutable std::mutex m_mutex;
    uintptr_t m_nextHandle{0xA70000};
    std::unordered_map<uintptr_t, TaskRecord> m_activeTasks;
};

// ============================================================================
// C APIs for avrt.dll
// ============================================================================

inline HANDLE WINAPI AvSetMmThreadCharacteristicsW(LPCWSTR TaskName, LPDWORD TaskIndex) noexcept {
    return MmcssManager::get().setThreadCharacteristics(TaskName, TaskIndex);
}

inline HANDLE WINAPI AvSetMmThreadCharacteristicsA(LPCSTR TaskName, LPDWORD TaskIndex) noexcept {
    return MmcssManager::get().setThreadCharacteristicsA(TaskName, TaskIndex);
}

inline BOOL WINAPI AvRevertMmThreadCharacteristics(HANDLE AvrtHandle) noexcept {
    return MmcssManager::get().revertThreadCharacteristics(AvrtHandle);
}

inline HANDLE WINAPI AvSetMmMaxThreadCharacteristicsW(LPCWSTR FirstTask, LPCWSTR SecondTask, LPDWORD TaskIndex) noexcept {
    return MmcssManager::get().setMaxThreadCharacteristics(FirstTask, SecondTask, TaskIndex);
}

inline HANDLE WINAPI AvSetMmMaxThreadCharacteristicsA(LPCSTR FirstTask, LPCSTR SecondTask, LPDWORD TaskIndex) noexcept {
    return MmcssManager::get().setMaxThreadCharacteristicsA(FirstTask, SecondTask, TaskIndex);
}

inline BOOL WINAPI AvQuerySystemResponsiveness(HANDLE AvrtHandle, PULONG SystemResponsivenessValue) noexcept {
    return MmcssManager::get().querySystemResponsiveness(AvrtHandle, SystemResponsivenessValue);
}

inline BOOL WINAPI AvSetMmThreadPriority(HANDLE AvrtHandle, AVRT_PRIORITY Priority) noexcept {
    return MmcssManager::get().setThreadPriority(AvrtHandle, Priority);
}

inline void InitializeAvrtSubsystemExports() noexcept {
    auto& ldr = ldr::DynamicLoader::get();

    ldr.registerExport("avrt.dll", "AvSetMmThreadCharacteristicsW", reinterpret_cast<void*>(AvSetMmThreadCharacteristicsW));
    ldr.registerExport("avrt.dll", "AvSetMmThreadCharacteristicsA", reinterpret_cast<void*>(AvSetMmThreadCharacteristicsA));
    ldr.registerExport("avrt.dll", "AvRevertMmThreadCharacteristics", reinterpret_cast<void*>(AvRevertMmThreadCharacteristics));
    ldr.registerExport("avrt.dll", "AvSetMmMaxThreadCharacteristicsW", reinterpret_cast<void*>(AvSetMmMaxThreadCharacteristicsW));
    ldr.registerExport("avrt.dll", "AvSetMmMaxThreadCharacteristicsA", reinterpret_cast<void*>(AvSetMmMaxThreadCharacteristicsA));
    ldr.registerExport("avrt.dll", "AvQuerySystemResponsiveness", reinterpret_cast<void*>(AvQuerySystemResponsiveness));
    ldr.registerExport("avrt.dll", "AvSetMmThreadPriority", reinterpret_cast<void*>(AvSetMmThreadPriority));
}

} // namespace micant::avrt
