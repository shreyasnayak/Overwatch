[Setup]
AppName=Overwatch
AppVersion=0.0.0.1
AppPublisher=Interlink Network Systems, Inc.
WizardStyle=modern
DefaultDirName={pf32}\Syntax Masons\Overwatch 
DefaultGroupName=Overwatch
UninstallDisplayIcon={app}\interlink.ico
Compression=lzma2
SolidCompression=yes
OutputDir=..\..\Package_Build
OutputBaseFilename={#SetupSetting("AppName")}_i386_{#SetupSetting("AppVersion")}
DisableWelcomePage=no
PrivilegesRequired=admin

[Files]
; Your existing application files
Source: "..\Overwatch_Build\*"; DestDir: "{app}\"; Flags: ignoreversion recursesubdirs

; The reference image to show in the wizard (Make sure to save the generated image as a .bmp)
Source: "face.bmp"; Flags: dontcopy

[Icons]
Name: "{group}\Overwatch"; Filename: "{app}\Overwatch.exe" 

[UninstallRun]
Filename: "taskkill"; Parameters: "/F /IM Overwatch.exe"; WorkingDir: "{app}"; Flags: shellexec waituntilterminated

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
Type: filesandordirs; Name: "{userappdata}\Syntax Masons\Overwatch"

[Code]
var
  FaceImagePage: TInputFileWizardPage;
  FaceImageControl: TBitmapImage;

// Function to actually read the file's binary header to ensure it's a real JPG
function IsRealJpg(const FileName: string): Boolean;
var
  Stream: TFileStream;
  Buffer: AnsiString;
begin
  Result := False;
  try
    Stream := TFileStream.Create(FileName, fmOpenRead or fmShareDenyNone);
    try
      if Stream.Size >= 2 then
      begin
        SetLength(Buffer, 2);
        Stream.ReadBuffer(Buffer, 2);
        // A valid JPEG file always starts with hex values FF D8 (255 216)
        if (Ord(Buffer[1]) = 255) and (Ord(Buffer[2]) = 216) then
          Result := True;
      end;
    finally
      Stream.Free;
    end;
  except
    // If the file can't be read, treat it as invalid
    Result := False;
  end;
end;

procedure InitializeWizard;
begin
  // Create a custom page asking for the JPG image
  FaceImagePage := CreateInputFilePage(wpSelectDir,
    'Facial Recognition Setup', 
    'Please select a reference photo for face recognition.',
    'Click Browse to select your JPG file. This will be copied to your installation folder.');

  // Restrict the file browser strictly to .jpg files only
  FaceImagePage.Add('Face Image (*.jpg)|*.jpg', '.*', '.jpg');

  // Extract the reference image we added in the [Files] section
  ExtractTemporaryFile('face.bmp');

  // Display the reference image below the browse section
  FaceImageControl := TBitmapImage.Create(FaceImagePage);
  FaceImageControl.Parent := FaceImagePage.Surface;
  FaceImageControl.Bitmap.LoadFromFile(ExpandConstant('{tmp}\face.bmp'));
  
  // Position the image nicely below the input box
  FaceImageControl.Top := 60; 
  FaceImageControl.Left := 0;
  FaceImageControl.Width := FaceImagePage.SurfaceWidth; 
  FaceImageControl.Height := 170;
  FaceImageControl.Stretch := True;
  FaceImageControl.BackColor := FaceImagePage.Surface.Color;
end;

// Validate the image when the user clicks "Next"
function NextButtonClick(CurPageID: Integer): Boolean;
var
  SelectedFile: string;
begin
  Result := True;

  // Only run this check on our custom page
  if CurPageID = FaceImagePage.ID then
  begin
    SelectedFile := FaceImagePage.Values[0];
    
    // Check if empty
    if SelectedFile = '' then
    begin
      MsgBox('Please select a JPG image to continue.', mbError, MB_OK);
      Result := False;
      Exit;
    end;

    // Verify it is a valid JPG (both extension and internal header)
    if not IsRealJpg(SelectedFile) then
    begin
      MsgBox('The selected file is not a valid JPG image. Please select a real .jpg file.', mbError, MB_OK);
      Result := False;
      Exit;
    end;
  end;
end;

// Copy the validated JPG to the Root directory during installation as my_face.jpg
procedure CurStepChanged(CurStep: TSetupStep);
var
  SelectedFile, DestFile: string;
begin
  if CurStep = ssPostInstall then
  begin
    SelectedFile := FaceImagePage.Values[0];
    if SelectedFile <> '' then
    begin
      // Copy the file to the {app} root directory as "my_face.jpg"
      DestFile := ExpandConstant('{app}\my_face.jpg');
      FileCopy(SelectedFile, DestFile, False);
    end;
  end;
end;