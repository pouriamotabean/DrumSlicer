; Windows installer for ReKit 2 (Inno Setup 6). Built by GitHub Actions: iscc installer\ReKit2.iss
; ReKit 2 is its own plugin: it installs NEXT TO ReKit 1.x (ReKit.vst3 stays, so both can be compared)
#define AppName "ReKit 2"
#define AppVersion "2.1.0"
#define Publisher "Pouria Motabean"

[Setup]
AppId={{3F9A6D21-7C4E-4B8A-A1D5-2E6F0B9C4D72}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#Publisher}
DefaultDirName={commoncf64}\VST3\{#AppName}.vst3
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=..\setup
OutputBaseFilename=ReKit2-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayName={#AppName} {#AppVersion} (VST3)
UninstallDisplayIcon={sys}\shell32.dll,168
WizardStyle=modern

[Files]
Source: "..\dist\ReKit 2.vst3\*"; DestDir: "{commoncf64}\VST3\ReKit 2.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion

[InstallDelete]
; only an older ReKit 2 is replaced - ReKit.vst3 (1.x) is left alone
Type: filesandordirs; Name: "{commoncf64}\VST3\ReKit 2.vst3"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\ReKit 2.vst3"

[Messages]
FinishedLabel=ReKit 2 is installed next to ReKit in C:\Program Files\Common Files\VST3.%n%nIn Cubase: Studio > VST Plug-in Manager > Rescan. Both use the same kits and sample folders (Documents\ReKit).
