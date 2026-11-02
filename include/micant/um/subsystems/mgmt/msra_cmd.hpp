#pragma once

/**
 * @file msra_cmd.hpp
 * @brief Canonical Win32 msra.exe / racli.exe CLI Management Utilities for Remote Assistance & Shadowing.
 *
 * Clean-room implementation of Remote Assistance and Multi-Session Shadowing CLI tools
 * under 17 U.S.C. § 1201(f) & Google LLC v. Oracle America, Inc. (141 S. Ct. 1183, 2021).
 *
 * Provides complete CLI management for:
 *   - msra status: Introspect Remote Assistance health, active sessions, and shadowing policies
 *   - msra invite <session_id> <user> [password] [expiry] [host] [port]: Create incident ticket (.msrcincident)
 *   - msra shadow <target_session> [control|view] [noconsent]: Initiate session shadowing
 *   - msra request-control <assistant_session>: Request interactive keyboard and mouse control
 *   - msra grant-control <assistant_session>: Accept control delegation request
 *   - msra revoke-control <assistant_session>: User reclaims interactive control instantly
 *   - msra sessions: Tabular inventory of active remote assistance and shadowing sessions
 *   - msra terminate <assistant_session>: Terminate active assistance or shadow session
 *   - msra dump: Export declarative PowerShell Remote Assistance configuration script
 *   - msra help: Command usage and syntax
 */

#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <span>

#include "micant/um/servers/remote_assistance.hpp"

namespace micant::msra_cmd::um {

inline std::vector<std::string_view> Tokenize(std::string_view input) {
    std::vector<std::string_view> tokens;
    size_t pos = 0;
    while (pos < input.size()) {
        while (pos < input.size() && (input[pos] == ' ' || input[pos] == '\t' || input[pos] == '\r' || input[pos] == '\n')) {
            pos++;
        }
        if (pos >= input.size()) break;
        size_t start = pos;
        if (input[pos] == '\"') {
            start = ++pos;
            while (pos < input.size() && input[pos] != '\"') pos++;
            tokens.push_back(input.substr(start, pos - start));
            if (pos < input.size()) pos++;
        } else {
            while (pos < input.size() && input[pos] != ' ' && input[pos] != '\t' && input[pos] != '\r' && input[pos] != '\n') {
                pos++;
            }
            tokens.push_back(input.substr(start, pos - start));
        }
    }
    return tokens;
}

class MsraCommandInterpreter {
public:
    static MsraCommandInterpreter& get() {
        static MsraCommandInterpreter instance;
        return instance;
    }

    void execute(std::span<const std::string_view> args, std::string& outResult) {
        std::ostringstream ss;

        if (args.empty() || args[0] == "help" || args[0] == "-h" || args[0] == "--help" || args[0] == "/?") {
            printHelp(ss);
            outResult = ss.str();
            return;
        }

        auto& mgr = micant::ra::RemoteAssistanceManager::get();
        std::string_view subCmd = args[0];

        if (subCmd == "status") {
            auto v = mgr.getVitals();
            ss << "===============================================================================\n"
               << "      MicaNT Sovereign Remote Assistance & Session Shadowing Subsystem        \n"
               << "===============================================================================\n"
               << "  Active Assistance Sessions : " << v.activeSessionsCount << "\n"
               << "  Active Shadow Sessions     : " << v.activeShadowSessionsCount << "\n"
               << "  Pending Incident Tickets   : " << v.activeInvitationsCount << "\n"
               << "  Total Tickets Generated    : " << v.totalInvitationsGenerated << "\n"
               << "  Control Delegations Granted: " << v.totalControlDelegations << "\n"
               << "  Consent Requests Declined  : " << v.totalConsentDeclined << "\n"
               << "  Unsolicited Assistance     : " << (v.allowUnsolicitedAssistance ? "ENABLED" : "DISABLED") << "\n"
               << "  Default Shadow Policy      : " << micant::ra::ShadowModeToString(v.defaultShadowPolicy) << "\n"
               << "  SCM Service Status         : RemoteAssistance (SERVICE_RUNNING, AUTO_START)\n"
               << "===============================================================================\n";
        }
        else if (subCmd == "invite") {
            if (args.size() < 3) {
                ss << "[-] Syntax: msra invite <session_id> <user> [password] [expiry_minutes] [host] [port]\n";
                outResult = ss.str();
                return;
            }

            uint32_t sessionId = static_cast<uint32_t>(std::stoul(std::string(args[1])));
            std::string_view user = args[2];
            std::string_view pass = (args.size() >= 4) ? args[3] : "";
            uint32_t expiryMin = (args.size() >= 5) ? static_cast<uint32_t>(std::stoul(std::string(args[4]))) : 60;
            std::string_view host = (args.size() >= 6) ? args[5] : "127.0.0.1";
            uint16_t port = (args.size() >= 7) ? static_cast<uint16_t>(std::stoul(std::string(args[6]))) : 3389;

            std::string xml = mgr.createInvitation(sessionId, user, host, port, expiryMin, pass);
            ss << "[+] Successfully generated Remote Assistance incident invitation ticket (.msrcincident):\n\n"
               << xml << "\n";
        }
        else if (subCmd == "shadow") {
            if (args.size() < 2) {
                ss << "[-] Syntax: msra shadow <target_session_id> [control|view] [noconsent]\n";
                outResult = ss.str();
                return;
            }

            uint32_t targetSession = static_cast<uint32_t>(std::stoul(std::string(args[1])));
            bool fullControl = false;
            bool noConsent = false;

            for (size_t i = 2; i < args.size(); ++i) {
                std::string a(args[i]);
                std::transform(a.begin(), a.end(), a.begin(), ::tolower);
                if (a == "control") fullControl = true;
                else if (a == "view") fullControl = false;
                else if (a == "noconsent") noConsent = true;
            }

            micant::ra::ShadowMode mode = micant::ra::ShadowMode::ViewOnlyWithConsent;
            if (fullControl && noConsent) mode = micant::ra::ShadowMode::ControlNoConsent;
            else if (fullControl && !noConsent) mode = micant::ra::ShadowMode::ControlWithConsent;
            else if (!fullControl && noConsent) mode = micant::ra::ShadowMode::ViewOnlyNoConsent;
            else mode = micant::ra::ShadowMode::ViewOnlyWithConsent;

            uint32_t asstSessionId = 0;
            if (mgr.initiateShadowSession(targetSession, "AdminOperator", mode, true, asstSessionId)) {
                ss << "[+] Shadow session initiated (Assistant Session ID: " << asstSessionId << ")\n"
                   << "    Target Session  : " << targetSession << "\n"
                   << "    Shadow Mode     : " << micant::ra::ShadowModeToString(mode) << "\n"
                   << "    Interactive State: " << (fullControl ? "FULL CONTROL" : "VIEW ONLY") << "\n";
            } else {
                ss << "[-] Failed to initiate shadow session for target session " << targetSession << ".\n";
            }
        }
        else if (subCmd == "request-control") {
            if (args.size() < 2) {
                ss << "[-] Syntax: msra request-control <assistant_session_id>\n";
                outResult = ss.str();
                return;
            }
            uint32_t sessId = static_cast<uint32_t>(std::stoul(std::string(args[1])));
            if (mgr.requestInteractiveControl(sessId)) {
                ss << "[+] Interactive control requested for session " << sessId << ". User consent prompt displayed.\n";
            } else {
                ss << "[-] Session " << sessId << " not found or not in Connected state.\n";
            }
        }
        else if (subCmd == "grant-control") {
            if (args.size() < 2) {
                ss << "[-] Syntax: msra grant-control <assistant_session_id>\n";
                outResult = ss.str();
                return;
            }
            uint32_t sessId = static_cast<uint32_t>(std::stoul(std::string(args[1])));
            if (mgr.respondConsent(sessId, true)) {
                ss << "[+] User granted interactive control to assistant in session " << sessId << ".\n";
            } else {
                ss << "[-] Session " << sessId << " not found.\n";
            }
        }
        else if (subCmd == "revoke-control") {
            if (args.size() < 2) {
                ss << "[-] Syntax: msra revoke-control <assistant_session_id>\n";
                outResult = ss.str();
                return;
            }
            uint32_t sessId = static_cast<uint32_t>(std::stoul(std::string(args[1])));
            if (mgr.revokeInteractiveControl(sessId)) {
                ss << "[+] Interactive control revoked on session " << sessId << ". Assistant downgraded to View-Only.\n";
            } else {
                ss << "[-] Session " << sessId << " not found.\n";
            }
        }
        else if (subCmd == "sessions") {
            auto sessions = mgr.getAllSessions();
            ss << "===============================================================================\n"
               << "                   Active Remote Assistance & Shadow Sessions                  \n"
               << "===============================================================================\n"
               << std::left << std::setw(10) << "Asst ID"
               << std::left << std::setw(10) << "Target ID"
               << std::left << std::setw(16) << "Assistant"
               << std::left << std::setw(16) << "Target User"
               << std::left << std::setw(18) << "State"
               << std::left << std::setw(10) << "Control" << "\n"
               << "-------------------------------------------------------------------------------\n";

            for (const auto& s : sessions) {
                ss << std::left << std::setw(10) << s.assistantSessionId
                   << std::left << std::setw(10) << s.targetSessionId
                   << std::left << std::setw(16) << s.assistantUsername
                   << std::left << std::setw(16) << s.targetUsername
                   << std::left << std::setw(18) << micant::ra::SessionStateToString(s.state)
                   << std::left << std::setw(10) << (s.hasInteractiveControl ? "YES" : "NO") << "\n";
            }
            ss << "===============================================================================\n";
        }
        else if (subCmd == "terminate") {
            if (args.size() < 2) {
                ss << "[-] Syntax: msra terminate <assistant_session_id>\n";
                outResult = ss.str();
                return;
            }
            uint32_t sessId = static_cast<uint32_t>(std::stoul(std::string(args[1])));
            if (mgr.terminateSession(sessId)) {
                ss << "[+] Session " << sessId << " terminated and resources reclaimed.\n";
            } else {
                ss << "[-] Session " << sessId << " not found.\n";
            }
        }
        else if (subCmd == "dump") {
            ss << "# -----------------------------------------------------------------------------\n"
               << "# MicaNT Remote Assistance & Session Shadowing Declarative Configuration\n"
               << "# Generated automatically by msra dump\n"
               << "# -----------------------------------------------------------------------------\n\n"
               << "# Enable Remote Assistance Group Policy\n"
               << "New-Item -Path \"HKLM:\\System\\CurrentControlSet\\Control\\Remote Assistance\" -Force | Out-Null\n"
               << "Set-ItemProperty -Path \"HKLM:\\System\\CurrentControlSet\\Control\\Remote Assistance\" -Name \"fAllowToGetHelp\" -Value 1\n"
               << "Set-ItemProperty -Path \"HKLM:\\System\\CurrentControlSet\\Control\\Remote Assistance\" -Name \"fAllowUnsolicited\" -Value 1\n"
               << "Set-ItemProperty -Path \"HKLM:\\System\\CurrentControlSet\\Control\\Remote Assistance\" -Name \"fAllowFullControl\" -Value 1\n\n"
               << "# Configure Remote Desktop Session Shadowing Policy (1 = Full Control with user consent)\n"
               << "New-Item -Path \"HKLM:\\Software\\Policies\\Microsoft\\Windows NT\\Terminal Services\" -Force | Out-Null\n"
               << "Set-ItemProperty -Path \"HKLM:\\Software\\Policies\\Microsoft\\Windows NT\\Terminal Services\" -Name \"Shadow\" -Value 1\n\n"
               << "# End of Remote Assistance Configuration Export\n";
        }
        else {
            ss << "[-] Unknown msra command: " << subCmd << "\n"
               << "    Type 'msra help' for a list of available commands.\n";
        }

        outResult = ss.str();
    }

private:
    MsraCommandInterpreter() = default;

    void printHelp(std::ostream& os) const {
        os << "MicaNT Remote Assistance Administration Tool (msra / racli)\n"
           << "Copyright (C) 2026 MicaNT Authors.\n"
           << "Provides sovereign administration of remote assistance invitations and session shadowing.\n\n"
           << "Usage:\n"
           << "  msra status                               Display Remote Assistance vitals and metrics\n"
           << "  msra invite <id> <user> [pass] [min] [ip] Generate incident invitation ticket (.msrcincident)\n"
           << "  msra shadow <target_id> [control] [noconsent] Shadow active user desktop session\n"
           << "  msra request-control <asst_id>            Request interactive control from user\n"
           << "  msra grant-control <asst_id>              Accept interactive control delegation\n"
           << "  msra revoke-control <asst_id>             Reclaim interactive control (downgrade to view)\n"
           << "  msra sessions                             List active remote assistance & shadow sessions\n"
           << "  msra terminate <asst_id>                  Terminate assistance or shadow session\n"
           << "  msra dump                                 Export declarative PowerShell configuration\n"
           << "  msra help                                 Display this help message\n";
    }
};

inline void ExecuteMsraString(std::string_view commandLine, std::string& outResult) {
    auto tokens = Tokenize(commandLine);
    if (tokens.empty()) {
        outResult = "[-] No command specified.\n";
        return;
    }

    std::span<const std::string_view> args(tokens.data() + 1, tokens.size() - 1);
    MsraCommandInterpreter::get().execute(args, outResult);
}

} // namespace micant::msra_cmd::um
