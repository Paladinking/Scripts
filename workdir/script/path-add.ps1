

if (!$args) {
    Break
}

if (!$Env:SYSTEMROOT) {
    $Env:SYSTEMROOT = C:\Windows
}

foreach ($arg in $args) {
    if ($arg -eq "RESET") {
        echo Reset Not Supported yet...
    }

    foreach ($line in $(Select-String -Path $PSScriptRoot\paths.txt -Pattern ("\|" + $arg + '$')).line) {
		$path_val = $line.Split('|')[0]
        $new_path = pathc.exe add -b $path_val
        if ($LastExitCode -eq 0) {
            echo "Added $path_val to path"
            $Env:Path = $new_path
        }
    }

    foreach ($line in $(Select-String -Path $PSScriptRoot\paths.txt -Pattern ("<" + $arg + '$')).line) {
        $path_name = $line.Split('<')[0]
        path-add $path_name
    }

	foreach ($line in $(Select-String -Path $PSScriptRoot\paths.txt -Pattern (">" + $arg + '$')).line) {
		$path_command = $line.Split('>')[0] -replace '"', ""
		& $path_command
	}
}
