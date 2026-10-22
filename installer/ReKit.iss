; Windows installer for ReKit (Inno Setup 6). Built by GitHub Actions: iscc installer\ReKit.iss
#define AppName "ReKit"
#define AppVersion "1.12.2"
#define Publisher "Pouria Motabean"

[Setup]
; same id as the "Drum Slicer" installer: this one replaces it (and its entry in Apps & features)
AppId={{6B1E2C47-8D3A-4F5E-9C21-D5A1C0F7B9E3}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#Publisher}
DefaultDirName={commoncf64}\VST3\{#AppName}.vst3
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=..\setup
OutputBaseFilename=ReKit-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayName={#AppName} {#AppVersion} (VST3)
UninstallDisplayIcon={sys}\shell32.dll,168
WizardStyle=modern

[Files]
; the whole .vst3 bundle goes to the standard VST3 folder: C:\Program Files\Common Files\VST3
Source: "..\dist\ReKit.vst3\*"; DestDir: "{commoncf64}\VST3\ReKit.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion

[InstallDelete]
; an older version is replaced cleanly - also the one still called "Drum Slicer" (same plugin, old name)
Type: filesandordirs; Name: "{commoncf64}\VST3\ReKit.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Drum Slicer.vst3"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\ReKit.vst3"

[Messages]
FinishedLabel=ReKit is installed in C:\Program Files\Common Files\VST3.%n%nIn Cubase: Studio > VST Plug-in Manager > Rescan. Your kits and analyses are in Documents\ReKit.
