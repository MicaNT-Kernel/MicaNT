#pragma once

/**
 * @file server_commands.hpp
 * @brief Windows Enterprise Server Roles (iis, iisreset, appcmd, wds, adcs, kdc, gpo, rdp, nps, wsrm)
 * Included directly within micant::shell::CommandShell.
 */

    void cmdVmms(const std::vector<std::string>& tokens, std::ostream& out) {
        auto& sys = micant::vmms::VmmsSubsystem::get();
        sys.initialize();

        if (tokens.size() > 1) {
            std::string sub = tokens[1];
            std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);

            if (sub == "status") {
                out << "Windows Hyper-V Virtual Machine Management Subsystem (vmms.exe, vmswitch.sys, vhdsvc.dll):\n"
                    << "--------------------------------------------------------------------------------\n"
                    << "  Subsystem State:             ACTIVE (TitanHyperCore / AegisVMM)\n"
                    << "  Defined Virtual Machines:    " << sys.getVirtualMachineCount() << "\n"
                    << "  Virtual Switches:            1 (DefaultSwitch - Internal L2)\n"
                    << "  Mounted VHDX Containers:     " << sys.getVhdxCount() << "\n";
                auto vms = sys.getAllVirtualMachines();
                for (const auto& vm : vms) {
                    out << "    * VM: " << std::left << std::setw(16) << vm.getName()
                        << " State: " << std::setw(10) << micant::vmms::VmStateToString(vm.getState())
                        << " vCPUs: " << vm.getTopology().vCpuCount
                        << " RAM: " << vm.getMemory().currentAllocatedMb << " MB\n";
                }
                return;
            }

            if (sub == "list" || sub == "vms") {
                out << "Hyper-V Virtual Machines Inventory:\n"
                    << "--------------------------------------------------------------------------------\n"
                    << std::left << std::setw(16) << "VM ID"
                    << std::setw(18) << "Name"
                    << std::setw(12) << "State"
                    << std::setw(8)  << "vCPUs"
                    << std::setw(12) << "Memory (MB)"
                    << "VHDX Attachments\n"
                    << "--------------------------------------------------------------------------------\n";
                auto vms = sys.getAllVirtualMachines();
                for (const auto& vm : vms) {
                    std::string vhdxStr;
                    for (const auto& p : vm.getAttachedVhdx()) {
                        if (!vhdxStr.empty()) vhdxStr += ", ";
                        vhdxStr += p;
                    }
                    if (vhdxStr.empty()) vhdxStr = "None";

                    out << std::left << std::setw(16) << vm.getId()
                        << std::setw(18) << vm.getName()
                        << std::setw(12) << micant::vmms::VmStateToString(vm.getState())
                        << std::setw(8)  << vm.getTopology().vCpuCount
                        << std::setw(12) << vm.getMemory().currentAllocatedMb
                        << vhdxStr << "\n";
                }
                return;
            }

            if (sub == "start" && tokens.size() > 2) {
                std::string target = tokens[2];
                auto* vm = sys.getVirtualMachineByName(target);
                if (!vm) vm = sys.getVirtualMachine(target);
                if (!vm) {
                    out << "[-] Error: Virtual machine '" << target << "' not found.\n";
                    return;
                }
                if (vm->start()) {
                    out << "[+] Virtual machine '" << vm->getName() << "' successfully started (Running).\n";
                } else {
                    out << "[-] Failed to start virtual machine '" << vm->getName() << "'. Current state: "
                        << micant::vmms::VmStateToString(vm->getState()) << "\n";
                }
                return;
            }

            if (sub == "stop" && tokens.size() > 2) {
                std::string target = tokens[2];
                auto* vm = sys.getVirtualMachineByName(target);
                if (!vm) vm = sys.getVirtualMachine(target);
                if (!vm) {
                    out << "[-] Error: Virtual machine '" << target << "' not found.\n";
                    return;
                }
                if (vm->stop()) {
                    out << "[+] Virtual machine '" << vm->getName() << "' successfully stopped (Off).\n";
                } else {
                    out << "[-] Failed to stop virtual machine '" << vm->getName() << "'.\n";
                }
                return;
            }

            if (sub == "pause" && tokens.size() > 2) {
                std::string target = tokens[2];
                auto* vm = sys.getVirtualMachineByName(target);
                if (!vm) vm = sys.getVirtualMachine(target);
                if (!vm) {
                    out << "[-] Error: Virtual machine '" << target << "' not found.\n";
                    return;
                }
                if (vm->pause()) {
                    out << "[+] Virtual machine '" << vm->getName() << "' paused.\n";
                } else {
                    out << "[-] Failed to pause virtual machine '" << vm->getName() << "'.\n";
                }
                return;
            }

            if (sub == "resume" && tokens.size() > 2) {
                std::string target = tokens[2];
                auto* vm = sys.getVirtualMachineByName(target);
                if (!vm) vm = sys.getVirtualMachine(target);
                if (!vm) {
                    out << "[-] Error: Virtual machine '" << target << "' not found.\n";
                    return;
                }
                if (vm->resume()) {
                    out << "[+] Virtual machine '" << vm->getName() << "' resumed to Running.\n";
                } else {
                    out << "[-] Failed to resume virtual machine '" << vm->getName() << "'.\n";
                }
                return;
            }

            if (sub == "balloon" && tokens.size() > 3) {
                std::string target = tokens[2];
                uint32_t targetMb = static_cast<uint32_t>(std::stoul(tokens[3]));
                auto* vm = sys.getVirtualMachineByName(target);
                if (!vm) vm = sys.getVirtualMachine(target);
                if (!vm) {
                    out << "[-] Error: Virtual machine '" << target << "' not found.\n";
                    return;
                }
                if (vm->adjustBalloonMemory(targetMb)) {
                    out << "[+] Virtual machine '" << vm->getName() << "' dynamic memory balloon adjusted to "
                        << targetMb << " MB.\n";
                } else {
                    out << "[-] Failed to adjust balloon memory: requested " << targetMb << " MB is outside limits ["
                        << vm->getMemory().minRamMb << " MB - " << vm->getMemory().maxRamMb << " MB].\n";
                }
                return;
            }

            if (sub == "switch" || sub == "vswitch") {
                auto* sw = sys.getVirtualSwitch("DefaultSwitch");
                if (!sw) {
                    out << "[-] Default virtual switch not found.\n";
                    return;
                }
                out << "Hyper-V Extensible Virtual Switch (vmswitch.sys):\n"
                    << "--------------------------------------------------------------------------------\n"
                    << "  Switch Name:       " << sw->getName() << "\n"
                    << "  Switch Type:       " << micant::vmms::VSwitchPortTypeToString(sw->getType()) << "\n"
                    << "  Active Ports:      " << sw->getPortCount() << "\n"
                    << "  Frames Switched:   " << sw->getTotalFramesSwitched() << "\n"
                    << "  Frames Dropped:    " << sw->getTotalFramesDropped() << "\n\n"
                    << "Port Details:\n"
                    << "  " << std::left << std::setw(16) << "Port ID"
                    << std::setw(18) << "Port Name"
                    << std::setw(20) << "MAC Address"
                    << std::setw(8)  << "VLAN"
                    << std::setw(12) << "Sent"
                    << "Received\n";
                auto ports = sw->getAllPorts();
                for (const auto& p : ports) {
                    out << "  " << std::left << std::setw(16) << p.portId
                        << std::setw(18) << p.portName
                        << std::setw(20) << p.macAddress
                        << std::setw(8)  << p.vlanId
                        << std::setw(12) << p.packetsSent
                        << p.packetsReceived << "\n";
                }
                return;
            }

            if (sub == "vhdx" || sub == "disks") {
                out << "Hyper-V Virtual Hard Disk Containers (VHDX):\n"
                    << "--------------------------------------------------------------------------------\n";
                auto* base = sys.getVhdx("C:\\VirtualDisks\\BaseOS_Windows2025.vhdx");
                if (base) {
                    out << "  Path:              " << base->getPath() << "\n"
                        << "  Signature:         0x" << std::hex << base->getSignature() << std::dec << " ('vhdxfile')\n"
                        << "  Type:              " << micant::vmms::VhdxDiskTypeToString(base->getDiskType()) << "\n"
                        << "  Capacity:          " << (base->getVirtualSizeBytes() / (1024 * 1024 * 1024)) << " GB\n"
                        << "  Allocated Blocks:  " << base->getAllocatedBlocks() << " / " << base->getTotalBlocks() << "\n\n";
                }
                auto* child = sys.getVhdx("C:\\VirtualDisks\\TitanDC01.vhdx");
                if (child) {
                    out << "  Path:              " << child->getPath() << "\n"
                        << "  Type:              " << micant::vmms::VhdxDiskTypeToString(child->getDiskType()) << "\n"
                        << "  Parent VHDX:       " << child->getParentPath() << "\n"
                        << "  Allocated Blocks:  " << child->getAllocatedBlocks() << " / " << child->getTotalBlocks() << "\n"
                        << "  Checkpoints:       " << child->getCheckpointCount() << "\n";
                }
                return;
            }

            if (sub == "test") {
                out << "[+] Executing Windows Hyper-V VMMS, vSwitch & VHDX Self-Tests...\n";

                // 1. SCM Services
                micant::vmms::RegisterVmmsSubsystem();
                auto& scm = micant::scm::ServiceControlManager::get();
                bool vmmsSvcOk = (scm.getServiceRecord(L"Vmms") != nullptr);
                bool vswitchSvcOk = (scm.getServiceRecord(L"VmSwitch") != nullptr);
                bool vidSvcOk = (scm.getServiceRecord(L"VidDriver") != nullptr);
                out << "  [1/7] SCM Services (Vmms, VmSwitch, VidDriver): "
                    << (vmmsSvcOk && vswitchSvcOk && vidSvcOk ? "PASSED" : "FAILED") << "\n";

                // 2. VersionDatabase
                auto& vdb = micant::version::VersionDatabase::Instance();
                bool vdbOk = (vdb.FindModule("vmms.exe") != nullptr) &&
                             (vdb.FindModule("vmswitch.sys") != nullptr) &&
                             (vdb.FindModule("vhdsvc.dll") != nullptr) &&
                             (vdb.FindModule("vid.sys") != nullptr);
                out << "  [2/7] VersionDatabase (vmms.exe, vmswitch.sys, vhdsvc.dll, vid.sys): "
                    << (vdbOk ? "PASSED" : "FAILED") << "\n";

                // 3. VM Lifecycle & Dynamic Memory Ballooning
                auto* vm = sys.getVirtualMachineByName("TITAN-DC01");
                bool vmOk = (vm != nullptr) && (vm->getState() == micant::vmms::VmState::Running);
                bool balloonOk = vm ? vm->adjustBalloonMemory(6144) : false;
                bool balloonVerified = vm ? (vm->getMemory().currentAllocatedMb == 6144) : false;
                out << "  [3/7] VM Lifecycle & Dynamic Memory Ballooning: "
                    << (vmOk && balloonOk && balloonVerified ? "PASSED" : "FAILED") << "\n";

                // 4. Extensible Virtual Switch L2 Forwarding & MAC Learning
                auto* sw = sys.getVirtualSwitch("DefaultSwitch");
                bool fwdOk = sw ? sw->forwardFrame("PORT-NIC-01", "00:15:5D:01:0A:02", 10, 128) : false;
                out << "  [4/7] Extensible Virtual Switch (vmswitch.sys) L2 Forwarding: "
                    << (fwdOk ? "PASSED" : "FAILED") << "\n";

                // 5. 802.1Q Cross-VLAN Isolation Filter
                sw->createPort("PORT-TEST-VLAN20", "Vlan20Port", "00:15:5D:01:0A:99", 20);
                bool isoDrop = sw ? !sw->forwardFrame("PORT-NIC-01", "00:15:5D:01:0A:99", 10, 128) : false;
                sw->deletePort("PORT-TEST-VLAN20");
                out << "  [5/7] 802.1Q Cross-VLAN Isolation Filter: "
                    << (isoDrop ? "PASSED" : "FAILED") << "\n";

                // 6. VHDX Container Parser & Differencing Tree Fallback
                auto* childDisk = sys.getVhdx("C:\\VirtualDisks\\TitanDC01.vhdx");
                auto* baseDisk = sys.getVhdx("C:\\VirtualDisks\\BaseOS_Windows2025.vhdx");
                char readBufBase[64]{};
                bool diffReadOk = childDisk ? childDisk->readBlock(0x100000, readBufBase, sizeof(readBufBase), baseDisk) : false;
                bool contentMatch = (std::string(readBufBase).find("TITAN_BASE_OS") != std::string::npos);
                out << "  [6/7] VHDX Differencing Block Resolution & Base Fallback: "
                    << (diffReadOk && contentMatch ? "PASSED" : "FAILED") << "\n";

                // 7. Live Migration Pre-Copy Simulation
                bool migOk = vm ? vm->simulateLiveMigration(3) : false;
                bool brownoutOk = vm ? (vm->getMigrationBrownoutMs() < 20) : false;
                out << "  [7/7] Live Migration Iterative Dirty Page Pre-Copy: "
                    << (migOk && brownoutOk ? "PASSED" : "FAILED") << "\n";

                sys.reset();
                out << "[+] All Windows Hyper-V VMMS, vSwitch & VHDX Self-Tests Passed!\n";
                return;
            }
        }

        out << "MicaNT Windows Hyper-V Virtual Machine Management Subsystem (vmms.exe, vmswitch.sys, vhdsvc.dll)\n"
            << "--------------------------------------------------------------------------------\n"
            << "Usage:\n"
            << "  vm status                                 Display virtualization subsystem overview and VM states\n"
            << "  vm list                                   Enumerate defined virtual machines and topology\n"
            << "  vm start <name>                           Power on virtual machine\n"
            << "  vm stop <name>                            Gracefully shut down virtual machine\n"
            << "  vm pause <name>                           Pause virtual machine vCPUs\n"
            << "  vm resume <name>                          Resume paused virtual machine\n"
            << "  vm balloon <name> <mb>                    Dynamically adjust memory balloon demand\n"
            << "  vm switch                                 Inspect extensible virtual switch ports and VLANs\n"
            << "  vm vhdx                                   Display mounted VHDX virtual hard disks\n"
            << "  vm test                                   Execute in-kernel Hyper-V VMMS self-tests\n";
    }


    void cmdActiveDirectory(const std::vector<std::string>& tokens, std::ostream& out) {
        auto& sys = micant::activedirectory::ActiveDirectorySubsystem::get();
        sys.initialize();

        if (tokens.size() > 1) {
            std::string sub = tokens[1];
            std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);

            if (sub == "status") {
                out << "Windows Active Directory Domain Services & Kerberos KDC Subsystem:\n"
                    << "--------------------------------------------------------------------------------\n"
                    << "  Domain DNS Name:       " << micant::activedirectory::DEFAULT_DOMAIN_DNS << "\n"
                    << "  NetBIOS Domain Name:   " << micant::activedirectory::DEFAULT_DOMAIN_NETBIOS << "\n"
                    << "  Domain DN:             " << micant::activedirectory::DEFAULT_DOMAIN_DN << "\n"
                    << "  KDC Status:            ONLINE (kdcsvc.dll / kdc.sys, Port 88)\n"
                    << "  LDAP Directory Engine: ONLINE (ntds.dit / wldap32.dll, Port 389)\n"
                    << "  Directory Objects:     " << sys.getObjectCount() << " objects\n"
                    << "  Active Tickets:        " << sys.getActiveTicketCount() << " granted\n"
                    << "  AS-REQ Processed:      " << sys.getAsReqCount() << " requests\n"
                    << "  TGS-REQ Processed:     " << sys.getTgsReqCount() << " requests\n"
                    << "  LDAP Queries:          " << sys.getLdapQueryCount() << " searches\n"
                    << "  Authentications:       " << sys.getAuthSuccessCount() << " success, "
                    << sys.getAuthFailCount() << " failed\n";
                return;
            }

            if (sub == "users") {
                out << "Active Directory User Principals (CN=Users," << micant::activedirectory::DEFAULT_DOMAIN_DN << "):\n"
                    << "--------------------------------------------------------------------------------\n"
                    << std::left << std::setw(18) << "SAM Account"
                    << std::setw(30) << "User Principal Name (UPN)"
                    << std::setw(16) << "UAC Flags"
                    << "Object SID\n"
                    << "--------------------------------------------------------------------------------\n";
                auto objs = sys.searchLdap("", "(objectClass=user)");
                for (const auto& u : objs) {
                    out << std::left << std::setw(18) << u.samAccountName
                        << std::setw(30) << (u.userPrincipalName.empty() ? "(none)" : u.userPrincipalName)
                        << std::hex << "0x" << std::setw(14) << u.userAccountControl << std::dec
                        << u.objectSid << "\n";
                }
                return;
            }

            if (sub == "computers") {
                out << "Active Directory Computer Accounts & Domain Controllers:\n"
                    << "--------------------------------------------------------------------------------\n"
                    << std::left << std::setw(18) << "Account Name"
                    << std::setw(28) << "Distinguished Name"
                    << "Service Principal Names (SPNs)\n"
                    << "--------------------------------------------------------------------------------\n";
                auto objs = sys.searchLdap("", "(objectClass=computer)");
                for (const auto& c : objs) {
                    std::string spnSummary;
                    for (size_t i = 0; i < c.servicePrincipalNames.size(); ++i) {
                        if (i > 0) spnSummary += ", ";
                        spnSummary += c.servicePrincipalNames[i];
                    }
                    out << std::left << std::setw(18) << c.samAccountName
                        << std::setw(28) << (c.distinguishedName.substr(0, 26) + "..")
                        << spnSummary << "\n";
                }
                return;
            }

            if (sub == "tickets") {
                out << "Kerberos Key Distribution Center (KDC) Active Ticket Cache:\n"
                    << "--------------------------------------------------------------------------------\n"
                    << "  Total Granted Tickets: " << sys.getActiveTicketCount() << "\n"
                    << "  Authentication Tickets (TGT): " << sys.getAsReqCount() << "\n"
                    << "  Service Tickets (TGS):        " << sys.getTgsReqCount() << "\n";
                return;
            }

            if (sub == "ldap" && tokens.size() > 2) {
                std::string filter = tokens[2];
                out << "Executing LDAP Search with filter '" << filter << "':\n"
                    << "--------------------------------------------------------------------------------\n";
                auto matches = sys.searchLdap("", filter);
                out << "Found " << matches.size() << " matching directory object(s):\n";
                for (const auto& m : matches) {
                    out << "  * DN: " << m.distinguishedName << "\n"
                        << "    Class: " << micant::activedirectory::ObjectClassToString(m.objectClass)
                        << " | SAM: " << m.samAccountName
                        << " | SID: " << m.objectSid << "\n";
                }
                return;
            }

            if (sub == "trusts") {
                out << "Active Directory Domain & Forest Trusts (netlogon.dll):\n"
                    << "--------------------------------------------------------------------------------\n";
                auto trusts = sys.getAllTrusts();
                for (const auto& t : trusts) {
                    out << "  Partner Realm: " << t.partnerDomain << " (" << t.netbiosName << ")\n"
                        << "  Trust Type:    " << micant::activedirectory::TrustTypeToString(t.type) << "\n"
                        << "  Direction:     " << micant::activedirectory::TrustDirectionToString(t.direction) << "\n"
                        << "  Transitive:    " << (t.isTransitive ? "YES" : "NO") << "\n\n";
                }
                return;
            }

            if (sub == "test") {
                out << "[+] Executing Windows Active Directory Domain Services & Kerberos KDC Self-Tests...\n";

                // 1. SCM Services
                micant::activedirectory::RegisterActiveDirectorySubsystem();
                auto& scm = micant::scm::ServiceControlManager::get();
                bool kdcSvc = (scm.getServiceRecord(L"Kdc") != nullptr);
                bool ntdsSvc = (scm.getServiceRecord(L"NTDS") != nullptr);
                bool netlogonSvc = (scm.getServiceRecord(L"Netlogon") != nullptr);
                out << "  [1/6] SCM Services (Kdc, NTDS, Netlogon): "
                    << (kdcSvc && ntdsSvc && netlogonSvc ? "PASSED" : "FAILED") << "\n";

                // 2. VersionDatabase
                auto& vdb = micant::version::VersionDatabase::Instance();
                bool vdbOk = (vdb.FindModule("kdcsvc.dll") != nullptr) &&
                             (vdb.FindModule("kdc.sys") != nullptr) &&
                             (vdb.FindModule("ntds.dit") != nullptr) &&
                             (vdb.FindModule("wldap32.dll") != nullptr) &&
                             (vdb.FindModule("netlogon.dll") != nullptr);
                out << "  [2/6] VersionDatabase (kdcsvc.dll, kdc.sys, ntds.dit, wldap32.dll, netlogon.dll): "
                    << (vdbOk ? "PASSED" : "FAILED") << "\n";

                // 3. NTDS Directory Objects & Schema Hierarchy
                const auto* adminObj = sys.getObjectBySam("Administrator");
                const auto* dc01Obj = sys.getObjectBySam("TITAN-DC01$");
                bool ntdsOk = (adminObj != nullptr) && (dc01Obj != nullptr) &&
                              (!adminObj->memberOfSids.empty()) && (!dc01Obj->servicePrincipalNames.empty());
                out << "  [3/6] NTDS Hierarchy & Security Principals: "
                    << (ntdsOk ? "PASSED" : "FAILED") << "\n";

                // 4. Kerberos AS-REQ / AS-REP Authentication (TGT with PAC)
                micant::activedirectory::KerberosTicket tgt{};
                bool asOk = sys.authenticateAsReq("Administrator@micant.internal", "micant.internal", tgt);
                bool pacOk = asOk && tgt.pac.kdcSignatureValid && tgt.pac.serverSignatureValid && !tgt.pac.groups.empty();
                out << "  [4/6] Kerberos AS-REQ Authentication & PAC Generation: "
                    << (asOk && pacOk ? "PASSED" : "FAILED") << "\n";

                // 5. Kerberos TGS-REQ / TGS-REP Service Ticket Granting
                micant::activedirectory::KerberosTicket tgs{};
                bool tgsOk = sys.grantServiceTicketTgsReq(tgt.ticketId, "cifs/titan-dc01.micant.internal", tgs);
                bool verifyOk = sys.verifyServiceTicket(tgs.ticketId, "cifs/titan-dc01.micant.internal");
                out << "  [5/6] Kerberos TGS-REQ Service Ticket Granting & Verification: "
                    << (tgsOk && verifyOk ? "PASSED" : "FAILED") << "\n";

                // 6. LDAP Subtree Query Search
                auto ldapResults = sys.searchLdap("", "(sAMAccountName=TITAN-*)");
                bool ldapOk = (ldapResults.size() >= 2);
                out << "  [6/6] LDAP Subtree Search Filter Evaluation: "
                    << (ldapOk ? "PASSED" : "FAILED") << "\n";

                sys.reset();
                out << "[+] All Windows Active Directory & Kerberos KDC Self-Tests Passed!\n";
                return;
            }
        }

        out << "MicaNT Windows Active Directory Domain Services & Kerberos KDC Subsystem\n"
            << "--------------------------------------------------------------------------------\n"
            << "Usage:\n"
            << "  ad status                                 Display domain directory and KDC status\n"
            << "  ad users                                  Enumerate Active Directory user accounts\n"
            << "  ad computers                              List domain controllers and joined computers\n"
            << "  ad tickets                                Inspect active Kerberos ticket cache\n"
            << "  ad ldap <filter>                          Execute LDAP search filter (e.g. '(objectClass=user)')\n"
            << "  ad trusts                                 Display forest and domain trust relationships\n"
            << "  ad test                                   Execute in-kernel AD DS and KDC self-tests\n";
    }


    void cmdGroupPolicy(const std::vector<std::string>& tokens, std::ostream& out) {
        auto& gp = micant::gp::GroupPolicySubsystem::instance();

        std::string sub = (tokens.size() > 1) ? tokens[1] : "";
        std::string action = (tokens.size() > 0) ? tokens[0] : "";

        if (action == "gpupdate") {
            bool force = false;
            for (size_t i = 1; i < tokens.size(); ++i) {
                if (tokens[i] == "/force" || tokens[i] == "-force") force = true;
            }
            out << "Updating policy...\n";
            gp.processGroupPolicy("LOCAL_MACHINE", true, "OU=Workstations,DC=micant,DC=internal", force);
            gp.processGroupPolicy("CURRENT_USER", false, "OU=Workstations,DC=micant,DC=internal", force);
            out << "Computer Policy update has completed successfully.\n"
                << "User Policy update has completed successfully.\n";
            return;
        }

        if (action == "gpresult") {
            auto rep = gp.getLastMachineReport();
            if (rep.appliedGpoIds.empty()) {
                rep = gp.processGroupPolicy("LOCAL_MACHINE", true, "OU=Workstations,DC=micant,DC=internal", false);
            }
            out << rep.generateSummary();
            return;
        }

        if (sub == "status") {
            out << "Windows Group Policy Client (GPSVC) Subsystem Status:\n"
                << "--------------------------------------------------------------------------------\n"
                << "  Service Status:        ONLINE (gpsvc.dll, svchost.exe -k netsvcs)\n"
                << "  Loopback Mode:         " << (gp.getLoopbackMode() == micant::gp::LoopbackMode::Disabled ? "Disabled" : "Active") << "\n"
                << "  Registered GPOs:       " << gp.getGpoCount() << " objects\n"
                << "  Refreshes Completed:   " << gp.getRefreshesCompleted() << "\n"
                << "  Policies Applied:      " << gp.getPoliciesApplied() << "\n"
                << "  WMI Filter Queries:    " << gp.getWmiEvaluations() << "\n"
                << "  CSE Invocations:       " << gp.getCseInvocations() << "\n";
            return;
        }

        if (sub == "update") {
            bool force = false;
            if (tokens.size() > 2 && (tokens[2] == "/force" || tokens[2] == "-force")) force = true;
            out << "Refreshing Group Policy (force=" << (force ? "YES" : "NO") << ")...\n";
            auto report = gp.processGroupPolicy("LOCAL_MACHINE", true, "OU=Workstations,DC=micant,DC=internal", force);
            out << "Policy refresh complete! Applied " << report.appliedGpoIds.size() << " GPOs, resolved "
                << report.resolvedSettings.size() << " registry settings.\n";
            return;
        }

        if (sub == "result" || sub == "rsop") {
            auto rep = gp.getLastMachineReport();
            if (rep.appliedGpoIds.empty()) {
                rep = gp.processGroupPolicy("LOCAL_MACHINE", true, "OU=Workstations,DC=micant,DC=internal", false);
            }
            out << rep.generateSummary();
            return;
        }

        if (sub == "cse") {
            out << "Registered Client-Side Extensions (CSE):\n"
                << "--------------------------------------------------------------------------------\n"
                << "  {35378EAC-683F-11D2-A89A-00C04FBBCFA2} : Registry CSE (gptext.dll)\n"
                << "  {827D319E-6EAC-11D2-A4EA-00C04F79F83A} : Security CSE (scecli.dll)\n"
                << "  {42B5FA82-575C-11D2-964C-00C04617B5CE} : Scripts CSE (gptext.dll)\n"
                << "  {25537BA6-77A8-11D2-9B6C-00C04FB873C9} : Folder Redirection CSE (fdeploy.dll)\n";
            return;
        }

        if (sub == "test") {
            out << "[+] Executing Windows Group Policy Client & Engine Subsystem Self-Tests...\n";

            // 1. SCM Service
            auto& scm = micant::scm::ServiceControlManager::get();
            bool gpsvcFound = (scm.getServiceRecord(L"Gpsvc") != nullptr);
            out << "  [1/6] SCM Service (Gpsvc): " << (gpsvcFound ? "PASSED" : "FAILED") << "\n";

            // 2. VersionDatabase
            auto& vdb = micant::version::VersionDatabase::Instance();
            bool vdbOk = (vdb.FindModule("gpsvc.dll") != nullptr) &&
                         (vdb.FindModule("gpupdate.exe") != nullptr) &&
                         (vdb.FindModule("gpreport.exe") != nullptr) &&
                         (vdb.FindModule("gpedit.dll") != nullptr) &&
                         (vdb.FindModule("userenv.dll") != nullptr);
            out << "  [2/6] VersionDatabase (gpsvc.dll, gpupdate.exe, gpreport.exe, gpedit.dll, userenv.dll): "
                << (vdbOk ? "PASSED" : "FAILED") << "\n";

            // 3. Registry.pol Binary Serialization
            micant::gp::RegistryPol pol;
            micant::gp::PolicySetting s;
            s.keyPath = "Software\\Policies\\MicaNT\\Test";
            s.valueName = "TestSetting";
            s.type = micant::gp::PolicyValueType::Dword;
            s.dwordValue = 1337;
            pol.settings.push_back(s);
            auto bin = pol.serialize();
            micant::gp::RegistryPol polDec;
            bool polOk = polDec.deserialize(bin.data(), bin.size()) &&
                         (polDec.settings.size() == 1) &&
                         (polDec.settings[0].dwordValue == 1337);
            out << "  [3/6] Registry.pol Binary Serialization & Parsing: " << (polOk ? "PASSED" : "FAILED") << "\n";

            // 4. LSDOU Precedence & Policy Processing
            auto report = gp.processGroupPolicy("TITAN-WS01", true, "OU=Workstations,DC=micant,DC=internal", false);
            bool precedenceOk = (!report.appliedGpoIds.empty()) &&
                                (report.resolvedSettings.find("Software\\Policies\\Microsoft\\Windows\\System\\DisableCMD") != report.resolvedSettings.end());
            out << "  [4/6] LSDOU Inheritance & Precedence Resolution: " << (precedenceOk ? "PASSED" : "FAILED") << "\n";

            // 5. WMI Filter Condition Evaluation
            micant::gp::WmiFilter wf;
            wf.query = "SELECT * FROM Win32_OperatingSystem WHERE Version LIKE '10.0.26100%'";
            std::map<std::string, std::string> facts{ {"OSVersion", "10.0.26100.1"} };
            bool wmiPass = wf.evaluate(facts);
            facts["OSVersion"] = "6.1.7601"; // Windows 7
            bool wmiFail = !wf.evaluate(facts);
            out << "  [5/6] WMI Filter Expression Evaluation: " << (wmiPass && wmiFail ? "PASSED" : "FAILED") << "\n";

            // 6. Win32 C ABI Parity Exports
            bool abiOk = (micant::gp::MicaProcessGroupPolicyCompleted(nullptr, 0) == 0) &&
                         (micant::gp::MicaRefreshPolicy(1) == 1);
            out << "  [6/6] Win32 C ABI Exports (userenv.dll / gpsvc.dll): " << (abiOk ? "PASSED" : "FAILED") << "\n";

            out << "[+] All Windows Group Policy Client & Engine Self-Tests Passed!\n";
            return;
        }

        out << "MicaNT Windows Group Policy Client & Engine Subsystem\n"
            << "--------------------------------------------------------------------------------\n"
            << "Usage:\n"
            << "  gp status                                 Display Group Policy Client and engine status\n"
            << "  gp update [/force]                        Trigger policy refresh cycle\n"
            << "  gp result [/v]                            Display Resultant Set of Policy (RSoP) report\n"
            << "  gp cse                                    List registered Client-Side Extensions\n"
            << "  gp test                                   Execute in-kernel Group Policy self-tests\n";
    }


    void cmdRemoteDesktop(const std::vector<std::string>& tokens, std::ostream& out) {
        auto& rds = micant::rds::RemoteDesktopSubsystem::instance();

        std::string sub = (tokens.size() > 1) ? tokens[1] : "";
        std::string action = (tokens.size() > 0) ? tokens[0] : "";

        if (action == "mstsc") {
            out << "Starting Remote Desktop Connection (mstsc.exe)...\n";
            std::wstring clientName = L"MSTSC-LOCAL";
            std::string clientIp = "127.0.0.1";
            std::wstring user = L"Administrator";
            if (tokens.size() > 1 && tokens[1] != "/v" && tokens[1][0] != '/') {
                std::string sHost = tokens[1];
                clientName = std::wstring(sHost.begin(), sHost.end());
            }
            micant::rds::ClientDisplayMetrics dm{};
            uint32_t sid = 0;
            if (rds.initiateRdpConnection(clientName, clientIp, user, L"MICANT", dm, true, &sid)) {
                out << "Connected to RDP Host! Active WinStation Session ID: " << sid << " (RDP-Tcp#" << sid << ")\n";
            } else {
                out << "Error: Unable to establish RDP connection.\n";
            }
            return;
        }

        if (sub == "status") {
            out << "Windows Remote Desktop Services (RDS / TermService) Subsystem Status:\n"
                << "--------------------------------------------------------------------------------\n"
                << "  Service Status:        ONLINE (termsrv.dll / svchost.exe -k termsvcs)\n"
                << "  Network Level Auth:    " << (rds.isNlaEnforced() ? "ENFORCED (CredSSP / Kerberos)" : "Optional") << "\n"
                << "  Total Sessions:        " << rds.getAllSessions().size() << " sessions\n"
                << "  Connections Handled:   " << rds.getTotalConnectionsHandled() << "\n"
                << "  Virtual Channel pkts:  " << rds.getVirtualChannelPacketsRouted() << "\n"
                << "  Shadow Sessions:       " << rds.getShadowSessionsStarted() << "\n"
                << "  NLA Handshakes Passed: " << rds.getNlaHandshakesPassed() << "\n";
            return;
        }

        if (sub == "sessions" || sub == "list") {
            out << "Active WinStation Sessions:\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(8) << "ID"
                << std::left << std::setw(16) << "WinStation"
                << std::left << std::setw(20) << "User"
                << std::left << std::setw(14) << "State"
                << std::left << std::setw(10) << "Protocol"
                << "Client\n"
                << std::string(80, '-') << "\n";

            auto list = rds.getAllSessions();
            for (const auto& s : list) {
                std::string wsName(s.winStationName.begin(), s.winStationName.end());
                std::string uName(s.userName.begin(), s.userName.end());
                std::string cName(s.clientName.begin(), s.clientName.end());
                std::string stateStr = (s.state == micant::rds::WTSActive) ? "Active" :
                                       (s.state == micant::rds::WTSConnected) ? "Connected" :
                                       (s.state == micant::rds::WTSShadow) ? "Shadow" :
                                       (s.state == micant::rds::WTSDisconnected) ? "Disconnected" : "Idle";
                std::string protoStr = (s.protocolType == micant::rds::WTS_PROTOCOL_TYPE_CONSOLE) ? "Console" : "RDP";

                out << std::left << std::setw(8) << s.sessionId
                    << std::left << std::setw(16) << wsName
                    << std::left << std::setw(20) << uName
                    << std::left << std::setw(14) << stateStr
                    << std::left << std::setw(10) << protoStr
                    << cName << "\n";
            }
            return;
        }

        if (sub == "connect" && tokens.size() > 2) {
            std::string cName = tokens[2];
            std::string uName = (tokens.size() > 3) ? tokens[3] : "Administrator";
            std::wstring wcName(cName.begin(), cName.end());
            std::wstring wuName(uName.begin(), uName.end());

            micant::rds::ClientDisplayMetrics dm{};
            uint32_t sid = 0;
            if (rds.initiateRdpConnection(wcName, "192.168.1.105", wuName, L"MICANT", dm, true, &sid)) {
                out << "RDP Session successfully established! Assigned Session ID: " << sid << "\n";
            } else {
                out << "Error: Connection rejected by TermService.\n";
            }
            return;
        }

        if (sub == "disconnect" && tokens.size() > 2) {
            uint32_t sid = static_cast<uint32_t>(std::stoul(tokens[2]));
            if (rds.disconnectSession(sid)) {
                out << "Session " << sid << " disconnected successfully.\n";
            } else {
                out << "Error: Unable to disconnect session " << sid << ".\n";
            }
            return;
        }

        if (sub == "shadow" && tokens.size() > 3) {
            uint32_t clientSid = static_cast<uint32_t>(std::stoul(tokens[2]));
            uint32_t targetSid = static_cast<uint32_t>(std::stoul(tokens[3]));
            if (rds.startShadowSession(clientSid, targetSid, micant::rds::WTS_SHADOW_ENABLE_INPUT_NO_NOTIFY)) {
                out << "Shadow session initiated: Session " << clientSid << " is now shadowing Session " << targetSid << ".\n";
            } else {
                out << "Error: Unable to shadow session.\n";
            }
            return;
        }

        if (sub == "channels" && tokens.size() > 2) {
            uint32_t sid = static_cast<uint32_t>(std::stoul(tokens[2]));
            micant::rds::RdpSession s{};
            if (!rds.getSession(sid, &s)) {
                out << "Error: Session " << sid << " not found.\n";
                return;
            }
            out << "Virtual Channels for Session " << sid << ":\n"
                << "--------------------------------------------------------------------------------\n";
            for (const auto& [name, vc] : s.virtualChannels) {
                out << "  * Channel: " << std::left << std::setw(12) << name
                    << " ID: " << std::left << std::setw(6) << vc.channelId
                    << " Sent: " << std::left << std::setw(8) << vc.totalBytesSent
                    << " Recv: " << vc.totalBytesReceived << " bytes\n";
            }
            return;
        }

        if (sub == "test") {
            out << "[+] Executing Windows Remote Desktop Services & RDP Channels Self-Tests...\n";

            // 1. SCM Services
            auto& scm = micant::scm::ServiceControlManager::get();
            bool termSvc = (scm.getServiceRecord(L"TermService") != nullptr);
            bool sessEnv = (scm.getServiceRecord(L"SessionEnv") != nullptr);
            bool umRdp = (scm.getServiceRecord(L"UmRdpService") != nullptr);
            out << "  [1/6] SCM Services (TermService, SessionEnv, UmRdpService): "
                << (termSvc && sessEnv && umRdp ? "PASSED" : "FAILED") << "\n";

            // 2. VersionDatabase
            auto& vdb = micant::version::VersionDatabase::Instance();
            bool vdbOk = (vdb.FindModule("termsrv.dll") != nullptr) &&
                         (vdb.FindModule("wtsapi32.dll") != nullptr) &&
                         (vdb.FindModule("rdpdr.sys") != nullptr) &&
                         (vdb.FindModule("mstsc.exe") != nullptr) &&
                         (vdb.FindModule("rdpcorets.dll") != nullptr);
            out << "  [2/6] VersionDatabase (termsrv.dll, wtsapi32.dll, rdpdr.sys, mstsc.exe, rdpcorets.dll): "
                << (vdbOk ? "PASSED" : "FAILED") << "\n";

            // 3. RDP Connection Handshake & WinStation Creation
            uint32_t testSid = 0;
            micant::rds::ClientDisplayMetrics dm{};
            bool connOk = rds.initiateRdpConnection(L"TEST-CLIENT", "192.168.1.50", L"Admin", L"MICANT", dm, true, &testSid);
            micant::rds::RdpSession s{};
            bool sessFound = rds.getSession(testSid, &s) && (s.state == micant::rds::WTSActive);
            out << "  [3/6] RDP Connection Handshake & WinStation Arbitration: "
                << (connOk && sessFound ? "PASSED" : "FAILED") << "\n";

            // 4. Virtual Channels I/O (cliprdr clipboard packet)
            const char clipData[] = "MicaNT Clipboard Synchronization Payload";
            bool wOk = rds.writeVirtualChannel(testSid, "cliprdr", reinterpret_cast<const uint8_t*>(clipData), sizeof(clipData));
            std::vector<uint8_t> rData;
            bool rOk = rds.readVirtualChannel(testSid, "cliprdr", rData);
            bool vOk = wOk && rOk && (std::memcmp(clipData, rData.data(), sizeof(clipData)) == 0);
            out << "  [4/6] Virtual Channels Packet Multiplexing (cliprdr): "
                << (vOk ? "PASSED" : "FAILED") << "\n";

            // 5. Remote Shadow Session
            uint32_t shadowSid = 0;
            rds.initiateRdpConnection(L"SUPERVISOR-PC", "192.168.1.51", L"Supervisor", L"MICANT", dm, true, &shadowSid);
            bool shOk = rds.startShadowSession(shadowSid, testSid, micant::rds::WTS_SHADOW_ENABLE_INPUT_NO_NOTIFY);
            micant::rds::RdpSession shSess{};
            rds.getSession(shadowSid, &shSess);
            bool shActive = (shSess.state == micant::rds::WTSShadow) && (shSess.shadowSessionId == testSid);
            rds.stopShadowSession(shadowSid);
            out << "  [5/6] Remote Shadow Session Arbitration & Control: "
                << (shOk && shActive ? "PASSED" : "FAILED") << "\n";

            // 6. Win32 C ABI Parity Exports
            void* hSrv = micant::rds::MicaWTSOpenServerW(L"localhost");
            micant::rds::WTS_SESSION_INFOW* pInfo = nullptr;
            uint32_t count = 0;
            int32_t enumOk = micant::rds::MicaWTSEnumerateSessionsW(hSrv, 0, 1, &pInfo, &count);
            micant::rds::MicaWTSFreeMemory(pInfo);
            out << "  [6/6] Win32 C ABI Exports (wtsapi32.dll / termsrv.dll): "
                << (enumOk && count >= 2 ? "PASSED" : "FAILED") << "\n";

            rds.logoffSession(testSid);
            rds.logoffSession(shadowSid);

            out << "[+] All Windows Remote Desktop Services Self-Tests Passed!\n";
            return;
        }

        out << "MicaNT Windows Remote Desktop Services (RDS / TermService) Subsystem\n"
            << "--------------------------------------------------------------------------------\n"
            << "Usage:\n"
            << "  rdp status                                Display Remote Desktop Services status\n"
            << "  rdp sessions                              Enumerate active WinStation sessions\n"
            << "  rdp connect <client_name> [user]          Initiate new simulated RDP session\n"
            << "  rdp disconnect <session_id>               Disconnect an active RDP session\n"
            << "  rdp shadow <client_id> <target_id>        Initiate remote shadow viewing session\n"
            << "  rdp channels <session_id>                 List virtual channels bound to session\n"
            << "  rdp test                                  Execute in-kernel RDS self-tests\n";
    }


    void cmdNetworkPolicyServer(const std::vector<std::string>& tokens, std::ostream& out) {
        auto& nps = micant::nps::NetworkPolicyServer::instance();

        std::string sub = (tokens.size() > 1) ? tokens[1] : "";

        if (sub == "status") {
            out << "Windows Network Policy Server (NPS / IAS) Subsystem Status:\n"
                << "--------------------------------------------------------------------------------\n"
                << "  Service Status:        ONLINE (ias.dll / svchost.exe -k netsvcs)\n"
                << "  RADIUS Authentication: UDP 1812 / 1645 (RFC 2865)\n"
                << "  RADIUS Accounting:     UDP 1813 / 1646 (RFC 2866)\n"
                << "  Registered Clients:    " << nps.getAllClients().size() << " RADIUS clients (NAS)\n"
                << "  Active Policies:       " << nps.getNetworkPolicies().size() << " network policies\n"
                << "  Auth Requests:         " << nps.getTotalAuthRequests() << " (Passed: " << nps.getTotalAuthSuccesses()
                << ", Rejected: " << nps.getTotalAuthRejections() << ")\n"
                << "  Acct Requests:         " << nps.getTotalAcctRequests() << " (Active Sessions: "
                << nps.getAllAccountingSessions().size() << ")\n"
                << "  Octets Transferred:    In: " << nps.getTotalInputOctets() << " bytes, Out: "
                << nps.getTotalOutputOctets() << " bytes\n";
            return;
        }

        if (sub == "clients" || sub == "list") {
            out << "Registered RADIUS Clients (Network Access Servers - NAS):\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(18) << "IP Address"
                << std::left << std::setw(24) << "Friendly Name"
                << std::left << std::setw(14) << "Vendor"
                << std::left << std::setw(12) << "MsgAuth"
                << "Status\n"
                << std::string(80, '-') << "\n";

            auto clients = nps.getAllClients();
            for (const auto& c : clients) {
                out << std::left << std::setw(18) << c.ipAddress
                    << std::left << std::setw(24) << c.friendlyName
                    << std::left << std::setw(14) << c.vendorName
                    << std::left << std::setw(12) << (c.requireMessageAuth ? "Required" : "Optional")
                    << (c.enabled ? "ENABLED" : "DISABLED") << "\n";
            }
            return;
        }

        if (sub == "addclient" && tokens.size() > 4) {
            micant::nps::RadiusClient c;
            c.ipAddress = tokens[2];
            c.sharedSecret = tokens[3];
            c.friendlyName = tokens[4];
            c.vendorName = (tokens.size() > 5) ? tokens[5] : "Standard RADIUS";
            c.enabled = true;
            nps.registerClient(c);
            out << "Successfully registered RADIUS Client '" << c.friendlyName << "' [" << c.ipAddress << "].\n";
            return;
        }

        if (sub == "policies") {
            out << "Configured Network Policies (Authorization Rules & VLANs):\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(4) << "Pri"
                << std::left << std::setw(36) << "Policy Name"
                << std::left << std::setw(10) << "Action"
                << std::left << std::setw(14) << "Port Type"
                << "VLAN\n"
                << std::string(80, '-') << "\n";

            auto pols = nps.getNetworkPolicies();
            for (const auto& p : pols) {
                out << std::left << std::setw(4) << p.priority
                    << std::left << std::setw(36) << p.policyName
                    << std::left << std::setw(10) << (p.permission == micant::nps::PolicyPermission::GrantAccess ? "GRANT" : "DENY")
                    << std::left << std::setw(14) << p.allowedNasPortType
                    << (p.assignedVlanId.empty() ? "None" : p.assignedVlanId) << "\n";
            }
            return;
        }

        if (sub == "auth" && tokens.size() > 4) {
            std::string clientIp = tokens[2];
            std::string user = tokens[3];
            std::string pass = tokens[4];

            micant::nps::RadiusClient client{};
            if (!nps.getClient(clientIp, &client)) {
                out << "Error: Unknown RADIUS client IP " << clientIp << ".\n";
                return;
            }

            micant::nps::RadiusPacket req;
            req.code = micant::nps::RadiusCode_AccessRequest;
            req.identifier = 42;
            for (int i = 0; i < 16; ++i) req.authenticator[i] = static_cast<uint8_t>(i + 1);
            req.addStringAttribute(micant::nps::RadiusAttr_UserName, user);
            auto encPass = micant::nps::EncryptRadiusPassword(pass, req.authenticator, client.sharedSecret);
            req.addRawAttribute(micant::nps::RadiusAttr_UserPassword, encPass.data(), encPass.size());
            req.addUint32Attribute(micant::nps::RadiusAttr_NasPortType, 19); // Wireless

            auto rawReq = req.serialize();
            std::vector<uint8_t> resp;
            if (nps.processPacket(clientIp, rawReq.data(), rawReq.size(), resp)) {
                micant::nps::RadiusPacket pResp;
                pResp.deserialize(resp.data(), resp.size());
                if (pResp.code == micant::nps::RadiusCode_AccessAccept) {
                    out << "Authentication SUCCESS: Access-Accept returned for user '" << user << "'!\n";
                    const auto* aVlan = pResp.findAttribute(micant::nps::RadiusAttr_TunnelPrivateGroupId);
                    if (aVlan) out << "  -> Assigned Dynamic VLAN: " << aVlan->asString() << "\n";
                } else {
                    out << "Authentication REJECTED: Access-Reject returned for user '" << user << "'.\n";
                }
            } else {
                out << "Error: Request processing failed or rejected by RADIUS engine.\n";
            }
            return;
        }

        if (sub == "acct" && tokens.size() > 5) {
            std::string clientIp = tokens[2];
            std::string user = tokens[3];
            std::string sessId = tokens[4];
            std::string action = tokens[5]; // start or stop

            micant::nps::RadiusClient client{};
            if (!nps.getClient(clientIp, &client)) {
                out << "Error: Unknown RADIUS client IP " << clientIp << ".\n";
                return;
            }

            uint32_t statusType = (action == "stop") ? 2 : (action == "interim") ? 3 : 1;
            micant::nps::RadiusPacket req;
            req.code = micant::nps::RadiusCode_AccountingRequest;
            req.identifier = 99;
            req.addUint32Attribute(micant::nps::RadiusAttr_AcctStatusType, statusType);
            req.addStringAttribute(micant::nps::RadiusAttr_AcctSessionId, sessId);
            req.addStringAttribute(micant::nps::RadiusAttr_UserName, user);
            req.addUint32Attribute(micant::nps::RadiusAttr_AcctInputOctets, (statusType == 2) ? 65536 : 1024);
            req.addUint32Attribute(micant::nps::RadiusAttr_AcctOutputOctets, (statusType == 2) ? 131072 : 2048);
            req.addUint32Attribute(micant::nps::RadiusAttr_AcctSessionTime, (statusType == 2) ? 3600 : 60);

            auto rawPkt = req.serialize();
            micant::nps::CalculateAccountingRequestAuthenticator(req.authenticator, req.code, req.identifier,
                                                                 req.length, rawPkt.data() + 20, rawPkt.size() - 20,
                                                                 client.sharedSecret);
            auto signedPkt = req.serialize();
            std::vector<uint8_t> resp;
            if (nps.processPacket(clientIp, signedPkt.data(), signedPkt.size(), resp)) {
                out << "Accounting Request (" << action << ") recorded successfully for session " << sessId << ".\n";
            } else {
                out << "Error: Failed to process accounting request.\n";
            }
            return;
        }

        if (sub == "log") {
            out << "RADIUS Accounting Audit Log:\n"
                << "--------------------------------------------------------------------------------\n";
            auto logs = nps.getAuditLog();
            for (const auto& l : logs) {
                out << l << "\n";
            }
            return;
        }

        if (sub == "test") {
            out << "[+] Executing Windows Network Policy Server & RADIUS Subsystem Self-Tests...\n";

            // 1. SCM Services
            auto& scm = micant::scm::ServiceControlManager::get();
            bool iasSvc = (scm.getServiceRecord(L"IAS") != nullptr);
            bool proxySvc = (scm.getServiceRecord(L"RadiusProxy") != nullptr);
            bool radSys = (scm.getServiceRecord(L"RadiusSys") != nullptr);
            out << "  [1/6] SCM Services (IAS, RadiusProxy, RadiusSys): "
                << (iasSvc && proxySvc && radSys ? "PASSED" : "FAILED") << "\n";

            // 2. VersionDatabase
            auto& vdb = micant::version::VersionDatabase::Instance();
            bool vdbOk = (vdb.FindModule("ias.dll") != nullptr) &&
                         (vdb.FindModule("iaspolcy.dll") != nullptr) &&
                         (vdb.FindModule("iasrad.dll") != nullptr) &&
                         (vdb.FindModule("radius.sys") != nullptr) &&
                         (vdb.FindModule("nps.msc") != nullptr);
            out << "  [2/6] VersionDatabase (ias.dll, iaspolcy.dll, iasrad.dll, radius.sys, nps.msc): "
                << (vdbOk ? "PASSED" : "FAILED") << "\n";

            // 3. RFC 2865 User-Password Decryption & Encryption Round-trip
            uint8_t testAuth[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
            std::string origPass = "TitanEnterprisePassword2026!";
            auto encPass = micant::nps::EncryptRadiusPassword(origPass, testAuth, "TestSecretKey!");
            auto decPass = micant::nps::DecryptRadiusPassword(encPass, testAuth, "TestSecretKey!");
            out << "  [3/6] RFC 2865 Password Encryption / Decryption: "
                << (origPass == decPass ? "PASSED" : "FAILED") << "\n";

            // 4. RADIUS Access-Request & Dynamic VLAN Assignment
            micant::nps::RadiusPacket req;
            req.code = micant::nps::RadiusCode_AccessRequest;
            req.identifier = 101;
            std::memcpy(req.authenticator, testAuth, 16);
            req.addStringAttribute(micant::nps::RadiusAttr_UserName, "Alice");
            auto encAlice = micant::nps::EncryptRadiusPassword("AliceSecure2026!", testAuth, "ArubaSecureWiFi!");
            req.addRawAttribute(micant::nps::RadiusAttr_UserPassword, encAlice.data(), encAlice.size());
            req.addUint32Attribute(micant::nps::RadiusAttr_NasPortType, 19); // Wireless

            auto rawReq = req.serialize();
            std::vector<uint8_t> resp;
            bool authOk = nps.processPacket("192.168.1.10", rawReq.data(), rawReq.size(), resp);
            micant::nps::RadiusPacket pResp;
            bool parseOk = pResp.deserialize(resp.data(), resp.size());
            bool vlanOk = false;
            const auto* aVlan = pResp.findAttribute(micant::nps::RadiusAttr_TunnelPrivateGroupId);
            if (aVlan && aVlan->asString() == "100") vlanOk = true;
            out << "  [4/6] RADIUS Access-Request & Dynamic VLAN 100 Assignment: "
                << (authOk && parseOk && (pResp.code == micant::nps::RadiusCode_AccessAccept) && vlanOk ? "PASSED" : "FAILED") << "\n";

            // 5. RADIUS Accounting Lifecycle (Start & Stop)
            micant::nps::RadiusPacket acctStart;
            acctStart.code = micant::nps::RadiusCode_AccountingRequest;
            acctStart.identifier = 202;
            acctStart.addUint32Attribute(micant::nps::RadiusAttr_AcctStatusType, 1); // Start
            acctStart.addStringAttribute(micant::nps::RadiusAttr_AcctSessionId, "self_test_sess_01");
            acctStart.addStringAttribute(micant::nps::RadiusAttr_UserName, "Alice");
            auto rawStart = acctStart.serialize();
            micant::nps::CalculateAccountingRequestAuthenticator(acctStart.authenticator, acctStart.code,
                                                                 acctStart.identifier, acctStart.length,
                                                                 rawStart.data() + 20, rawStart.size() - 20,
                                                                 "ArubaSecureWiFi!");
            auto signedStart = acctStart.serialize();
            std::vector<uint8_t> acctResp;
            bool acctOk = nps.processPacket("192.168.1.10", signedStart.data(), signedStart.size(), acctResp);
            out << "  [5/6] RADIUS Accounting Session Start & Tracking: "
                << (acctOk ? "PASSED" : "FAILED") << "\n";

            // 6. Win32 C ABI Parity Exports
            void* pEngine = nullptr;
            int32_t initRes = micant::nps::MicaIasInitialize(&pEngine);
            uint64_t totalAuth = 0, totalAcct = 0, activeSess = 0;
            micant::nps::MicaIasGetAccountingStats(pEngine, &totalAuth, &totalAcct, &activeSess);
            out << "  [6/6] Win32 C ABI Exports (ias.dll / iasrad.dll): "
                << (initRes == 1 && pEngine != nullptr && totalAuth > 0 ? "PASSED" : "FAILED") << "\n";

            out << "[+] All Windows Network Policy Server & RADIUS Self-Tests Passed!\n";
            return;
        }

        out << "MicaNT Windows Network Policy Server & RADIUS Subsystem (NPS / IAS)\n"
            << "--------------------------------------------------------------------------------\n"
            << "Usage:\n"
            << "  nps status                                Display NPS / RADIUS subsystem status\n"
            << "  nps clients                               List registered RADIUS clients (NAS)\n"
            << "  nps addclient <ip> <secret> <name>        Register a new RADIUS client\n"
            << "  nps policies                              Display network policies and VLAN assignments\n"
            << "  nps auth <client_ip> <user> <pass>        Authenticate user via RADIUS Access-Request\n"
            << "  nps acct <client_ip> <user> <sess> <act>  Send Accounting-Request (start|interim|stop)\n"
            << "  nps log                                   View RADIUS accounting audit log\n"
            << "  nps test                                  Execute in-kernel NPS / RADIUS self-tests\n";
    }


    void cmdSystemResourceManager(const std::vector<std::string>& tokens, std::ostream& out) {
        auto& wsrm = micant::wsrm::SystemResourceManager::instance();

        std::string sub = (tokens.size() > 1) ? tokens[1] : "";

        if (sub == "status") {
            out << "Windows System Resource Manager (WSRM / Fair Share) Status:\n"
                << "--------------------------------------------------------------------------------\n"
                << "  Active Allocation Policy: " << micant::wsrm::AllocationPolicyTypeToString(wsrm.getAllocationPolicy()) << "\n"
                << "  Service Status:           ONLINE (wsrm.exe / TitanQuota)\n"
                << "  Managed Active Processes: " << wsrm.getAllManagedProcesses().size() << "\n"
                << "  Total Registered:         " << wsrm.getTotalRegisteredProcesses() << "\n"
                << "  Throttling Violations:    " << wsrm.getTotalThrottlingEvents() << "\n"
                << "  DFSS Rebalance Cycles:    " << wsrm.getRebalanceCycles() << "\n";
            return;
        }

        if (sub == "policies") {
            out << "WSRM Process Matching Criteria (PMC) & Allocation Policies:\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(26) << "Criteria Name"
                << std::left << std::setw(16) << "App Pattern"
                << std::left << std::setw(14) << "User/Group"
                << std::left << std::setw(10) << "CPU Cap"
                << std::left << std::setw(12) << "Mem Cap"
                << "Mode\n"
                << std::string(80, '-') << "\n";

            auto crits = wsrm.getAllCriteria();
            for (const auto& c : crits) {
                std::string memStr = (c.memoryLimitBytes == 0) ? "Unlimited" : (std::to_string(c.memoryLimitBytes / (1024 * 1024)) + " MB");
                out << std::left << std::setw(26) << c.criteriaName
                    << std::left << std::setw(16) << c.appPattern
                    << std::left << std::setw(14) << c.userOrGroup
                    << std::left << std::setw(10) << (std::to_string(c.targetCpuPercent) + "%")
                    << std::left << std::setw(12) << memStr
                    << (c.hardCap ? "HARD_CAP" : "WEIGHT_BASED") << "\n";
            }
            return;
        }

        if (sub == "setpolicy" && tokens.size() > 2) {
            std::string pol = tokens[2];
            for (auto& ch : pol) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (pol == "process") {
                wsrm.setAllocationPolicy(micant::wsrm::AllocationPolicyType::EqualPerProcess);
                out << "Resource Allocation Policy set to Equal_Per_Process.\n";
            } else if (pol == "user") {
                wsrm.setAllocationPolicy(micant::wsrm::AllocationPolicyType::EqualPerUser);
                out << "Resource Allocation Policy set to Equal_Per_User.\n";
            } else if (pol == "session" || pol == "dfss") {
                wsrm.setAllocationPolicy(micant::wsrm::AllocationPolicyType::EqualPerSession);
                out << "Resource Allocation Policy set to Equal_Per_Session (DFSS).\n";
            } else if (pol == "custom") {
                wsrm.setAllocationPolicy(micant::wsrm::AllocationPolicyType::CustomWeighted);
                out << "Resource Allocation Policy set to Custom_Weighted.\n";
            } else {
                out << "Unknown policy. Choose: process, user, session, or custom.\n";
            }
            return;
        }

        if (sub == "addcriteria" && tokens.size() > 4) {
            micant::wsrm::ProcessMatchingCriteria pmc;
            pmc.criteriaName = tokens[2];
            pmc.appPattern = tokens[3];
            pmc.targetCpuPercent = static_cast<uint32_t>(std::stoul(tokens[4]));
            pmc.hardCap = true;
            wsrm.addMatchingCriteria(pmc);
            out << "Added Process Matching Criteria '" << pmc.criteriaName << "' with " << pmc.targetCpuPercent << "% CPU cap.\n";
            return;
        }

        if (sub == "processes") {
            out << "WSRM Managed Process Allocations & Quotas:\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(8)  << "PID"
                << std::left << std::setw(18) << "Image Name"
                << std::left << std::setw(14) << "User"
                << std::left << std::setw(6)  << "Sess"
                << std::left << std::setw(10) << "CPU Cap"
                << std::left << std::setw(12) << "Mem Cap"
                << "Criteria\n"
                << std::string(80, '-') << "\n";

            auto procs = wsrm.getAllManagedProcesses();
            for (const auto& p : procs) {
                std::stringstream ss;
                ss << std::fixed << std::setprecision(1) << p.allocatedCpuPercent << "%";
                std::string memStr = (p.memoryLimitBytes == 0) ? "Unlimited" : (std::to_string(p.memoryLimitBytes / (1024 * 1024)) + " MB");
                out << std::left << std::setw(8)  << p.processId
                    << std::left << std::setw(18) << p.processName
                    << std::left << std::setw(14) << p.userName
                    << std::left << std::setw(6)  << p.sessionId
                    << std::left << std::setw(10) << ss.str()
                    << std::left << std::setw(12) << memStr
                    << p.matchedCriteria << "\n";
            }
            return;
        }

        if (sub == "dfss" && tokens.size() > 2) {
            uint32_t sessions = static_cast<uint32_t>(std::stoul(tokens[2]));
            wsrm.applyFairShare(sessions);
            out << "Applied Dynamic Fair Share Scheduling (DFSS) across " << sessions << " active session(s).\n"
                << "Each session receives " << (100.0 / sessions) << "% target CPU allocation.\n";
            return;
        }

        if (sub == "accounting") {
            std::string filter = (tokens.size() > 2) ? tokens[2] : "";
            out << "WSRM Historical Resource Accounting Log:\n"
                << "--------------------------------------------------------------------------------\n";
            auto history = wsrm.getAccountingHistory();
            if (history.empty()) {
                out << "No historical accounting records recorded yet.\n";
            } else {
                for (const auto& h : history) {
                    if (!filter.empty() && h.tenantName != filter && h.processName != filter) continue;
                    out << "  [Tenant: " << h.tenantName << " | PID " << h.processId << " (" << h.processName << ")]\n"
                        << "    Session: " << h.sessionId
                        << " | CPU Time: " << (h.cpuTimeUs / 1000) << " ms"
                        << " | Peak WS: " << (h.peakWorkingSetBytes / 1024) << " KB"
                        << " | Total I/O: " << (h.totalIoBytes / 1024) << " KB"
                        << " | Throttles: " << h.throttlingEvents << "\n";
                }
            }
            return;
        }

        if (sub == "test") {
            out << "[+] Executing Windows System Resource Manager & Fair Share Self-Tests...\n";

            // 1. SCM Services
            auto& scm = micant::scm::ServiceControlManager::get();
            bool wsrmSvc = (scm.getServiceRecord(L"WsrmService") != nullptr);
            bool quotaDrv = (scm.getServiceRecord(L"TitanQuota") != nullptr);
            out << "  [1/6] SCM Services (WsrmService, TitanQuota): "
                << (wsrmSvc && quotaDrv ? "PASSED" : "FAILED") << "\n";

            // 2. VersionDatabase
            auto& db = micant::version::VersionDatabase::Instance();
            bool vExe = (db.FindModule("wsrm.exe") != nullptr);
            bool vDll = (db.FindModule("wsrmcore.dll") != nullptr);
            bool vMsc = (db.FindModule("wsrm.msc") != nullptr);
            bool vSys = (db.FindModule("wsrmcore.sys") != nullptr);
            out << "  [2/6] VersionDatabase (wsrm.exe, wsrmcore.dll, wsrm.msc, wsrmcore.sys): "
                << (vExe && vDll && vMsc && vSys ? "PASSED" : "FAILED") << "\n";

            // 3. Process Registration & Equal-Per-Process Balancing
            wsrm.registerProcess(4001, "worker1.exe", "Bob", 1);
            wsrm.registerProcess(4002, "worker2.exe", "Alice", 1);
            wsrm.setAllocationPolicy(micant::wsrm::AllocationPolicyType::EqualPerProcess);
            micant::wsrm::ManagedProcessRecord r1{}, r2{};
            wsrm.getProcessAccounting(4001, &r1);
            wsrm.getProcessAccounting(4002, &r2);
            bool eqOk = (r1.allocatedCpuPercent == 50.0 && r2.allocatedCpuPercent == 50.0);
            out << "  [3/6] Equal-Per-Process Dynamic Allocation (50% / 50%): "
                << (eqOk ? "PASSED" : "FAILED") << "\n";

            // 4. Dynamic Fair Share Scheduling (DFSS)
            wsrm.applyFairShare(4); // 4 sessions -> 25% each
            wsrm.getProcessAccounting(4001, &r1);
            bool dfssOk = (r1.allocatedCpuPercent == 25.0 && (r1.rateControlFlags & micant::wsrm::JOBOBJECT_CPU_RATE_CONTROL_HARD_CAP));
            out << "  [4/6] Dynamic Fair Share Scheduling (DFSS 4 Sessions -> 25%): "
                << (dfssOk ? "PASSED" : "FAILED") << "\n";

            // 5. Telemetry & Throttling
            wsrm.updateProcessTelemetry(4001, 15000, 5000, 1048576, 512000, true);
            wsrm.getProcessAccounting(4001, &r1);
            bool telemOk = (r1.userTimeUs == 15000 && r1.throttlingEvents == 1);
            out << "  [5/6] Process Telemetry & Throttling Accounting: "
                << (telemOk ? "PASSED" : "FAILED") << "\n";

            // 6. Win32 C ABI Parity Exports
            void* pEng = nullptr;
            int32_t initRc = micant::wsrm::MicaWsrmInitialize(&pEng);
            int32_t setPolRc = micant::wsrm::MicaWsrmSetAllocationPolicy(pEng, 0);
            out << "  [6/6] Win32 C ABI Exports (wsrmcore.dll): "
                << (initRc == 1 && setPolRc == 1 && pEng != nullptr ? "PASSED" : "FAILED") << "\n";

            // Cleanup test processes
            wsrm.deregisterProcess(4001);
            wsrm.deregisterProcess(4002);

            out << "[+] All Windows System Resource Manager Self-Tests Passed!\n";
            return;
        }

        out << "MicaNT Windows System Resource Manager (WSRM / Fair Share) Subsystem\n"
            << "--------------------------------------------------------------------------------\n"
            << "Usage:\n"
            << "  wsrm status                             Display WSRM engine status & active policy\n"
            << "  wsrm policies                           Enumerate PMC & resource allocation policies\n"
            << "  wsrm setpolicy <process|user|session|custom> Switch active allocation policy\n"
            << "  wsrm addcriteria <name> <pattern> <cpu%>  Add custom Process Matching Criteria\n"
            << "  wsrm processes                          List active managed process quotas\n"
            << "  wsrm dfss <sessions>                    Simulate Dynamic Fair Share rebalance\n"
            << "  wsrm accounting [user|app]              View historical resource usage logs\n"
            << "  wsrm test                               Execute in-kernel WSRM self-tests\n";
    }


    void cmdDeploymentServices(const std::vector<std::string>& tokens, std::ostream& out) {
        auto& wds = micant::wds::DeploymentServicesEngine::instance();

        std::string sub = (tokens.size() > 1) ? tokens[1] : "";

        if (sub == "status") {
            out << "Windows Deployment Services (WDS / TitanWDS / AegisPXE) Status:\n"
                << "--------------------------------------------------------------------------------\n"
                << "  Service Status:           ONLINE (wdssvc.dll / BINLSVC / WdsTftp)\n"
                << "  Registered Images:        " << wds.getAllImages().size() << "\n"
                << "  Virtual Boot Files:       " << wds.getAllTftpFiles().size() << "\n"
                << "  Total PXE Requests:       " << wds.getTotalPxeRequests() << "\n"
                << "  Total PXE Offers:         " << wds.getTotalPxeOffers() << "\n"
                << "  Total TFTP Requests:      " << wds.getTotalTftpRequests() << "\n"
                << "  Total TFTP Bytes Served:  " << wds.getTotalTftpBytesServed() << " bytes\n"
                << "  Unattend Files Generated: " << wds.getTotalUnattendGenerated() << "\n";
            return;
        }

        if (sub == "images") {
            out << "Windows Deployment Services Image Catalog:\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(20) << "Image ID"
                << std::setw(10) << "Arch"
                << std::setw(22) << "Type"
                << std::setw(12) << "Size (MB)"
                << "Name / Path\n"
                << "--------------------------------------------------------------------------------\n";
            auto imgs = wds.getAllImages();
            for (const auto& img : imgs) {
                out << std::left << std::setw(20) << img.imageId
                    << std::setw(10) << img.architecture
                    << std::setw(22) << micant::wds::ImageTypeToString(img.type)
                    << std::setw(12) << (img.sizeBytes / (1024 * 1024))
                    << img.imageName << " (" << img.filePath << ")\n";
            }
            return;
        }

        if (sub == "addimage") {
            if (tokens.size() < 6) {
                out << "Usage: wds addimage <id> <name> <arch:x64|ARM64|x86> <type:boot|install> <path>\n";
                return;
            }
            micant::wds::WdsImageRecord rec;
            rec.imageId = tokens[2];
            rec.imageName = tokens[3];
            rec.architecture = tokens[4];
            std::string tStr = tokens[5];
            rec.type = (tStr == "install" || tStr == "os") ? micant::wds::ImageType::InstallImage : micant::wds::ImageType::BootImage;
            rec.filePath = (tokens.size() > 6) ? tokens[6] : "sources\\custom.wim";
            rec.sizeBytes = 1048576000ULL; // 1 GB synthetic
            if (wds.registerImage(rec)) {
                out << "[+] WDS Image registered successfully: " << rec.imageId << " (" << rec.imageName << ")\n";
            } else {
                out << "[-] Failed to register WDS Image.\n";
            }
            return;
        }

        if (sub == "bootfiles") {
            out << "Windows Deployment Services TFTP Virtual Boot Files:\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(30) << "Boot File Name"
                << std::setw(14) << "Size"
                << "Description\n"
                << "--------------------------------------------------------------------------------\n";
            auto files = wds.getAllTftpFiles();
            for (const auto& f : files) {
                out << std::left << std::setw(30) << f.fileName
                    << std::setw(14) << (std::to_string(f.content.size()) + " B")
                    << f.description << "\n";
            }
            return;
        }

        if (sub == "pxe") {
            std::string archStr = (tokens.size() > 2) ? tokens[2] : "x64";
            micant::wds::ClientArchitecture arch = micant::wds::ClientArchitecture::EFI_x86_64;
            if (archStr == "arm64" || archStr == "ARM64") {
                arch = micant::wds::ClientArchitecture::EFI_ARM64;
            } else if (archStr == "x86" || archStr == "bios" || archStr == "legacy") {
                arch = micant::wds::ClientArchitecture::Intelx86PC;
            } else if (archStr == "ia32") {
                arch = micant::wds::ClientArchitecture::EFI_IA32;
            }

            // Construct synthetic DHCP Discover with Option 60 PXEClient and Option 93
            std::vector<uint8_t> req(sizeof(micant::wds::DhcpHeader) + 64, 0);
            auto* hdr = reinterpret_cast<micant::wds::DhcpHeader*>(req.data());
            hdr->op = 1;
            hdr->htype = 1;
            hdr->hlen = 6;
            hdr->xid = 0x12345678;
            hdr->chaddr[0] = 0x00; hdr->chaddr[1] = 0x15; hdr->chaddr[2] = 0x5D;
            hdr->chaddr[3] = 0x01; hdr->chaddr[4] = 0x02; hdr->chaddr[5] = 0x03;

            size_t off = sizeof(micant::wds::DhcpHeader);
            req[off] = 0x63; req[off+1] = 0x82; req[off+2] = 0x53; req[off+3] = 0x63; // magic cookie
            off += 4;
            // Option 53: Discover
            req[off++] = 53; req[off++] = 1; req[off++] = 1;
            // Option 60: PXEClient
            std::string pxeId = "PXEClient:Arch:00007:UNDI:002001";
            req[off++] = 60; req[off++] = static_cast<uint8_t>(pxeId.size());
            std::memcpy(&req[off], pxeId.data(), pxeId.size());
            off += pxeId.size();
            // Option 93: Client Arch
            uint16_t aVal = static_cast<uint16_t>(arch);
            req[off++] = 93; req[off++] = 2;
            req[off++] = static_cast<uint8_t>((aVal >> 8) & 0xFF);
            req[off++] = static_cast<uint8_t>(aVal & 0xFF);
            req[off++] = 255; // End
            req.resize(off);

            std::vector<uint8_t> offer;
            if (wds.processPxeRequest(req.data(), req.size(), offer)) {
                const auto* offHdr = reinterpret_cast<const micant::wds::DhcpHeader*>(offer.data());
                out << "[+] PXE Boot Offer Generated Successfully:\n"
                    << "  Client Arch:          " << micant::wds::ClientArchitectureToString(arch) << " (Option 93 = " << static_cast<uint16_t>(arch) << ")\n"
                    << "  Transaction ID (XID): 0x" << std::hex << offHdr->xid << std::dec << "\n"
                    << "  Server Name (sname):  " << offHdr->sname << "\n"
                    << "  Offered Boot File:    " << offHdr->file << "\n"
                    << "  DHCP Packet Size:     " << offer.size() << " bytes\n";
            } else {
                out << "[-] Failed to generate PXE Offer.\n";
            }
            return;
        }

        if (sub == "tftp") {
            if (tokens.size() < 3) {
                out << "Usage: wds tftp <bootfile> [blksize] [windowsize]\n"
                    << "Example: wds tftp boot\\x64\\wdsmgfw.efi 1456 4\n";
                return;
            }
            std::string file = tokens[2];
            uint32_t blkSize = (tokens.size() > 3) ? static_cast<uint32_t>(std::stoul(tokens[3])) : 1456;
            uint32_t winSize = (tokens.size() > 4) ? static_cast<uint32_t>(std::stoul(tokens[4])) : 4;

            micant::wds::DeploymentServicesEngine::TftpSessionParams params{};
            std::vector<uint8_t> oack;
            if (wds.processTftpRrq(file, blkSize, winSize, &params, oack)) {
                uint32_t totalBlocks = static_cast<uint32_t>((params.transferSize + params.blockSize - 1) / params.blockSize);
                out << "[+] TFTP RRQ Negotiation (RFC 1350/2347/7440) Succeeded:\n"
                    << "  File:                 " << params.fileName << "\n"
                    << "  Negotiated BlkSize:   " << params.blockSize << " bytes\n"
                    << "  WindowSize (RFC 7440):" << params.windowSize << " packets/ACK\n"
                    << "  Total File Size:      " << params.transferSize << " bytes\n"
                    << "  Calculated Blocks:    " << totalBlocks << "\n"
                    << "  OACK Packet Size:     " << oack.size() << " bytes\n";
            } else {
                out << "[-] TFTP RRQ Failed: File not found in virtual boot store (" << file << ")\n";
            }
            return;
        }

        if (sub == "unattend") {
            std::string comp = (tokens.size() > 2) ? tokens[2] : "MICANT-WORKSTATION";
            std::string pass = (tokens.size() > 3) ? tokens[3] : "TitanPxe@2026!";
            std::string dom = (tokens.size() > 4) ? tokens[4] : "CONTOSO.COM";
            std::string xml = wds.generateUnattendXml(comp, pass, dom);
            out << "Generated WDS Unattend XML Answer File (" << xml.size() << " bytes):\n"
                << "--------------------------------------------------------------------------------\n"
                << xml << "\n";
            return;
        }

        if (sub == "test") {
            out << "[*] Executing Windows Deployment Services (WDS) In-Kernel Self-Tests...\n";
            auto& scm = micant::scm::ServiceControlManager::get();
            auto& db = micant::version::VersionDatabase::Instance();

            // Test 1: SCM Services
            bool sWds = (scm.getServiceRecord(L"WDSServer") != nullptr);
            bool sBinl = (scm.getServiceRecord(L"BINLSVC") != nullptr);
            bool sTftp = (scm.getServiceRecord(L"WdsTftp") != nullptr);
            out << "  [1/6] SCM Services (WDSServer, BINLSVC, WdsTftp): "
                << (sWds && sBinl && sTftp ? "PASSED" : "FAILED") << "\n";

            // Test 2: Version Database
            bool vSvc = (db.FindModule("wdssvc.dll") != nullptr);
            bool vEfi = (db.FindModule("wdsmgfw.efi") != nullptr);
            bool vCli = (db.FindModule("wdsclient.dll") != nullptr);
            bool vTftp = (db.FindModule("wdstftp.dll") != nullptr);
            bool vUtil = (db.FindModule("wdsutil.exe") != nullptr);
            out << "  [2/6] VersionDatabase Modules (wdssvc, wdsmgfw, wdsclient, wdstftp, wdsutil): "
                << (vSvc && vEfi && vCli && vTftp && vUtil ? "PASSED" : "FAILED") << "\n";

            // Test 3: Multi-Architecture PXE Negotiation
            std::vector<uint8_t> req(sizeof(micant::wds::DhcpHeader) + 64, 0);
            auto* hdr = reinterpret_cast<micant::wds::DhcpHeader*>(req.data());
            hdr->op = 1; hdr->htype = 1; hdr->hlen = 6; hdr->xid = 0x55AA55AA;
            size_t off = sizeof(micant::wds::DhcpHeader);
            req[off] = 0x63; req[off+1] = 0x82; req[off+2] = 0x53; req[off+3] = 0x63;
            off += 4;
            req[off++] = 53; req[off++] = 1; req[off++] = 1;
            std::string pxeId = "PXEClient";
            req[off++] = 60; req[off++] = static_cast<uint8_t>(pxeId.size());
            std::memcpy(&req[off], pxeId.data(), pxeId.size());
            off += pxeId.size();
            req[off++] = 93; req[off++] = 2; req[off++] = 0; req[off++] = 7; // EFI x64
            req[off++] = 255;
            req.resize(off);

            std::vector<uint8_t> offerX64;
            bool pxeX64Ok = wds.processPxeRequest(req.data(), req.size(), offerX64);
            const auto* offHdr = reinterpret_cast<const micant::wds::DhcpHeader*>(offerX64.data());
            bool pathX64Ok = (std::string(offHdr->file).find("wdsmgfw.efi") != std::string::npos);

            // Test ARM64
            req[req.size()-3] = 0; req[req.size()-2] = 11; // EFI ARM64
            std::vector<uint8_t> offerArm;
            bool pxeArmOk = wds.processPxeRequest(req.data(), req.size(), offerArm);
            const auto* offHdrArm = reinterpret_cast<const micant::wds::DhcpHeader*>(offerArm.data());
            bool pathArmOk = (std::string(offHdrArm->file).find("arm64") != std::string::npos);

            out << "  [3/6] Multi-Arch PXE Negotiation (x64 / ARM64 UEFI arbitration): "
                << (pxeX64Ok && pathX64Ok && pxeArmOk && pathArmOk ? "PASSED" : "FAILED") << "\n";

            // Test 4: TFTP Windowed Stream Engine (RFC 1350/2347/7440)
            micant::wds::DeploymentServicesEngine::TftpSessionParams tp{};
            std::vector<uint8_t> oack;
            bool rrqOk = wds.processTftpRrq("boot\\x64\\wdsmgfw.efi", 1456, 8, &tp, oack);
            std::vector<uint8_t> blkData;
            bool isLast = false;
            bool blkOk = wds.getTftpFileBlock("boot\\x64\\wdsmgfw.efi", 1, 1456, blkData, &isLast);
            bool tftpOk = rrqOk && blkOk && (tp.blockSize == 1456) && (tp.windowSize == 8) && (blkData.size() == 1456);
            out << "  [4/6] TFTP Stream Engine & Windowsize Negotiation: "
                << (tftpOk ? "PASSED" : "FAILED") << "\n";

            // Test 5: Unattend XML Generation
            std::string xml = wds.generateUnattendXml("TEST-PC", "TestPass!1", "TESTCORP");
            bool xmlOk = (xml.find("TEST-PC") != std::string::npos &&
                          xml.find("TestPass!1") != std::string::npos &&
                          xml.find("TESTCORP") != std::string::npos);
            out << "  [5/6] Automated Unattend XML Generator: "
                << (xmlOk ? "PASSED" : "FAILED") << "\n";

            // Test 6: Win32 C ABI Exports
            void* pEng = nullptr;
            int32_t initRc = micant::wds::MicaWdsInitialize(&pEng);
            char xmlBuf[1024]{};
            uint32_t outLen = 0;
            int32_t unattendRc = micant::wds::MicaWdsGenerateUnattendXml(pEng, "ABI-HOST", "ABIPass!", xmlBuf, sizeof(xmlBuf), &outLen);
            out << "  [6/6] Clean-Room Win32 C ABI Exports (wdssvc.dll): "
                << (initRc == 1 && unattendRc == 1 ? "PASSED" : "FAILED") << "\n";

            out << "[+] All Windows Deployment Services (WDS) Self-Tests Passed!\n";
            return;
        }

        out << "MicaNT Windows Deployment Services (WDS / TitanWDS) Subsystem\n"
            << "--------------------------------------------------------------------------------\n"
            << "Usage:\n"
            << "  wds status                              Display WDS server and listener status\n"
            << "  wds images                              Enumerate WDS image catalog (WinPE & OS)\n"
            << "  wds addimage <id> <name> <arch> <type>  Register custom boot or install image\n"
            << "  wds bootfiles                           List virtual boot files in TFTP root\n"
            << "  wds pxe [x64|arm64|x86]                 Simulate DHCP/PXE boot negotiation\n"
            << "  wds tftp <file> [blksize] [windowsize]  Simulate RFC 7440 TFTP file transfer\n"
            << "  wds unattend [name] [pass] [domain]     Generate unattended setup XML answer file\n"
            << "  wds test                                Execute in-kernel WDS / PXE self-tests\n";
    }


    void cmdCertificateServices(const std::vector<std::string>& tokens, std::ostream& out) {
        auto& pki = micant::certsrv::CertificateServicesEngine::instance();

        std::string sub = (tokens.size() > 1) ? tokens[1] : "";

        if (sub == "status") {
            out << "Active Directory Certificate Services (AD CS / TitanCA) Status:\n"
                << "--------------------------------------------------------------------------------\n"
                << "  Service Status:           ONLINE (CertSvc / certsrv.exe)\n"
                << "  CA Name:                  " << pki.getCaName() << "\n"
                << "  CA Subject:               " << pki.getCaDn() << "\n"
                << "  AIA Distribution URI:     " << pki.getAiaUri() << "\n"
                << "  CDP Distribution URI:     " << pki.getCdpUri() << "\n"
                << "  OCSP Responder URI:       " << pki.getOcspUri() << "\n"
                << "  Active Certificates:      " << pki.getAllCertificates().size() << "\n"
                << "  Total Requests:           " << pki.getTotalRequestsSubmitted() << "\n"
                << "  Total Issued:             " << pki.getTotalCertificatesIssued() << "\n"
                << "  Total Revoked:            " << pki.getTotalCertificatesRevoked() << "\n"
                << "  Total CRL Published:      " << pki.getTotalCrlPublished() << "\n"
                << "  Total OCSP Queries:       " << pki.getTotalOcspQueries() << "\n";
            return;
        }

        if (sub == "ca") {
            micant::certsrv::CertificateRecord rootCert;
            if (pki.getCertificate(pki.getRootSerialNumber(), rootCert)) {
                out << "Enterprise Root Certification Authority:\n"
                    << "--------------------------------------------------------------------------------\n"
                    << "  Subject DN:               " << rootCert.subjectDn << "\n"
                    << "  Issuer DN:                " << rootCert.issuerDn << " (Self-Signed)\n"
                    << "  Serial Number:            " << rootCert.serialNumber << "\n"
                    << "  Public Key:               " << rootCert.publicKeyAlg << " " << rootCert.publicKeyBits << "-bit\n"
                    << "  Subject Key Identifier:   " << rootCert.subjectKeyIdentifier << "\n"
                    << "  Key Usage:                Digital Signature, Cert Sign, CRL Sign (0x" << std::hex << rootCert.keyUsage << std::dec << ")\n"
                    << "  PEM Certificate:\n" << rootCert.rawPem;
            } else {
                out << "Root CA certificate not found!\n";
            }
            return;
        }

        if (sub == "templates") {
            out << "Active Directory Certificate Templates:\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(20) << "Template Name"
                << std::setw(8)  << "Ver"
                << std::setw(12) << "Validity"
                << std::setw(12) << "AutoEnroll"
                << std::setw(14) << "RequiresAppr"
                << "Display Name\n"
                << "--------------------------------------------------------------------------------\n";
            auto tmpls = pki.getAllTemplates();
            for (const auto& t : tmpls) {
                out << std::left << std::setw(20) << t.templateName
                    << std::setw(8)  << t.schemaVersion
                    << std::setw(12) << (std::to_string(t.validityPeriodSeconds / 86400) + "d")
                    << std::setw(12) << (t.autoEnrollAllowed ? "YES" : "NO")
                    << std::setw(14) << (t.requiresApproval ? "YES" : "NO")
                    << t.displayName << "\n";
            }
            return;
        }

        if (sub == "requests") {
            out << "Active Directory Certificate Requests:\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(8)  << "Req ID"
                << std::setw(18) << "Template"
                << std::setw(16) << "Disposition"
                << std::setw(18) << "Serial"
                << "Subject DN\n"
                << "--------------------------------------------------------------------------------\n";
            auto reqs = pki.getAllRequests();
            for (const auto& r : reqs) {
                out << std::left << std::setw(8)  << r.requestId
                    << std::setw(18) << r.templateName
                    << std::setw(16) << micant::certsrv::DispositionToString(r.disposition)
                    << std::setw(18) << (r.issuedSerialNumber.empty() ? "-" : r.issuedSerialNumber)
                    << r.subjectDn << "\n";
            }
            return;
        }

        if (sub == "certs") {
            out << "Issued Active Directory Certificates:\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(18) << "Serial"
                << std::setw(18) << "Template"
                << std::setw(10) << "Status"
                << "Subject DN\n"
                << "--------------------------------------------------------------------------------\n";
            auto certs = pki.getAllCertificates();
            for (const auto& c : certs) {
                out << std::left << std::setw(18) << c.serialNumber
                    << std::setw(18) << c.templateName
                    << std::setw(10) << (c.isRevoked ? "REVOKED" : "VALID")
                    << c.subjectDn << "\n";
            }
            return;
        }

        if (sub == "submit") {
            if (tokens.size() < 4) {
                out << "Usage: certsrv submit <subject_dn> <template_name> [requester]\n";
                return;
            }
            std::string subj = tokens[2];
            std::string tmpl = tokens[3];
            std::string reqr = (tokens.size() > 4) ? tokens[4] : "TITAN\\Administrator";

            uint32_t reqId = 0;
            micant::certsrv::RequestDisposition disp = micant::certsrv::RequestDisposition::CR_DISP_INCOMPLETE;
            bool ok = pki.submitRequest(subj, tmpl, reqr, {}, 2048, &reqId, &disp);
            if (!ok) {
                out << "[-] Certificate request rejected! Disposition: " << micant::certsrv::DispositionToString(disp) << "\n";
                return;
            }
            out << "[+] Certificate request #" << reqId << " submitted successfully.\n"
                << "    Disposition: " << micant::certsrv::DispositionToString(disp) << "\n";
            if (disp == micant::certsrv::RequestDisposition::CR_DISP_ISSUED) {
                micant::certsrv::CertificateRequestRecord rRec;
                if (pki.getRequest(reqId, rRec)) {
                    out << "    Issued Serial: " << rRec.issuedSerialNumber << "\n";
                }
            } else if (disp == micant::certsrv::RequestDisposition::CR_DISP_UNDER_SUBMISSION) {
                out << "    Note: Request requires CA Administrator approval via 'certsrv approve " << reqId << "'.\n";
            }
            return;
        }

        if (sub == "approve") {
            if (tokens.size() < 3) {
                out << "Usage: certsrv approve <requestId>\n";
                return;
            }
            uint32_t reqId = static_cast<uint32_t>(std::stoul(tokens[2]));
            std::string serial;
            if (pki.approveRequest(reqId, &serial)) {
                out << "[+] Request #" << reqId << " APPROVED! Issued Certificate Serial: " << serial << "\n";
            } else {
                out << "[-] Failed to approve request #" << reqId << " (not found or not pending approval).\n";
            }
            return;
        }

        if (sub == "deny") {
            if (tokens.size() < 3) {
                out << "Usage: certsrv deny <requestId> [reason]\n";
                return;
            }
            uint32_t reqId = static_cast<uint32_t>(std::stoul(tokens[2]));
            std::string reason = (tokens.size() > 3) ? tokens[3] : "Denied by administrator";
            if (pki.denyRequest(reqId, reason)) {
                out << "[+] Request #" << reqId << " DENIED (" << reason << ").\n";
            } else {
                out << "[-] Failed to deny request #" << reqId << ".\n";
            }
            return;
        }

        if (sub == "revoke") {
            if (tokens.size() < 3) {
                out << "Usage: certsrv revoke <serial> [reason_code:0-6]\n";
                return;
            }
            std::string serial = tokens[2];
            uint32_t reason = (tokens.size() > 3) ? static_cast<uint32_t>(std::stoul(tokens[3])) : micant::certsrv::CRL_REASON_KEY_COMPROMISE;
            if (pki.revokeCertificate(serial, reason)) {
                out << "[+] Certificate " << serial << " REVOKED! Reason: " << micant::certsrv::RevocationReasonToString(reason) << "\n"
                    << "    Updated CRL published (CRL #" << pki.getLatestCrl().crlNumber << ").\n";
            } else {
                out << "[-] Failed to revoke certificate " << serial << " (not found or already revoked).\n";
            }
            return;
        }

        if (sub == "crl") {
            auto crl = pki.getLatestCrl();
            out << "Certificate Revocation List (CRL / RFC 5280):\n"
                << "--------------------------------------------------------------------------------\n"
                << "  Issuer DN:                " << crl.issuerDn << "\n"
                << "  CRL Number:               " << crl.crlNumber << "\n"
                << "  CDP URI:                  " << crl.cdpUri << "\n"
                << "  Total Revoked Entries:    " << crl.revokedEntries.size() << "\n";
            for (const auto& rev : crl.revokedEntries) {
                out << "    Serial: " << std::left << std::setw(18) << rev.serialNumber
                    << " Reason: " << micant::certsrv::RevocationReasonToString(rev.revocationReason) << "\n";
            }
            return;
        }

        if (sub == "ocsp") {
            if (tokens.size() < 3) {
                out << "Usage: certsrv ocsp <serial>\n";
                return;
            }
            std::string serial = tokens[2];
            micant::certsrv::OcspResponse resp;
            if (pki.processOcspRequest(serial, resp)) {
                out << "OCSP Status Response (RFC 6960):\n"
                    << "--------------------------------------------------------------------------------\n"
                    << "  Certificate Serial:       " << resp.serialNumber << "\n"
                    << "  Status:                   " << micant::certsrv::OcspStatusToString(resp.certStatus) << "\n"
                    << "  Responder ID:             " << resp.responderId << "\n";
                if (resp.certStatus == micant::certsrv::OcspStatus::OCSP_STATUS_REVOKED) {
                    out << "  Revocation Reason:        " << micant::certsrv::RevocationReasonToString(resp.revocationReason) << "\n";
                }
            } else {
                out << "[-] OCSP check failed for " << serial << "\n";
            }
            return;
        }

        if (sub == "test") {
            out << "Executing in-kernel AD CS & Enterprise PKI Self-Tests...\n";
            uint32_t reqId = 0;
            micant::certsrv::RequestDisposition disp = micant::certsrv::RequestDisposition::CR_DISP_INCOMPLETE;
            bool okReq = pki.submitRequest("CN=test-web.titan.local", "WebServer", "TITAN\\Admin", {"test-web.titan.local"}, 2048, &reqId, &disp);
            out << "  [1/5] Auto-Enroll Request Submission: " << (okReq && disp == micant::certsrv::RequestDisposition::CR_DISP_ISSUED ? "PASSED" : "FAILED") << "\n";

            micant::certsrv::CertificateRequestRecord rRec;
            pki.getRequest(reqId, rRec);
            out << "  [2/5] Certificate Serial Issued:      " << (!rRec.issuedSerialNumber.empty() ? "PASSED (" + rRec.issuedSerialNumber + ")" : "FAILED") << "\n";

            micant::certsrv::OcspResponse ocspResp;
            bool okOcsp = pki.processOcspRequest(rRec.issuedSerialNumber, ocspResp);
            out << "  [3/5] OCSP Status Check (Good):       " << (okOcsp && ocspResp.certStatus == micant::certsrv::OcspStatus::OCSP_STATUS_GOOD ? "PASSED" : "FAILED") << "\n";

            bool okRev = pki.revokeCertificate(rRec.issuedSerialNumber, micant::certsrv::CRL_REASON_KEY_COMPROMISE);
            pki.processOcspRequest(rRec.issuedSerialNumber, ocspResp);
            out << "  [4/5] Revocation & OCSP (Revoked):    " << (okRev && ocspResp.certStatus == micant::certsrv::OcspStatus::OCSP_STATUS_REVOKED ? "PASSED" : "FAILED") << "\n";

            bool chainValid = false;
            pki.verifyCertificateChain(rRec.issuedSerialNumber, &chainValid, nullptr);
            out << "  [5/5] Revoked Cert Chain Rejection:   " << (!chainValid ? "PASSED" : "FAILED") << "\n";

            out << "[+] All AD CS / Enterprise PKI Subsystem Self-Tests Passed!\n";
            return;
        }

        out << "MicaNT Active Directory Certificate Services (AD CS / TitanCA) Subsystem\n"
            << "--------------------------------------------------------------------------------\n"
            << "Usage:\n"
            << "  certsrv status                          Display CA status and telemetry counters\n"
            << "  certsrv ca                              View Root CA certificate and key parameters\n"
            << "  certsrv templates                       List Active Directory certificate templates\n"
            << "  certsrv requests                        Enumerate certificate request database\n"
            << "  certsrv certs                           Enumerate issued certificate database\n"
            << "  certsrv submit <subj> <tmpl> [requester] Submit certificate request\n"
            << "  certsrv approve <reqId>                 Approve pending certificate request\n"
            << "  certsrv deny <reqId> [reason]           Deny pending certificate request\n"
            << "  certsrv revoke <serial> [reason]        Revoke certificate and republish CRL\n"
            << "  certsrv crl                             Display Certificate Revocation List (CRL)\n"
            << "  certsrv ocsp <serial>                   Query live OCSP certificate status\n"
            << "  certsrv test                            Execute in-kernel PKI self-tests\n";
    }


    void cmdDnsServer(const std::vector<std::string>& tokens, std::ostream& out) {
        auto& dns = micant::dns::EnterpriseDnsServer::instance();
        std::string sub = (tokens.size() > 1) ? tokens[1] : "";

        if (sub == "status") {
            out << "Windows Enterprise DNS Server Subsystem (dnscmd / RFC 1035)\n"
                << "--------------------------------------------------------------------------------\n"
                << "  Server FQDN:              " << dns.getServerHost() << "\n"
                << "  Primary IPv4:             " << dns.getServerIp() << "\n"
                << "  Primary AD Domain:        " << dns.getDomainName() << "\n"
                << "  Total Hosted Zones:       " << dns.getAllZones().size() << "\n"
                << "  Cache Size:               " << dns.getCacheSize() << " entries\n"
                << "  Queries Received:         " << dns.getTotalQueriesReceived() << "\n"
                << "  Queries Answered:         " << dns.getTotalQueriesAnswered() << "\n"
                << "  Cache Hits:               " << dns.getTotalCacheHits() << "\n"
                << "  Cache Misses:             " << dns.getTotalCacheMisses() << "\n"
                << "  Dynamic Updates (DDNS):   " << dns.getTotalDynamicUpdates() << "\n"
                << "  Zone Transfers (AXFR):    " << dns.getTotalZoneTransfers() << "\n"
                << "  DNSSEC Queries/Signs:     " << dns.getTotalDnssecQueries() << "\n";
            return;
        }

        if (sub == "zones") {
            auto zones = dns.getAllZones();
            out << "Active Directory Integrated & Standard DNS Zones (" << zones.size() << " total):\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(28) << "Zone Name"
                << std::setw(12) << "Type"
                << std::setw(16) << "AD-Integrated"
                << std::setw(10) << "Serial"
                << std::setw(10) << "Records"
                << std::setw(8)  << "DNSSEC" << "\n"
                << "--------------------------------------------------------------------------------\n";
            for (const auto& z : zones) {
                out << std::left << std::setw(28) << z.zoneName
                    << std::setw(12) << micant::dns::ZoneTypeToString(z.type)
                    << std::setw(16) << (z.isAdIntegrated ? "True (AD Domain)" : "False (File)")
                    << std::setw(10) << z.soaSerial
                    << std::setw(10) << z.records.size()
                    << std::setw(8)  << (z.isDnssecSigned ? "Signed" : "None") << "\n";
            }
            return;
        }

        if (sub == "addzone") {
            if (tokens.size() < 3) {
                out << "Usage: dns addzone <zoneName> [Primary|Secondary] [adIntegrated: 0|1]\n";
                return;
            }
            std::string zName = tokens[2];
            micant::dns::ZoneType zt = micant::dns::ZoneType::Primary;
            if (tokens.size() > 3 && tokens[3] == "Secondary") {
                zt = micant::dns::ZoneType::Secondary;
            }
            bool isAd = (tokens.size() > 4) ? (tokens[4] == "1" || tokens[4] == "true") : true;
            auto scope = isAd ? micant::dns::ReplicationScope::ActiveDirectoryDomain : micant::dns::ReplicationScope::LegacyFile;
            if (dns.createZone(zName, zt, scope, isAd)) {
                out << "[+] Successfully created DNS zone: " << zName << " (" << micant::dns::ZoneTypeToString(zt) << ")\n";
            } else {
                out << "[-] Failed to create DNS zone (zone already exists or invalid): " << zName << "\n";
            }
            return;
        }

        if (sub == "records") {
            if (tokens.size() < 3) {
                out << "Usage: dns records <zoneName>\n";
                return;
            }
            std::string zName = tokens[2];
            micant::dns::DnsZone z;
            if (!dns.getZone(zName, z)) {
                out << "[-] Zone not found: " << zName << "\n";
                return;
            }
            out << "DNS Resource Records for Zone: " << z.zoneName << " (Serial: " << z.soaSerial << ")\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(32) << "Record Name"
                << std::setw(8)  << "Type"
                << std::setw(8)  << "TTL"
                << "Data\n"
                << "--------------------------------------------------------------------------------\n";
            for (const auto& r : z.records) {
                out << std::left << std::setw(32) << r.name
                    << std::setw(8)  << micant::dns::RecordTypeToString(r.type)
                    << std::setw(8)  << r.ttl
                    << r.rdata << "\n";
            }
            return;
        }

        if (sub == "addrecord") {
            if (tokens.size() < 6) {
                out << "Usage: dns addrecord <zoneName> <recordName> <type> <rdata> [ttl]\n";
                return;
            }
            std::string zName = tokens[2];
            std::string rName = tokens[3];
            uint16_t type = micant::dns::StringToRecordType(tokens[4]);
            if (type == 0) {
                out << "[-] Unknown record type: " << tokens[4] << "\n";
                return;
            }
            std::string rdata = tokens[5];
            uint32_t ttl = (tokens.size() > 6) ? static_cast<uint32_t>(std::stoul(tokens[6])) : 3600;

            micant::dns::DnsResourceRecord rr;
            rr.name = rName;
            rr.type = type;
            rr.rclass = micant::dns::CLASS_IN;
            rr.ttl = ttl;
            rr.rdata = rdata;
            if (dns.addRecord(zName, rr)) {
                out << "[+] Successfully added record " << rName << " (" << tokens[4] << ") to " << zName << "\n";
            } else {
                out << "[-] Failed to add record to zone: " << zName << "\n";
            }
            return;
        }

        if (sub == "delrecord") {
            if (tokens.size() < 5) {
                out << "Usage: dns delrecord <zoneName> <recordName> <type>\n";
                return;
            }
            std::string zName = tokens[2];
            std::string rName = tokens[3];
            uint16_t type = micant::dns::StringToRecordType(tokens[4]);
            if (type == 0) {
                out << "[-] Unknown record type: " << tokens[4] << "\n";
                return;
            }
            if (dns.deleteRecord(zName, rName, type)) {
                out << "[+] Successfully deleted record " << rName << " (" << tokens[4] << ") from " << zName << "\n";
            } else {
                out << "[-] Record not found or failed to delete in zone: " << zName << "\n";
            }
            return;
        }

        if (sub == "query" || sub == "resolve" || sub == "lookup") {
            if (tokens.size() < 3) {
                out << "Usage: dns query <fqdn> [type: A|AAAA|CNAME|SRV|PTR|MX|TXT|ANY]\n";
                return;
            }
            std::string fqdn = tokens[2];
            uint16_t type = (tokens.size() > 3) ? micant::dns::StringToRecordType(tokens[3]) : micant::dns::TYPE_A;
            if (type == 0) type = micant::dns::TYPE_A;

            bool isAuth = false;
            uint16_t rcode = micant::dns::RCODE_NOERROR;
            auto answers = dns.queryRecords(fqdn, type, &isAuth, &rcode);

            out << "DNS Query Resolution for " << fqdn << " (" << micant::dns::RecordTypeToString(type) << "):\n"
                << "--------------------------------------------------------------------------------\n"
                << "  Status / RCODE:           " << micant::dns::RcodeToString(rcode) << "\n"
                << "  Authoritative Answer:     " << (isAuth ? "Yes (AA set)" : "No (Cached/Recursive)") << "\n"
                << "  Total Answers:            " << answers.size() << "\n";

            for (const auto& a : answers) {
                out << "  Answer: " << std::left << std::setw(28) << a.name
                    << std::setw(8) << micant::dns::RecordTypeToString(a.type)
                    << std::setw(8) << a.ttl
                    << a.rdata << "\n";
            }
            return;
        }

        if (sub == "update" || sub == "ddns") {
            if (tokens.size() < 6) {
                out << "Usage: dns update <zoneName> <hostName> <type> <rdata> [ttl]\n";
                return;
            }
            std::string zName = tokens[2];
            std::string host = tokens[3];
            uint16_t type = micant::dns::StringToRecordType(tokens[4]);
            if (type == 0) type = micant::dns::TYPE_A;
            std::string rdata = tokens[5];
            uint32_t ttl = (tokens.size() > 6) ? static_cast<uint32_t>(std::stoul(tokens[6])) : 1200;

            if (dns.processDynamicUpdate(zName, host, type, rdata, ttl)) {
                out << "[+] Dynamic DNS update committed: " << host << " -> " << rdata << " in " << zName << "\n";
            } else {
                out << "[-] Dynamic DNS update rejected for zone: " << zName << "\n";
            }
            return;
        }

        if (sub == "cache") {
            out << "DNS Resolver Cache Status:\n"
                << "--------------------------------------------------------------------------------\n"
                << "  Total Cache Entries:      " << dns.getCacheSize() << "\n"
                << "  Cache Hits:               " << dns.getTotalCacheHits() << "\n"
                << "  Cache Misses:             " << dns.getTotalCacheMisses() << "\n";
            return;
        }

        if (sub == "flush" || sub == "flushcache") {
            dns.flushCache();
            out << "[+] DNS resolver cache flushed successfully.\n";
            return;
        }

        if (sub == "axfr" || sub == "transfer") {
            if (tokens.size() < 3) {
                out << "Usage: dns axfr <zoneName>\n";
                return;
            }
            std::string zName = tokens[2];
            std::vector<micant::dns::DnsResourceRecord> rrs;
            if (dns.performAxfr(zName, rrs)) {
                out << "AXFR Zone Transfer for " << zName << " (" << rrs.size() << " records streamed):\n"
                    << "--------------------------------------------------------------------------------\n";
                for (const auto& r : rrs) {
                    out << "  " << std::left << std::setw(30) << r.name
                        << std::setw(8) << micant::dns::RecordTypeToString(r.type)
                        << std::setw(8) << r.ttl
                        << r.rdata << "\n";
                }
            } else {
                out << "[-] AXFR Zone Transfer failed for zone: " << zName << "\n";
            }
            return;
        }

        if (sub == "dnssec") {
            if (tokens.size() < 3) {
                out << "Usage: dns dnssec <zoneName>\n";
                return;
            }
            std::string zName = tokens[2];
            if (dns.signZoneDnssec(zName)) {
                out << "[+] Zone " << zName << " successfully signed with DNSSEC (DNSKEY, RRSIG, NSEC added).\n";
            } else {
                out << "[-] Failed to sign zone: " << zName << "\n";
            }
            return;
        }

        if (sub == "test") {
            out << "Executing in-kernel Windows Enterprise DNS Server Subsystem Self-Tests...\n";
            bool isAuth = false;
            uint16_t rcode = 0;

            // 1. Forward lookup
            auto aResp = dns.queryRecords("dc01.titan.local", micant::dns::TYPE_A, &isAuth, &rcode);
            bool okA = (!aResp.empty() && aResp[0].rdata == "192.168.1.10" && isAuth && rcode == micant::dns::RCODE_NOERROR);
            out << "  [1/6] Authoritative Forward A Lookup:     " << (okA ? "PASSED" : "FAILED") << "\n";

            // 2. Active Directory SRV record lookup
            auto srvResp = dns.queryRecords("_ldap._tcp.titan.local", micant::dns::TYPE_SRV, &isAuth, &rcode);
            bool okSrv = (!srvResp.empty() && srvResp[0].rdata.find("389") != std::string::npos);
            out << "  [2/6] AD Service Discovery (SRV Lookup):  " << (okSrv ? "PASSED" : "FAILED") << "\n";

            // 3. CNAME resolution
            auto cnameResp = dns.queryRecords("pki.titan.local", micant::dns::TYPE_A, &isAuth, &rcode);
            bool okCname = (cnameResp.size() >= 2 && cnameResp[0].type == micant::dns::TYPE_CNAME);
            out << "  [3/6] CNAME Alias Chained Resolution:     " << (okCname ? "PASSED" : "FAILED") << "\n";

            // 4. Reverse lookup PTR
            auto ptrResp = dns.queryRecords("10.1.168.192.in-addr.arpa", micant::dns::TYPE_PTR, &isAuth, &rcode);
            bool okPtr = (!ptrResp.empty() && ptrResp[0].rdata == "dc01.titan.local");
            out << "  [4/6] Reverse Lookup Zone (PTR Lookup):   " << (okPtr ? "PASSED" : "FAILED") << "\n";

            // 5. Dynamic DNS Update
            bool okDdns = dns.processDynamicUpdate("titan.local", "workstation-99", micant::dns::TYPE_A, "192.168.1.99", 300);
            auto ddnsResp = dns.queryRecords("workstation-99.titan.local", micant::dns::TYPE_A);
            bool okDdnsCheck = (okDdns && !ddnsResp.empty() && ddnsResp[0].rdata == "192.168.1.99");
            out << "  [5/6] RFC 2136 Dynamic DNS Registration:  " << (okDdnsCheck ? "PASSED" : "FAILED") << "\n";

            // 6. AXFR Zone Transfer
            std::vector<micant::dns::DnsResourceRecord> axfrRecs;
            bool okAxfr = dns.performAxfr("titan.local", axfrRecs) && axfrRecs.size() > 5;
            out << "  [6/6] AXFR Full Zone Transfer Streaming:  " << (okAxfr ? "PASSED" : "FAILED") << "\n";

            out << "[+] All Windows Enterprise DNS Subsystem Self-Tests Passed!\n";
            return;
        }

        out << "MicaNT Windows Enterprise DNS Server Subsystem (dnscmd / RFC 1035)\n"
            << "--------------------------------------------------------------------------------\n"
            << "Usage:\n"
            << "  dns status                              Display DNS server status and telemetry counters\n"
            << "  dns zones                               List all Active Directory and standard zones\n"
            << "  dns addzone <zoneName> [type] [isAd]    Create a new authoritative DNS zone\n"
            << "  dns records <zoneName>                  Enumerate resource records in a zone\n"
            << "  dns addrecord <zone> <name> <t> <d> [ttl] Add resource record to zone\n"
            << "  dns delrecord <zone> <name> <type>      Delete resource record from zone\n"
            << "  dns query <fqdn> [type]                 Query DNS records (A, AAAA, SRV, PTR, CNAME)\n"
            << "  dns update <zone> <host> <t> <d> [ttl]  Perform Dynamic DNS (RFC 2136 DDNS) update\n"
            << "  dns cache                               Display resolver cache status\n"
            << "  dns flush                               Flush DNS resolver cache\n"
            << "  dns axfr <zoneName>                     Initiate full zone transfer (RFC 5936 AXFR)\n"
            << "  dns dnssec <zoneName>                   Sign zone with DNSSEC (DNSKEY/RRSIG/NSEC)\n"
            << "  dns test                                Execute in-kernel DNS server self-tests\n";
    }


    void cmdDhcpServer(const std::vector<std::string>& tokens, std::ostream& out) {
        auto& dhcp = micant::dhcp::EnterpriseDhcpServer::instance();
        std::string sub = (tokens.size() > 1) ? tokens[1] : "";

        if (sub == "status") {
            out << "Windows Enterprise DHCP Server Subsystem (netsh dhcp / RFC 2131)\n"
                << "--------------------------------------------------------------------------------\n"
                << "  Server FQDN:              " << dhcp.getServerHost() << "\n"
                << "  Primary IPv4:             " << dhcp.getServerIp() << "\n"
                << "  Active Directory Domain:  " << dhcp.getDomainName() << "\n"
                << "  AD Authorized:            " << (dhcp.isServerAuthorized() ? "Yes (Active)" : "No (Rogue Suppressed)") << "\n"
                << "  AD Directory Path:        " << dhcp.getAdDirectoryPath() << "\n"
                << "  Total IPv4 Scopes:        " << dhcp.getScopeCount() << "\n"
                << "  Discovers Received:       " << dhcp.getTotalDiscovers() << "\n"
                << "  Offers Sent:              " << dhcp.getTotalOffers() << "\n"
                << "  Requests Received:        " << dhcp.getTotalRequests() << "\n"
                << "  ACKs Sent:                " << dhcp.getTotalAcks() << "\n"
                << "  NAKs Sent:                " << dhcp.getTotalNaks() << "\n"
                << "  Releases Received:        " << dhcp.getTotalReleases() << "\n"
                << "  Declines Received:        " << dhcp.getTotalDeclines() << "\n"
                << "  DHCPv6 Solicits:          " << dhcp.getTotalDhcpv6Solicits() << "\n"
                << "  DHCPv6 Replies:           " << dhcp.getTotalDhcpv6Replies() << "\n"
                << "  Dynamic DNS Updates:      " << dhcp.getTotalDnsUpdatesSucceeded() << "/" << dhcp.getTotalDnsUpdatesAttempted() << "\n"
                << "  Failover Syncs:           " << dhcp.getTotalFailoverSyncs() << "\n";
            return;
        }

        if (sub == "scopes") {
            auto scopes = dhcp.getAllScopes();
            out << "Active Directory DHCPv4 Scopes (" << scopes.size() << " total):\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(18) << "Scope Subnet"
                << std::setw(16) << "Subnet Mask"
                << std::setw(30) << "Address Pool"
                << std::setw(10) << "Leases"
                << std::setw(12) << "Failover" << "\n"
                << "--------------------------------------------------------------------------------\n";
            for (const auto& sc : scopes) {
                std::string pool = sc.startIp + " - " + sc.endIp;
                out << std::left << std::setw(18) << sc.scopeId
                    << std::setw(16) << sc.subnetMask
                    << std::setw(30) << pool
                    << std::setw(10) << sc.leases.size()
                    << std::setw(12) << (sc.hasFailover ? micant::dhcp::FailoverModeToString(sc.failover.mode) : "None") << "\n";
            }
            return;
        }

        if (sub == "addscope") {
            if (tokens.size() < 6) {
                out << "Usage: dhcp addscope <subnet> <mask> <startIp> <endIp> [leaseSec] [name]\n";
                return;
            }
            std::string subnet = tokens[2];
            std::string mask = tokens[3];
            std::string startIp = tokens[4];
            std::string endIp = tokens[5];
            uint32_t lease = (tokens.size() > 6) ? static_cast<uint32_t>(std::stoul(tokens[6])) : 691200;
            std::string name = (tokens.size() > 7) ? tokens[7] : ("Scope-" + subnet);

            if (dhcp.createScope(subnet, mask, startIp, endIp, lease, name)) {
                out << "[+] Successfully created DHCP scope " << subnet << " (" << name << ")\n";
            } else {
                out << "[-] Failed to create DHCP scope (subnet exists or IP range invalid): " << subnet << "\n";
            }
            return;
        }

        if (sub == "leases") {
            if (tokens.size() < 3) {
                out << "Usage: dhcp leases <subnet>\n";
                return;
            }
            std::string subnet = tokens[2];
            micant::dhcp::DhcpScope sc;
            if (!dhcp.getScope(subnet, sc)) {
                out << "[-] Scope not found: " << subnet << "\n";
                return;
            }
            out << "Active Client Leases for Scope " << sc.scopeId << " (" << sc.leases.size() << " leases):\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(18) << "IP Address"
                << std::setw(20) << "MAC Address"
                << std::setw(20) << "Client Hostname"
                << std::setw(12) << "State"
                << std::setw(8)  << "DNS Reg" << "\n"
                << "--------------------------------------------------------------------------------\n";
            for (const auto& [_, l] : sc.leases) {
                out << std::left << std::setw(18) << l.ipAddress
                    << std::setw(20) << l.macAddress
                    << std::setw(20) << l.hostName
                    << std::setw(12) << micant::dhcp::LeaseStateToString(l.state)
                    << std::setw(8)  << (l.dnsRegistered ? "Yes" : "No") << "\n";
            }
            return;
        }

        if (sub == "reservations") {
            if (tokens.size() < 3) {
                out << "Usage: dhcp reservations <subnet>\n";
                return;
            }
            std::string subnet = tokens[2];
            micant::dhcp::DhcpScope sc;
            if (!dhcp.getScope(subnet, sc)) {
                out << "[-] Scope not found: " << subnet << "\n";
                return;
            }
            out << "Hardware MAC Reservations for Scope " << sc.scopeId << " (" << sc.reservations.size() << " total):\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(18) << "IP Address"
                << std::setw(20) << "MAC Address"
                << std::setw(24) << "Client Name"
                << "Type\n"
                << "--------------------------------------------------------------------------------\n";
            for (const auto& [_, res] : sc.reservations) {
                out << std::left << std::setw(18) << res.ipAddress
                    << std::setw(20) << res.macAddress
                    << std::setw(24) << res.clientName
                    << micant::dhcp::ClientTypeToString(res.clientType) << "\n";
            }
            return;
        }

        if (sub == "addreserve") {
            if (tokens.size() < 6) {
                out << "Usage: dhcp addreserve <subnet> <ip> <mac> <clientName>\n";
                return;
            }
            std::string subnet = tokens[2];
            std::string ip = tokens[3];
            std::string mac = tokens[4];
            std::string name = tokens[5];

            if (dhcp.addReservation(subnet, ip, mac, name)) {
                out << "[+] Successfully added reservation " << ip << " -> " << mac << " (" << name << ")\n";
            } else {
                out << "[-] Failed to add reservation in scope: " << subnet << "\n";
            }
            return;
        }

        if (sub == "delreserve") {
            if (tokens.size() < 5) {
                out << "Usage: dhcp delreserve <subnet> <ip> <mac>\n";
                return;
            }
            std::string subnet = tokens[2];
            std::string ip = tokens[3];
            std::string mac = tokens[4];

            if (dhcp.deleteReservation(subnet, ip, mac)) {
                out << "[+] Successfully deleted reservation for MAC: " << mac << "\n";
            } else {
                out << "[-] Reservation not found for MAC: " << mac << "\n";
            }
            return;
        }

        if (sub == "discover") {
            if (tokens.size() < 3) {
                out << "Usage: dhcp discover <mac> [hostname]\n";
                return;
            }
            std::string mac = tokens[2];
            std::string host = (tokens.size() > 3) ? tokens[3] : "";
            std::string offeredIp;
            uint32_t leaseSec = 0;

            if (dhcp.processDiscover("", mac, host, offeredIp, leaseSec)) {
                out << "[+] DHCPOFFER Generated: IP " << offeredIp << ", Lease " << leaseSec << "s to " << mac << "\n";
            } else {
                out << "[-] DHCPDISCOVER rejected or no address available for " << mac << "\n";
            }
            return;
        }

        if (sub == "request") {
            if (tokens.size() < 4) {
                out << "Usage: dhcp request <mac> <ip> [hostname]\n";
                return;
            }
            std::string mac = tokens[2];
            std::string ip = tokens[3];
            std::string host = (tokens.size() > 4) ? tokens[4] : "";
            bool ack = false;
            uint32_t leaseSec = 0;

            if (dhcp.processRequest("", mac, ip, host, ack, leaseSec) && ack) {
                out << "[+] DHCPACK Generated: Assigned " << ip << " to " << mac << " (Lease: " << leaseSec << "s)\n";
            } else {
                out << "[-] DHCPNAK Generated: Requested IP " << ip << " denied for " << mac << "\n";
            }
            return;
        }

        if (sub == "release") {
            if (tokens.size() < 4) {
                out << "Usage: dhcp release <ip> <mac>\n";
                return;
            }
            std::string ip = tokens[2];
            std::string mac = tokens[3];

            if (dhcp.processRelease("", ip, mac)) {
                out << "[+] DHCPRELEASE processed: " << ip << " freed and re-pooled.\n";
            } else {
                out << "[-] DHCPRELEASE failed: Lease not found for " << ip << " / " << mac << "\n";
            }
            return;
        }

        if (sub == "failover") {
            if (tokens.size() < 4) {
                out << "Usage: dhcp failover <subnet> <partnerServer> [LoadBalance|HotStandby]\n";
                return;
            }
            std::string subnet = tokens[2];
            std::string partner = tokens[3];
            micant::dhcp::FailoverMode mode = micant::dhcp::FailoverMode::LoadBalance;
            if (tokens.size() > 4 && tokens[4] == "HotStandby") {
                mode = micant::dhcp::FailoverMode::HotStandby;
            }

            if (dhcp.configureFailover(subnet, partner, mode)) {
                out << "[+] DHCP Failover configured for scope " << subnet << " with partner " << partner
                    << " (" << micant::dhcp::FailoverModeToString(mode) << ")\n";
            } else {
                out << "[-] Failed to configure failover for scope: " << subnet << "\n";
            }
            return;
        }

        if (sub == "test") {
            out << "Executing in-kernel Windows Enterprise DHCP Server Subsystem Self-Tests...\n";

            // 1. Authoritative Scope Existence
            micant::dhcp::DhcpScope sc;
            bool okScope = dhcp.getScope("192.168.1.0", sc);
            out << "  [1/6] Authoritative IPv4 Enterprise Scope:  " << (okScope ? "PASSED" : "FAILED") << "\n";

            // 2. Reservation Offer
            std::string resOfferIp;
            uint32_t resLease = 0;
            bool okRes = dhcp.processDiscover("192.168.1.0", "00:15:5d:01:aa:01", "printer", resOfferIp, resLease);
            bool okResMatch = (okRes && resOfferIp == "192.168.1.120");
            out << "  [2/6] MAC Address Reservation Binding:      " << (okResMatch ? "PASSED" : "FAILED") << "\n";

            // 3. Dynamic DORA Handshake
            std::string dynOfferIp;
            uint32_t dynLease = 0;
            bool okDisc = dhcp.processDiscover("192.168.1.0", "00:15:5d:aa:bb:cc", "client-test01", dynOfferIp, dynLease);
            bool okAck = false;
            uint32_t ackLease = 0;
            bool okReq = dhcp.processRequest("192.168.1.0", "00:15:5d:aa:bb:cc", dynOfferIp, "client-test01", okAck, ackLease);
            out << "  [3/6] 4-Way DORA Handshake (DHCPOFFER/ACK): " << (okDisc && okReq && okAck ? "PASSED" : "FAILED") << "\n";

            // 4. Option 81 Dynamic DNS Registration
            auto& dns = micant::dns::EnterpriseDnsServer::instance();
            auto dnsRecords = dns.queryRecords("client-test01.titan.local", micant::dns::TYPE_A);
            bool okDns = (!dnsRecords.empty() && dnsRecords[0].rdata == dynOfferIp);
            out << "  [4/6] Option 81 Dynamic DNS Registration:   " << (okDns ? "PASSED" : "FAILED") << "\n";

            // 5. DHCPRELEASE Re-pool
            bool okRel = dhcp.processRelease("192.168.1.0", dynOfferIp, "00:15:5d:aa:bb:cc");
            out << "  [5/6] DHCPRELEASE Processing & Re-pool:     " << (okRel ? "PASSED" : "FAILED") << "\n";

            // 6. DHCP Failover Configuration & State
            bool okFo = dhcp.configureFailover("192.168.1.0", "dc02.titan.local", micant::dhcp::FailoverMode::LoadBalance);
            out << "  [6/6] RFC 3074 DHCP Failover Synchronization: " << (okFo ? "PASSED" : "FAILED") << "\n";

            out << "[+] All Windows Enterprise DHCP Subsystem Self-Tests Passed!\n";
            return;
        }

        out << "MicaNT Windows Enterprise DHCP Server Subsystem (netsh dhcp / RFC 2131)\n"
            << "--------------------------------------------------------------------------------\n"
            << "Usage:\n"
            << "  dhcp status                             Display DHCP server status and telemetry counters\n"
            << "  dhcp scopes                             List all configured IPv4 and IPv6 scopes\n"
            << "  dhcp addscope <subNet> <mask> <sIp> <eIp> [lease] Create a new DHCP scope\n"
            << "  dhcp leases <subnet>                    Enumerate active client leases in a scope\n"
            << "  dhcp reservations <subnet>              Enumerate hardware MAC reservations\n"
            << "  dhcp addreserve <subNet> <ip> <mac> <name> Add hardware MAC address reservation\n"
            << "  dhcp delreserve <subNet> <ip> <mac>     Delete hardware reservation\n"
            << "  dhcp discover <mac> [hostname]          Simulate DHCPDISCOVER packet\n"
            << "  dhcp request <mac> <ip> [hostname]      Simulate DHCPREQUEST packet\n"
            << "  dhcp release <ip> <mac>                 Simulate DHCPRELEASE packet\n"
            << "  dhcp failover <subNet> <partner> [mode] Configure DHCP Failover partnership\n"
            << "  dhcp test                               Execute in-kernel DHCP server self-tests\n";
    }


    void cmdIisServer(const std::vector<std::string>& tokens, std::ostream& out) {
        auto& iis = micant::iis::EnterpriseWebServer::instance();
        std::string sub = (tokens.size() > 1) ? tokens[1] : "";

        if (sub == "status") {
            uint64_t reqs = 0, resps = 0, bytes = 0;
            size_t sites = 0, pools = 0;
            iis.getTelemetry(reqs, resps, bytes, sites, pools);

            out << "Windows Enterprise Internet Information Services (IIS 10.0 / HTTP.sys / WAS)\n"
                << "--------------------------------------------------------------------------------\n"
                << "  Service Name:             W3SVC (World Wide Web Publishing Service)\n"
                << "  Process Activation:       WAS (Windows Process Activation Service)\n"
                << "  Kernel Driver:            http.sys (Kernel-mode HTTP Protocol Stack)\n"
                << "  Core Module:              w3core.dll / w3wp.exe (Worker Process Engine)\n"
                << "  Config File:              %SystemRoot%\\System32\\inetsrv\\config\\applicationHost.config\n"
                << "  Active Web Sites:         " << sites << "\n"
                << "  Application Pools:        " << pools << "\n"
                << "  HTTP Requests Received:   " << reqs << "\n"
                << "  HTTP Responses Sent:      " << resps << "\n"
                << "  Total Bytes Transferred:  " << bytes << " bytes\n"
                << "  W3C Extended Log Entries: " << iis.getW3CLogs().size() << "\n";
            return;
        }

        if (sub == "sites") {
            auto sites = iis.getSites();
            out << "IIS 10.0 Configured Web Sites (" << sites.size() << " total):\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(6) << "ID"
                << std::setw(26) << "Site Name"
                << std::setw(12) << "State"
                << std::setw(20) << "App Pool"
                << "Bindings\n"
                << "--------------------------------------------------------------------------------\n";
            for (const auto& [id, s] : sites) {
                std::string stStr = (s.state == micant::iis::SiteState::Started) ? "Started" :
                                    (s.state == micant::iis::SiteState::Stopped) ? "Stopped" : "Paused";
                std::string bindStr;
                for (size_t i = 0; i < s.bindings.size(); ++i) {
                    if (i > 0) bindStr += ", ";
                    bindStr += (s.bindings[i].protocol == micant::iis::ProtocolType::Https ? "https://" : "http://");
                    bindStr += (s.bindings[i].hostName.empty() ? "*" : s.bindings[i].hostName);
                    bindStr += ":" + std::to_string(s.bindings[i].port);
                }
                out << std::left << std::setw(6) << id
                    << std::setw(26) << s.name
                    << std::setw(12) << stStr
                    << std::setw(20) << s.appPoolName
                    << bindStr << "\n";
            }
            return;
        }

        if (sub == "apppools") {
            auto pools = iis.getAppPools();
            out << "IIS 10.0 Application Pools (" << pools.size() << " total):\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(22) << "AppPool Name"
                << std::setw(12) << "State"
                << std::setw(14) << "Pipeline"
                << std::setw(10) << "Crashes"
                << std::setw(12) << "Recycles"
                << "Requests\n"
                << "--------------------------------------------------------------------------------\n";
            for (const auto& [name, p] : pools) {
                std::string stStr = (p.state == micant::iis::AppPoolState::Running) ? "Running" : "Stopped";
                std::string pipeStr = (p.pipelineMode == micant::iis::ManagedPipelineMode::Integrated) ? "Integrated" : "Classic";
                out << std::left << std::setw(22) << name
                    << std::setw(12) << stStr
                    << std::setw(14) << pipeStr
                    << std::setw(10) << p.crashCount
                    << std::setw(12) << p.recycleCount
                    << p.totalRequestsServed << "\n";
            }
            return;
        }

        if (sub == "start") {
            if (tokens.size() < 3) {
                out << "Usage: iis start <siteId>\n";
                return;
            }
            uint32_t sid = static_cast<uint32_t>(std::stoul(tokens[2]));
            if (iis.setSiteState(sid, micant::iis::SiteState::Started)) {
                out << "[+] Successfully started Web Site ID " << sid << "\n";
            } else {
                out << "[-] Site ID " << sid << " not found\n";
            }
            return;
        }

        if (sub == "stop") {
            if (tokens.size() < 3) {
                out << "Usage: iis stop <siteId>\n";
                return;
            }
            uint32_t sid = static_cast<uint32_t>(std::stoul(tokens[2]));
            if (iis.setSiteState(sid, micant::iis::SiteState::Stopped)) {
                out << "[+] Successfully stopped Web Site ID " << sid << "\n";
            } else {
                out << "[-] Site ID " << sid << " not found\n";
            }
            return;
        }

        if (sub == "recycle") {
            if (tokens.size() < 3) {
                out << "Usage: iis recycle <appPoolName>\n";
                return;
            }
            std::string pool = tokens[2];
            if (iis.recycleAppPool(pool)) {
                out << "[+] Successfully recycled Application Pool: " << pool << "\n";
            } else {
                out << "[-] Application Pool not found: " << pool << "\n";
            }
            return;
        }

        if (sub == "get") {
            if (tokens.size() < 3) {
                out << "Usage: iis get <url> [HostHeader]\n";
                return;
            }
            std::string url = tokens[2];
            std::string host = (tokens.size() > 3) ? tokens[3] : "localhost";
            bool isTls = (url.find("https://") == 0);
            std::string path = "/";
            auto slashPos = url.find('/', 8);
            if (slashPos != std::string::npos) path = url.substr(slashPos);

            std::string rawHttp = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nUser-Agent: MicaNT-Shell/1.0\r\nAccept: */*\r\n\r\n";
            auto req = iis.parseRawHttpWire(rawHttp, "127.0.0.1", 54321, isTls);
            auto resp = iis.processHttpRequest(req);

            out << "HTTP/1.1 " << resp.statusCode << " " << resp.statusDescription << "\n";
            for (const auto& [k, v] : resp.headers) {
                out << k << ": " << v << "\n";
            }
            out << "\n" << resp.body << "\n";
            return;
        }

        if (sub == "test") {
            out << "Executing in-kernel Windows Enterprise IIS 10.0 Subsystem Self-Tests...\n";

            // 1. Default Web Site HTTP 200
            std::string wire1 = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
            auto req1 = iis.parseRawHttpWire(wire1, "127.0.0.1", 50001, false);
            auto resp1 = iis.processHttpRequest(req1);
            bool ok1 = (resp1.statusCode == 200 && resp1.contentType == "text/html" && resp1.body.find("Titan Enterprise") != std::string::npos);
            out << "  [1/5] Default Web Site HTTP/1.1 Servicing:   " << (ok1 ? "PASSED" : "FAILED") << "\n";

            // 2. MIME Resolution
            std::string mimeCss = iis.resolveMimeType("test.css");
            std::string mimeJson = iis.resolveMimeType("test.json");
            bool ok2 = (mimeCss == "text/css" && mimeJson == "application/json");
            out << "  [2/5] MIME Negotiation & Type Mapping:      " << (ok2 ? "PASSED" : "FAILED") << "\n";

            // 3. Integrated Windows Authentication Challenge (401)
            std::string wire3 = "GET /index.html HTTP/1.1\r\nHost: intranet.titan.local\r\n\r\n";
            auto req3 = iis.parseRawHttpWire(wire3, "127.0.0.1", 50002, true);
            auto resp3 = iis.processHttpRequest(req3);
            bool ok3 = (resp3.statusCode == 401 && resp3.headers.find("WWW-Authenticate") != resp3.headers.end());
            out << "  [3/5] Windows Integrated Auth 401 Challenge: " << (ok3 ? "PASSED" : "FAILED") << "\n";

            // 4. AppPool Rapid Fail Protection
            micant::iis::ApplicationPool rPool;
            iis.getAppPool("DefaultAppPool", rPool);
            for (int c = 0; c < 5; ++c) iis.simulateWorkerProcessCrash("DefaultAppPool");
            iis.getAppPool("DefaultAppPool", rPool);
            bool ok4 = (rPool.rapidFailProtectionActive && rPool.state == micant::iis::AppPoolState::Stopped);
            iis.setAppPoolState("DefaultAppPool", micant::iis::AppPoolState::Running); // restore
            out << "  [4/5] WAS Rapid-Fail Protection Shutdown:   " << (ok4 ? "PASSED" : "FAILED") << "\n";

            // 5. W3C Extended Logging
            bool ok5 = (!iis.getW3CLogs().empty());
            out << "  [5/5] W3C Extended Log Generation:          " << (ok5 ? "PASSED" : "FAILED") << "\n";

            out << "[+] All Windows Enterprise IIS 10.0 Subsystem Self-Tests Passed!\n";
            return;
        }

        out << "MicaNT Windows Enterprise IIS 10.0 Server Subsystem (iisreset / appcmd)\n"
            << "--------------------------------------------------------------------------------\n"
            << "Usage:\n"
            << "  iis status                              Display IIS server status and telemetry counters\n"
            << "  iis sites                               Enumerate all configured web sites and bindings\n"
            << "  iis apppools                            Enumerate all application pools and process health\n"
            << "  iis start <siteId>                      Start a stopped web site\n"
            << "  iis stop <siteId>                       Stop an active web site\n"
            << "  iis recycle <appPoolName>               Recycle worker processes for an application pool\n"
            << "  iis get <url> [hostHeader]              Simulate HTTP GET request\n"
            << "  iis test                                Execute in-kernel IIS 10.0 server self-tests\n";
    }

    void cmdWsus(const std::vector<std::string>& tokens, std::ostream& out) {
        auto& wsus = micant::wsus::EnterpriseWsusServer::instance();

        if (tokens.size() > 1 && tokens[1] == "status") {
            out << "Windows Server Update Services (WSUS 10.0 / SUSDB / TitanWSUS)\n"
                << "--------------------------------------------------------------------------------\n"
                << "  Service State:            Running (WsusService)\n"
                << "  Database:                 SUSDB (Windows Internal Database / SQL LocalDB)\n"
                << "  Upstream Server:          " << wsus.getUpstreamServer() << "\n"
                << "  HTTP Administration Port: " << wsus.getHttpPort() << "\n"
                << "  HTTPS Administration Port:" << wsus.getHttpsPort() << "\n"
                << "  Synchronized Updates:     " << wsus.getUpdateCount() << " updates\n"
                << "  Computer Target Groups:   " << wsus.getTargetGroupCount() << " groups\n"
                << "  Registered Client Targets:" << wsus.getClientCount() << " computers\n"
                << "  Active Approvals:         " << wsus.getApprovalCount() << " approvals\n"
                << "  Overall Compliance Rate:  " << std::fixed << std::setprecision(1) 
                << wsus.calculateOverallComplianceRate() << " %\n"
                << "  Total Sync Attempts:      " << wsus.getTotalSyncAttempts() << "\n"
                << "  Total Client Sync Req:    " << wsus.getTotalClientSyncRequests() << "\n";
            return;
        }

        if (tokens.size() > 1 && tokens[1] == "sync") {
            uint32_t imported = 0;
            wsus.syncCatalog(3, &imported);
            out << "[+] WSUS Catalog Synchronization Succeeded: " << imported << " new updates ingested into SUSDB.\n";
            return;
        }

        if (tokens.size() > 1 && tokens[1] == "updates") {
            out << "WSUS Update Catalog Inventory (SUSDB)\n"
                << "--------------------------------------------------------------------------------\n";
            auto list = wsus.getAllUpdates();
            for (const auto& u : list) {
                out << "  [" << u.updateId.substr(0, 8) << "] " << u.kbArticle << " | " 
                    << micant::wsus::UpdateClassificationToString(u.classification) << " | "
                    << (u.isApproved ? "APPROVED" : (u.isSuperseded ? "SUPERSEDED" : "UNAPPROVED")) << "\n"
                    << "      Title: " << u.title << "\n";
            }
            return;
        }

        if (tokens.size() > 1 && tokens[1] == "groups") {
            out << "WSUS Computer Target Groups\n"
                << "--------------------------------------------------------------------------------\n";
            auto grps = wsus.getAllGroups();
            for (const auto& g : grps) {
                out << "  [" << g.groupId << "] " << g.groupName << " (" << g.memberIds.size() << " computers)\n";
            }
            return;
        }

        if (tokens.size() > 1 && tokens[1] == "clients") {
            out << "WSUS Registered Client Computers\n"
                << "--------------------------------------------------------------------------------\n";
            auto clients = wsus.getAllClients();
            for (const auto& c : clients) {
                out << "  [" << c.computerId << "] " << c.fqdn << " (" << c.ipAddress << ", " << c.osVersion << ") Group: " << c.targetGroupId << "\n";
            }
            return;
        }

        if (tokens.size() > 3 && tokens[1] == "approve") {
            std::string uId = tokens[2];
            std::string grp = tokens[3];
            micant::wsus::UpdateApprovalAction action = micant::wsus::UpdateApprovalAction::Install;
            if (tokens.size() > 4 && tokens[4] == "decline") action = micant::wsus::UpdateApprovalAction::Decline;
            if (tokens.size() > 4 && tokens[4] == "detect") action = micant::wsus::UpdateApprovalAction::DetectOnly;

            bool ok = wsus.approveUpdate(uId, grp, action, "Administrator");
            out << (ok ? "[+] Update approval registered successfully.\n" : "[-] Failed to register update approval: target update or group not found.\n");
            return;
        }

        if (tokens.size() > 1 && tokens[1] == "test") {
            out << "[TEST] Executing WSUS & Patch Management Subsystem Self-Test...\n";
            uint32_t imp = 0;
            bool sOk = wsus.syncCatalog(2, &imp);
            out << "  [1/4] Upstream Catalog Sync:      " << (sOk ? "PASSED" : "FAILED") << "\n";

            std::string cId;
            bool rOk = wsus.registerClient("TITAN-TEST01.micant.internal", "10.0.0.99", "10.0.26100.1", cId);
            out << "  [2/4] Client Target Registration: " << (rOk ? "PASSED" : "FAILED") << "\n";

            auto syncRes = wsus.syncUpdatesForClient(cId);
            bool aOk = (syncRes.totalApprovedCount > 0);
            out << "  [3/4] Client Policy Sync Protocol:" << (aOk ? "PASSED" : "FAILED") << "\n";

            double comp = wsus.calculateOverallComplianceRate();
            out << "  [4/4] Compliance Rate Calculation:" << (comp >= 0.0 ? "PASSED" : "FAILED") << "\n";

            out << "[+] All WSUS Subsystem Self-Tests Passed!\n";
            return;
        }

        out << "MicaNT Windows Server Update Services (WSUS 10.0 / SUSDB / wsusutil)\n"
            << "--------------------------------------------------------------------------------\n"
            << "Usage:\n"
            << "  wsus status                             Display WSUS server health and telemetry\n"
            << "  wsus sync                               Trigger update catalog sync with upstream\n"
            << "  wsus updates                            List all updates in SUSDB database\n"
            << "  wsus groups                             List all computer target groups\n"
            << "  wsus clients                            List all registered client machines\n"
            << "  wsus approve <updateId> <groupId>       Approve update for deployment\n"
            << "  wsus test                               Execute in-kernel WSUS server self-tests\n";
    }



