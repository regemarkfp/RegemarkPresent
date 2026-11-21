; Inno Setup script for the Windows installer.
; Built by CI after windeployqt has staged everything into ..\deploy:
;   iscc /DAppVersion=0.1.0 installer\RegemarkPresent.iss

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

[Setup]
AppId={{6F1C2E4A-8B7D-4C55-9E2A-3D1F0B7A9C10}
AppName=Regemark Present
AppVersion={#AppVersion}
AppPublisher=Regemark
DefaultDirName={autopf}\Regemark Present
DefaultGroupName=Regemark Present
UninstallDisplayIcon={app}\RegemarkPresent.exe
OutputDir=..\dist
OutputBaseFilename=RegemarkPresent-{#AppVersion}-Setup
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
SetupIconFile=..\resources\app.ico
PrivilegesRequired=admin

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\deploy\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Regemark Present"; Filename: "{app}\RegemarkPresent.exe"
Name: "{group}\Uninstall Regemark Present"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Regemark Present"; Filename: "{app}\RegemarkPresent.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing Visual C++ runtime..."; Check: VCRedistNeeded; Flags: waituntilterminated skipifdoesntexist
Filename: "{app}\RegemarkPresent.exe"; Description: "{cm:LaunchProgram,Regemark Present}"; Flags: nowait postinstall skipifsilent

[Code]
function VCRedistNeeded(): Boolean;
var
  Installed: Cardinal;
begin
  Result := not (RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) and (Installed = 1));
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpFinished then
  begin
    if GetEnv('NDI_RUNTIME_DIR_V6') + GetEnv('NDI_RUNTIME_DIR_V5') = '' then
      WizardForm.FinishedLabel.Caption := WizardForm.FinishedLabel.Caption + #13#10#13#10 +
        'For NDI output, install the free NDI Runtime or NDI Tools from ndi.video.' + #13#10 +
        'For SDI output, install Blackmagic Desktop Video from blackmagicdesign.com.';
  end;
end;
