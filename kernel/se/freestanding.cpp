#include "micant/km/executive/se.hpp"

namespace micant::se {
namespace {
ob::ObjectType g_FreestandingTokenType{};

void deleteFreestandingToken(ob::ObjectHeader* header) {
    header->as<FreestandingAccessToken>()->~FreestandingAccessToken();
}

SidValue makeSid(uint32_t authority, uint8_t count,
                uint32_t a = 0, uint32_t b = 0, uint32_t c = 0) noexcept {
    SidValue sid{};
    sid.SubAuthorityCount = count;
    sid.IdentifierAuthority[5] = static_cast<uint8_t>(authority);
    sid.IdentifierAuthority[4] = static_cast<uint8_t>(authority >> 8);
    sid.IdentifierAuthority[3] = static_cast<uint8_t>(authority >> 16);
    sid.IdentifierAuthority[2] = static_cast<uint8_t>(authority >> 24);
    if (count > 0) sid.SubAuthority[0] = a;
    if (count > 1) sid.SubAuthority[1] = b;
    if (count > 2) sid.SubAuthority[2] = c;
    return sid;
}

void addGroup(FreestandingAccessToken& token, const SidValue& sid) noexcept {
    if (token.GroupCount >= token.Groups.size()) __builtin_trap();
    token.Groups[token.GroupCount++] = TokenGroupValue{sid, 0x00000004}; // SE_GROUP_ENABLED
}

void initializeTokenType() noexcept {
    if (g_FreestandingTokenType.typeId == ob::ObjectTypeId::Token) return;
    g_FreestandingTokenType.typeId = ob::ObjectTypeId::Token;
    g_FreestandingTokenType.validAccessMask = 0x000F01FF;
    g_FreestandingTokenType.genericMapping = {
        ob::READ_CONTROL | 0x0008,
        ob::READ_CONTROL | 0x0002 | 0x0001 | 0x0020 | 0x0040,
        ob::READ_CONTROL,
        ob::STANDARD_RIGHTS_REQUIRED | 0x01FF
    };
    g_FreestandingTokenType.deleteProcedure = deleteFreestandingToken;
}
}

bool SidValue::equals(const SidValue& other) const noexcept {
    if (Revision != other.Revision || SubAuthorityCount != other.SubAuthorityCount ||
        SubAuthorityCount > std::size(SubAuthority)) return false;
    for (size_t i = 0; i < sizeof(IdentifierAuthority); ++i)
        if (IdentifierAuthority[i] != other.IdentifierAuthority[i]) return false;
    for (size_t i = 0; i < SubAuthorityCount; ++i)
        if (SubAuthority[i] != other.SubAuthority[i]) return false;
    return true;
}

bool FreestandingAccessToken::hasEnabledGroup(const SidValue& sid) const noexcept {
    for (size_t i = 0; i < GroupCount && i < Groups.size(); ++i)
        if ((Groups[i].Attributes & 0x00000004) && Groups[i].Sid.equals(sid)) return true;
    return false;
}

bool FreestandingAccessToken::matchesSid(const SidValue& sid, bool includeDenyOnly) const noexcept {
    if (UserSid.equals(sid)) return true;
    for (size_t i = 0; i < GroupCount && i < Groups.size(); ++i) {
        const uint32_t attributes = Groups[i].Attributes;
        if (Groups[i].Sid.equals(sid) &&
            (((attributes & 0x00000004) != 0) ||
             (includeDenyOnly && (attributes & 0x00000010) != 0))) return true;
    }
    return false;
}

NtStatus SeCreateFreestandingPrimaryToken(bool system,
    FreestandingAccessToken*& token, ob::ObjectHeader*& reference) noexcept {
    token = nullptr;
    reference = nullptr;
    initializeTokenType();
    void* body{};
    ob::ObjectHeader* header{};
    const auto status = ob::ObCreateObject(ob::KernelMode, &g_FreestandingTokenType,
        nullptr, sizeof(FreestandingAccessToken), &body, &header);
    if (status != NtStatus::Success) return status;
    auto* created = new (body) FreestandingAccessToken{};
    created->IsSystem = system;
    created->IntegrityLevel = system ? 0x4000 : 0x1000;
    if (system) {
        created->UserSid = makeSid(5, 1, 18); // NT AUTHORITY\SYSTEM
        addGroup(*created, makeSid(5, 2, 32, 544)); // BUILTIN\Administrators
        addGroup(*created, makeSid(5, 2, 32, 556)); // Network Configuration Operators
        addGroup(*created, makeSid(5, 1, 11)); // Authenticated Users
        addGroup(*created, makeSid(1, 1, 0)); // Everyone
        // SeAssignPrimaryTokenPrivilege and SeTcbPrivilege, using their
        // documented LUID low parts as the bounded bitmap index.
        created->EnabledPrivileges |= (uint64_t{1} << 3) | (uint64_t{1} << 7);
    } else {
        created->UserSid = makeSid(5, 1, 7); // NT AUTHORITY\ANONYMOUS LOGON
        addGroup(*created, makeSid(1, 1, 0)); // Everyone
    }
    token = created;
    reference = header;
    return NtStatus::Success;
}

void SeDereferenceFreestandingToken(ob::ObjectHeader* reference) noexcept {
    if (reference) ob::ObDereferenceObject(reference);
}

const FreestandingAccessToken* SeGetEffectiveFreestandingToken(
    const FreestandingAccessToken* primary,
    const FreestandingAccessToken* impersonation) noexcept {
    if (impersonation && !impersonation->IsPrimary) return impersonation;
    return primary;
}

bool SeAccessCheckFreestanding(const FreestandingAccessToken* token,
    const FreestandingSecurityDescriptor& descriptor, uint32_t desiredAccess,
    uint32_t& grantedAccess) noexcept {
    grantedAccess = 0;
    if (!token || descriptor.AceCount > descriptor.Dacl.size()) return false;
    uint32_t remaining = desiredAccess;
    if (token->UserSid.equals(descriptor.Owner)) {
        const uint32_t ownerRights = ob::READ_CONTROL | ob::WRITE_DAC;
        grantedAccess |= desiredAccess & ownerRights;
        remaining &= ~ownerRights;
    }
    for (size_t i = 0; i < descriptor.AceCount && remaining; ++i) {
        const auto& ace = descriptor.Dacl[i];
        const bool matched = token->matchesSid(ace.Sid, ace.Type == FreestandingAceType::Deny);
        if (!matched) continue;
        const uint32_t relevant = ace.AccessMask & remaining;
        if (!relevant) continue;
        if (ace.Type == FreestandingAceType::Deny) {
            grantedAccess = 0;
            return false;
        }
        remaining &= ~relevant;
        grantedAccess |= relevant;
    }
    return remaining == 0;
}

void SeInitializeNetworkConfigurationAcl(FreestandingSecurityDescriptor& descriptor) noexcept {
    descriptor = {};
    descriptor.Owner = makeSid(5, 1, 18); // NT AUTHORITY\SYSTEM
    descriptor.Dacl[0] = {makeSid(5, 1, 18), SE_NETWORK_CONFIG_WRITE, FreestandingAceType::Allow};
    descriptor.Dacl[1] = {makeSid(5, 2, 32, 544), SE_NETWORK_CONFIG_WRITE, FreestandingAceType::Allow};
    descriptor.Dacl[2] = {makeSid(5, 2, 32, 556), SE_NETWORK_CONFIG_WRITE, FreestandingAceType::Allow};
    descriptor.AceCount = 3;
}
}
