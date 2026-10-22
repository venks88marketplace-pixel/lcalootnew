Write-Host "Building Calibration Tool in debug mode"
# ---- Qt path ----
$QT6_PATH = $args[0]
$BUILD_TYPE = if ($args.Length -ge 2) { $args[1] } else { "Debug" }

if (-not $QT6_PATH) {
    Write-Host "Usage: .\build.ps1 <Qt install path> [Debug|Release]"
    Write-Host "Example: .\build.ps1 C:/Qt/6.8.3/msvc2022_64"
    exit 1
}
Write-Host "Qt path is $QT6_PATH"
Write-Host "Build type is $BUILD_TYPE"

# Remaining args for CMake
$cmakeArgs = @()
if ($args.Length -gt 2) {
    $cmakeArgs = $args[2..($args.Length - 1)]
}

$BUILD_DIR = "build"
$conanOut = "$BUILD_DIR/$BUILD_TYPE"

function Install-Conan { 
	Write-Host "Running Conan install..." 
	conan install . `
	-s build_type=$BUILD_TYPE `
	-s compiler.cppstd=17 `
	-s compiler.version=194 `
	-s:b compiler.cppstd=17 `
	-s:b compiler.version=194 `
	--build=missing `
	-pr=calprofiledebug 
	if ($LASTEXITCODE -ne 0) { 
		Write-Host "Conan install failed!" exit 1 
	} 
}
function CMake-Build {
    Write-Host "Running CMake configure..."
	
	$toolchain = Join-Path (Get-Location) "$BUILD_DIR/generators/conan_toolchain.cmake"

    Write-Host "Toolchain: $toolchain"
    Write-Host "Exists: $(Test-Path $toolchain)"
	Write-Host "Qt6 Path: $QT6_PATH"
	
	$qtDir = "$QT6_PATH/lib/cmake/Qt6"
	
    cmake `
        -S . `
        -B $conanOut `
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" `
		-DQt6_DIR="$qtDir" `
        -DCMAKE_BUILD_TYPE=Debug

    if ($LASTEXITCODE -ne 0) {
        Write-Host "CMake configure failed!"
        exit 1
    }
    Write-Host "Building..."
    cmake --build $conanOut
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed!"
        exit 1
    }
}
Install-Conan
CMake-Build
Write-Host "Build completed successfully!"
