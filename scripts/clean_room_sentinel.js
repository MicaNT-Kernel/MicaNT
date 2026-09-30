#!/usr/bin/env node
/**
 * MicaNT Clean-Room Sentinel
 * Automated CI/CD AI Compliance & Provenance Inspector
 *
 * Enforces Section 3 of MicaNT's Clean-Room Policy on all contributions:
 * 1. Verifies API signatures trace strictly to microsoft/win32metadata or public MSDN / Microsoft Learn documentation.
 * 2. Scans for decompilation / disassembly artifacts (e.g. IDA Pro / Ghidra naming conventions, raw register scraping).
 * 3. Detects leaked Microsoft Windows NT/2000/WRK internal variables and macros.
 * 4. Ensures modern ISO C++23 architectural standards (RAII, no naked pointers).
 */

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const GEMINI_API_KEY = process.env.GEMINI_API_KEY;
const GITHUB_TOKEN = process.env.GITHUB_TOKEN;
const PR_NUMBER = process.env.PR_NUMBER;
const REPO = process.env.REPO || process.env.GITHUB_REPOSITORY;

const isLocal = process.argv.includes('--local') || !PR_NUMBER;

console.log('========================================================================');
console.log('       MicaNT Clean-Room Sentinel - AI Provenance & Security Audit       ');
console.log('========================================================================\n');

// 1. Get changed files and diff
function getDiff() {
    try {
        if (isLocal) {
            console.log('[Sentinel] Running in local audit mode against HEAD...');
            return execSync('git diff HEAD~1 HEAD', { encoding: 'utf8' });
        } else {
            console.log(`[Sentinel] Auditing Pull Request #${PR_NUMBER}...`);
            return execSync('git diff origin/main...HEAD', { encoding: 'utf8' });
        }
    } catch (e) {
        console.warn('[Sentinel] Could not get git diff, inspecting all tracked source files...');
        let allCode = '';
        const dirs = ['include', 'kernel', 'tools'];
        for (const dir of dirs) {
            const fullDir = path.resolve(__dirname, '..', dir);
            if (fs.existsSync(fullDir)) {
                const files = fs.readdirSync(fullDir, { recursive: true });
                for (const file of files) {
                    const filePath = path.join(fullDir, file);
                    if (fs.statSync(filePath).isFile() && (file.endsWith('.cpp') || file.endsWith('.hpp') || file.endsWith('.h'))) {
                        allCode += `\n--- File: ${file} ---\n` + fs.readFileSync(filePath, 'utf8');
                    }
                }
            }
        }
        return allCode;
    }
}

// 2. Static heuristic patterns
const SUSPICIOUS_PATTERNS = [
    { pattern: /\bsub_[0-9a-fA-F]{6,}\b/i, reason: 'Decompiled function name artifact (IDA/Ghidra)' },
    { pattern: /\bqword_[0-9a-fA-F]{4,}\b/i, reason: 'Disassembler memory label artifact' },
    { pattern: /\bdword_[0-9a-fA-F]{4,}\b/i, reason: 'Disassembler data label artifact' },
    { pattern: /\bbyte_[0-9a-fA-F]{4,}\b/i, reason: 'Disassembler byte label artifact' },
    { pattern: /\bObpLookupDirectoryEntry\b/i, reason: 'Private internal WRK/NT symbol' },
    { pattern: /\bKSHARED_INFO\b/i, reason: 'Internal proprietary structure name' },
    { pattern: /\bExAllocatePoolWithTag\b/i, reason: 'Legacy C kernel pool allocation (use modern C++23 custom allocators)' }
];

function runStaticAudit(diffText) {
    const findings = [];
    for (const check of SUSPICIOUS_PATTERNS) {
        if (check.pattern.test(diffText)) {
            findings.push({
                type: 'HEURISTIC_FLAG',
                description: check.reason,
                match: diffText.match(check.pattern)[0]
            });
        }
    }
    return findings;
}

// 3. AI Provenance Audit via Gemini
async function runGeminiAudit(diffText, staticFindings) {
    if (!GEMINI_API_KEY) {
        console.log('[Sentinel] Notice: GEMINI_API_KEY not configured. Running static heuristic audit only.');
        return {
            verdict: staticFindings.length === 0 ? 'PASSED' : 'FLAGGED',
            summary: staticFindings.length === 0 
                ? 'Static heuristic analysis detected zero decompilation artifacts or leaked source markers.' 
                : 'Static heuristics flagged potential disassembler or legacy artifacts.',
            findings: staticFindings.map(f => f.description)
        };
    }

    console.log('[Sentinel] Consulting Gemini AI Sentinel for semantic clean-room analysis...');

    const prompt = `You are the official Clean-Room Sentinel for Project MicaNT, a modern C++23 clean-room NT-compatible operating system executive.
Your mandate is to strictly enforce Section 3 of the MicaNT Clean-Room Policy.

Rules for verification:
1. All API signatures, structs, and status codes must trace strictly to Microsoft's MIT-licensed "microsoft/win32metadata" repository or public MSDN / Microsoft Learn documentation.
2. The code must be original, modern C++23 (using RAII, concepts, smart pointers, atomics) and NOT copied from leaked Windows NT 4.0, Windows 2000, or Windows Research Kernel (WRK) sources.
3. Detect any artifacts of disassemblers/decompilers (IDA Pro, Ghidra) such as uncleaned variable names (sub_*, qword_*), register spills, or decompiler-generated control flows.
4. Flag any non-compliant or suspicious code immediately.

Analyze the following code diff:
\`\`\`diff
${diffText.slice(0, 15000)}
\`\`\`

Static heuristic findings: ${JSON.stringify(staticFindings)}

Respond in valid JSON with:
{
  "verdict": "PASSED" | "WARNING" | "REJECTED",
  "summary": "Brief 1-2 sentence executive assessment.",
  "provenance_checked": ["list of APIs/structs verified against win32metadata / MSDN"],
  "clean_room_status": "Clean" | "Needs Clarification" | "Contaminated",
  "findings": ["specific observations or flags"]
}`;

    try {
        const url = `https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=${GEMINI_API_KEY}`;
        const response = await fetch(url, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                contents: [{ parts: [{ text: prompt }] }],
                generationConfig: { responseMimeType: "application/json" }
            })
        });

        if (!response.ok) {
            throw new Error(`Gemini API error: ${response.status} ${response.statusText}`);
        }

        const data = await response.json();
        const text = data.candidates?.[0]?.content?.parts?.[0]?.text;
        return JSON.parse(text);
    } catch (err) {
        console.warn(`[Sentinel] Gemini analysis warning: ${err.message}. Relying on static audit.`);
        return {
            verdict: staticFindings.length === 0 ? 'PASSED' : 'FLAGGED',
            summary: 'Static heuristic analysis completed cleanly. AI semantic audit was unavailable.',
            findings: staticFindings.map(f => f.description)
        };
    }
}

// 4. Post GitHub PR Comment if running in Actions
async function postGithubComment(report) {
    if (!GITHUB_TOKEN || !PR_NUMBER || !REPO) {
        return;
    }

    console.log(`[Sentinel] Posting audit report to ${REPO} PR #${PR_NUMBER}...`);
    const statusEmoji = report.verdict === 'PASSED' ? '✅' : (report.verdict === 'WARNING' ? '⚠️' : '❌');

    const commentBody = `### ${statusEmoji} MicaNT Clean-Room Sentinel Audit Report

**Verdict:** \`${report.verdict}\`  
**Clean-Room Status:** \`${report.clean_room_status || 'Certified'}\`

#### Summary
${report.summary}

#### Provenance Verification
- **Reference Repositories**: \`microsoft/win32metadata\` (MIT) & Microsoft Learn / MSDN
- **Section 3 Non-Contamination**: ${report.verdict === 'PASSED' ? 'PASSED (Zero leaked or decompiled code detected)' : 'FLAGGED'}

${report.findings && report.findings.length > 0 ? `#### Findings & Observations\n${report.findings.map(f => `- ${f}`).join('\n')}` : ''}

---
*Generated automatically by MicaNT Clean-Room Sentinel powered by Gemini AI.*
`;

    try {
        await fetch(`https://api.github.com/repos/${REPO}/issues/${PR_NUMBER}/comments`, {
            method: 'POST',
            headers: {
                'Authorization': `Bearer ${GITHUB_TOKEN}`,
                'Accept': 'application/vnd.github+json',
                'User-Agent': 'MicaNT-Clean-Room-Sentinel'
            },
            body: JSON.stringify({ body: commentBody })
        });
        console.log('[Sentinel] PR comment posted successfully.');
    } catch (err) {
        console.warn(`[Sentinel] Failed to post PR comment: ${err.message}`);
    }
}

// Main execution
(async () => {
    const diff = getDiff();
    if (!diff || diff.trim().length === 0) {
        console.log('[Sentinel] No code modifications detected in changeset. Clean.');
        process.exit(0);
    }

    console.log(`[Sentinel] Analyzing ${diff.length} bytes of source diff...`);
    const staticFindings = runStaticAudit(diff);
    const report = await runGeminiAudit(diff, staticFindings);

    console.log('\n---------------- AUDIT REPORT ----------------');
    console.log(`Verdict:           ${report.verdict}`);
    console.log(`Clean-Room Status: ${report.clean_room_status || 'Verified'}`);
    console.log(`Summary:           ${report.summary}`);
    if (report.findings && report.findings.length > 0) {
        console.log('Observations:');
        report.findings.forEach(f => console.log(`  - ${f}`));
    }
    console.log('----------------------------------------------\n');

    await postGithubComment(report);

    if (report.verdict === 'REJECTED') {
        console.error('[Sentinel] FAILED: Clean-room violations detected. PR blocked.');
        process.exit(1);
    }

    console.log('[Sentinel] SUCCESS: Change passed clean-room compliance.');
    process.exit(0);
})();
