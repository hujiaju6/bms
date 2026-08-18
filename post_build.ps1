param(
    [string]$buildDir,
    [string]$elfName
)

$armBin = "G:\arm-gcc\bin"

Set-Location $buildDir

$baseName = [System.IO.Path]::GetFileNameWithoutExtension($elfName)

# Generate .bin
& "$armBin\arm-none-eabi-objcopy.exe" -O binary $elfName "$baseName.bin"
Write-Output "Generated $baseName.bin"

# Generate .hex
& "$armBin\arm-none-eabi-objcopy.exe" -O ihex $elfName "$baseName.hex"
Write-Output "Generated $baseName.hex"

# Size report
& "$armBin\arm-none-eabi-size.exe" --format=berkeley $elfName
