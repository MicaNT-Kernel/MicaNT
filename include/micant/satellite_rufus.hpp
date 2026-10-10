// ============================================================================
// MicaNT: Rufus 4.x Storage & Low-Level Hardware Satellite (satellite_rufus.hpp)
//
// Modern Clean-Room Forwarding Bridge in pure ISO C++23.
// All 100 exports have graduated directly to Canonical Core MicaNT Subsystems:
//   - virtdisk.dll -> include/micant/virtdisk.hpp
//   - wininet.dll  -> include/micant/wininet.hpp
//   - wintrust.dll -> include/micant/wintrust.hpp
//   - ole32.dll    -> include/micant/ole32.hpp
//   - gdi32.dll    -> include/micant/gdi32.hpp
//   - shell32.dll  -> include/micant/shell32.hpp
//   - crypt32.dll  -> include/micant/crypt32.hpp
//   - advapi32.dll -> include/micant/advapi32.hpp
//   - user32.dll   -> include/micant/user32.hpp
//   - setupapi.dll -> include/micant/setupapi.hpp
//   - ntdll.dll    -> include/micant/ntdll.hpp
//   - kernel32.dll -> include/micant/kernel32.hpp
// ============================================================================

#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <cwchar>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <vector>
#include <mutex>
#include <atomic>

#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "kernel32.hpp"
#include "user32.hpp"
#include "gdi32.hpp"
#include "ole32.hpp"
#include "setupapi.hpp"
#include "shell32.hpp"
#include "crypt32.hpp"
#include "advapi32.hpp"
#include "virtdisk.hpp"
#include "wininet.hpp"
#include "wintrust.hpp"
#include "ntdll.hpp"
#include "ldr.hpp"

namespace micant::satellite::rufus {

using BOOL = int32_t;
using BOOLEAN = uint8_t;
using DWORD = uint32_t;
using ULONG = uint32_t;
using USHORT = uint16_t;
using UCHAR = uint8_t;
using WORD = uint16_t;
using BYTE = uint8_t;
using HANDLE = void*;
using HWND = void*;
using HDC = void*;
using HICON = void*;
using HKEY = void*;
using HCRYPTPROV = uintptr_t;
using HCRYPTKEY = uintptr_t;
using HCRYPTHASH = uintptr_t;
using HCRYPTMSG = void*;
using HWINEVENTHOOK = void*;
using HDEVINFO = void*;
using LSTATUS = int32_t;
using HRESULT = int32_t;
using LCID = uint32_t;
using ULONGLONG = uint64_t;
using DWORD64 = uint64_t;
using LONG_PTR = intptr_t;

inline constexpr BOOL TRUE_VAL = 1;
inline constexpr BOOL FALSE_VAL = 0;
inline constexpr DWORD ERROR_SUCCESS_VAL = 0;
inline constexpr DWORD ERROR_NO_MORE_FILES_VAL = 18;
inline constexpr HRESULT S_OK_VAL = 0;

// ============================================================================
// Forwarding Bridges to Canonical Core MicaNT Subsystems
// ============================================================================

// 1. advapi32.dll
inline BOOL WINAPI ConvertStringSecurityDescriptorToSecurityDescriptorA(
    const char* StringSecurityDescriptor,
    DWORD StringSDRevision,
    void** SecurityDescriptor,
    ULONG* SecurityDescriptorSize
) noexcept {
    return advapi32::ConvertStringSecurityDescriptorToSecurityDescriptorA(
        StringSecurityDescriptor, StringSDRevision, SecurityDescriptor, SecurityDescriptorSize);
}

inline BOOL WINAPI ConvertStringSidToSidA(const char* StringSid, void** Sid) noexcept {
    return advapi32::ConvertStringSidToSidA(StringSid, Sid);
}

inline BOOL WINAPI CryptImportKey(
    HCRYPTPROV hProv, const BYTE* pbData, DWORD dwDataLen,
    HCRYPTKEY hPubKey, DWORD dwFlags, HCRYPTKEY* phKey
) noexcept {
    return advapi32::CryptImportKey(hProv, pbData, dwDataLen, hPubKey, dwFlags, phKey);
}

inline BOOL WINAPI CryptVerifySignatureW(
    HCRYPTHASH hHash, const BYTE* pbSignature, DWORD dwSigLen,
    HCRYPTKEY hPubKey, const wchar_t* szDescription, DWORD dwFlags
) noexcept {
    return advapi32::CryptVerifySignatureW(hHash, pbSignature, dwSigLen, hPubKey, szDescription, dwFlags);
}

inline LSTATUS WINAPI RegDeleteValueA(HKEY hKey, const char* lpValueName) noexcept {
    return advapi32::RegDeleteValueA(hKey, lpValueName);
}

inline LSTATUS WINAPI RegGetValueA(
    HKEY hkey, const char* lpSubKey, const char* lpValue,
    DWORD dwFlags, DWORD* pdwType, void* pvData, DWORD* pcbData
) noexcept {
    return advapi32::RegGetValueA(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
}

inline LSTATUS WINAPI RegLoadKeyA(HKEY hKey, const char* lpSubKey, const char* lpFile) noexcept {
    return advapi32::RegLoadKeyA(hKey, lpSubKey, lpFile);
}

inline LSTATUS WINAPI RegUnLoadKeyA(HKEY hKey, const char* lpSubKey) noexcept {
    return advapi32::RegUnLoadKeyA(hKey, lpSubKey);
}

inline BOOLEAN WINAPI SystemFunction036(void* RandomBuffer, ULONG RandomBufferLength) noexcept {
    return static_cast<BOOLEAN>(advapi32::SystemFunction036(RandomBuffer, RandomBufferLength));
}

// 2. crypt32.dll
inline BOOL WINAPI CertGetCertificateChain(
    void* hChainEngine, void* pCertContext, void* pTime,
    void* hAdditionalStore, void* pChainPara, DWORD dwFlags,
    void* pvReserved, void** ppChainContext
) noexcept {
    return crypt32::CertGetCertificateChain(hChainEngine, pCertContext, pTime, hAdditionalStore, pChainPara, dwFlags, pvReserved, ppChainContext);
}

inline BOOL WINAPI CryptDecodeObjectEx(
    DWORD dwCertEncodingType, const char* lpszStructType,
    const BYTE* pbEncoded, DWORD cbEncoded, DWORD dwFlags,
    void* pDecodePara, void* pvStructInfo, DWORD* pcbStructInfo
) noexcept {
    return crypt32::CryptDecodeObjectEx(dwCertEncodingType, lpszStructType, pbEncoded, cbEncoded, dwFlags, pDecodePara, pvStructInfo, pcbStructInfo);
}

inline BOOL WINAPI CryptHashCertificate(
    uintptr_t hCryptProv, DWORD Algid, DWORD dwFlags,
    const BYTE* pbEncoded, DWORD cbEncoded,
    BYTE* pbComputedHash, DWORD* pcbComputedHash
) noexcept {
    return crypt32::CryptHashCertificate(hCryptProv, Algid, dwFlags, pbEncoded, cbEncoded, pbComputedHash, pcbComputedHash);
}

inline HCRYPTMSG WINAPI CryptMsgOpenToDecode(
    DWORD dwMsgEncodingType, DWORD dwFlags, DWORD dwMsgType,
    uintptr_t hCryptProv, void* pRecipientInfo, void* pStreamInfo
) noexcept {
    return crypt32::CryptMsgOpenToDecode(dwMsgEncodingType, dwFlags, dwMsgType, hCryptProv, pRecipientInfo, pStreamInfo);
}

inline BOOL WINAPI CryptMsgUpdate(HCRYPTMSG hCryptMsg, const BYTE* pbData, DWORD cbData, BOOL fFinal) noexcept {
    return crypt32::CryptMsgUpdate(hCryptMsg, pbData, cbData, fFinal);
}

// 3. gdi32.dll
inline int WINAPI EnumFontFamiliesExA(HDC hdc, void* lpLogfont, void* lpProc, LONG_PTR lParam, DWORD dwFlags) noexcept {
    return gdi32::EnumFontFamiliesExA(hdc, lpLogfont, lpProc, lParam, dwFlags);
}

// 4. kernel32.dll
inline HANDLE WINAPI FindFirstVolumeA(char* lpszVolumeName, DWORD cchBufferLength) noexcept {
    return win32::FindFirstVolumeA(lpszVolumeName, cchBufferLength);
}

inline BOOL WINAPI FindNextVolumeA(HANDLE hFindVolume, char* lpszVolumeName, DWORD cchBufferLength) noexcept {
    return win32::FindNextVolumeA(hFindVolume, lpszVolumeName, cchBufferLength);
}

inline BOOL WINAPI FindVolumeClose(HANDLE hFindVolume) noexcept {
    return win32::FindVolumeClose(hFindVolume);
}

inline BOOL WINAPI GetVolumeNameForVolumeMountPointA(const char* lpszVolumeMountPoint, char* lpszVolumeName, DWORD cchBufferLength) noexcept {
    return win32::GetVolumeNameForVolumeMountPointA(lpszVolumeMountPoint, lpszVolumeName, cchBufferLength);
}

inline BOOL WINAPI GetVolumePathNameA(const char* lpszFileName, char* lpszVolumePathName, DWORD cchBufferLength) noexcept {
    return win32::GetVolumePathNameA(lpszFileName, lpszVolumePathName, cchBufferLength);
}

inline BOOL WINAPI GetVolumeInformationA(
    const char* lpRootPathName, char* lpVolumeNameBuffer, DWORD nVolumeNameSize,
    DWORD* lpVolumeSerialNumber, DWORD* lpMaximumComponentLength,
    DWORD* lpFileSystemFlags, char* lpFileSystemNameBuffer, DWORD nFileSystemNameSize
) noexcept {
    return win32::GetVolumeInformationA(lpRootPathName, lpVolumeNameBuffer, nVolumeNameSize, lpVolumeSerialNumber, lpMaximumComponentLength, lpFileSystemFlags, lpFileSystemNameBuffer, nFileSystemNameSize);
}

inline BOOL WINAPI GetVolumeInformationByHandleW(
    HANDLE hFile, wchar_t* lpVolumeNameBuffer, DWORD nVolumeNameSize,
    DWORD* lpVolumeSerialNumber, DWORD* lpMaximumComponentLength,
    DWORD* lpFileSystemFlags, wchar_t* lpFileSystemNameBuffer, DWORD nFileSystemNameSize
) noexcept {
    return win32::GetVolumeInformationByHandleW(hFile, lpVolumeNameBuffer, nVolumeNameSize, lpVolumeSerialNumber, lpMaximumComponentLength, lpFileSystemFlags, lpFileSystemNameBuffer, nFileSystemNameSize);
}

inline BOOL WINAPI SetVolumeMountPointA(const char* lpszVolumeMountPoint, const char* lpszVolumeName) noexcept {
    return win32::SetVolumeMountPointA(lpszVolumeMountPoint, lpszVolumeName);
}

inline BOOL WINAPI DeleteVolumeMountPointA(const char* lpszVolumeMountPoint) noexcept {
    return win32::DeleteVolumeMountPointA(lpszVolumeMountPoint);
}

inline BOOL WINAPI SetVolumeLabelA(const char* lpRootPathName, const char* lpVolumeName) noexcept {
    return win32::SetVolumeLabelA(lpRootPathName, lpVolumeName);
}

inline BOOL WINAPI DefineDosDeviceA(DWORD dwFlags, const char* lpDeviceName, const char* lpTargetPath) noexcept {
    return win32::DefineDosDeviceA(dwFlags, lpDeviceName, lpTargetPath);
}

inline DWORD WINAPI QueryDosDeviceA(const char* lpDeviceName, char* lpTargetPath, DWORD ucchMax) noexcept {
    return win32::QueryDosDeviceA(lpDeviceName, lpTargetPath, ucchMax);
}

inline BOOL WINAPI GetDiskFreeSpaceExA(
    const char* lpDirectoryName, uint64_t* lpFreeBytesAvailableToCaller,
    uint64_t* lpTotalNumberOfBytes, uint64_t* lpTotalNumberOfFreeBytes
) noexcept {
    return win32::GetDiskFreeSpaceExA(lpDirectoryName, lpFreeBytesAvailableToCaller, lpTotalNumberOfBytes, lpTotalNumberOfFreeBytes);
}

inline DWORD WINAPI GetLogicalDriveStringsA(DWORD nBufferLength, char* lpBuffer) noexcept {
    return win32::GetLogicalDriveStringsA(nBufferLength, lpBuffer);
}

inline BOOL WINAPI CancelIoEx(HANDLE hFile, void* lpOverlapped) noexcept {
    return win32::CancelIoEx(hFile, lpOverlapped);
}

inline BOOL WINAPI CancelSynchronousIo(HANDLE hThread) noexcept {
    return win32::CancelSynchronousIo(hThread);
}

inline BOOL WINAPI GetOverlappedResultEx(
    HANDLE hFile, void* lpOverlapped, DWORD* lpNumberOfBytesTransferred,
    DWORD dwMilliseconds, BOOL bAlertable
) noexcept {
    return win32::GetOverlappedResultEx(hFile, lpOverlapped, lpNumberOfBytesTransferred, dwMilliseconds, bAlertable);
}

inline BOOL WINAPI SleepConditionVariableCS(void* ConditionVariable, void* CriticalSection, DWORD dwMilliseconds) noexcept {
    return win32::SleepConditionVariableCS(ConditionVariable, CriticalSection, dwMilliseconds);
}

inline BOOL WINAPI CreateSymbolicLinkW(const wchar_t* lpSymlinkFileName, const wchar_t* lpTargetFileName, DWORD dwFlags) noexcept {
    return win32::CreateSymbolicLinkW(lpSymlinkFileName, lpTargetFileName, dwFlags);
}

inline HWND WINAPI GetConsoleWindow() noexcept {
    return win32::GetConsoleWindow();
}

inline BOOL WINAPI EnumUILanguagesW(void* lpUILanguageEnumProc, DWORD dwFlags, LONG_PTR lParam) noexcept {
    return win32::EnumUILanguagesW(lpUILanguageEnumProc, dwFlags, lParam);
}

inline LCID WINAPI GetSystemDefaultLCID() noexcept {
    return win32::GetSystemDefaultLCID();
}

inline WORD WINAPI GetThreadUILanguage() noexcept {
    return win32::GetThreadUILanguage();
}

inline int WINAPI LCIDToLocaleName(LCID Locale, wchar_t* lpName, int cchName, DWORD dwFlags) noexcept {
    return win32::LCIDToLocaleName(Locale, lpName, cchName, dwFlags);
}

inline BOOL WINAPI SetDefaultDllDirectories(DWORD DirectoryFlags) noexcept {
    return win32::SetDefaultDllDirectories(DirectoryFlags);
}

inline BOOL WINAPI SetFileAttributesA(const char* lpFileName, DWORD dwFileAttributes) noexcept {
    return win32::SetFileAttributesA(lpFileName, dwFileAttributes);
}

inline BOOL WINAPI VerifyVersionInfoA(void* lpVersionInformation, DWORD dwTypeMask, uint64_t dwlConditionMask) noexcept {
    return win32::VerifyVersionInfoA(lpVersionInformation, dwTypeMask, dwlConditionMask);
}

inline DWORD WINAPI K32GetModuleFileNameExW(HANDLE hProcess, void* hModule, wchar_t* lpFilename, DWORD nSize) noexcept {
    return win32::K32GetModuleFileNameExW(hProcess, hModule, lpFilename, nSize);
}

inline DWORD WINAPI K32GetProcessImageFileNameW(HANDLE hProcess, wchar_t* lpImageFileName, DWORD nSize) noexcept {
    return win32::K32GetProcessImageFileNameW(hProcess, lpImageFileName, nSize);
}

// 5. ntdll.dll
inline NtStatus WINAPI NtAdjustPrivilegesToken(
    HANDLE TokenHandle, BOOLEAN DisableAllPrivileges, void* NewState,
    ULONG BufferLength, void* PreviousState, ULONG* ReturnLength
) noexcept {
    return ntdll::NtAdjustPrivilegesToken(TokenHandle, DisableAllPrivileges, NewState, BufferLength, PreviousState, ReturnLength);
}

inline NtStatus WINAPI NtCreateFile(
    HANDLE* FileHandle, uint32_t DesiredAccess, void* ObjectAttributes,
    void* IoStatusBlock, void* AllocationSize, uint32_t FileAttributes,
    uint32_t ShareAccess, uint32_t CreateDisposition, uint32_t CreateOptions,
    void* EaBuffer, uint32_t EaLength
) noexcept {
    return ntdll::NtCreateFile_Export(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, AllocationSize, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, EaBuffer, EaLength);
}

inline NtStatus WINAPI NtDelayExecution(BOOLEAN Alertable, const int64_t* DelayInterval) noexcept {
    return ntdll::NtDelayExecution_Export(Alertable, DelayInterval);
}

inline NtStatus WINAPI NtDeviceIoControlFile(
    HANDLE FileHandle, HANDLE Event, void* ApcRoutine, void* ApcContext,
    void* IoStatusBlock, uint32_t IoControlCode, const void* InputBuffer,
    uint32_t InputBufferLength, void* OutputBuffer, uint32_t OutputBufferLength
) noexcept {
    return ntdll::NtDeviceIoControlFile_Export(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, IoControlCode, InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength);
}

inline NtStatus WINAPI NtDuplicateObject(
    HANDLE SourceProcessHandle, HANDLE SourceHandle, HANDLE TargetProcessHandle,
    HANDLE* TargetHandle, uint32_t DesiredAccess, uint32_t HandleAttributes, uint32_t Options
) noexcept {
    return ntdll::NtDuplicateObject(SourceProcessHandle, SourceHandle, TargetProcessHandle, TargetHandle, DesiredAccess, HandleAttributes, Options);
}

inline NtStatus WINAPI NtFlushBuffersFile(HANDLE FileHandle, void* IoStatusBlock) noexcept {
    return ntdll::NtFlushBuffersFile(FileHandle, IoStatusBlock);
}

inline NtStatus WINAPI NtFsControlFile(
    HANDLE FileHandle, HANDLE Event, void* ApcRoutine, void* ApcContext,
    void* IoStatusBlock, uint32_t FsControlCode, const void* InputBuffer,
    uint32_t InputBufferLength, void* OutputBuffer, uint32_t OutputBufferLength
) noexcept {
    return ntdll::NtFsControlFile(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, FsControlCode, InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength);
}

inline NtStatus WINAPI NtOpenProcess(
    HANDLE* ProcessHandle, uint32_t DesiredAccess, void* ObjectAttributes, void* ClientId
) noexcept {
    return ntdll::NtOpenProcess(ProcessHandle, DesiredAccess, ObjectAttributes, ClientId);
}

inline NtStatus WINAPI NtOpenProcessToken(HANDLE ProcessHandle, uint32_t DesiredAccess, HANDLE* TokenHandle) noexcept {
    return ntdll::NtOpenProcessToken_Export(ProcessHandle, DesiredAccess, TokenHandle);
}

inline NtStatus WINAPI NtOpenSymbolicLinkObject(HANDLE* LinkHandle, uint32_t DesiredAccess, void* ObjectAttributes) noexcept {
    return ntdll::NtOpenSymbolicLinkObject(LinkHandle, DesiredAccess, ObjectAttributes);
}

inline NtStatus WINAPI NtQueryEaFile(
    HANDLE FileHandle, void* IoStatusBlock, void* Buffer, uint32_t Length,
    BOOLEAN ReturnSingleEntry, void* EaList, uint32_t EaListLength, uint32_t* EaIndex, BOOLEAN RestartScan
) noexcept {
    return ntdll::NtQueryEaFile(FileHandle, IoStatusBlock, Buffer, Length, ReturnSingleEntry, EaList, EaListLength, EaIndex, RestartScan);
}

inline NtStatus WINAPI NtQueryObject(
    HANDLE Handle, uint32_t ObjectInformationClass, void* ObjectInformation, uint32_t Length, uint32_t* ReturnLength
) noexcept {
    return ntdll::NtQueryObject(Handle, ObjectInformationClass, ObjectInformation, Length, ReturnLength);
}

inline NtStatus WINAPI NtQuerySecurityObject(
    HANDLE Handle, uint32_t SecurityInformation, void* SecurityDescriptor, uint32_t Length, uint32_t* LengthNeeded
) noexcept {
    return ntdll::NtQuerySecurityObject(Handle, SecurityInformation, SecurityDescriptor, Length, LengthNeeded);
}

inline NtStatus WINAPI NtQuerySystemInformation(
    uint32_t SystemInformationClass, void* SystemInformation, uint32_t SystemInformationLength, uint32_t* ReturnLength
) noexcept {
    return ntdll::NtQuerySystemInformation_Export(SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);
}

inline NtStatus WINAPI NtQueryVolumeInformationFile(
    HANDLE FileHandle, void* IoStatusBlock, void* FsInformation, uint32_t Length, uint32_t FsInformationClass
) noexcept {
    return ntdll::NtQueryVolumeInformationFile(FileHandle, IoStatusBlock, FsInformation, Length, FsInformationClass);
}

inline NtStatus WINAPI NtSetEaFile(HANDLE FileHandle, void* IoStatusBlock, void* Buffer, uint32_t Length) noexcept {
    return ntdll::NtSetEaFile(FileHandle, IoStatusBlock, Buffer, Length);
}

inline NtStatus WINAPI NtSetSecurityObject(HANDLE Handle, uint32_t SecurityInformation, void* SecurityDescriptor) noexcept {
    return ntdll::NtSetSecurityObject(Handle, SecurityInformation, SecurityDescriptor);
}

inline void WINAPI RtlCaptureContext(void* ContextRecord) noexcept {
    ntdll::RtlCaptureContext(ContextRecord);
}

inline void* WINAPI RtlLookupFunctionEntry(DWORD64 ControlPc, DWORD64* ImageBase, void* HistoryTable) noexcept {
    return ntdll::RtlLookupFunctionEntry(ControlPc, ImageBase, HistoryTable);
}

inline void* WINAPI RtlPcToFileHeader(void* PcValue, void** BaseOfImage) noexcept {
    return ntdll::RtlPcToFileHeader(PcValue, BaseOfImage);
}

inline void WINAPI RtlUnwind(void* TargetFrame, void* TargetIp, void* ExceptionRecord, void* ReturnValue) noexcept {
    ntdll::RtlUnwind(TargetFrame, TargetIp, ExceptionRecord, ReturnValue);
}

inline void WINAPI RtlUnwindEx(void* TargetFrame, void* TargetIp, void* ExceptionRecord, void* ReturnValue, void* ContextRecord, void* HistoryTable) noexcept {
    ntdll::RtlUnwindEx(TargetFrame, TargetIp, ExceptionRecord, ReturnValue, ContextRecord, HistoryTable);
}

inline DWORD64 WINAPI RtlVirtualUnwind(
    ULONG HandlerType, DWORD64 ImageBase, DWORD64 ControlPc,
    void* FunctionEntry, void* ContextRecord, void** HandlerData,
    DWORD64* EstablisherFrame, void* ContextPointers
) noexcept {
    return ntdll::RtlVirtualUnwind(HandlerType, ImageBase, ControlPc, FunctionEntry, ContextRecord, HandlerData, EstablisherFrame, ContextPointers);
}

inline uint64_t WINAPI VerSetConditionMask(uint64_t ConditionMask, uint32_t TypeMask, uint8_t Condition) noexcept {
    return ntdll::VerSetConditionMask(ConditionMask, TypeMask, Condition);
}

// 6. ole32.dll
inline HRESULT WINAPI CoInitializeSecurity(
    void* pSecDesc, int32_t cAuthSvc, void* asAuthSvc, void* pReserved1,
    DWORD dwAuthnLevel, DWORD dwImpLevel, void* pAuthList, DWORD dwCapabilities, void* pReserved3
) noexcept {
    return ole32::CoInitializeSecurity(pSecDesc, cAuthSvc, asAuthSvc, pReserved1, dwAuthnLevel, dwImpLevel, pAuthList, dwCapabilities, pReserved3);
}

// 7. setupapi.dll
inline DWORD WINAPI CM_Get_Child(ULONG* pdnDevInst, ULONG dnDevInst, ULONG ulFlags) noexcept {
    return setupapi::CM_Get_Child(pdnDevInst, dnDevInst, ulFlags);
}

inline DWORD WINAPI CM_Get_DevNode_Registry_PropertyA(
    ULONG dnDevInst, ULONG ulProperty, ULONG* pulRegDataType,
    void* Buffer, ULONG* pulLength, ULONG ulFlags
) noexcept {
    return setupapi::CM_Get_DevNode_Registry_PropertyA(dnDevInst, ulProperty, pulRegDataType, Buffer, pulLength, ulFlags);
}

inline DWORD WINAPI CM_Get_DevNode_Status(ULONG* pulStatus, ULONG* pulProblemNumber, ULONG dnDevInst, ULONG ulFlags) noexcept {
    return setupapi::CM_Get_DevNode_Status(pulStatus, pulProblemNumber, dnDevInst, ulFlags);
}

inline DWORD WINAPI CM_Get_Device_IDA(ULONG dnDevInst, char* Buffer, ULONG BufferLen, ULONG ulFlags) noexcept {
    return setupapi::CM_Get_Device_IDA(dnDevInst, Buffer, BufferLen, ulFlags);
}

inline DWORD WINAPI CM_Get_Device_ID_ListA(const char* pszFilter, char* Buffer, ULONG BufferLen, ULONG ulFlags) noexcept {
    return setupapi::CM_Get_Device_ID_ListA(pszFilter, Buffer, BufferLen, ulFlags);
}

inline DWORD WINAPI CM_Get_Device_ID_List_SizeA(ULONG* pulLen, const char* pszFilter, ULONG ulFlags) noexcept {
    return setupapi::CM_Get_Device_ID_List_SizeA(pulLen, pszFilter, ulFlags);
}

inline DWORD WINAPI CM_Get_Parent(ULONG* pdnDevInst, ULONG dnDevInst, ULONG ulFlags) noexcept {
    return setupapi::CM_Get_Parent(pdnDevInst, dnDevInst, ulFlags);
}

inline DWORD WINAPI CM_Get_Sibling(ULONG* pdnDevInst, ULONG dnDevInst, ULONG ulFlags) noexcept {
    return setupapi::CM_Get_Sibling(pdnDevInst, dnDevInst, ulFlags);
}

inline DWORD WINAPI CM_Locate_DevNodeA(ULONG* pdnDevInst, const char* pDeviceID, ULONG ulFlags) noexcept {
    return setupapi::CM_Locate_DevNodeA(pdnDevInst, pDeviceID, ulFlags);
}

inline BOOL WINAPI SetupDiChangeState(HDEVINFO DeviceInfoSet, void* DeviceInfoData) noexcept {
    return setupapi::SetupDiChangeState(DeviceInfoSet, DeviceInfoData);
}

inline HDEVINFO WINAPI SetupDiGetClassDevsA(const void* ClassGuid, const char* Enumerator, HWND hwndParent, DWORD Flags) noexcept {
    return setupapi::SetupDiGetClassDevsA(ClassGuid, Enumerator, hwndParent, Flags);
}

inline BOOL WINAPI SetupDiGetDeviceInstanceIdA(
    HDEVINFO DeviceInfoSet, void* DeviceInfoData, char* DeviceInstanceId,
    DWORD DeviceInstanceIdSize, DWORD* RequiredSize
) noexcept {
    return setupapi::SetupDiGetDeviceInstanceIdA(DeviceInfoSet, DeviceInfoData, DeviceInstanceId, DeviceInstanceIdSize, RequiredSize);
}

inline BOOL WINAPI SetupDiGetDeviceInterfaceDetailA(
    HDEVINFO DeviceInfoSet, void* DeviceInterfaceData, void* DeviceInterfaceDetailData,
    DWORD DeviceInterfaceDetailDataSize, DWORD* RequiredSize, void* DeviceInfoData
) noexcept {
    return setupapi::SetupDiGetDeviceInterfaceDetailA(DeviceInfoSet, DeviceInterfaceData, DeviceInterfaceDetailData, DeviceInterfaceDetailDataSize, RequiredSize, DeviceInfoData);
}

inline BOOL WINAPI SetupDiGetDeviceRegistryPropertyA(
    HDEVINFO DeviceInfoSet, void* DeviceInfoData, DWORD Property,
    DWORD* PropertyRegDataType, BYTE* PropertyBuffer, DWORD PropertyBufferSize, DWORD* RequiredSize
) noexcept {
    return setupapi::SetupDiGetDeviceRegistryPropertyA(DeviceInfoSet, DeviceInfoData, Property, PropertyRegDataType, PropertyBuffer, PropertyBufferSize, RequiredSize);
}

inline BOOL WINAPI SetupDiSetClassInstallParamsW(
    HDEVINFO DeviceInfoSet, void* DeviceInfoData, void* ClassInstallParams, DWORD ClassInstallParamsSize
) noexcept {
    return setupapi::SetupDiSetClassInstallParamsW(DeviceInfoSet, DeviceInfoData, ClassInstallParams, ClassInstallParamsSize);
}

// 8. shell32.dll
inline int WINAPI SHCreateDirectoryExA(HWND hwnd, const char* pszPath, void* psa) noexcept {
    return shell32::SHCreateDirectoryExA(hwnd, pszPath, psa);
}

inline int WINAPI SHCreateDirectoryExW(HWND hwnd, const wchar_t* pszPath, void* psa) noexcept {
    return shell32::SHCreateDirectoryExW(hwnd, pszPath, psa);
}

inline ULONG WINAPI SHChangeNotifyRegister_Ordinal2(
    HWND hwnd, int fSources, int32_t fEvents, uint32_t wMsg, int cItems, const void* pItems
) noexcept {
    return shell32::SHChangeNotifyRegister_Ordinal2(hwnd, fSources, fEvents, wMsg, cItems, pItems);
}

inline BOOL WINAPI SHChangeNotifyDeregister_Ordinal4(ULONG ulID) noexcept {
    return shell32::SHChangeNotifyDeregister_Ordinal4(ulID);
}

// 9. shlwapi.dll
inline int wnsprintfW(wchar_t* lpOut, int cchLimitIn, const wchar_t* lpFmt, ...) noexcept {
    if (!lpOut || cchLimitIn <= 0 || !lpFmt) return -1;
    va_list args;
    va_start(args, lpFmt);
    int written = std::vswprintf(lpOut, static_cast<size_t>(cchLimitIn), lpFmt, args);
    va_end(args);
    return written;
}

// 10. user32.dll
inline BOOL WINAPI ChangeWindowMessageFilterEx(HWND hwnd, uint32_t message, DWORD action, void* pChangeFilterStruct) noexcept {
    return user32::ChangeWindowMessageFilterEx(hwnd, message, action, pChangeFilterStruct);
}

inline char* WINAPI CharLowerA(char* lpsz) noexcept {
    return user32::CharLowerA(lpsz);
}

inline char* WINAPI CharUpperA(char* lpsz) noexcept {
    return user32::CharUpperA(lpsz);
}

inline HICON WINAPI CreateIconFromResourceEx(
    BYTE* pbIconBits, DWORD cbIconBits, BOOL fIcon, DWORD dwVersion,
    int cxDesired, int cyDesired, uint32_t uFlags
) noexcept {
    return user32::CreateIconFromResourceEx(pbIconBits, cbIconBits, fIcon, dwVersion, cxDesired, cyDesired, uFlags);
}

inline int WINAPI DrawTextExA(HDC hdc, char* lpchText, int cchText, void* lprc, uint32_t format, void* lpdtp) noexcept {
    return user32::DrawTextExA(hdc, lpchText, cchText, lprc, format, lpdtp);
}

inline BOOL WINAPI GetKeyboardLayoutNameA(char* pwszKLID) noexcept {
    return user32::GetKeyboardLayoutNameA(pwszKLID);
}

inline int WINAPI MessageBoxExW(HWND hWnd, const wchar_t* lpText, const wchar_t* lpCaption, uint32_t uType, WORD wLanguageId) noexcept {
    return user32::MessageBoxExW(hWnd, lpText, lpCaption, uType, wLanguageId);
}

inline BOOL WINAPI SetProcessDefaultLayout(DWORD dwDefaultLayout) noexcept {
    return user32::SetProcessDefaultLayout(dwDefaultLayout);
}

inline HWINEVENTHOOK WINAPI SetWinEventHook(
    DWORD eventMin, DWORD eventMax, void* hmodWinEventProc,
    void* pfnWinEventProc, DWORD idProcess, DWORD idThread, DWORD dwFlags
) noexcept {
    return user32::SetWinEventHook(eventMin, eventMax, hmodWinEventProc, pfnWinEventProc, idProcess, idThread, dwFlags);
}

inline BOOL WINAPI UnhookWinEvent(HWINEVENTHOOK hWinEventHook) noexcept {
    return user32::UnhookWinEvent(hWinEventHook);
}

// 11. virtdisk.dll
inline uint32_t WINAPI GetVirtualDiskOperationProgress(void* VirtualDiskHandle, void* Overlapped, void* Progress) noexcept {
    return virtdisk::GetVirtualDiskOperationProgress(VirtualDiskHandle, Overlapped, Progress);
}

// 12. wininet.dll
inline BOOL WINAPI InternetGetConnectedState(DWORD* lpdwFlags, DWORD dwReserved) noexcept {
    return wininet::InternetGetConnectedState(lpdwFlags, dwReserved);
}

// 13. wintrust.dll
inline int32_t WINAPI WinVerifyTrustEx(HWND hwnd, void* pgActionID, void* pWVTData) noexcept {
    return wintrust::WinVerifyTrustEx(hwnd, pgActionID, pWVTData);
}

// ============================================================================
// Master Dynamic Loader Registration Bridge (0 registered satellite exports)
// ============================================================================

inline void InitializeRufusExports() {
    // 0 exports defined in satellite header.
    // All 100 Rufus exports have graduated into canonical Core NT headers!
}

} // namespace micant::satellite::rufus
