<#
.SYNOPSIS
    PixoBook recursive self-improvement scanner.
    Detects known bug patterns discovered during manual codebase audit.
    Zero external dependencies — pure PowerShell + Select-String.

.DESCRIPTION
    Each check encodes a real bug pattern found in this codebase.
    When a NEW bug type is discovered, add a new check block below.
    This is the "recursive" part: the scanner improves itself over time.

    Output format per check:
        [PASS]  CheckName — no issues
        [FAIL]  CheckName — issue at path:line
        [SKIP]  CheckName — reason

    Exit code: 0 = all pass, 1 = one or more FAIL.

.NOTES
    Add new checks by following the pattern:
        $results += [PSCustomObject]@{
            Check   = "your-check-name"
            Status  = "PASS" / "FAIL" / "SKIP"
            Details = "path:line — description"
        }
#>

param(
    [string]$SrcDir = (Join-Path $PSScriptRoot "..\src"),
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"
$results = @()

# ── Helper ──
function Search-Files {
    param([string]$Pattern, [string]$Path, [string]$Include = "*.cpp,*.h")
    $includes = $Include -split ','
    $matches = @()
    foreach ($inc in $includes) {
        $matches += Get-ChildItem -Path $Path -Recurse -Filter $inc.Trim() |
            Select-String -Pattern $Pattern |
            ForEach-Object { "$($_.Filename):$($_.LineNumber)" }
    }
    return $matches
}

function Check-Pattern {
    param([string]$Name, [string]$Pattern, [string]$Path, [string]$Include = "*.cpp,*.h", [string]$FailMsg = "")
    $matches = Search-Files -Pattern $Pattern -Path $Path -Include $Include
    if ($matches.Count -eq 0) {
        $results += [PSCustomObject]@{ Check = $Name; Status = "PASS"; Details = "" }
    } else {
        $results += [PSCustomObject]@{
            Check   = $Name
            Status  = "FAIL"
            Details = ($matches | ForEach-Object { "$_ $FailMsg" }) -join "; "
        }
    }
}

# ====================================================================
# CHECK 1: Memory leak — destructor with raw pointer not deleted
# ====================================================================
$leaks = @()
Get-ChildItem $SrcDir -Recurse -Filter "*.cpp" | Select-String -Pattern "delete\s+m_" | ForEach-Object {
    $delFile = $_.Filename
    $delLine = $_.LineNumber
    $delMember = [regex]::Match($_.Line, "delete\s+(m_\w+)").Groups[1].Value
    # Find the corresponding header and check for raw pointer member
    $hFile = $_.Path -replace '\.cpp$', '.h'
    if (Test-Path $hFile) {
        $hasPtr = Select-String -Path $hFile -Pattern "^\s+\S+\s+\*?\s*$delMember\s*;" -SimpleMatch
        if (-not $hasPtr) {
            # This delete might be in the right place — skip; flag only missing deletes
        }
    }
}
# Check for classes with raw pointer members but no matching delete in destructor
Get-ChildItem $SrcDir -Recurse -Filter "*.h" | Select-String -Pattern "class\s+\w+\s*:" | ForEach-Object {
    $classLine = $_.LineNumber
    $className = [regex]::Match($_.Line, "class\s+(\w+)\s*:").Groups[1].Value
    $hFile = $_.Path
    $cppFile = $hFile -replace '\.h$', '.cpp'
    if (-not (Test-Path $cppFile)) { return }

    # Find raw pointer members (type followed by * followed by m_)
    $rawPtrs = Select-String -Path $hFile -Pattern "\w+\s+\*\s*m_\w+\s*;" | ForEach-Object {
        [regex]::Match($_.Line, "(m_\w+)").Groups[1].Value
    }
    if ($rawPtrs.Count -eq 0) { return }

    # Check if destructor exists and deletes these members
    $hContent = Get-Content $hFile -Raw
    $isQObject = $hContent -match '\bQ_OBJECT\b'
    $cppContent = Get-Content $cppFile -Raw
    $hasDestructor = $cppContent -match "~$className\s*\(\s*\)"
    $hasDelete = $cppContent -match '\bdelete\s+m_'
    $isDefaultDtor = $cppContent -match "~\w+\s*\(\s*\)\s*=\s*default"
    if ($hasDestructor -and -not $hasDelete -and $rawPtrs.Count -gt 0) {
        # QObject with default destructor: pointers are injected dependencies, not owned
        if ($isQObject -and $isDefaultDtor) { return }
        if ($isQObject) {
            # For QObject classes, only flag non-Qt-style raw pointers
            # QObject children are auto-deleted; check if member is allocated in this class
            $nonQObjectPtrs = $rawPtrs | Where-Object {
                $memberName = $_
                $memberLine = Select-String -Path $hFile -Pattern "\b$_\b" | Select-Object -First 1
                $typeName = if ($memberLine) { [regex]::Match($memberLine.Line, '(\w+)\s+\*\s*' + [regex]::Escape($memberName)).Groups[1].Value }
                # Check if this type is ever new'd in the cpp file (if not, it's injected)
                $isAllocated = $cppContent -match "new\s+$typeName\b"
                $isQObjectType = $memberLine.Line -match '\bQ\w+\s+\*|DatabaseManager\s+\*'
                -not $isQObjectType -and $isAllocated
            }
            if ($nonQObjectPtrs.Count -gt 0) {
                $leaks += "$($hFile) — class $className (QObject) has non-child raw pointer members ($($nonQObjectPtrs -join ', ')) that may need explicit deletion"
            }
        } else {
            $leaks += "$($hFile) — class $className has raw pointer members ($($rawPtrs -join ', ')) but destructor doesn't delete them"
        }
    }
}
if ($leaks.Count -eq 0) {
    $results += [PSCustomObject]@{ Check = "destructor-raw-pointer-leak"; Status = "PASS"; Details = "" }
} else {
    $results += [PSCustomObject]@{ Check = "destructor-raw-pointer-leak"; Status = "FAIL"; Details = ($leaks -join "; ") }
}

# ====================================================================
# CHECK 2: Integer overflow in multiplication (pixmapBytes pattern)
# ====================================================================
$overflowRaw = Search-Files -Pattern "\(\s*(int|quint32|qint32)\s*\)\s*\([^)]+\*[^)]+\)" -Path $SrcDir
# Exclude known false positives (float/double casts, not integer overflow)
$overflow = @()
foreach ($m in $overflowRaw) {
    $fullPath = Get-ChildItem $SrcDir -Recurse -Filter ($m -split ':')[0] | Select-Object -First 1 -ExpandProperty FullName
    $line = (Get-Content $fullPath | Select-Object -Index (([int](($m -split ':')[1])) - 1))
    if ($line -notmatch 'm_zoom|_zoom\b|double|float') {
        $overflow += $m
    }
}
if ($overflow.Count -eq 0) {
    $results += [PSCustomObject]@{ Check = "integer-overflow-multiplication"; Status = "PASS"; Details = "" }
} else {
    $results += [PSCustomObject]@{ Check = "integer-overflow-multiplication"; Status = "FAIL"; Details = ($overflow -join "; ") }
}

# ====================================================================
# CHECK 3: SQL string concatenation / injection risk
# ====================================================================
$sqlInjection = Search-Files -Pattern 'QString\s*(sql|query|stmt).*\+' -Path $SrcDir -Include "*.cpp"
# Refine: only flag if the concatenation is part of SQL building
$sqlRisks = @()
foreach ($m in $sqlInjection) {
    $file = ($m -split ':')[0]
    $line = ($m -split ':')[1]
    $content = Get-ChildItem $SrcDir -Recurse -Filter $file | Select-String -Pattern $line | Select-Object -First 1
    if ($content -and $content.Line -match 'SELECT|INSERT|UPDATE|DELETE|PRAGMA') {
        $sqlRisks += $m
    }
}
if ($sqlRisks.Count -eq 0) {
    $results += [PSCustomObject]@{ Check = "sql-injection-concatenation"; Status = "PASS"; Details = "" }
} else {
    $results += [PSCustomObject]@{ Check = "sql-injection-concatenation"; Status = "FAIL"; Details = ($sqlRisks -join "; ") }
}

# ====================================================================
# CHECK 4: Dead code — declared but unused signal/slot
# ====================================================================
# Check for slots that exist in header but are never connected
$deadSlots = @()
Get-ChildItem $SrcDir -Recurse -Filter "*.h" | Select-String -Pattern "void\s+\w+\([^)]*\)\s*;\s*$" | ForEach-Object {
    $slotName = [regex]::Match($_.Line, "void\s+(\w+)\(").Groups[1].Value
    if (-not $slotName -or $slotName -match '^~' -or $slotName -eq 'paintEvent' -or $slotName -eq 'wheelEvent' -or
        $slotName -eq 'mousePressEvent' -or $slotName -eq 'mouseMoveEvent' -or $slotName -eq 'mouseReleaseEvent' -or
        $slotName -eq 'keyPressEvent' -or $slotName -eq 'resizeEvent' -or $slotName -eq 'hideEvent' -or
        $slotName -eq 'dragEnterEvent' -or $slotName -eq 'dragMoveEvent' -or $slotName -eq 'dragLeaveEvent' -or
        $slotName -eq 'dropEvent') {
        return
    }
    $hFile = $_.Path
    $cppFile = $hFile -replace '\.h$', '.cpp'
    if (-not (Test-Path $cppFile)) {
        # Interface/header-only class — dead code check not applicable
        return
    }
        $connectCount = (Select-String -Path $cppFile -Pattern "onThumbnailReady" -SimpleMatch).Count
        $directCallCount = (Select-String -Path $cppFile -Pattern "\b$slotName\b").Count
        $ownRefs = (Select-String -Path $cppFile -Pattern "$slotName" -SimpleMatch).Count
        # Also check ALL source files for references (handles interface signals emitted from concrete classes)
        $allRefs = 0
        Get-ChildItem $SrcDir -Recurse -Filter "*.cpp" | ForEach-Object {
            $allRefs += (Select-String -Path $_.FullName -Pattern "$slotName" -SimpleMatch).Count
        }
        # Signal is definitely dead if neither the matching .cpp NOR any other .cpp references it
        if ($ownRefs -eq 0 -and $allRefs -eq 0) {
            # Check if it's a virtual override
            $deadSlots += "$hFile — $slotName declared but never referenced in $cppFile"
        }
}
if ($deadSlots.Count -eq 0) {
    $results += [PSCustomObject]@{ Check = "dead-signal-slot"; Status = "PASS"; Details = "" }
} else {
    $results += [PSCustomObject]@{ Check = "dead-signal-slot"; Status = "FAIL"; Details = ($deadSlots -join "; ") }
}

# ====================================================================
# CHECK 5: QtConcurrent::run with potential dangling 'this'
# ====================================================================
$dangling = Search-Files -Pattern "QtConcurrent::run" -Path $SrcDir -Include "*.cpp"
$danglingIssues = @()
foreach ($m in $dangling) {
    $file = ($m -split ':')[0]
    $lineNum = [int](($m -split ':')[1])
    $fullPath = Get-ChildItem $SrcDir -Recurse -Filter $file | Select-Object -First 1 -ExpandProperty FullName
    $content = Get-Content $fullPath
    $start = [Math]::Max(0, $lineNum - 1)
    $end = [Math]::Min($content.Count, $lineNum)
    $context = $content[$start..$end] -join "`n"
    if ($context -match '\[this\]') {
        $danglingIssues += "$($m) — lambda captures [this] in QtConcurrent::run (use QPointer guard)"
    }
}
if ($danglingIssues.Count -eq 0) {
    $results += [PSCustomObject]@{ Check = "qtconcurrent-dangling-this"; Status = "PASS"; Details = "" }
} else {
    $results += [PSCustomObject]@{ Check = "qtconcurrent-dangling-this"; Status = "FAIL"; Details = ($danglingIssues -join "; ") }
}

# ====================================================================
# CHECK 6: qUncompress without isEmpty guard
# ====================================================================
$uncompress = Search-Files -Pattern "qUncompress" -Path $SrcDir -Include "*.cpp"
$unguarded = @()
foreach ($m in $uncompress) {
    $file = ($m -split ':')[0]
    $lineNum = [int](($m -split ':')[1])
    $fullPath = Get-ChildItem $SrcDir -Recurse -Filter $file | Select-Object -First 1 -ExpandProperty FullName
    $content = Get-Content $fullPath
    $start = [Math]::Max(0, $lineNum - 2)
    $end = [Math]::Min($content.Count, $lineNum + 2)
    $context = $content[$start..$end] -join "`n"
    # Check for isEmpty guard in context
    $hasGuard = $context -match "isEmpty|isNull|size\(\)\s*==\s*0|length\s*==\s*0"
    if (-not $hasGuard) {
        $unguarded += "$($m) — qUncompress result not checked for empty"
    }
}
if ($unguarded.Count -eq 0) {
    $results += [PSCustomObject]@{ Check = "quncompress-empty-guard"; Status = "PASS"; Details = "" }
} else {
    $results += [PSCustomObject]@{ Check = "quncompress-empty-guard"; Status = "FAIL"; Details = ($unguarded -join "; ") }
}

# ====================================================================
# CHECK 7: Unchecked PRAGMA / exec() return values
# ====================================================================
$uncheckedExec = Search-Files -Pattern '^\s+(db\.|database\.|m_db\.|query\.)exec\(' -Path $SrcDir -Include "*.cpp"
$uncheckedIssues = @()
foreach ($m in $uncheckedExec) {
    $file = ($m -split ':')[0]
    $lineNum = [int](($m -split ':')[1])
    $fullPath = Get-ChildItem $SrcDir -Recurse -Filter $file | Select-Object -First 1 -ExpandProperty FullName
    $content = Get-Content $fullPath
    $start = [Math]::Max(0, $lineNum - 1)
    $end = [Math]::Min($content.Count, $lineNum)
    $context = $content[$start..$end] -join "`n"
    # Check if exec result is assigned or checked
    $hasCheck = $context -match "if\s*\(" -or $context -match "=\s*\w+\.exec" -or $context -match "bool\s+\w+\s*="
    if (-not $hasCheck) {
        $uncheckedIssues += "$($m) — exec() return value not checked (may mask failure)"
    }
}
if ($uncheckedIssues.Count -eq 0) {
    $results += [PSCustomObject]@{ Check = "unchecked-exec-return"; Status = "PASS"; Details = "" }
} else {
    $results += [PSCustomObject]@{ Check = "unchecked-exec-return"; Status = "FAIL"; Details = ($uncheckedIssues -join "; ") }
}

# ====================================================================
# CHECK 8: setScaledSize without KeepAspectRatio
# ====================================================================
$scaled = Search-Files -Pattern "setScaledSize" -Path $SrcDir -Include "*.cpp"
$aspectIssues = @()
foreach ($m in $scaled) {
    $file = ($m -split ':')[0]
    $lineNum = [int](($m -split ':')[1])
    $fullPath = Get-ChildItem $SrcDir -Recurse -Filter $file | Select-Object -First 1 -ExpandProperty FullName
    $content = Get-Content $fullPath
    $start = [Math]::Max(0, $lineNum - 3)
    $end = [Math]::Min($content.Count, $lineNum + 3)
    $context = $content[$start..$end] -join "`n"
    # Check if KeepAspectRatio is used nearby
    $hasKeep = $context -match "KeepAspectRatio"
    $hasIgnore = $context -match "IgnoreAspectRatio"
    if ($hasIgnore -and -not $hasKeep) {
        $aspectIssues += "$($m) — setScaledSize uses IgnoreAspectRatio (likely stretching)"
    }
}
if ($aspectIssues.Count -eq 0) {
    $results += [PSCustomObject]@{ Check = "setscaledsize-aspect-ratio"; Status = "PASS"; Details = "" }
} else {
    $results += [PSCustomObject]@{ Check = "setscaledsize-aspect-ratio"; Status = "FAIL"; Details = ($aspectIssues -join "; ") }
}

# ====================================================================
# CHECK 9: clear() without resetting accumulators (m_currentBytes pattern)
# ====================================================================
$clears = Search-Files -Pattern "void\s+\w+::clear\s*\(\s*\)" -Path $SrcDir -Include "*.cpp"
$clearIssues = @()
foreach ($m in $clears) {
    $file = ($m -split ':')[0]
    $lineNum = [int](($m -split ':')[1])
    $fullPath = Get-ChildItem $SrcDir -Recurse -Filter $file | Select-Object -First 1 -ExpandProperty FullName
    $content = Get-Content $fullPath
    # Check if clear() only clears containers but doesn't reset counters
    $className = [regex]::Match((Get-Content $fullPath | Select-Object -Index ($lineNum - 1)), 'void\s+(\w+)::').Groups[1].Value
    $hFile = Join-Path (Split-Path $fullPath -Parent) "$className.h"
    if (Test-Path $hFile) {
        $hContent = Get-Content $hFile -Raw
        $hasCounter = $hContent -match "m_currentBytes|m_currentSize|m_count|m_total\w+"
        $cppClearBlock = $content[($lineNum - 1)..($lineNum + 15)] -join "`n"
        $resetsCounter = $cppClearBlock -match "m_currentBytes\s*=" -or $cppClearBlock -match "m_currentSize\s*=" -or
                          $cppClearBlock -match "m_count\s*=" -or $cppClearBlock -match "m_total\w+\s*="
        if ($hasCounter -and -not $resetsCounter) {
            $clearIssues += "$($m) — clear() clears containers but doesn't reset accumulator"
        }
    }
}
if ($clearIssues.Count -eq 0) {
    $results += [PSCustomObject]@{ Check = "clear-accumulator-reset"; Status = "PASS"; Details = "" }
} else {
    $results += [PSCustomObject]@{ Check = "clear-accumulator-reset"; Status = "FAIL"; Details = ($clearIssues -join "; ") }
}

# ====================================================================
# CHECK 10: QPainterPath / heap allocations in paintEvent
# ====================================================================
$paintAllocs = Search-Files -Pattern "QPainterPath\b" -Path $SrcDir -Include "*.cpp"
$paintIssues = @()
foreach ($m in $paintAllocs) {
    $file = ($m -split ':')[0]
    $lineNum = [int](($m -split ':')[1])
    $fullPath = Get-ChildItem $SrcDir -Recurse -Filter $file | Select-Object -First 1 -ExpandProperty FullName
    $content = Get-Content $fullPath
    $start = [Math]::Max(0, $lineNum - 5)
    $end = [Math]::Min($content.Count, $lineNum + 5)
    $context = $content[$start..$end] -join "`n"
    # Check if this is inside paintEvent
    $isInsidePaint = ($context -match "paintEvent|paint\s*\(") -or
                     ($content[0..$lineNum] -join "`n" -match "::paintEvent")
    if ($isInsidePaint -and -not ($context -match "static\s+QPainterPath|m_\w+Path")) {
        $paintIssues += "$($m) — heap-allocated QPainterPath inside paintEvent (cache it)"
    }
}
if ($paintIssues.Count -eq 0) {
    $results += [PSCustomObject]@{ Check = "paintevent-heap-alloc"; Status = "PASS"; Details = "" }
} else {
    $results += [PSCustomObject]@{ Check = "paintevent-heap-alloc"; Status = "FAIL"; Details = ($paintIssues -join "; ") }
}

# ====================================================================
# CHECK 11: QFont created per frame in paintEvent
# ====================================================================
$fontIssues = @()
Get-ChildItem $SrcDir -Recurse -Filter "*.cpp" | Select-String -Pattern "QFont\b" | ForEach-Object {
    $lineNum = $_.LineNumber
    $fullPath = $_.Path
    $content = Get-Content $fullPath
    $start = [Math]::Max(0, $lineNum - 5)
    $end = [Math]::Min($content.Count, $lineNum + 5)
    $context = $content[$start..$end] -join "`n"
    $isInsidePaint = ($content[0..$lineNum] -join "`n" -match "::paintEvent") -or
                     ($content[0..$lineNum] -join "`n" -match "::paint\s*\(")
    $isStaticOrMember = $context -match "static\s+QFont|m_\w+Font"
    if ($isInsidePaint -and -not $isStaticOrMember) {
        $fontIssues += "$($_.Filename):$($_.LineNumber) — QFont created inside paintEvent (cache it as member)"
    }
}
if ($fontIssues.Count -eq 0) {
    $results += [PSCustomObject]@{ Check = "paintevent-font-alloc"; Status = "PASS"; Details = "" }
} else {
    $results += [PSCustomObject]@{ Check = "paintevent-font-alloc"; Status = "FAIL"; Details = ($fontIssues -join "; ") }
}

# ====================================================================
# CHECK 12: Raw QObject pointer without QPointer
# ====================================================================
$rawToast = Search-Files -Pattern "ToastNotification\s*\*" -Path $SrcDir -Include "*.cpp,*.h"
if ($rawToast.Count -eq 0) {
    $results += [PSCustomObject]@{ Check = "raw-qpointer-usage"; Status = "PASS"; Details = "" }
} else {
    $results += [PSCustomObject]@{ Check = "raw-qpointer-usage"; Status = "FAIL"; Details = ($rawToast -join "; ") }
}

# ====================================================================
# CHECK 13: Case-insensitive path comparison on Windows
# ====================================================================
$pathCompare = Search-Files -Pattern '\.startsWith\(.*[Pp]ath' -Path $SrcDir -Include "*.cpp,*.h"
$caseIssues = @()
foreach ($m in $pathCompare) {
    $file = ($m -split ':')[0]
    $line = ($m -split ':')[1]
    $fullPath = Get-ChildItem $SrcDir -Recurse -Filter $file | Select-Object -First 1 -ExpandProperty FullName
    $content = Get-Content $fullPath | Select-Object -Index ($line - 1)
    if ($content -and -not ($content -match "CaseInsensitive")) {
        $caseIssues += "$($m) — path comparison may be case-sensitive on Windows"
    }
}
if ($caseIssues.Count -eq 0) {
    $results += [PSCustomObject]@{ Check = "path-case-sensitive"; Status = "PASS"; Details = "" }
} else {
    $results += [PSCustomObject]@{ Check = "path-case-sensitive"; Status = "FAIL"; Details = ($caseIssues -join "; ") }
}

# ====================================================================
# CHECK 14: String formatting inside paintEvent (metaLine pattern)
# ====================================================================
function Get-NearestFunctionName {
    param([string[]]$Content, [int]$LineNum)
    $idx = $LineNum - 1
    while ($idx -ge 0) {
        $line = $Content[$idx]
        if ($line -match '(?:\w+::)+(\w+)\(') {
            return $Matches[1]
        } elseif ($line -match '^\s+(void|bool|int|Q\w+|Asset)\s+\w+\(') {
            if ($line -match 'paintEvent\b') { return 'paintEvent' }
            return $Matches[0]
        }
        $idx--
    }
    return $null
}

$formatIssues = @()
Get-ChildItem $SrcDir -Recurse -Filter "*.cpp" | Select-String -Pattern "\.arg\(" | ForEach-Object {
    $lineNum = $_.LineNumber
    $fullPath = $_.Path
    $content = Get-Content $fullPath
    $fnName = Get-NearestFunctionName -Content $content -LineNum $lineNum
    $isPrecomputed = ($_.Line -match "m_\w+")
    if ($fnName -eq 'paintEvent' -and -not $isPrecomputed) {
        $formatIssues += "$($_.Filename):$lineNum — string formatting inside paintEvent (pre-compute)"
    }
}
if ($formatIssues.Count -eq 0) {
    $results += [PSCustomObject]@{ Check = "paintevent-string-format"; Status = "PASS"; Details = "" }
} else {
    $results += [PSCustomObject]@{ Check = "paintevent-string-format"; Status = "FAIL"; Details = ($formatIssues -join "; ") }
}

# ====================================================================
# CHECK 15: Missing transaction rollback
# ====================================================================
$txIssues = @()
Get-ChildItem $SrcDir -Recurse -Filter "*.cpp" | Select-String -Pattern "\bbeginTransaction\s*\(" | ForEach-Object {
    $lineNum = $_.LineNumber
    $fullPath = $_.Path
    $content = Get-Content $fullPath
    $lookAhead = $content[$lineNum..[Math]::Min($content.Count-1, $lineNum+30)] -join "`n"
    $hasCommit = $lookAhead -match "commitTransaction|commit\s*\("
    $hasRollback = $lookAhead -match "rollbackTransaction|rollback\s*\("
    if ($hasCommit -and -not $hasRollback) {
        $txIssues += "$($_.Filename):$lineNum — transaction without rollback path"
    }
}
if ($txIssues.Count -eq 0) {
    $results += [PSCustomObject]@{ Check = "transaction-rollback"; Status = "PASS"; Details = "" }
} else {
    $results += [PSCustomObject]@{ Check = "transaction-rollback"; Status = "FAIL"; Details = ($txIssues -join "; ") }
}

# ====================================================================
# CHECK 16: draw* helpers not marked const
# ====================================================================
$nonConstDraws = @()
Get-ChildItem $SrcDir -Recurse -Filter "*.h" | Select-String -Pattern 'void\s+draw\w+\s*\(' | ForEach-Object {
    $lineNum = $_.LineNumber
    $fullPath = $_.Path
    $line = $_.Line
    $hContent = Get-Content $fullPath
    # Only flag draw* methods that take QPainter& as first param
    if ($line -notmatch 'QPainter\s*&') { return }
    # Check if already const — check this line and next line (multi-line sigs)
    $nextLine = if ($lineNum -lt $hContent.Count) { $hContent[$lineNum] } else { '' }
    if ($line -match 'const\s*;|const\s*$|const\s*$|const\s*\)' -or $nextLine -match '^\s+.*const\s*;') { return }
    # Exclude methods that have side effects (modify member rects/state)
    $methodName = [regex]::Match($line, 'draw\w+').Value
    if ($methodName -match 'drawBatchToolbar|drawRubberBand') { return }
    # Exclude interface/override methods
    if ($line -match '= 0|override\s*$') { return }
    # Check the matching .cpp for const on the implementation
    $cppFile = $fullPath -replace '\.h$', '.cpp'
    if (Test-Path $cppFile) {
        $cppLine = Select-String -Path $cppFile -Pattern "$methodName\s*\(" | Select-Object -First 1
        $cppNextLine = if ($cppLine) { (Get-Content $cppFile)[$cppLine.LineNumber] } else { '' }
        $isConstCpp = $cppLine.Line -match 'const\s*$|const\s*\)' -or $cppNextLine -match 'const\s*$|const\s*\)'
        if (-not $isConstCpp) {
            $nonConstDraws += "$($_.Filename):$lineNum — $methodName(QPainter&) should be const"
        }
    }
}
if ($nonConstDraws.Count -eq 0) {
    $results += [PSCustomObject]@{ Check = "draw-helper-const"; Status = "PASS"; Details = "" }
} else {
    $results += [PSCustomObject]@{ Check = "draw-helper-const"; Status = "FAIL"; Details = ($nonConstDraws -join "; ") }
}

# ====================================================================
# CHECK 17: Missing Q_DECLARE_METATYPE for types used in cross-thread signals
# ====================================================================
$metatypeIssues = @()
$checkedMetatype = @{}
Get-ChildItem $SrcDir -Recurse -Filter "*.h" | Where-Object {
    $content = Get-Content $_.FullName -Raw
    $content -match '\bsignals:\s*' -and $content -match 'Q_OBJECT'
} | ForEach-Object {
    $hFile = $_.FullName
    $lines = Get-Content $hFile
    $hasSignals = $false
    foreach ($line in $lines) {
        if ($line -match '^\s*signals\s*:' -or $line -match 'Q_SIGNALS\s*:') {
            $hasSignals = $true
            continue
        }
        if ($line -match '^\s*(public|private|protected)\s*(signals|slots)?\s*:') {
            if ($line -notmatch 'signals') { $hasSignals = $false }
            continue
        }
        if ($hasSignals -and $line -match 'void\s+\w+\s*\(([^)]*)\)') {
            $params = $Matches[1]
            $params -split ',' | ForEach-Object {
                $typeCandidate = $_.Trim() -replace '^(const\s+)?' -replace '\s+(&|\*)?\s*\w*\s*$' -replace '^const ' -replace '\s*&$' -replace '\s*\*$' -replace '^\s+|\s+$'
                $typeCandidate = $typeCandidate.Trim()
                if ($typeCandidate -match '^(int|bool|double|float|qint64|quint32|qint32|QString|QSize|QPoint|QRect|QColor|QPixmap|QImage|QByteArray|QUrl|QDate|QTime|QDateTime|QVariant|QModelIndex|QWidget\*|QObject\*)$') { return }
                if ($typeCandidate -eq '' -or $typeCandidate -match '^\s*$') { return }
                if ($checkedMetatype.ContainsKey($typeCandidate)) { return }
                $checkedMetatype[$typeCandidate] = $true

                $typeHeader = Get-ChildItem $SrcDir -Recurse -Filter "*.h" | Where-Object {
                    (Get-Content $_.FullName -Raw) -match "(struct|class)\s+$typeCandidate\b"
                } | Select-Object -First 1
                if (-not $typeHeader) { return }

                $typeContent = Get-Content $typeHeader.FullName -Raw
                if ($typeContent -notmatch "Q_DECLARE_METATYPE\s*\(\s*$typeCandidate\s*\)") {
                    $metatypeIssues += "$($typeHeader.FullName) — type '$typeCandidate' used in signal but missing Q_DECLARE_METATYPE"
                }
            }
        }
    }
}
if ($metatypeIssues.Count -eq 0) {
    $results += [PSCustomObject]@{ Check = "missing-q-declare-metatype"; Status = "PASS"; Details = "" }
} else {
    $results += [PSCustomObject]@{ Check = "missing-q-declare-metatype"; Status = "FAIL"; Details = ($metatypeIssues -join "; ") }
}

# ====================================================================
# SUMMARY
# ====================================================================
$passCount = ($results | Where-Object { $_.Status -eq "PASS" }).Count
$failCount = ($results | Where-Object { $_.Status -eq "FAIL" }).Count
$totalCount = $results.Count

Write-Output ""
Write-Output "======================================================"
Write-Output "  PixoBook Recursive Self-Improvement Scan Report"
Write-Output "======================================================"
Write-Output ""

foreach ($r in $results) {
    switch ($r.Status) {
        "PASS" { $icon = "[OK]"; $color = "Green" }
        "FAIL" { $icon = "[!!]"; $color = "Red" }
        "SKIP" { $icon = "[--]"; $color = "Yellow" }
    }
    if ($r.Details) {
        Write-Host "$icon $($r.Check)" -ForegroundColor $color
        Write-Output "       $($r.Details)"
    } else {
        Write-Host "$icon $($r.Check)" -ForegroundColor $color
    }
}

Write-Output ""
Write-Output "--- Results ---"
Write-Output "  Total: $totalCount  |  Pass: $passCount  |  Fail: $failCount"

if ($failCount -gt 0) {
    Write-Output ""
    Write-Host "  [WARN] $failCount issues found. Review before proceeding." -ForegroundColor Yellow
    Write-Output ""
}

# ── Self-improvement reminder ──
Write-Output "--- Recursive self-improvement ---"
Write-Output "  Found a new bug type? Add a new CHECK block to this script."
Write-Output "  Core idea: each discovery is encoded into automated detection."
Write-Output ""

if ($failCount -gt 0) { exit 1 } else { exit 0 }
