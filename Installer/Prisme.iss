; Installeur / mise a jour Prisme v3.6 - Inno Setup 6
; VST3 (obligatoire) + Standalone (optionnel) + manuel.
; Reconstruit a partir de la copie de dev C:\PrismeMix.

#define MyAppName "Prisme"
#define MyAppVersion "3.6"
#define MyAppPublisher "Atelier Prisme"
#define SrcVST3 "C:\PrismeMix\build\Prisme_artefacts\Release\VST3\Prisme.vst3"
#define SrcStandalone "C:\PrismeMix\build\Prisme_artefacts\Release\Standalone\Prisme.exe"
#define SrcReadme "C:\PrismeMix\README.md"

[Setup]
AppId={{A3F1B2C4-8D2E-4A5B-9C1D-PRISME32026}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppPublisher}\{#MyAppName}
DefaultGroupName={#MyAppPublisher}\{#MyAppName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
OutputDir=C:\Users\Utilisateur\Desktop\BrI update
OutputBaseFilename=PRISME_v3.6_MiseAJour
UninstallDisplayName={#MyAppName} {#MyAppVersion}

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Types]
Name: "full"; Description: "Complete (VST3 + Standalone)"
Name: "vst3only"; Description: "VST3 uniquement"

[Components]
Name: "vst3"; Description: "Plugin VST3 (pour votre DAW)"; Types: full vst3only; Flags: fixed
Name: "standalone"; Description: "Version autonome (test sans DAW)"; Types: full

[Files]
Source: "{#SrcVST3}\*"; DestDir: "{commoncf}\VST3\{#MyAppName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#SrcStandalone}"; DestDir: "{app}"; DestName: "{#MyAppName}.exe"; Flags: ignoreversion; Components: standalone
Source: "{#SrcReadme}"; DestDir: "{app}"; DestName: "Lisez-moi.txt"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{group}\{#MyAppName} (autonome)"; Filename: "{app}\{#MyAppName}.exe"; Components: standalone
Name: "{group}\Lisez-moi"; Filename: "{app}\Lisez-moi.txt"; Components: standalone
Name: "{group}\Desinstaller {#MyAppName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\{#MyAppName}.exe"; Description: "Lancer {#MyAppName} maintenant"; Flags: nowait postinstall skipifsilent unchecked; Components: standalone
