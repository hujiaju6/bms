$projectDir = "E:\桌面\BMS正确485驱动\BMS\BMS\my_f407rtos"
$buildDir = "$projectDir\build_final"
$cmakeExe = "G:\尚硅谷嵌入式技术之LVGL基础之模拟开发和移植\2.资料\lvgl自定义开发环境构建\cmake-4.1.2-windows-x86_64\cmake-4.1.2-windows-x86_64\bin\cmake.exe"
$ninjaExe = "G:\尚硅谷嵌入式技术之LVGL基础之模拟开发和移植\2.资料\lvgl自定义开发环境构建\ninja-win\ninja.exe"

if (Test-Path $buildDir) {
    Remove-Item -LiteralPath $buildDir -Recurse -Force
}
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

Write-Output "=== CMake Generate ==="
& $cmakeExe -S $projectDir -B $buildDir -G Ninja `
    -DCMAKE_MAKE_PROGRAM="$ninjaExe" `
    -DCMAKE_TOOLCHAIN_FILE="$projectDir/cmake/arm-none-eabi-gcc.cmake" `
    2>&1

Write-Output "`n=== Ninja Build ==="
Set-Location $buildDir
& $ninjaExe -j4 2>&1

Write-Output "`n=== Build Products ==="
Get-ChildItem $buildDir -Include "*.elf","*.bin","*.hex","*.map" | Select-Object Name, @{N='Size(KB)';E={[math]::Round($_.Length/1KB,2)}}
