$ErrorActionPreference = 'Stop'

Set-Location -Path $PSScriptRoot

$pythonCmd = $null
if (Get-Command py -ErrorAction SilentlyContinue) {
    $pythonCmd = @('py', '-3')
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
    $pythonCmd = @('python')
} else {
    Write-Error 'Python 3를 찾을 수 없습니다. Python 설치 후 다시 실행하세요.'
}

if ($pythonCmd.Length -gt 1) {
    & $pythonCmd[0] $pythonCmd[1] 'run.py' @args
} else {
    & $pythonCmd[0] 'run.py' @args
}
exit $LASTEXITCODE
