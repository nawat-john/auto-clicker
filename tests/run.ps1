$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$compiler = 'C:\msys64\ucrt64\bin\g++.exe'
$originalPath = $env:Path
$env:Path = (Split-Path -Parent $compiler) + ';' + $env:Path
$originalTestRegistry = $env:CLICKER_TEST_REGISTRY
$env:CLICKER_TEST_REGISTRY = 'Software\UltraAutoClickerTests-' + [guid]::NewGuid()
$testDirectory = Join-Path ([IO.Path]::GetTempPath()) ('clicker-tests-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $testDirectory | Out-Null
try {
    & $compiler -std=c++20 -O2 -Wall -Wextra -ftrapv -D_GLIBCXX_ASSERTIONS -DUNICODE -D_UNICODE `
        (Join-Path $PSScriptRoot 'keyboard_macro_test.cpp') `
        (Join-Path $projectRoot 'src/app_ui.cpp') `
        (Join-Path $projectRoot 'src/hotkeys.cpp') `
        (Join-Path $projectRoot 'src/registry.cpp') `
        (Join-Path $projectRoot 'src/config.cpp') `
        -o (Join-Path $testDirectory 'keyboard_macro_test.exe') -static -lwinmm -lcomctl32 -lcomdlg32 -ldwmapi -luxtheme -lgdi32 -ladvapi32
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed.' }
    & (Join-Path $testDirectory 'keyboard_macro_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Keyboard macro tests failed.' }
} finally {
    $env:Path = $originalPath
    [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKey($env:CLICKER_TEST_REGISTRY, $false)
    $env:CLICKER_TEST_REGISTRY = $originalTestRegistry
    $resolved = [IO.Path]::GetFullPath($testDirectory)
    $temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (!$resolved.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase) -or
        !(Split-Path -Leaf $resolved).StartsWith('clicker-tests-')) { throw 'Unexpected test directory.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
