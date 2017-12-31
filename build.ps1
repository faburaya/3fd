# Script written by Felipe Vieira Aburaya (last update: October 12th 2017)
#
# Usage:
#
# .\build.ps1 -arch {x86|x64|ARM|all} -xp {yes|no} -rebuild {yes|no}
#
# Builds all the projects in VC++ solution 3FD.sln that are compilable for the given
# architecture(s). Builds are by default incremental, but can also be complete rebuilds.
# Projects of UWP apps produce packages that are easy to deploy with a click. All of
# this goes to the generated build directory in the solution root directory, along with
# headers, libraries, templates, samples and test applications.
#
# There is the option of downloading and building from source the dependency libraries
# Boost and POCO C++. They are set to the specific release with which 3FD projects are 
# tested and configured to work. They are installed into a version specific directory
# in a location specified by the enviroment variables BOOST_HOME and POCO_ROOT, those
# MUST BE set prior to script execution.
#
# In order to build the projects, MSBuild is employed. The script looks for MSBuild.exe
# (32 bits) executable first in %PROGRAMFILES(x86)%\MSBuild, then in "Visual Studio 1X"
# directories, giving priority to the latest release.
#

# =============================================
#           PARAMETERS & ENVIRONMENT
# =============================================

param (
    [string]$arch = $(Read-Host 'Architecture (x86, x64, ARM, all)'),
    [string]$xp = $(Read-Host 'Use WinXP-compatible toolset? (no/yes)'),
    [string]$rebuild = $(Read-Host 'Rebuild? (no/yes)')
)

if (($arch.ToLower() -eq 'arm') -and ($xp -eq 'yes'))
{
    Write-Host 'Nothing to build for ARM with WinXP-compatible toolset!' -ForegroundColor yellow;
    exit;
}

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
            move -Force "$PSHOME\powershell.exe.config" "$PSHOME\powershell.exe.config.old";
        }
        
        if ($(Test-Path "$PSHOME\powershell_ise.exe.config"))
        {
            move -Force "$PSHOME\powershell_ise.exe.config" "$PSHOME\powershell_ise.exe.config.old";
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


[string]$msBuildPath;

# # # # #
# build solution for a given configuration
#
function MsBuild (
    [string]$what,   # solution or project to build
    [string]$task,   # build|rebuild
    [string]$arch,   # x86|x64|ARM
    [string]$config, # debug|release
    [string]$toolset # (optional) ex: v140_xp
)
{
    ###
    # single initialization:

    if (-not $global:msBuildPath)
    {
        $msBuildInstalls = @(gci $([Environment]::GetEnvironmentVariable("PROGRAMFILES(X86)")) `
            | Where-Object { $_.Name.Contains('Visual Studio 1') -or $_.Name.Contains('MSBuild')  } `
            | Sort-Object @{ Expression="Name"; Descending=$true });

        if ($msBuildInstalls.Length -eq 0)
        {
            RaiseError 'Could not find a MSBuild installation!';
        }

        $msBuildExecs = @(gci -Recurse ($msBuildInstalls[0].FullName) `
            | Where-Object { ($_.Name -eq 'MSBuild.exe') -and (-not $_.FullName.Contains('amd64')) } `
            | Sort-Object @{ Expression="FullName"; Descending=$true });

        if ($msBuildExecs -eq 0)
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

    [string]$command = '. "' + $global:msBuildPath + '" "' + $what + '" /verbosity:minimal /m';

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
    
    # Use custom toolset?
    if ($toolset -and ($toolset -ne ''))
    {
        $command += ' /p:PlatformToolset=' + $toolset;
    }
    
    Write-Host '';
    Write-Host $command -ForegroundColor Cyan;
    Write-Host '';

    Invoke-Expression -Command $command;

    if ($lastexitcode -ne 0)
    {
        DisplayError ('Failed to build <' + $what + '>');
    }
}


###
# Check BOOST_HOME:

[string]$boostVersion = '1.65.1';
[string]$boostZipLabel = 'boost_1_65_1';
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

[string]$vcvarsall;

# # # # #
# Build boost libraries
#
function BuildBoostLibs ()
{
    LoadDotNetAssemblies;
    
    ###
    # single initialization:

    if (-not $global:vcvarsall)
    {
        $global:vcvarsall = $(gci -Recurse `
            $($(gci $([Environment]::GetEnvironmentVariable("PROGRAMFILES(X86)")) `
                    | Where-Object { $_.Name.Contains('Visual Studio 1') } `
                    | Sort-Object @{ Expression="Name"; Descending=$true })[0].FullName + '\VC') `
            | Where-Object { ($_.Name -eq 'vcvarsall.bat') }
        ).FullName;
    }

    if (-not $global:vcvarsall)
    {
        RaiseError 'vcvarsall.bat not found!';
    }

    ###
    # files & directories:
    
    if ($(Test-Path $envVarBoostHome))
    {
        [string]$continue = $(Read-Host 'Boost libraries appears to be already installed. Do you want to erase and rebuild that? (no/yes)');

        if ($continue.ToLower() -ne 'yes')
        {
            return;
        }

        ri -Force -Recurse ($envVarBoostHome + '\*');
    }

    mkdir $envVarBoostHome -Force;
    
    [string]$boostDir = $(gi $(Join-Path -Path $envVarBoostHome -ChildPath '\..\')).FullName;

    [string]$boostZipFName = $boostZipLabel + '.zip';
    [string]$boostZipFPath = $boostDir + '\' + $boostZipFName;
    [string]$boostInstallDir = $envVarBoostHome;

    ###
    # download:

    WriteConsole 'Downloading boost source code...';
    
    if ($(Test-Path $boostZipFPath))
    {
        ri $boostZipFPath -Force;
    }
    
    (New-Object System.Net.WebClient).DownloadFile(
        'https://dl.bintray.com/boostorg/release/' + $boostVersion + '/source/' + $boostZipFName,
        $boostZipFPath
    );

    if (-not $(Test-Path $boostZipFPath))
    {
        RaiseError 'Failed to download boost source code!';
    }
    
    ###
    # extract:

    WriteConsole 'Extracting downloaded file...';
    
    [string]$boostSourceDir = $boostDir + '\' + $boostZipLabel;
    
    if ($(Test-Path $boostSourceDir))
    {
        ri -Force -Recurse $boostSourceDir;
    }

    [System.IO.Compression.ZipFile]::ExtractToDirectory($boostZipFPath, $boostDir);
    
    if (-not $(Test-Path $boostSourceDir))
    {
        RaiseError 'Failed to extract boost source code!';
    }
    
    ###
    # generate build script:

    WriteConsole 'Generating build script...';

    mkdir ($boostInstallDir + '\include') -Force;
    mkdir ($boostInstallDir + '\lib\Win32') -Force;
    mkdir ($boostInstallDir + '\lib\x64') -Force;
    
    [string]$boostBuildBatchScriptFPath = $boostSourceDir + '\build_boost.bat';

    if ($(Test-Path $boostBuildBatchScriptFPath))
    {
        ri $boostBuildBatchScriptFPath;
    }

    AppendFile -file $boostBuildBatchScriptFPath -line ('call "' + $global:vcvarsall + '" x86_amd64');
    AppendFile -file $boostBuildBatchScriptFPath -line ('cd "' + $boostSourceDir + '\tools\build"');
    AppendFile -file $boostBuildBatchScriptFPath -line ('call bootstrap.bat');
    AppendFile -file $boostBuildBatchScriptFPath -line ('cd ..\..\');

    [UInt32]$nCpuCores = $(Get-WmiObject -Class win32_processor).NumberOfCores;

    [string]$buildCmdDbgX64 =
        "tools\build\b2.exe -j $nCpuCores --abbreviate-paths address-model=64 variant=debug link=static threading=multi runtime-link=shared";
    
    [string]$buildCmdRelX64 =
        "tools\build\b2.exe -j $nCpuCores --abbreviate-paths address-model=64 variant=release link=static threading=multi runtime-link=shared";
    
    [string]$buildCmdDbgX86 =
        "tools\build\b2.exe -j $nCpuCores --abbreviate-paths address-model=32 variant=debug link=static threading=multi runtime-link=shared";

    [string]$buildCmdRelX86 =
        "tools\build\b2.exe -j $nCpuCores --abbreviate-paths address-model=32 variant=release link=static threading=multi runtime-link=shared";

    # For XP toolset:
    if ($xp -eq 'yes')
    {
        $buildCmdDbgX64 += ' toolset=msvc-14.0_xp';
        $buildCmdRelX64 += ' toolset=msvc-14.0_xp';
        $buildCmdDbgX86 += ' toolset=msvc-14.0_xp';
        $buildCmdRelX86 += ' toolset=msvc-14.0_xp';
    }
    
    AppendFile -file $boostBuildBatchScriptFPath -line $buildCmdDbgX64;
    AppendFile -file $boostBuildBatchScriptFPath -line $buildCmdRelX64;
    AppendFile -file $boostBuildBatchScriptFPath -line ('move stage\lib\* "' + $boostInstallDir + '\lib\x64"');
    AppendFile -file $boostBuildBatchScriptFPath -line ('call "' + $global:vcvarsall + '" x86');
    AppendFile -file $boostBuildBatchScriptFPath -line $buildCmdDbgX86;
    AppendFile -file $boostBuildBatchScriptFPath -line $buildCmdRelX86;
    AppendFile -file $boostBuildBatchScriptFPath -line ('move stage\lib\* "' + $boostInstallDir + '\lib\Win32"');

    ###
    # start build & install:
    
    WriteConsole 'Building boost...';

    $cmdPID = $(Start-Process -FilePath ('"' + $boostBuildBatchScriptFPath + '"') -PassThru).Id;

    Wait-Process -Id $cmdPID;

    if (@(gci ($boostInstallDir + '\lib\x64')).Length -eq 0)
    {
        RaiseError 'Failed to build boost x64!';
    }

    if (@(gci ($boostInstallDir + '\lib\Win32')).Length -eq 0)
    {
        RaiseError 'Failed to build boost x86!';
    }

    mv ($boostSourceDir + '\boost') ($boostInstallDir + '\include\');

    ###
    # clean-up:

    rmdir $boostSourceDir -Recurse -Force;
    ri $boostZipFPath -Force;
}


###
# Check POCO C++ availability:

[string]$pocoVersion = '1.7.9';
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
    
    if ($(Test-Path $envVarPocoRoot))
    {
        [string]$continue = $(Read-Host 'POCO C++ libraries appears to be already installed. Do you want to erase and rebuild that? (no/yes)');

        if ($continue.ToLower() -ne 'yes')
        {
            return;
        }

        ri -Force -Recurse ($envVarPocoRoot + '\*');
    }
    
    mkdir -Path $envVarPocoRoot -Force;

    [string]$pocoDir = $(gi $(Join-Path -Path $envVarPocoRoot -ChildPath '\..\')).FullName;

    [string]$pocoZipLabel = 'poco-' + $pocoVersion + '-all';
    [string]$pocoZipFName = $pocoZipLabel + '.zip';
    [string]$pocoZipFPath = $pocoDir + '\' + $pocoZipFName;
    [string]$pocoInstallDir = $envVarPocoRoot;
    
    ###
    # download:
    
    WriteConsole 'Downloading POCO C++ source code...';

    if ($(Test-Path $pocoZipFPath))
    {
        ri $pocoZipFPath -Force;
    }
    
    (New-Object System.Net.WebClient).DownloadFile(
        'https://pocoproject.org/releases/poco-' + $pocoVersion + '/' + $pocoZipFName,
        $pocoZipFPath
    );

    if (-not $(Test-Path $pocoZipFPath))
    {
        RaiseError 'Failed to download POCO C++ source code!';
    }
    
    ###
    # extract:

    WriteConsole 'Extracting downloaded file...';
    
    [string]$pocoSourceDir = $pocoDir + '\' + $pocoZipLabel;
    
    if ($(Test-Path $pocoSourceDir))
    {
        ri -Force -Recurse $pocoSourceDir;
    }

    [System.IO.Compression.ZipFile]::ExtractToDirectory($pocoZipFPath, $pocoDir);

    if (-not $(Test-Path $pocoSourceDir))
    {
        RaiseError 'Failed to extract POCO C++ source code!';
    }
    
    mv ($pocoSourceDir + '\*') $pocoInstallDir;
    ri $pocoSourceDir -Force;

    ###
    # build:
    
    [string]$customToolset;
    if ($xp -eq 'yes')
    {
        $customToolset = 'v140_xp';
    }

    WriteConsole 'Building POCO C++ Foundation x86...';

    MsBuild -what ($pocoInstallDir + '\Foundation\Foundation_vs140.vcxproj') -task build -arch x86 -config debug_static_md -toolset $customToolset
    MsBuild -what ($pocoInstallDir + '\Foundation\Foundation_vs140.vcxproj') -task build -arch x86 -config release_static_md -toolset $customToolset
    
    WriteConsole 'Building POCO C++ Foundation x64...';

    MsBuild -what ($pocoInstallDir + '\Foundation\Foundation_x64_vs140.vcxproj') -task build -arch x64 -config debug_static_md -toolset $customToolset
    MsBuild -what ($pocoInstallDir + '\Foundation\Foundation_x64_vs140.vcxproj') -task build -arch x64 -config release_static_md -toolset $customToolset
    
    WriteConsole 'Building POCO C++ XML x86...';

    MsBuild -what ($pocoInstallDir + '\XML\XML_vs140.vcxproj') -task build -arch x86 -config debug_static_md -toolset $customToolset
    MsBuild -what ($pocoInstallDir + '\XML\XML_vs140.vcxproj') -task build -arch x86 -config release_static_md -toolset $customToolset
    
    WriteConsole 'Building POCO C++ XML x64...';

    MsBuild -what ($pocoInstallDir + '\XML\XML_x64_vs140.vcxproj') -task build -arch x64 -config debug_static_md -toolset $customToolset
    MsBuild -what ($pocoInstallDir + '\XML\XML_x64_vs140.vcxproj') -task build -arch x64 -config release_static_md -toolset $customToolset
    
    WriteConsole 'Building POCO C++ Util x86...';

    MsBuild -what ($pocoInstallDir + '\Util\Util_vs140.vcxproj') -task build -arch x86 -config debug_static_md -toolset $customToolset
    MsBuild -what ($pocoInstallDir + '\Util\Util_vs140.vcxproj') -task build -arch x86 -config release_static_md -toolset $customToolset
    
    WriteConsole 'Building POCO C++ Util x64...';

    MsBuild -what ($pocoInstallDir + '\Util\Util_x64_vs140.vcxproj') -task build -arch x64 -config debug_static_md -toolset $customToolset
    MsBuild -what ($pocoInstallDir + '\Util\Util_x64_vs140.vcxproj') -task build -arch x64 -config release_static_md -toolset $customToolset
    
    WriteConsole 'Building POCO C++ Data x86...';

    MsBuild -what ($pocoInstallDir + '\Data\Data_vs140.vcxproj') -task build -arch x86 -config debug_static_md -toolset $customToolset
    MsBuild -what ($pocoInstallDir + '\Data\Data_vs140.vcxproj') -task build -arch x86 -config release_static_md -toolset $customToolset
    
    WriteConsole 'Building POCO C++ Data x64...';

    MsBuild -what ($pocoInstallDir + '\Data\Data_x64_vs140.vcxproj') -task build -arch x64 -config debug_static_md -toolset $customToolset
    MsBuild -what ($pocoInstallDir + '\Data\Data_x64_vs140.vcxproj') -task build -arch x64 -config release_static_md -toolset $customToolset
    
    WriteConsole 'Building POCO C++ Data\ODBC x86...';

    MsBuild -what ($pocoInstallDir + '\Data\ODBC\ODBC_vs140.vcxproj') -task build -arch x86 -config debug_static_md -toolset $customToolset
    MsBuild -what ($pocoInstallDir + '\Data\ODBC\ODBC_vs140.vcxproj') -task build -arch x86 -config release_static_md -toolset $customToolset
    
    WriteConsole 'Building POCO C++ Data\ODBC x64...';

    MsBuild -what ($pocoInstallDir + '\Data\ODBC\ODBC_x64_vs140.vcxproj') -task build -arch x64 -config debug_static_md -toolset $customToolset
    MsBuild -what ($pocoInstallDir + '\Data\ODBC\ODBC_x64_vs140.vcxproj') -task build -arch x64 -config release_static_md -toolset $customToolset
    
    ###
    # clean-up:

    rmdir ($pocoInstallDir + '\Foundation\obj')   -Recurse -Force;
    rmdir ($pocoInstallDir + '\Foundation\obj64') -Recurse -Force;
    rmdir ($pocoInstallDir + '\XML\obj')          -Recurse -Force;
    rmdir ($pocoInstallDir + '\XML\obj64')        -Recurse -Force;
    rmdir ($pocoInstallDir + '\Util\obj')         -Recurse -Force;
    rmdir ($pocoInstallDir + '\Util\obj64')       -Recurse -Force;
    rmdir ($pocoInstallDir + '\Data\obj')         -Recurse -Force;
    rmdir ($pocoInstallDir + '\Data\obj64')       -Recurse -Force;
    rmdir ($pocoInstallDir + '\Data\ODBC\obj')    -Recurse -Force;
    rmdir ($pocoInstallDir + '\Data\ODBC\obj64')  -Recurse -Force;

    ri $pocoZipFPath -Force;
}


# # # # #
# Installs temporary certificates for the UWP apps
#
function InstallCertificates ()
{
    WriteConsole 'Installing temporary certificates for UWP apps...';

    $certificates = @(
        @{
            path = $(pwd).Path + "\IntegrationTests\WinRT.UWP\IntegrationTestsApp.WinRT.UWP_TemporaryKey.pfx";
            password = "IntegrationTestsApp.WinRT.UWP_TemporaryKey.pfx";
            thumbprint = "96C28522337AD47DE34DD587105FCDA6194E93EE";
        },
        @{
            path = $(pwd).Path + "\UnitTests\WinRT.UWP\UnitTestsApp.WinRT.UWP_TemporaryKey.pfx";
            password = "UnitTestsApp.WinRT.UWP_TemporaryKey.pfx";
            thumbprint = "6A052BC56B5F0F365BF306C65D12A329916DCC3D";
        }
    );

    for ([Int32]$idx = 0; $idx -lt $certificates.Length; $idx++)
    {
        $certInfo = $certificates[$idx];

        # certificate not yet installed?
        if (@(gci cert:CurrentUser\My | Where-Object { $_.Thumbprint -eq $certInfo.thumbprint }).Length -eq 0)
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
function InstallCopyX64 ([bool]$xpToolset)
{
    [string]$outReleaseDir;
    [string]$outDebugDir;
    
    if ($xpToolset -eq $false)
    {
        $outReleaseDir = 'x64\Release';
        $outDebugDir = 'x64\Debug';
    }
    else
    {
        $outReleaseDir = 'x64\Release_XP';
        $outDebugDir = 'x64\Debug_XP';
    }
    
    mkdir -Path build\lib\x64\Release -Force;
    mkdir -Path build\bin\x64\Release -Force;

    if ($(Test-Path "$outReleaseDir\3FD.WinRT.UWP"))
    {
        copy "$outReleaseDir\3FD.WinRT.UWP\*" build\lib\x64\Release;
    }

    copy "$outReleaseDir\3FD.lib"   build\lib\x64\Release;
    copy "$outReleaseDir\*.exe"     build\bin\x64\Release;
    copy "$outReleaseDir\*.config"  build\bin\x64\Release;
    copy "$outReleaseDir\*.wsdl"    build\bin\x64\Release;
    
    mkdir -Path build\bin\x64\Debug -Force;
    mkdir -Path build\lib\x64\Debug -Force;

    if ($(Test-Path "$outDebugDir\3FD.WinRT.UWP"))
    {
        copy "$outDebugDir\3FD.WinRT.UWP\*" build\lib\x64\Debug;
    }
    
    copy "$outDebugDir\3FD.lib"   build\lib\x64\Debug;
    copy "$outDebugDir\*.exe"     build\bin\x64\Debug;
    copy "$outDebugDir\*.config"  build\bin\x64\Debug;
    copy "$outDebugDir\*.wsdl"    build\bin\x64\Debug;
    
    if ($(Test-Path AppPackages))
    {
        gci AppPackages `
            | gci -Filter *x64* `
            | ForEach-Object -Process { move $_.FullName build\AppPackages\ -Force };
    }
}


# # # # #
# Installs a copy of all X86 build output
#
function InstallCopyX86 ([bool]$xpToolset)
{
    [string]$outReleaseDir;
    [string]$outDebugDir;
    
    if ($xpToolset -eq $false)
    {
        $outReleaseDir = 'Win32\Release';
        $outDebugDir = 'Win32\Debug';
    }
    else
    {
        $outReleaseDir = 'Win32\Release_XP';
        $outDebugDir = 'Win32\Debug_XP';
    }
    
    mkdir -Path build\lib\Win32\Release -Force;
    mkdir -Path build\bin\Win32\Release -Force;

    if ($(Test-Path "$outReleaseDir\3FD.WinRT.UWP"))
    {
        copy "$outReleaseDir\3FD.WinRT.UWP\*" build\lib\Win32\Release;
    }

    copy "$outReleaseDir\3FD.lib"   build\lib\Win32\Release;
    copy "$outReleaseDir\*.exe"     build\bin\Win32\Release;
    copy "$outReleaseDir\*.config"  build\bin\Win32\Release;
    copy "$outReleaseDir\*.wsdl"    build\bin\Win32\Release;
    
    mkdir -Path build\lib\Win32\Debug -Force;
    mkdir -Path build\bin\Win32\Debug -Force;

    if ($(Test-Path "$outDebugDir\3FD.WinRT.UWP"))
    {
        copy "$outDebugDir\3FD.WinRT.UWP\*" build\lib\Win32\Debug;
    }

    copy "$outDebugDir\3FD.lib"   build\lib\Win32\Debug;
    copy "$outDebugDir\*.exe"     build\bin\Win32\Debug;
    copy "$outDebugDir\*.config"  build\bin\Win32\Debug;
    copy "$outDebugDir\*.wsdl"    build\bin\Win32\Debug;

    if ($(Test-Path AppPackages))
    {
        gci AppPackages `
            | gci -Filter *Win32* `
            | ForEach-Object -Process { move $_.FullName build\AppPackages\ -Force };
    }
}


# # # # #
# Installs a copy of all ARM build output
#
function InstallCopyARM ([bool]$xpToolset)
{
    if ($xpToolset -eq $false)
    {
        mkdir -Path build\lib\ARM\Release -Force;
        mkdir -Path build\lib\ARM\Debug -Force;

        copy ARM\Release\3FD.WinRT.UWP\*  build\lib\ARM\Release;
        copy ARM\Debug\3FD.WinRT.UWP\*    build\lib\ARM\Debug;

        if ($(Test-Path AppPackages))
        {
            gci AppPackages `
                | gci -Filter *ARM* `
                | ForEach-Object -Process { move $_.FullName build\AppPackages\ -Force };
        }
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
        BuildBoostLibs;
    }

    if ($mustBuildPoco -eq 'yes')
    {
        BuildPocoLibs;
    }

    ###
    # BUILD:
    
    [string]$modeSuffix;

    if ($xp -eq 'yes')
    {
        $modeSuffix = '_xp';
    }
    else
    {
        InstallCertificates;
    }
    
    WriteConsole 'Building 3FD...';
    
    if ($arch.ToLower() -eq 'all')
    {
        MsBuild -what 3FD.sln -task $task -arch x86 -config ('Debug' + $modeSuffix);
        MsBuild -what 3FD.sln -task $task -arch x86 -config ('Release' + $modeSuffix);

        MsBuild -what 3FD.sln -task $task -arch x64 -config ('Debug' + $modeSuffix);
        MsBuild -what 3FD.sln -task $task -arch x64 -config ('Release' + $modeSuffix);

        MsBuild -what 3FD.sln -task $task -arch ARM -config ('Debug' + $modeSuffix);
        MsBuild -what 3FD.sln -task $task -arch ARM -config ('Release' + $modeSuffix);
    }
    else
    {
        MsBuild -what 3FD.sln -task $task -arch $arch -config ('Debug' + $modeSuffix);
        MsBuild -what 3FD.sln -task $task -arch $arch -config ('Release' + $modeSuffix);
    }
    
    ###
    # INSTALL:

    WriteConsole 'Installing 3FD...';
    
    if ($(Test-Path build))
    {
        ri build -Recurse -Force;
    }

    mkdir -Path build\AppPackages -Force;

    switch ($arch)
    {
        "x64" { InstallCopyX64 ($xp -eq 'yes'); }
        "x86" { InstallCopyX86 ($xp -eq 'yes'); }
        "arm" { InstallCopyARM ($xp -eq 'yes'); }
        "all" {
            InstallCopyX64 ($xp -eq 'yes');
            InstallCopyX86 ($xp -eq 'yes');
            InstallCopyARM ($xp -eq 'yes');
        }
        default {
            WriteConsole ('There is no defined installation procedure for architecture <' + $arch + '>');
        }
    }

    copy 3FD\wsdl-example.wsdl       build\
    copy 3FD\3fd-config-template.xml build\
    copy opencl-c-example*           build\
    copy CreateMsSqlSvcBrokerDB.sql  build\
    copy README                      build\
    copy LICENSE                     build\
    copy Acknowledgements.txt        build\

    mkdir -Path .\build\include\3FD -Force;

    copy 3FD\*.h build\include\3FD\;
    copy btree   build\include\ -Recurse;
    copy OpenCL  build\include\ -Recurse;

    WriteConsole ('Framework installation directory has been generated in <' + ($pwd).Path + '\build>');

}# end of Run


Run;
