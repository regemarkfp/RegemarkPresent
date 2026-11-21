; Inno Setup script for the Windows installer.
; Built by CI after windeployqt has staged everything into ..\deploy:
;   iscc /DAppVersion=0.1.0 installer\ChurchPresenter.iss

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

[Setup]
AppId={{6F1C2E4A-8B7D-4C55-9E2A-3D1F0B7A9C10}
AppName=ChurchPresenter
AppVersion={#AppVersion}
AppPublisher=ChurchPresenter
DefaultDirName={autopf}\ChurchPresenter
DefaultGroupName=ChurchPresenter
UninstallDisplayIcon={app}\ChurchPresenter.exe
OutputDir=..\dist
OutputBaseFilename=ChurchPresenter-{#AppVersion}-Setup
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
PrivilegesRequired=admin

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\deploy\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\ChurchPresenter"; Filename: "{app}\ChurchPresenter.exe"
Name: "{group}\Uninstall ChurchPresenter"; Filename: "{uninstallexe}"
Name: "{autodesktop}\ChurchPresenter"; Filename: "{app}\ChurchPresenter.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing Visual C++ runtime..."; Check: VCRedistNeeded; Flags: waituntilterminated skipifdoesntexist
Filename: "{app}\ChurchPresenter.exe"; Description: "{cm:LaunchProgram,ChurchPresenter}"; Flags: nowait postinstall skipifsilent

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
