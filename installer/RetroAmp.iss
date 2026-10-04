; RetroAmp installer (Inno Setup 6)
; Build: build.bat, then  "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\RetroAmp.iss
; Output: installer\Output\RetroAmp_Setup_<version>.exe

#define MyAppName "RetroAmp"
#define MyAppPublisher "fatman73"
#define MyAppURL "https://github.com/fatman73/RetroAmp"
#define MyAppExeName "RetroAmp.exe"
#define BuildDir "..\build\Release"
; version comes from the built exe (src/version.h -> VERSIONINFO), so it never gets out of sync
#define MyAppVersion GetStringFileInfo(AddBackslash(SourcePath) + BuildDir + "\" + MyAppExeName, "ProductVersion")

[Setup]
AppId={{6C2E5B1A-8F3D-4B7E-9A41-2D5C7E9F0B13}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
VersionInfoVersion={#MyAppVersion}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir=Output
OutputBaseFilename=RetroAmp_Setup_{#MyAppVersion}
SetupIconFile=..\res\retroamp.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName} {#MyAppVersion}
Compression=lzma2/ultra
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
; Install for all users (admin) or only for me (no admin) - the user chooses
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
; Refuse to continue while the player is running (same mutex as src/main.cpp)
AppMutex=RetroAmp.SingleInstance.Mutex
CloseApplications=yes
ChangesAssociations=yes

[Languages]
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"; LicenseFile: "..\LICENSE.pl.md"
Name: "english"; MessagesFile: "compiler:Default.isl"; LicenseFile: "..\LICENSE"

[CustomMessages]
polish.AssocAudio=Dodaj RetroAmp do „Otwórz za pomocą” dla plików audio i playlist
english.AssocAudio=Add RetroAmp to "Open with" for audio files and playlists
polish.AssocSkins=Otwieraj skórki Winampa (.wsz) w RetroAmp
english.AssocSkins=Open Winamp skins (.wsz) with RetroAmp
polish.Associations=Skojarzenia plików:
english.Associations=File associations:
polish.AudioFileDesc=Plik audio (RetroAmp)
english.AudioFileDesc=Audio file (RetroAmp)
polish.SkinFileDesc=Skórka Winampa (RetroAmp)
english.SkinFileDesc=Winamp skin (RetroAmp)
polish.PlayFolder=Odtwórz w RetroAmp
english.PlayFolder=Play in RetroAmp

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "assocaudio"; Description: "{cm:AssocAudio}"; GroupDescription: "{cm:Associations}"
Name: "assocskins"; Description: "{cm:AssocSkins}"; GroupDescription: "{cm:Associations}"

[Files]
Source: "{#BuildDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\Skins\*.wsz"; DestDir: "{app}\Skins"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\LICENSE.pl.md"; DestDir: "{app}"; DestName: "LICENSE.pl.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; --- ProgIDs ---------------------------------------------------------------
Root: HKA; Subkey: "Software\Classes\RetroAmp.AudioFile"; ValueType: string; ValueName: ""; ValueData: "{cm:AudioFileDesc}"; Flags: uninsdeletekey; Tasks: assocaudio
Root: HKA; Subkey: "Software\Classes\RetroAmp.AudioFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"",0"; Tasks: assocaudio
Root: HKA; Subkey: "Software\Classes\RetroAmp.AudioFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: assocaudio
Root: HKA; Subkey: "Software\Classes\RetroAmp.Skin"; ValueType: string; ValueName: ""; ValueData: "{cm:SkinFileDesc}"; Flags: uninsdeletekey; Tasks: assocskins
Root: HKA; Subkey: "Software\Classes\RetroAmp.Skin\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"",0"; Tasks: assocskins
Root: HKA; Subkey: "Software\Classes\RetroAmp.Skin\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: assocskins
; .wsz has no other owner - make RetroAmp the default
Root: HKA; Subkey: "Software\Classes\.wsz"; ValueType: string; ValueName: ""; ValueData: "RetroAmp.Skin"; Flags: uninsdeletevalue; Tasks: assocskins
; --- "Play in RetroAmp" on folders ---------------------------------------
Root: HKA; Subkey: "Software\Classes\Directory\shell\RetroAmp.Play"; ValueType: string; ValueName: ""; ValueData: "{cm:PlayFolder}"; Flags: uninsdeletekey; Tasks: assocaudio
Root: HKA; Subkey: "Software\Classes\Directory\shell\RetroAmp.Play"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\{#MyAppExeName}"",0"; Tasks: assocaudio
Root: HKA; Subkey: "Software\Classes\Directory\shell\RetroAmp.Play\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: assocaudio
; --- Applications\RetroAmp.exe (used by "Open with") ----------------------
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "{#MyAppName}"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""
; --- Default Programs registration (Settings -> Apps -> Default apps) -----
Root: HKA; Subkey: "Software\RetroAmp\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "{#MyAppName}"; Flags: uninsdeletekey; Tasks: assocaudio
Root: HKA; Subkey: "Software\RetroAmp\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "Classic skinnable audio player"; Tasks: assocaudio
Root: HKA; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "RetroAmp"; ValueData: "Software\RetroAmp\Capabilities"; Flags: uninsdeletevalue; Tasks: assocaudio
#define Ext(E) \
  "Root: HKA; Subkey: ""Software\Classes\." + E + "\OpenWithProgids""; ValueType: string; ValueName: ""RetroAmp.AudioFile""; ValueData: """"; Flags: uninsdeletevalue; Tasks: assocaudio" + NewLine + \
  "Root: HKA; Subkey: ""Software\RetroAmp\Capabilities\FileAssociations""; ValueType: string; ValueName: ""." + E + """; ValueData: ""RetroAmp.AudioFile""; Tasks: assocaudio" + NewLine
#emit Ext("mp3")
#emit Ext("mp2")
#emit Ext("wav")
#emit Ext("flac")
#emit Ext("ogg")
#emit Ext("oga")
#emit Ext("opus")
#emit Ext("m4a")
#emit Ext("aac")
#emit Ext("wma")
#emit Ext("ape")
#emit Ext("wv")
#emit Ext("aif")
#emit Ext("aiff")
#emit Ext("mka")
#emit Ext("ac3")
#emit Ext("dsf")
#emit Ext("mod")
#emit Ext("xm")
#emit Ext("s3m")
#emit Ext("it")
#emit Ext("m3u")
#emit Ext("m3u8")
#emit Ext("pls")

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
