# RXDK for Visual Studio 2022 / 2026 — Port Plan

Port of the [RXDK-VSCode](https://github.com/Team-Resurgent/RXDK-VSCode) original-Xbox
dev experience to Visual Studio 2022 and 2026.

## Decisions (locked)

- **Extensibility model:** Classic **VSSDK** (in-proc VSIX, .NET Framework shell).
  Chosen for mature Debug Adapter Host + Open Folder support; revisit
  VisualStudio.Extensibility (out-of-proc) later if warranted.
- **Engine:** **Pure .NET, no Node dependency.** The TypeScript build/deploy/launch
  orchestration and the DAP debug session are re-implemented in C#. The existing
  cross-platform **host tools** (`imagebld`, `xdvdfs`, `xbcp`, `xbox-launch`,
  `xboxdbg-bridge`, `xbwatson`) and `Rxdk.Pdb` are already .NET and are reused as-is.

## Why this is feasible

RXDK-VSCode is a thin UI over a UI-agnostic engine. Two layers already do the real
work and are reused unchanged:

1. **Host tools** — cross-platform .NET, downloaded at runtime into `…/RXDK/tools`.
   They implement xbdm networking, XISO packing, and PDB symbols. **The port does not
   touch these.**
2. **Toolchain + SDK** — Zig compiler + headers/libs cloned from RXDK-SDK, driven by
   `rxdk.project.json` (the stable project contract, reused verbatim).

What we re-implement in C# is only the **orchestration** (which exes to spawn, with
what args) and the **DAP translation** (bridge line-JSON ⇄ Debug Adapter Protocol).

### The debug lever

VS does not natively consume DAP like VS Code, **but it ships the
[Debug Adapter Host](https://github.com/microsoft/VSDebugAdapterHost)**, which lets a
VSIX host an external DAP adapter using the same `launch.json`/`launch.vs.json` config.
This lets us reuse the debugging *architecture* (adapter ⇄ `xboxdbg-bridge`) and avoid
writing a from-scratch AD7/Concord engine — the single largest cost we're sidestepping.
We write the adapter in **C#** (Microsoft ships `Microsoft.VisualStudio.Shared.
VSCodeDebugProtocol` for exactly this) instead of Node.

### The project-system lever

VS **Open Folder** mode reads `tasks.vs.json` and `launch.vs.json` — direct analogs of
VS Code's `tasks.json`/`launch.json`. So `rxdk.project.json` stays the source of truth
and we generate `tasks.vs.json`/`launch.vs.json`, rather than authoring a real
`.vcxproj`/CPS project system.

## Target repo layout (proposed)

```
RXDK-VS20XX/
  Rxdk.Engine/        net8 class lib — build/deploy/launch orchestration + bootstrap
  Rxdk.Cli/           net8 console — thin CLI over Rxdk.Engine (headless/CI/tasks.vs.json)
  Rxdk.Dap/           net8 console — DAP adapter over xboxdbg-bridge (VS Debug Adapter Host)
  RxdkVs.Package/     VSIX (.NET Framework 4.7.2) — commands, tool window, wizard, settings,
                      tasks.vs.json/launch.vs.json generator, prereq/tool/SDK bootstrap UI
  templates/          copied/derived from RXDK-VSCode/templates (rxdk.project.json unchanged)
  RXDK-VS20XX.sln
```

Runtime boundary: the VSIX is in-proc **.NET Framework** (the VS shell); the engine, CLI,
DAP adapter, and host tools are all out-of-proc **net8** processes it spawns. Clean split;
two runtimes coexist in the repo.

## Component mapping (VSCode → VS)

| RXDK-VSCode source | VS port target | Notes |
|---|---|---|
| `xboxBuild.ts`, `buildRunner.ts`, `optimizeMode.ts` | `Rxdk.Engine` build | Zig invocation + args |
| `xboxDeploy.ts`, `imageBuild.ts`, `packXiso.ts` | `Rxdk.Engine` deploy | orchestrate imagebld/xdvdfs/xbcp |
| `xboxLaunch.ts` | `Rxdk.Engine` launch | xbox-launch, reboot, DXT |
| `projectTypes.ts`, `sdkPath.ts`, `xboxSdkPaths.ts` | `Rxdk.Engine` model | parse `rxdk.project.json` |
| `hostTools.ts`, `sdkStaging.ts`, `zigRuntime.ts`, `dotnetRuntime.ts`, `prerequisites*.ts` | `Rxdk.Engine` bootstrap | same download URLs/layout |
| `cli.ts` | `Rxdk.Cli` | subcommands: build/deploy/run/reboot |
| `debug/debugSession.ts` (1361 LOC), `bridgeClient.ts` | `Rxdk.Dap` | **largest single port** |
| `vscodeGenerator.ts` | `tasks.vs.json`/`launch.vs.json` generator | in VSIX |
| `sidebarProvider.ts`, `settingsPanel.ts`, `newProjectWizard.ts` | VS tool window + wizard + options | new UI |
| `extension.ts` command wiring | VS `AsyncPackage` + command handlers | |
| `xbwatsonLauncher.ts`, `xbNeighborhoodLauncher.ts` | VS commands | spawn host tools |

## Phases

### Phase 0 — De-risk spike (½–1 day) ← do this first
Prove the two levers manually before building anything:
- Open an existing RXDK project folder in VS via **Open Folder**.
- Hand-write `tasks.vs.json` that calls the host tools (or a stub CLI) for build/deploy/run.
- Stand up a minimal **C# DAP adapter** that forwards a couple of requests to a running
  `xboxdbg-bridge`, register it with the **Debug Adapter Host** in a throwaway VSIX, and a
  `launch.vs.json` pointing at it.
- **Success = F5 stops at entry and shows locals.** If yes, the rest is engineering, not
  research. Validate on both VS 2022 and a VS 2026 preview.

### Phase 1 — Engine + CLI (build / deploy / run)
- `Rxdk.Engine`: port project-model parsing, Zig build, imagebld/xdvdfs/xbcp deploy,
  xbox-launch run, and the bootstrap (Zig/.NET/SDK/host-tools download).
- `Rxdk.Cli`: subcommands mirroring `cli.ts`.
- Verify on hardware against a real devkit, parity with the VSCode build output.

### Phase 2 — Debug
- `Rxdk.Dap`: port `debugSession.ts` + `bridgeClient.ts` to C# (breakpoints, stepping,
  scopes/locals, expression eval, the Globals-scope cycling, error hints).
- Register with Debug Adapter Host; `launch.vs.json` generator; prebuilt-XBE (.pdb) flow.
- HW-verify F5 parity with VSCode (stop-at-entry, locals, watch, stepping, reboot).

### Phase 3 — UX
- VS **tool window** (devkit IP, templates, Build/Deploy/Run/Debug, SDK/docs).
- **New Project** wizard (+ New Prebuilt XBE) writing `rxdk.project.json` + `.vs` config.
- **Options** page for settings (`rxdk.defaultConsole`, build type, globals scope).
- Command set mirroring the VSCode command palette (see `package.json` `contributes.commands`).

### Phase 4 — Packaging / CI
- VSIX manifest, VS 2022 + 2026 version ranges, Marketplace metadata.
- GitHub Actions VSIX build (parallels RXDK-VSCode's `build-vsix.yml`).
- Signing; `latest`-tag release flow.

## Key risks / open items

- **Debug Adapter Host fidelity.** Custom DAP requests (e.g. Globals-scope cycling) and
  variable formatting may render differently than in VS Code; may need VS-side command
  shims. *Retire in Phase 0.*
- **Engine drift.** The VSCode TS engine keeps evolving; a C# reimplementation can diverge.
  Mitigate by treating `rxdk.project.json` + host-tool CLIs as the stable contract and
  porting behavior, not code. Consider a shared conformance test set.
- **Template variables.** Templates/tasks use VS Code `${...}` variables; map to the
  `tasks.vs.json` equivalents (`${env.VAR}`, `${workspaceRoot}`, `${file}`).
- **Two runtimes.** VSIX (.NET Framework) + engine (net8) — spawn boundary must inject
  `DOTNET_ROOT` for the managed runtime (mirror `dotnetEnv.ts`).
- **VS 2026 preview churn.** VSSDK APIs are stable, but validate the Debug Adapter Host and
  Open Folder surfaces on each preview.

## Effort shape

Pure-.NET (vs. reuse-Node) roughly doubles Phases 1–2 — we port ~3k LOC of TS to C# —
but delivers a Node-free, fully .NET-native product cohesive with the existing host tools
and `Rxdk.Pdb`. Phase 0 is the gate; everything downstream is packaging + porting.

---
*Sources: [VS Debug Adapter Host](https://github.com/microsoft/VSDebugAdapterHost),
[Packaging a VS Code Debug Adapter for VS](https://github.com/Microsoft/VSDebugAdapterHost/wiki/Packaging-a-VS-Code-Debug-Adapter-For-Use-in-VS).*
