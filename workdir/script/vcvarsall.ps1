$bat_cmd = 'vcvarsall.bat ' + $args[0] + ' > nul && set'
$env:SystemDrive = $env:SystemRoot.Substring(0, 2)
$cmd_env = $(cmd /C "$bat_cmd") -Split "`r`n"

$msvc_env = @{
    CommandPromptType = $null;
    DevEnvDir = $null;
    ExtensionSdkDir = $null;
    Framework40Version = $null;
    FrameworkDir = $null;
    FrameworkDir64 = $null;
    FrameworkVersion = $null;
    FrameworkVersion64 = $null;
    INCLUDE = $null;
    LIB = $null;
    LIBPATH = $null;
    Path = $null;
    Platform = $null;
    UCRTVersion = $null;
    UniversalCRTSdkDir = $null;
    VCIDEInstallDir = $null;
    VCINSTALLDIR = $null;
    VCToolsInstallDir = $null;
    VCToolsRedistDir = $null;
    VCToolsVersion = $null;
    VisualStudioVersion = $null;
    VS160COMNTOOLS = $null;
    VSCMD_ARG_app_plat = $null;
    VSCMD_ARG_HOST_ARCH = $null;
    VSCMD_ARG_TGT_ARCH = $null;
    VSCMD_VER = $null;
    VSINSTALLDIR = $null;
    WindowsLibPath = $null;
    WindowsSdkBinPath = $null;
    WindowsSdkDir = $null;
    WindowsSDKLibVersion = $null;
    WindowsSdkVerBinPath = $null;
    WindowsSDKVersion = $null;
    __devinit_path = $null;
    __DOTNET_ADD_64BIT = $null;
    __DOTNET_PREFERRED_BITNESS = $null;
    __VSCMD_PREINIT_PATH = $null;
}

foreach ($variable in $cmd_env) {
	$name, $value = $variable -Split "=", 2
	if (-not $msvc_env.Contains($name)) {
		continue;
	}
	$msvc_env[$name] = $value
}

foreach ($element in $msvc_env.GetEnumerator()) {
    Invoke-Expression -Command ('${env:' + $element.name + '} = $element.Value')
}

if ($args[0] -eq $null) {
	Write-Output "Environment initialized for amd64"
} else {
	Write-Output "Environment initialized for $args[0]"
}
