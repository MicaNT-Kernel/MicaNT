#include "micant/km/executive/se.hpp"

namespace {
micant::se::SidValue makeTestSid(uint32_t rid) noexcept {
    micant::se::SidValue sid{};
    sid.SubAuthorityCount = 1;
    sid.IdentifierAuthority[5] = 5;
    sid.SubAuthority[0] = rid;
    return sid;
}
}

extern "C" bool MicantTestSeFreestandingAccess() noexcept {
    using namespace micant;
    using namespace micant::se;
    FreestandingAccessToken token{};
    token.UserSid = makeTestSid(21);
    FreestandingSecurityDescriptor descriptor{};
    descriptor.AceCount = 2;
    descriptor.Dacl[0] = {token.UserSid, 0x2, FreestandingAceType::Deny};
    descriptor.Dacl[1] = {token.UserSid, 0x3, FreestandingAceType::Allow};

    uint32_t granted{};
    if (SeAccessCheckFreestanding(&token, descriptor, 0x1, granted) != true || granted != 0x1)
        return false;
    if (SeAccessCheckFreestanding(&token, descriptor, 0x2, granted) || granted != 0)
        return false;
    if (!SeMaximumAccessFreestanding(&token, descriptor, 0x3, granted) || granted != 0x1)
        return false;

    // ACE order is significant: an earlier allow satisfies the requested bit
    // before a later deny is considered.
    descriptor.Dacl[0] = {token.UserSid, 0x3, FreestandingAceType::Allow};
    descriptor.Dacl[1] = {token.UserSid, 0x2, FreestandingAceType::Deny};
    if (!SeAccessCheckFreestanding(&token, descriptor, 0x2, granted) || granted != 0x2 ||
        !SeMaximumAccessFreestanding(&token, descriptor, 0x3, granted) || granted != 0x3)
        return false;

    // Enabled groups may grant access; deny-only groups may deny but never
    // satisfy an allow ACE, including in MAXIMUM_ALLOWED calculation.
    const auto enabledGroup = makeTestSid(22);
    const auto denyOnlyGroup = makeTestSid(23);
    token.GroupCount = 2;
    token.Groups[0] = {enabledGroup, 0x00000004}; // SE_GROUP_ENABLED
    token.Groups[1] = {denyOnlyGroup, 0x00000010}; // SE_GROUP_USE_FOR_DENY_ONLY
    descriptor.Dacl[0] = {enabledGroup, 0x4, FreestandingAceType::Allow};
    descriptor.Dacl[1] = {denyOnlyGroup, 0x8, FreestandingAceType::Allow};
    if (!SeAccessCheckFreestanding(&token, descriptor, 0x4, granted) || granted != 0x4 ||
        SeAccessCheckFreestanding(&token, descriptor, 0x8, granted) || granted != 0 ||
        !SeMaximumAccessFreestanding(&token, descriptor, 0xC, granted) || granted != 0x4)
        return false;
    descriptor.Dacl[1] = {denyOnlyGroup, 0x8, FreestandingAceType::Deny};
    if (SeAccessCheckFreestanding(&token, descriptor, 0x8, granted) || granted != 0)
        return false;

    // Malformed tokens and ACEs fail closed instead of silently truncating
    // groups or treating an unknown ACE type as an allow.
    const auto savedType = descriptor.Dacl[0].Type;
    descriptor.Dacl[0].Type = static_cast<FreestandingAceType>(0xff);
    if (SeAccessCheckFreestanding(&token, descriptor, 0x4, granted) || granted != 0 ||
        SeMaximumAccessFreestanding(&token, descriptor, 0x4, granted) || granted != 0)
        return false;
    descriptor.Dacl[0].Type = savedType;
    token.GroupCount = token.Groups.size() + 1;
    if (SeAccessCheckFreestanding(&token, descriptor, 0x4, granted) || granted != 0 ||
        SeMaximumAccessFreestanding(&token, descriptor, 0x4, granted) || granted != 0)
        return false;
    token.GroupCount = 2;
    token.UserSid.SubAuthorityCount = 0xff;
    if (SeAccessCheckFreestanding(&token, descriptor, 0x4, granted) || granted != 0)
        return false;
    token.UserSid = makeTestSid(21);
    token.GroupCount = 0;

    descriptor.DaclPresent = false;
    if (!SeAccessCheckFreestanding(&token, descriptor, 0x3, granted) || granted != 0x3 ||
        !SeMaximumAccessFreestanding(&token, descriptor, 0x3, granted) || granted != 0x3 ||
        SeAccessCheckFreestanding(nullptr, descriptor, 0x1, granted)) return false;
    return true;
}
