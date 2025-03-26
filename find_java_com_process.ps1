# PowerShell script to find Java processes with COM port handles
# Requires handle.exe from Sysinternals suite: https://docs.microsoft.com/en-us/sysinternals/downloads/handle

param(
    [string]$ComPort = "",  # Optional specific COM port to search for
    [switch]$ListAll = $false # List all available COM ports and processes
)

# Check if handle.exe is available
$handlePath = "handle.exe"
$handleFound = $false

# Check if handle.exe exists in current directory or PATH
if (Get-Command $handlePath -ErrorAction SilentlyContinue) {
    $handleFound = $true
} elseif (Test-Path $handlePath) {
    $handleFound = $true
    $handlePath = ".\handle.exe"
} else {
    $sysinternalsPath = "C:\Program Files\Sysinternals"
    if (Test-Path "$sysinternalsPath\handle.exe") {
        $handlePath = "$sysinternalsPath\handle.exe"
        $handleFound = $true
    }
}

if (-not $handleFound) {
    Write-Host "Error: handle.exe not found. Please download it from:" -ForegroundColor Red
    Write-Host "https://docs.microsoft.com/en-us/sysinternals/downloads/handle" -ForegroundColor Yellow
    Write-Host "Place it in this directory or add it to your PATH." -ForegroundColor Yellow
    exit 1
}

# Accept EULA if needed
& $handlePath -accepteula -nobanner 2>$null

# Check for active COM ports if listing all or no specific port was provided
if ($ListAll -or [string]::IsNullOrEmpty($ComPort)) {
    Write-Host "Checking for active COM ports..." -ForegroundColor Cyan
    
    $allComPortsOutput = & $handlePath -a "COM"
    $uniqueComPorts = @{}
    
    foreach ($line in $allComPortsOutput) {
        if ($line -match "COM\d+") {
            $comPort = $Matches[0]
            if (-not $uniqueComPorts.ContainsKey($comPort)) {
                $uniqueComPorts[$comPort] = $true
            }
        }
    }
    
    if ($uniqueComPorts.Count -eq 0) {
        # Try NT device name format
        $allComPortsOutput = & $handlePath -a "\Device\Serial"
        foreach ($line in $allComPortsOutput) {
            if ($line -match "\\Device\\Serial\d+") {
                $comPort = $Matches[0]
                if (-not $uniqueComPorts.ContainsKey($comPort)) {
                    $uniqueComPorts[$comPort] = $true
                }
            }
        }
    }
    
    Write-Host "Found active COM ports:" -ForegroundColor Green
    foreach ($port in $uniqueComPorts.Keys) {
        Write-Host "  $port" -ForegroundColor White
    }
    Write-Host ""
}

# Define search patterns
$searchPatterns = @()

if ([string]::IsNullOrEmpty($ComPort)) {
    # If no specific COM port, search for both formats
    $searchPatterns += "COM"
    $searchPatterns += "\Device\Serial"
} else {
    # If specific port, use it directly
    if ($ComPort -match "^COM\d+$") {
        $searchPatterns += $ComPort
    } else {
        $searchPatterns += "COM$ComPort"
    }
    
    if ($ComPort -match "^\d+$") {
        $searchPatterns += "\Device\Serial$ComPort"
    } else {
        $searchPatterns += $ComPort
    }
}

# Find Java processes with COM port handles
Write-Host "Searching for Java processes with COM port handles..." -ForegroundColor Cyan

$results = @()

foreach ($pattern in $searchPatterns) {
    Write-Host "Searching for pattern: $pattern" -ForegroundColor DarkGray
    $handleOutput = & $handlePath -a $pattern
    
    foreach ($line in $handleOutput) {
        if (($line -match "java(w)?\.exe\s+pid:\s+(\d+)") -or 
            ($line -match "tomcat\d*\.exe\s+pid:\s+(\d+)")) {
            $processName = $Matches[0]
            $processPID = $Matches[$Matches.Count - 1] # Last group should be the PID
            
            # Now find the exact handle line for this process
            $processLines = $handleOutput | Where-Object { $_ -match "^\s*$processName" }
            $handleLines = $handleOutput | Where-Object { $_ -match "^\s+[0-9A-F]+:" -and ($_ -match "COM\d+" -or $_ -match "\\Device\\Serial\d+") }
            
            foreach ($handleLine in $handleLines) {
                if (($handleLine -match "COM(\d+)") -or 
                    ($handleLine -match "\\Device\\Serial(\d+)")) {
                    $comNumber = $Matches[1]
                    $comIdentifier = if ($handleLine -match "COM\d+") { "COM$comNumber" } else { "\Device\Serial$comNumber" }
                    
                    $result = [PSCustomObject]@{
                        ProcessName = $processName
                        PID = $processPID
                        COMPort = $comIdentifier
                        HandleLine = $handleLine.Trim()
                    }
                    
                    $results += $result
                }
            }
        }
    }
}

# Display results
if ($results.Count -gt 0) {
    Write-Host "Found Java processes with COM port handles:" -ForegroundColor Green
    $uniqueProcesses = $results | Select-Object ProcessName, PID, COMPort -Unique
    
    foreach ($process in $uniqueProcesses) {
        Write-Host "Process: $($process.ProcessName), PID: $($process.PID), Port: $($process.COMPort)" -ForegroundColor White
    }
    
    # If we found multiple processes, ask which to target
    if ($uniqueProcesses.Count -gt 1) {
        Write-Host "`nMultiple Java processes found. Which one would you like to target?" -ForegroundColor Yellow
        for ($i = 0; $i -lt $uniqueProcesses.Count; $i++) {
            Write-Host "  $($i+1): $($uniqueProcesses[$i].ProcessName) (PID: $($uniqueProcesses[$i].PID), Port: $($uniqueProcesses[$i].COMPort))" -ForegroundColor White
        }
        
        $selection = Read-Host "Enter selection (1-$($uniqueProcesses.Count))"
        if ([int]::TryParse($selection, [ref]$null)) {
            $selectedIndex = [int]$selection - 1
            if ($selectedIndex -ge 0 -and $selectedIndex -lt $uniqueProcesses.Count) {
                $selectedProcess = $uniqueProcesses[$selectedIndex]
                Write-Host "`nSelected process: $($selectedProcess.ProcessName) (PID: $($selectedProcess.PID))" -ForegroundColor Green
                return $selectedProcess.PID
            }
        }
    } else {
        # Just one process found, return its PID
        Write-Host "`nTarget process: $($uniqueProcesses[0].ProcessName) (PID: $($uniqueProcesses[0].PID))" -ForegroundColor Green
        return $uniqueProcesses[0].PID
    }
} else {
    Write-Host "No Java processes with COM port handles found." -ForegroundColor Yellow
    Write-Host "Make sure your application is running and actually using COM ports." -ForegroundColor Yellow
    return $null
} 