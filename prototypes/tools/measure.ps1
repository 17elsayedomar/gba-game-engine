# =============================================================================
# measure.ps1 - measures both editor prototypes on THIS computer.
#
# Usage (from the repository root, in PowerShell):
#   powershell -ExecutionPolicy Bypass -File prototypes\tools\measure.ps1
#   ... -SkipBuild    reuse build\prototypes instead of timing clean builds
#
# What it measures (results also saved to build\measurements.json):
#   * clean build time of each prototype (in its own fresh build folder)
#   * rebuild time after touching one UI source file
#   * distribution size: the files a user would need to run the program
#   * startup time: launch -> first complete frame on screen (median of 5 runs)
#   * idle CPU and memory with the sample scene open
#   * CPU and memory while an object is dragged 60 times per second
#
# Note: the first launch of a freshly built .exe is slowed down by antivirus
# scanning, so every timing below starts with one discarded warm-up run.
# =============================================================================
param(
    [switch]$SkipBuild,
    [string]$QtPrefix = "C:/Qt/6.8.3/msvc2022_64"
)
# "Continue": both programs print log lines to stderr, which Windows PowerShell
# would otherwise treat as a fatal error.
$ErrorActionPreference = "Continue"
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
Set-Location $root
$scene = Join-Path $root "prototypes\sample_scene.json"
$results = [ordered]@{ machine = [ordered]@{}; imgui = [ordered]@{}; qt = [ordered]@{}; qt_opengl = [ordered]@{} }

# --- About this computer -------------------------------------------------------
$cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
$results.machine.cpu = $cpu.Name.Trim()
$results.machine.cores = $cpu.NumberOfCores
$results.machine.logical_processors = [Environment]::ProcessorCount
$results.machine.ram_gb = [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 1)
$gpu = Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name
$results.machine.gpu = ($gpu -join "; ")
$results.machine.os = (Get-CimInstance Win32_OperatingSystem).Caption
Write-Host "Machine: $($results.machine.cpu), $($results.machine.ram_gb) GB RAM, $($results.machine.gpu)"

# --- Build times -----------------------------------------------------------------
function Measure-Build($name, $imgui, $qt, $touchFile, $target) {
    $dir = "build\measure-$name"
    if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
    $configure = Measure-Command {
        cmake -S prototypes -B $dir -DPROTO_BUILD_IMGUI=$imgui -DPROTO_BUILD_QT=$qt "-DCMAKE_PREFIX_PATH=$QtPrefix" | Out-Null
    }
    $build = Measure-Command { cmake --build $dir --config Release --target $target | Out-Null }
    (Get-Item $touchFile).LastWriteTime = Get-Date
    $rebuild = Measure-Command { cmake --build $dir --config Release --target $target | Out-Null }
    return @{ configure_s = [math]::Round($configure.TotalSeconds, 1); clean_build_s = [math]::Round($build.TotalSeconds, 1);
              touch_rebuild_s = [math]::Round($rebuild.TotalSeconds, 1); dir = $dir }
}

if (-not $SkipBuild) {
    Write-Host "Timing clean builds (this takes a few minutes)..."
    $b = Measure-Build "imgui" "ON" "OFF" "prototypes\imgui_editor\editor_app.cpp" "imgui_editor"
    $results.imgui.build = $b
    $b = Measure-Build "qt" "OFF" "ON" "prototypes\qt_editor\inspector_panel.cpp" "qt_editor"
    $results.qt.build = $b
    $imguiDir = "build\measure-imgui\imgui_editor\Release"
    $qtDir = "build\measure-qt\qt_editor\Release"
} else {
    $imguiDir = "build\prototypes\imgui_editor\Release"
    $qtDir = "build\prototypes\qt_editor\Release"
}
$imguiExe = Join-Path $root "$imguiDir\imgui_editor.exe"
$qtExe = Join-Path $root "$qtDir\qt_editor.exe"

# --- Distribution size -------------------------------------------------------------
function Get-FolderSizeMB($dir) {
    $files = Get-ChildItem -Recurse -File $dir | Where-Object { $_.Extension -notin ".pdb", ".lib", ".exp" }
    return @{ mb = [math]::Round(($files | Measure-Object Length -Sum).Sum / 1MB, 2); files = $files.Count }
}
$results.imgui.exe_mb = [math]::Round((Get-Item $imguiExe).Length / 1MB, 2)
$results.imgui.dist = Get-FolderSizeMB $imguiDir
$results.qt.exe_mb = [math]::Round((Get-Item $qtExe).Length / 1MB, 2)
$results.qt.dist = Get-FolderSizeMB $qtDir

# --- Startup time ---------------------------------------------------------------------
function Measure-Startup($exe, $extraArgs) {
    $times = @()
    for ($i = 0; $i -lt 6; $i++) {
        $out = & $exe --open $scene --startup-probe @extraArgs 2>$null | Out-String
        if ($out -match "startup_ms=([\d.]+)") { if ($i -gt 0) { $times += [double]$Matches[1] } }
        Start-Sleep -Milliseconds 500
    }
    $sorted = $times | Sort-Object
    return @{ median_ms = $sorted[[int][math]::Floor($sorted.Count / 2)]; min_ms = $sorted[0]; max_ms = $sorted[-1]; runs = $times.Count }
}
Write-Host "Timing startup..."
$results.imgui.startup = Measure-Startup $imguiExe @()
$results.qt.startup = Measure-Startup $qtExe @()
$results.qt_opengl.startup = Measure-Startup $qtExe @("--opengl")

# --- Idle and interaction CPU / memory ---------------------------------------------------
# CPU is reported as "% of one CPU core": 100 means one core fully busy.
function Measure-Run($exe, $argList, $settleSeconds, $sampleSeconds) {
    $p = Start-Process $exe -ArgumentList $argList -PassThru
    Start-Sleep -Seconds $settleSeconds
    $p.Refresh(); $cpu0 = $p.TotalProcessorTime.TotalSeconds; $t0 = Get-Date
    Start-Sleep -Seconds $sampleSeconds
    $p.Refresh(); $cpu1 = $p.TotalProcessorTime.TotalSeconds; $t1 = Get-Date
    $r = @{ cpu_percent_of_one_core = [math]::Round(100 * ($cpu1 - $cpu0) / ($t1 - $t0).TotalSeconds, 1);
            working_set_mb = [math]::Round($p.WorkingSet64 / 1MB, 1);
            private_mb = [math]::Round($p.PrivateMemorySize64 / 1MB, 1);
            peak_working_set_mb = [math]::Round($p.PeakWorkingSet64 / 1MB, 1) }
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
    Start-Sleep -Seconds 1
    return $r
}
Write-Host "Measuring idle CPU and memory (about 15 s each)..."
$results.imgui.idle = Measure-Run $imguiExe @("--open", "`"$scene`"") 5 10
$results.qt.idle = Measure-Run $qtExe @("--open", "`"$scene`"") 5 10
$results.qt_opengl.idle = Measure-Run $qtExe @("--open", "`"$scene`"", "--opengl") 5 10
Write-Host "Measuring CPU while dragging an object (about 12 s each)..."
$results.imgui.drag = Measure-Run $imguiExe @("--open", "`"$scene`"", "--benchmark-drag", "30") 3 8
$results.qt.drag = Measure-Run $qtExe @("--open", "`"$scene`"", "--benchmark-drag", "30") 3 8
$results.qt_opengl.drag = Measure-Run $qtExe @("--open", "`"$scene`"", "--benchmark-drag", "30", "--opengl") 3 8

# --- Lines of code ---------------------------------------------------------------------------
$results.lines_of_code = (python prototypes\tools\count_lines.py | Out-String | ConvertFrom-Json)

New-Item -ItemType Directory -Force build | Out-Null
$results | ConvertTo-Json -Depth 6 | Set-Content -Encoding ascii build\measurements.json
$results | ConvertTo-Json -Depth 6
Write-Host "Saved build\measurements.json"
