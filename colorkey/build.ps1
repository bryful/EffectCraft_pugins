param([string]$Zig = "zig")

$ErrorActionPreference = "Stop"
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Force dist/obj | Out-Null
    & $Zig 'c++' -target wasm32-freestanding -std=c++20 -O2 -g0 -nostdlib `
        -fno-exceptions -fno-rtti -c src/plugin.cpp -o dist/obj/plugin.o
    if ($LASTEXITCODE -ne 0) {
        throw "Compilation failed"
    }

    & $Zig build-exe -target wasm32-freestanding --entry ec_api_version -O ReleaseFast -fstrip `
        --export=ec_api_version --export=ec_alloc --export=ec_render `
        --export=ec_manifest_ptr --export=ec_manifest_len --max-memory=1073741824 `
        dist/obj/plugin.o '-femit-bin=dist/Fs-ColorKey.wasm'
    if ($LASTEXITCODE -ne 0) {
        throw "Linking failed"
    }
}
finally {
    Pop-Location
}
