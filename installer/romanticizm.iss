; Inno Setup script: installs Romanticizm.vst3 into the folder every DAW scans.
#define AppName "Romanticizm"
#define AppVersion "1.0.0"

[Setup]
AppId={{B7C1E5D2-3A4F-4E7B-9A21-5F6D8C0E1A77}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppName}
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=Romanticizm-Setup
SetupIconFile=romanticizm.ico
UninstallDisplayIcon={uninstallexe}
UninstallDisplayName={#AppName} (VST3)
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\build\Romanticizm_artefacts\Release\VST3\Romanticizm.vst3\*"; DestDir: "{app}\Romanticizm.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{app}\Romanticizm.vst3"
