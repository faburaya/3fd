# Script written by Felipe Vieira Aburaya (last update: January 2020)
#
# Usage:
#
# .\build.ps1 -arch {x86|x64|ARM|all} -rebuild {yes|no}
#
# Builds all the projects in VC++ solution 3FD.sln that are compilable for the given
# architecture(s). Builds are by default incremental, but can also be complete rebuilds.
# Projects of UWP apps produce packages that are easy to deploy with a click. All of
# them go to the generated build directory in the solution root directory, along with
# test applications.
#
# In order to build the projects, MSBuild is employed. The script looks for MSBuild.exe
# (32 bits) executable first in "Microsoft Visual Studio" installation directory, then
# in %PROGRAMFILES(x86)%\MSBuild, giving priority to the latest release.


# =============================================
#           PARAMETERS & ENVIRONMENT
# =============================================

param (
    [string]$arch = $(Read-Host 'Architecture (x86, x64, ARM, all)').ToLower(),
    [string]$rebuild = $(Read-Host 'Rebuild? (no/yes)').ToLower(),
    [string]$winSdkVer = $(Read-Host 'Windows SDK version? (ex.: 10.0.17763.0)')
)

[boolean]$BUILD_X64 = ($arch -eq 'x64') -or ($arch -eq 'all');
[boolean]$BUILD_X86 = ($arch -eq 'x86') -or ($arch -eq 'all');
[boolean]$BUILD_ARM = ($arch -eq 'arm') -or ($arch -eq 'all');
[string]$task;

if ($rebuild -eq 'yes')
{
    $task = 'rebuild';
}
else
{
    $task = 'build';
}


# =============================================
#                  FUNCTIONS
# =============================================

# # # # #
# appends a new line to the file
function AppendFile ([string]$file, [string] $line)
{
    Out-File -FilePath $file `
             -InputObject $line `
             -Encoding ASCII `
             -Append;
}

# # # # #
# displays a message in the console but also in the log
function WriteConsole ([string] $message)
{
    Write-Host '';
    Write-Host $message -ForegroundColor yellow;
    Write-Host '';

    $now = Get-Date -UFormat "%d-%b-%Y %H:%M:%S";
    
    Out-File -FilePath '.\build.log' `
             -InputObject ($now + ' >> ' + $message) `
             -Encoding ASCII `
             -Append;
}

# # # # #
# displays an error message only
function DisplayError ([string] $message)
{
    Write-Host '';
    Write-Host ('ERROR! ' + $message) -ForegroundColor magenta;
    Write-Host '';
    
    $now = Get-Date -UFormat "%d-%b-%Y %H:%M:%S";
    
    Out-File -FilePath '.\build.log' `
             -InputObject ($now + ' >> ' + $message) `
             -Encoding ASCII `
             -Append;
}

# # # # #
# displays an error messages and terminates script execution
function RaiseError ([string] $message)
{
    DisplayError $message;
    exit;
}


# # # # #
# load the required .NET assemblies
function LoadDotNetAssemblies ()
{
    try
    {
        Add-Type -Path (
            [Environment]::GetEnvironmentVariable("WINDIR") +
            '\Microsoft.NET\Framework64\v4.0.30319\System.IO.Compression.FileSystem.dll'
        ) 2> $null;
    }
    catch
    {
        ###
        # Fix Powershell .NET Framework version:
        
        [string]$resetPowershell = $(Read-Host (
            'Could not load a required assembly because your Powershell installation ' +
            'is configured to use an old version of the .NET Framework. ' +
            'Do you wish to have it reconfigured? (no/yes)'
        ));
        
        if ($resetPowershell -ne 'yes')
        {
            RaiseError 'Script execution cannot succeed!';
        }
        
        if ($(Test-Path "$PSHOME\powershell.exe.config"))
        {
            Move-Item -Force "$PSHOME\powershell.exe.config" "$PSHOME\powershell.exe.config.old";
        }
        
        if ($(Test-Path "$PSHOME\powershell_ise.exe.config"))
        {
            Move-Item -Force "$PSHOME\powershell_ise.exe.config" "$PSHOME\powershell_ise.exe.config.old";
        }
    
        $configText = @"
<?xml version="1.0"?> 
<configuration> 
    <startup useLegacyV2RuntimeActivationPolicy="true"> 
        <supportedRuntime version="v4.0.30319"/> 
        <supportedRuntime version="v2.0.50727"/> 
    </startup> 
</configuration>
"@;
        $configText | Out-File "$PSHOME\powershell.exe.config"
        $configText | Out-File "$PSHOME\powershell_ise.exe.config"
        
        WriteConsole (
            'Your Powershell installation was reconfigured to use a newer version ' +
            'of .NET Framework. Any previous configuration has been saved. ' +
            'Please restart this prompt and try executing this script again.'
        );
        
        exit;
    }
}

# # # # #
# find most recent visual studio 2017+ installation directory
function FindVisualStudioInstallation ()
{
    if (-not $global:vsdir)
    {
        $global:vsdir = [Environment]::GetEnvironmentVariable("PROGRAMFILES(X86)") + '\Microsoft Visual Studio';
        
        if ($(Test-Path $global:vsdir))
        {
            $global:vsdir = @(Get-ChildItem $global:vsdir `
                | Where-Object { $_.Name.StartsWith('20') } `
                | Sort-Object @{ Expression="Name"; Descending=$true })[0];
        }
        else
        {
            RaiseError ('Visual Studio installation not found in ' + $global:vsdir);
        }
    }

    if (-not $global:VCTarget)
    {
        $global:VCTarget = @(Get-ChildItem $global:vsdir.FullName -Recurse `
            | Where-Object { $_.Name -eq 'VCTargets' }
        )[0];
    }

    if (-not $global:VCTarget)
    {
        RaiseError ('Visual C++ targets not found in ' + $global:vsdir.FullName);
    }

    if (-not $global:vcvarsall)
    {
        $global:vcvarsall = @(Get-ChildItem -Recurse $global:vsdir.FullName `
            | Where-Object { ($_.Name -eq 'vcvarsall.bat') }
        )[0];
    }

    if (-not $global:vcvarsall)
    {
        RaiseError 'vcvarsall.bat not found!';
    }
}

# # # # #
# build solution for a given configuration
#
function MsBuild (
    [string]$what,    # solution or project to build
    [string]$task,    # build|rebuild
    [string]$arch,    # x86|x64|ARM
    [string]$config,  # debug|release
    [string]$sdk,     # (optional) Windows SDK version
    [string]$toolset, # (optional) ex: v141_xp
    [boolean]$legacy  # (optional) must run in legacy prompt
)
{
    [string]$global:msBuildPath;

    FindVisualStudioInstallation;
    
    ###
    # single initialization:

    if (-not $global:msBuildPath)
    {
        $msBuildInstalls = @(
            $global:vsdir,
            @(Get-ChildItem $([Environment]::GetEnvironmentVariable("PROGRAMFILES(X86)")) `
              | Where-Object { $_.Name.Contains('MSBuild') } `
              | Sort-Object @{ Expression="FullName"; Descending=$true })[0]
        );

        #$msBuildInstalls = $msBuildInstalls | Sort-Object @{ Expression="Name"; };

        for ([int]$idx = 0; $idx -lt $msBuildInstalls.Length; ++$idx)
        {
            $msBuildExecs = @(Get-ChildItem -Recurse ($msBuildInstalls[$idx].FullName) `
                | Where-Object { ($_.Name -eq 'MSBuild.exe') -and (-not $_.FullName.Contains('amd64')) } `
                | Sort-Object @{ Expression="FullName"; Descending=$true });

            if ($msBuildExecs.Length -gt 0)
            {
                break;
            }
        }

        if ($msBuildExecs.Length -eq 0)
        {
            RaiseError 'MSBuild.exe (32 bits) not found!';
        }

        $global:msBuildPath = $msBuildExecs[0].FullName;
    }

    if (-not $global:msBuildPath)
    {
        RaiseError 'MSBuild.exe (32 bits) not found!';
    }

    ###
    # assemble command:

    [string]$command = '"' + $global:msBuildPath + '" "' + $what + '" /verbosity:minimal /m';

    $task = $task.ToLower();
    
    switch ($task)
    {
        "build"   { $command += ' /t:build'; }
        "rebuild" { $command += ' /t:rebuild'; }
        default {
            RaiseError ('Task <' + $task + '> is unknown!');
        }
    }
    
    $arch = $arch.ToLower();

    switch ($arch)
    {
        "x64" { $command += ' /p:Platform=x64'; }
        "x86" { $command += ' /p:Platform=x86'; }
        "arm" { $command += ' /p:Platform=ARM'; }
        default {
            RaiseError ('Architecture <' + $arch + '> is unknown!');
        }
    }

    $command += ' /p:Configuration=' + $config;

    # Override Windows SDK version?
    if ($sdk -and ($sdk -ne ''))
    {
        $command += ' /p:WindowsTargetPlatformVersion=' + $sdk;
    }
    
    # Use non-default toolset?
    if ($toolset -and ($toolset -ne ''))
    {
        $command += ' /p:PlatformToolset=' + $toolset;
    }

    ###
    # invoke command / append to file:
    
    Write-Host '';
    Write-Host $command -ForegroundColor Cyan;
    Write-Host '';

    if ($legacy -ne $true)
    {
        Invoke-Expression -Command ". $command";

        if ($lastexitcode -ne 0)
        {
            DisplayError ('Failed to build <' + $what + '>');
        }
    }
    else
    {
        [string]$msbuildScriptFPath = 'msbuild.bat';

        if ($(Test-Path $msbuildScriptFPath))
        {
            Remove-Item $msbuildScriptFPath;
        }
        
        switch ($arch)
        {
            "x64" {
                AppendFile -file $msbuildScriptFPath -line ('call "' + $global:vcvarsall.FullName + '" x86_amd64');
            }
            "x86" {
                AppendFile -file $msbuildScriptFPath -line ('call "' + $global:vcvarsall.FullName + '" x86');
            }
            "arm" {
                AppendFile -file $msbuildScriptFPath -line ('call "' + $global:vcvarsall.FullName + '" x64_arm64');
            }
            default {
                RaiseError ('Architecture <' + $arch + '> is unknown!');
            }
        }

        AppendFile -file $msbuildScriptFPath -line $command;
        AppendFile -file $msbuildScriptFPath -line 'exit %errorlevel%';

        WriteConsole 'Building in legacy prompt...';

        cmd.exe /C $msbuildScriptFPath;
    }
}


###
# download file:
#
function DownloadFile (
    [string]$url,      # URL to download file from
    [string]$filePath, # path where to place downloaded file
    [string]$checksum  # checksum by SHA256 (avoids downloading again already present file)
)
{
    WriteConsole "Downloading $url ...";

    if ($(Test-Path $filePath) -and
        ($(Get-FileHash $filePath).Hash -eq $checksum))
    {
        return;
    }

    LoadDotNetAssemblies;
    
    (New-Object System.Net.WebClient).DownloadFile($url, $filePath);
    
    if (-not $(Test-Path $filePath))
    {
        RaiseError "Failed to download $filePath";
    }

    if ($(Get-FileHash $filePath).Hash -ne $checksum)
    {
        RaiseError "Downloaded file $filePath did not match checksum!";
    }
}

###
# Check BOOST_HOME:
#

[string]$boostVersion = '1.72.0';
[string]$boostZipLabel = 'boost_1_72_0';
[string]$boostHomePathExpSuffix = 'Boost\v' + $boostVersion + '\';
[string]$envVarBoostHome = [Environment]::GetEnvironmentVariable("BOOST_HOME");

if (
     (-not $envVarBoostHome) -or
     ( (-not $envVarBoostHome.ToLower().EndsWith($boostHomePathExpSuffix.ToLower())) -and
       (-not ($envVarBoostHome + '\').ToLower().EndsWith($boostHomePathExpSuffix.ToLower())) )
   )
{
    RaiseError(
        'The enviroment variable BOOST_HOME must be set! ' +
        'This build depends of Boost version ' + $boostVersion +
        ', so you should set BOOST_HOME = path_of_your_choice\' + $boostHomePathExpSuffix
    );
}


# # # # #
# Build boost libraries
#
function StartBoostBuild (
    [string]$architectureLabel,
    [string]$libDirSuffix,
    [string]$devPromptArchParam,
    [string]$boostAddrModelParam
)
{
    ###
    # generate script:

    mkdir "$boostInstallDir\include" -Force;
    mkdir ($boostInstallDir + $libDirSuffix) -Force;
    
    [string]$boostBuildBatchScriptFPath = "$boostSourceDir\build_boost_$architectureLabel.bat";

    if ($(Test-Path $boostBuildBatchScriptFPath))
    {
        Remove-Item $boostBuildBatchScriptFPath;
    }

    AppendFile -file $boostBuildBatchScriptFPath -line ('cd "' + $boostSourceDir + '\tools\build"');
    AppendFile -file $boostBuildBatchScriptFPath -line ('call bootstrap.bat');
    AppendFile -file $boostBuildBatchScriptFPath -line ('cd ..\..\');

    [UInt32]$nCpuCores = $(Get-WmiObject -Class win32_processor).NumberOfCores;

    [string]$buildCmdDebug =
        "tools\build\b2.exe -j $nCpuCores --abbreviate-paths $boostAddrModelParam variant=debug link=static threading=multi runtime-link=shared";
    
    [string]$buildCmdRelease =
        "tools\build\b2.exe -j $nCpuCores --abbreviate-paths $boostAddrModelParam variant=release link=static threading=multi runtime-link=shared";

    AppendFile -file $boostBuildBatchScriptFPath -line $buildCmdDebug;
    AppendFile -file $boostBuildBatchScriptFPath -line $buildCmdRelease;
    AppendFile -file $boostBuildBatchScriptFPath -line ('move stage\lib\* "' + $boostInstallDir + $libDirSuffix);

    ###
    # start build:

    WriteConsole "Starting boost build for $architectureLabel...";

    $cmdPID = $(Start-Process -FilePath "$boostBuildBatchScriptFPath" -PassThru).Id;

    Wait-Process -Id $cmdPID;

    if (@(Get-ChildItem ($boostInstallDir + $libDirSuffix)).Length -eq 0)
    {
        RaiseError "Boost build for $architectureLabel has failed!";
    }
}

function InstallBoostLibs ()
{
    LoadDotNetAssemblies;
    
    FindVisualStudioInstallation;
    
    ###
    # files & directories:
    
    if ($(Test-Path $envVarBoostHome))
    {
        [string]$continue = $(Read-Host 'Boost libraries appear to be already installed. Do you want to erase and rebuild that? (no/yes)');

        if ($continue.ToLower() -ne 'yes')
        {
            return;
        }

        Remove-Item -Force -Recurse ($envVarBoostHome + '\*');
    }

    mkdir $envVarBoostHome -Force;
    
    [string]$boostDir = $(Get-Item $(Join-Path -Path $envVarBoostHome -ChildPath '\..\')).FullName;
    [string]$boostZipFName = $boostZipLabel + '.zip';
    [string]$boostZipFPath = $boostDir + '\' + $boostZipFName;
    [string]$boostInstallDir = $envVarBoostHome;

    ###
    # download:
    DownloadFile `
        "https://dl.bintray.com/boostorg/release/$boostVersion/source/$boostZipFName" `
        $boostZipFPath `
        '8C20440AABA21DD963C0F7149517445F50C62CE4EB689DF2B5544CC89E6E621E';
    
    ###
    # extract:

    WriteConsole 'Extracting downloaded file...';
    
    [string]$boostSourceDir = $boostDir + '\' + $boostZipLabel;
    
    if ($(Test-Path $boostSourceDir))
    {
        Remove-Item -Force -Recurse $boostSourceDir;
    }
#    [System.IO.Compression.ZipFile]::ExtractToDirectory($boostZipFPath, $boostDir);
    
    if (-not $(Test-Path $boostSourceDir))
    {
        RaiseError "Failed to extract $boostZipFPath!";
    }
    
    ###
    # build and install libs:

    [string]$libDirSuffix;

    if ($BUILD_X64)
    {
        $libDirSuffix = '\lib\x64';
        StartBoostBuild 'x64' $libDirSuffix 'x86_amd64' 'architecture=x86 address-model=64';
    }
    if ($BUILD_X86)
    {
        $libDirSuffix = '\lib\Win32';
        StartBoostBuild 'x86' $libDirSuffix 'x86' 'architecture=x86 address-model=32';
    }
    if ($BUILD_ARM)
    {
        $libDirSuffix = '\lib\ARM';
        StartBoostBuild 'ARM' $libDirSuffix 'x86_arm64' 'architecture=arm address-model=64';
    }
    
    # install headers:
    Move-Item "$boostSourceDir\boost" "$boostInstallDir\include\";

    # clean-up:
    Remove-Item $boostSourceDir -Recurse -Force;
}


###
# Check POCO C++ availability:
#

[string]$pocoVersion = '1.9.4';
[string]$pocoRootPathExpSuffix = 'POCO\v' + $pocoVersion + '\';
[string]$envVarPocoRoot = [Environment]::GetEnvironmentVariable("POCO_ROOT");

if (
     (-not $envVarPocoRoot) -or
     ( (-not $envVarPocoRoot.ToLower().EndsWith($pocoRootPathExpSuffix.ToLower())) -and
       (-not ($envVarPocoRoot + '\').ToLower().EndsWith($pocoRootPathExpSuffix.ToLower())) )
   )
{
    RaiseError(
        'The enviroment variable POCO_ROOT must be set! ' +
        'This build depends on POCO C++ version ' + $pocoVersion +
        ', so you should set POCO_ROOT = path_of_your_choice\' + $pocoRootPathExpSuffix
    );
}

# # # # #
# Build POCO C++ libraries
#
function BuildPocoLibs ()
{
    LoadDotNetAssemblies;
    
    [string]$pocoDir = $(Get-ChildItem $(Join-Path -Path $envVarPocoRoot -ChildPath '\..\')).FullName;
    [string]$pocoZipLabel = 'poco-' + $pocoVersion + '-all';
    [string]$pocoZipFName = $pocoZipLabel + '.zip';
    [string]$pocoZipFPath = $pocoDir + '\' + $pocoZipFName;
    [string]$pocoInstallDir = $envVarPocoRoot;

    if ($(Test-Path $envVarPocoRoot))
    {
        [string]$continue = $(Read-Host 'POCO C++ libraries appears to be already installed. Do you want to erase and rebuild that? (no/yes)');

        if ($continue.ToLower() -ne 'yes')
        {
            return;
        }

        Remove-Item -Force -Recurse "$envVarPocoRoot\*" -Exclude $pocoZipFName;
    }
    
    mkdir -Path $envVarPocoRoot -Force;

    ###
    # download:
    DownloadFile `
        "https://pocoproject.org/releases/poco-$pocoVersion/$pocoZipFName" `
        $pocoZipFPath `
        'B74ECDAA5BDF7F8BFDFBA66F72BA00B6A35934E55573DB2A5BC2D8A412AEA8E7';
    
    ###
    # extract:

    WriteConsole "Extracting $pocoZipFPath ...";
    
    [string]$pocoSourceDir = $pocoDir + '\' + $pocoZipLabel;
    
    if ($(Test-Path $pocoSourceDir))
    {
        Remove-Item -Force -Recurse $pocoSourceDir;
    }

    [System.IO.Compression.ZipFile]::ExtractToDirectory($pocoZipFPath, $pocoDir);

    if (-not $(Test-Path $pocoSourceDir))
    {
        RaiseError 'Failed to extract POCO C++ source code!';
    }
    
    Move-Item ($pocoSourceDir + '\*') $pocoInstallDir;
    Remove-Item $pocoSourceDir -Force;

    ###
    # build:
    
    if ($BUILD_X64)
    {
        WriteConsole 'Building POCO C++ Foundation x64...';
        MsBuild -what ($pocoInstallDir + '\Foundation\Foundation_x64_vs150.vcxproj') -task build -arch x64 -config debug_static_md -sdk $winSdkVer
        MsBuild -what ($pocoInstallDir + '\Foundation\Foundation_x64_vs150.vcxproj') -task build -arch x64 -config release_static_md -sdk $winSdkVer
        
        WriteConsole 'Building POCO C++ Data x64...';
        MsBuild -what ($pocoInstallDir + '\Data\Data_x64_vs150.vcxproj') -task build -arch x64 -config debug_static_md -sdk $winSdkVer
        MsBuild -what ($pocoInstallDir + '\Data\Data_x64_vs150.vcxproj') -task build -arch x64 -config release_static_md -sdk $winSdkVer
        
        WriteConsole 'Building POCO C++ Data\ODBC x64...';
        MsBuild -what ($pocoInstallDir + '\Data\ODBC\ODBC_x64_vs150.vcxproj') -task build -arch x64 -config debug_static_md -sdk $winSdkVer
        MsBuild -what ($pocoInstallDir + '\Data\ODBC\ODBC_x64_vs150.vcxproj') -task build -arch x64 -config release_static_md -sdk $winSdkVer    
        
        ###
        # clean-up:
        Remove-Item ($pocoInstallDir + '\Foundation\obj64') -Recurse -Force;
        Remove-Item ($pocoInstallDir + '\Data\obj64')       -Recurse -Force;
        Remove-Item ($pocoInstallDir + '\Data\ODBC\obj64')  -Recurse -Force;
    }
    if ($BUILD_X86)
    {
        WriteConsole 'Building POCO C++ Foundation x86...';
        MsBuild -what ($pocoInstallDir + '\Foundation\Foundation_vs150.vcxproj') -task build -arch x86 -config debug_static_md -sdk $winSdkVer
        MsBuild -what ($pocoInstallDir + '\Foundation\Foundation_vs150.vcxproj') -task build -arch x86 -config release_static_md -sdk $winSdkVer
        
        WriteConsole 'Building POCO C++ Data x86...';
        MsBuild -what ($pocoInstallDir + '\Data\Data_vs150.vcxproj') -task build -arch x86 -config debug_static_md -sdk $winSdkVer
        MsBuild -what ($pocoInstallDir + '\Data\Data_vs150.vcxproj') -task build -arch x86 -config release_static_md -sdk $winSdkVer
        
        WriteConsole 'Building POCO C++ Data\ODBC x86...';
        MsBuild -what ($pocoInstallDir + '\Data\ODBC\ODBC_vs150.vcxproj') -task build -arch x86 -config debug_static_md -sdk $winSdkVer
        MsBuild -what ($pocoInstallDir + '\Data\ODBC\ODBC_vs150.vcxproj') -task build -arch x86 -config release_static_m -sdk $winSdkVer
        
        ###
        # clean-up:
        Remove-Item ($pocoInstallDir + '\Foundation\obj') -Recurse -Force;
        Remove-Item ($pocoInstallDir + '\Data\obj')       -Recurse -Force;
        Remove-Item ($pocoInstallDir + '\Data\ODBC\obj')  -Recurse -Force;
    }
}


# # # # #
# Installs temporary certificates for the UWP apps
#
function InstallCertificates ()
{
    WriteConsole 'Installing temporary certificates for UWP apps...';

    $certificates = @(
        @{
            path = $(Get-Location).Path + "\IntegrationTests\UWP\IntegrationTestsApp_TemporaryKey.pfx";
            password = "IntegrationTestsApp_TemporaryKey.pfx";
            thumbprint = "96C28522337AD47DE34DD587105FCDA6194E93EE";
        },
        @{
            path = $(Get-Location).Path + "\UnitTests\UWP\UnitTestsApp_TemporaryKey.pfx";
            password = "UnitTestsApp_TemporaryKey.pfx";
            thumbprint = "6A052BC56B5F0F365BF306C65D12A329916DCC3D";
        }
    );

    for ([Int32]$idx = 0; $idx -lt $certificates.Length; $idx++)
    {
        $certInfo = $certificates[$idx];

        # certificate not yet installed?
        if (@(Get-ChildItem cert:CurrentUser\My | Where-Object { $_.Thumbprint -eq $certInfo.thumbprint }).Length -eq 0)
        {
            Import-PfxCertificate -FilePath $certInfo.path `
                                  -CertStoreLocation cert:CurrentUser\My `
                                  -Password $(ConvertTo-SecureString -String $certInfo.password -Force –AsPlainText);
        }
    }
}


# # # # #
# Installs a copy of all X64 build output
#
function InstallCopyX64 ()
{
    [string]$outReleaseDir = 'x64\Release';
    [string]$outDebugDir = 'x64\Debug';
    
    mkdir -Path build\lib\x64\Release -Force;
    mkdir -Path build\bin\x64\Release -Force;

    if ($(Test-Path "$outReleaseDir\3FD.WinRT.UWP"))
    {
        Copy-Item "$outReleaseDir\3FD.WinRT.UWP\*" build\lib\x64\Release;
    }

    Copy-Item "$outReleaseDir\3FD.lib"   build\lib\x64\Release;
    Copy-Item "$outReleaseDir\*.exe"     build\bin\x64\Release;
    Copy-Item "$outReleaseDir\*.config"  build\bin\x64\Release;
    Copy-Item "$outReleaseDir\*.wsdl"    build\bin\x64\Release;
    
    mkdir -Path build\bin\x64\Debug -Force;
    mkdir -Path build\lib\x64\Debug -Force;

    if ($(Test-Path "$outDebugDir\3FD.WinRT.UWP"))
    {
        Copy-Item "$outDebugDir\3FD.WinRT.UWP\*" build\lib\x64\Debug;
    }
    
    Copy-Item "$outDebugDir\3FD.lib"   build\lib\x64\Debug;
    Copy-Item "$outDebugDir\*.exe"     build\bin\x64\Debug;
    Copy-Item "$outDebugDir\*.config"  build\bin\x64\Debug;
    Copy-Item "$outDebugDir\*.wsdl"    build\bin\x64\Debug;
    
    if ($(Test-Path AppPackages))
    {
        Get-ChildItem AppPackages `
            | Get-ChildItem -Filter *x64* `
            | ForEach-Object -Process { Move-Item $_.FullName build\AppPackages\ -Force };
    }
}


# # # # #
# Installs a copy of all X86 build output
#
function InstallCopyX86 ()
{
    [string]$outReleaseDir = 'Win32\Release';
    [string]$outDebugDir = 'Win32\Debug';
    
    mkdir -Path build\lib\Win32\Release -Force;
    mkdir -Path build\bin\Win32\Release -Force;

    if ($(Test-Path "$outReleaseDir\3FD.WinRT.UWP"))
    {
        Copy-Item "$outReleaseDir\3FD.WinRT.UWP\*" build\lib\Win32\Release;
    }

    Copy-Item "$outReleaseDir\3FD.lib"   build\lib\Win32\Release;
    Copy-Item "$outReleaseDir\*.exe"     build\bin\Win32\Release;
    Copy-Item "$outReleaseDir\*.config"  build\bin\Win32\Release;
    Copy-Item "$outReleaseDir\*.wsdl"    build\bin\Win32\Release;
    
    mkdir -Path build\lib\Win32\Debug -Force;
    mkdir -Path build\bin\Win32\Debug -Force;

    if ($(Test-Path "$outDebugDir\3FD.WinRT.UWP"))
    {
        Copy-Item "$outDebugDir\3FD.WinRT.UWP\*" build\lib\Win32\Debug;
    }

    Copy-Item "$outDebugDir\3FD.lib"   build\lib\Win32\Debug;
    Copy-Item "$outDebugDir\*.exe"     build\bin\Win32\Debug;
    Copy-Item "$outDebugDir\*.config"  build\bin\Win32\Debug;
    Copy-Item "$outDebugDir\*.wsdl"    build\bin\Win32\Debug;

    if ($(Test-Path AppPackages))
    {
        Get-ChildItem AppPackages `
            | Get-ChildItem -Filter *Win32* `
            | ForEach-Object -Process { Move-Item $_.FullName build\AppPackages\ -Force };
    }
}


# # # # #
# Installs a copy of all ARM build output
#
function InstallCopyARM ()
{
    mkdir -Path build\lib\ARM\Release -Force;
    mkdir -Path build\lib\ARM\Debug -Force;

    Copy-Item ARM\Release\3FD.WinRT.UWP\*  build\lib\ARM\Release;
    Copy-Item ARM\Debug\3FD.WinRT.UWP\*    build\lib\ARM\Debug;

    if ($(Test-Path AppPackages))
    {
        Get-ChildItem AppPackages `
            | Get-ChildItem -Filter *ARM* `
            | ForEach-Object -Process { Move-Item $_.FullName build\AppPackages\ -Force };
    }
}


# =============================================
#                  MAIN ROUTINE
# =============================================

function Run ()
{
   [string]$mustBuildBoost = $(Read-Host 'Build Boost libraries? (no/yes)');
   [string]$mustBuildPoco = $(Read-Host 'Build POCO C++ libraries? (no/yes)');

    if ($mustBuildBoost -eq 'yes')
    {
        InstallBoostLibs;
    }

    if ($mustBuildPoco -eq 'yes')
    {
        BuildPocoLibs;
    }

#    ###
#    # BUILD:
#    
#    InstallCertificates;
#
#    WriteConsole 'Building 3FD...';
#    
#    if ($arch -eq 'all')
#    {
#        MsBuild -what 3FD.sln -task $task -arch x86 -config ('Debug');
#        MsBuild -what 3FD.sln -task $task -arch x86 -config ('Release');
#        MsBuild -what 3FD.sln -task $task -arch x64 -config ('Debug');
#        MsBuild -what 3FD.sln -task $task -arch x64 -config ('Release');
#        MsBuild -what 3FD.sln -task $task -arch ARM -config ('Debug');
#        MsBuild -what 3FD.sln -task $task -arch ARM -config ('Release');
#    }
#    else
#    {
#        MsBuild -what 3FD.sln -task $task -arch $arch -config ('Debug');
#        MsBuild -what 3FD.sln -task $task -arch $arch -config ('Release');
#    }
#    
#    ###
#    # INSTALL:
#
#    WriteConsole 'Installing 3FD...';
#    
#    if ($(Test-Path build))
#    {
#        Remove-Item build -Recurse -Force;
#    }
#
#    mkdir -Path build\AppPackages -Force;
#
#    switch ($arch)
#    {
#        "x64" { InstallCopyX64 ($xp -eq 'yes'); }
#        "x86" { InstallCopyX86 ($xp -eq 'yes'); }
#        "arm" { InstallCopyARM ($xp -eq 'yes'); }
#        "all" {
#            InstallCopyX64 ($xp -eq 'yes');
#            InstallCopyX86 ($xp -eq 'yes');
#            InstallCopyARM ($xp -eq 'yes');
#        }
#        default {
#            WriteConsole ('There is no defined installation procedure for architecture <' + $arch + '>');
#        }
#    }
#
#    Copy-Item 3FD\wsdl-example.wsdl       build\
#    Copy-Item 3FD\3fd-config-template.xml build\
#    Copy-Item opencl-c-example*           build\
#    Copy-Item CreateMsSqlSvcBrokerDB.sql  build\
#    Copy-Item README                      build\
#    Copy-Item LICENSE                     build\
#    Copy-Item Acknowledgements.txt        build\
#
#    mkdir -Path .\build\include\3FD -Force;
#
#    Copy-Item 3FD\*.h build\include\3FD\;
#    Copy-Item btree   build\include\ -Recurse;
#    Copy-Item OpenCL  build\include\ -Recurse;
#
#    WriteConsole ('Framework installation directory has been generated in <' + ($pwd).Path + '\build>');

}# end of Run


Run;
