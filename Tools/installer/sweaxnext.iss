; SWEAX NEXT — Windows kurulum betiği (Inno Setup 6)
; Derleme: ISCC /DVer=1.0.0 /DSrc=<SWEAX NEXT.vst3 klasörünün bulunduğu dizin> /O<çıkış> Tools\installer\sweaxnext.iss
; Varsayılan kurulum yönetici istemez: VST3 kullanıcı klasörüne (%LOCALAPPDATA%\Programs\Common\VST3) gider.
; "Tüm kullanıcılar" seçilirse C:\Program Files\Common Files\VST3 kullanılır (yönetici ister).

#ifndef Ver
  #define Ver "1.0.0"
#endif
#ifndef Src
  #define Src "..\..\build\SweaxNext_artefacts\Release\VST3"
#endif

[Setup]
AppId={{6E0B3D5A-58C1-4C1F-9C2E-5357A3E0B2A7}
AppName=SweaxNext
AppVersion={#Ver}
AppVerName=SweaxNext {#Ver}
AppPublisher=Sweax Studios
AppPublisherURL=https://github.com/Sweax3169/sweaxnext
AppSupportURL=https://github.com/Sweax3169/sweaxnext/issues
AppUpdatesURL=https://github.com/Sweax3169/sweaxnext/releases
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DefaultDirName={autocf}\VST3
DirExistsWarning=no
DisableProgramGroupPage=yes
UninstallFilesDir={autopf}\SweaxNext
UninstallDisplayName=SweaxNext {#Ver} (VST3)
LicenseFile=..\..\LICENSE
OutputBaseFilename=SweaxNext-{#Ver}-Setup
Compression=lzma2/ultra
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "tr"; MessagesFile: "compiler:Languages\Turkish.isl"

[CustomMessages]
en.Ready=SWEAX NEXT.vst3 will be placed in this folder. Studio One, FL Studio, Ableton Live, Cubase and REAPER read it after a plug-in rescan.
tr.Ready=SWEAX NEXT.vst3 bu klasöre konur. Studio One, FL Studio, Ableton Live, Cubase ve REAPER eklenti taramasından sonra görür.

[Files]
Source: "{#Src}\SWEAX NEXT.vst3\*"; DestDir: "{app}\SWEAX NEXT.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\LICENSE"; DestDir: "{autopf}\SweaxNext"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\..\THIRD_PARTY_NOTICES.md"; DestDir: "{autopf}\SweaxNext"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{app}\SWEAX NEXT.vst3"

[Code]
procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpSelectDir then
    WizardForm.SelectDirLabel.Caption := CustomMessage('Ready');
end;
