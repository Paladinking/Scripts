$saved_env = @{
	USERPROFILE = $env:USERPROFILE ;
	SystemRoot = $env:SystemRoot ;
	ComSpec = $env:ComSpec ;
	CommonProgramFiles = $env:CommonProgramFiles ;
	"CommonProgramFiles(x86)" = ${env:CommonProgramFiles(x86)}
}

$envget_path = (Get-Command envget.exe).path

foreach ($item in Get-Childitem -path env:) {
	Invoke-Expression -Command ('${env:' + $item.name + '} = $null')
}
foreach ($item in $saved_env.GetEnumerator()) {
	Invoke-Expression -Command ('${env:' + $item.name + '} = $item.value')
}

$default_env = Invoke-Expression -Command $envget_path

foreach ($item in $default_env) {
	$name, $value = $item -Split "=", 2
    Invoke-Expression -Command ('${env:' + $name + '} = $value')
}
