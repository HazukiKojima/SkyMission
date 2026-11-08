param(
    [Parameter(Mandatory)][string]$InputDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [ValidateRange(1, 10)][int]$ProcessRuns = 3
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$rows = @()
for ($run = 1; $run -le $ProcessRuns; ++$run) {
    $source = Join-Path $InputDirectory "native-$run.csv"
    $rows += @(Import-Csv -LiteralPath $source)
    Copy-Item -LiteralPath $source -Destination $OutputDirectory
    Copy-Item -LiteralPath (Join-Path $InputDirectory "native-$run-environment.txt") -Destination $OutputDirectory
}
$summary = $rows | Group-Object workload,dataset,parser | ForEach-Object {
    $values = @($_.Group | ForEach-Object { [double]::Parse($_.ns_per_operation, [Globalization.CultureInfo]::InvariantCulture) } | Sort-Object)
    if ($values.Count -ne 7 * $ProcessRuns) { throw "Unexpected sample count for $($_.Name)" }
    $first = $_.Group[0]
    [pscustomobject]@{
        Workload=$first.workload; Dataset=$first.dataset; Parser=$first.parser; Samples=$values.Count
        MedianUs=$values[[int][Math]::Floor($values.Count / 2)] / 1000
        P10Us=$values[[int][Math]::Floor(($values.Count - 1) * 0.1)] / 1000
        P90Us=$values[[int][Math]::Floor(($values.Count - 1) * 0.9)] / 1000
        Utf8Bytes=$first.utf8_bytes; Documents=$first.documents
    }
}
$summary | Sort-Object Workload,Dataset,Parser | Export-Csv (Join-Path $OutputDirectory 'native-summary.csv') -NoTypeInformation -Encoding utf8
$manifest = foreach ($line in Get-Content -LiteralPath (Join-Path $InputDirectory 'datasets.tsv')) {
    $parts = $line -split "`t"
    [pscustomobject]@{Dataset=$parts[0];Index=[IO.Path]::GetFileNameWithoutExtension($parts[1]);
        Utf8Bytes=(Get-Item -LiteralPath $parts[1]).Length;SHA256=(Get-FileHash -LiteralPath $parts[1] -Algorithm SHA256).Hash}
}
$manifest | Export-Csv (Join-Path $OutputDirectory 'input-hashes.csv') -NoTypeInformation -Encoding utf8
foreach ($name in @('jit-1.csv','jit-1-environment.txt','processor-brand.txt')) {
    $path = Join-Path $InputDirectory $name
    if (Test-Path -LiteralPath $path) { Copy-Item -LiteralPath $path -Destination $OutputDirectory }
}
$parsers = @('REDox-current','REDox-utf8','STJ-current','STJ-utf8','nlohmann-json')
$table = @('| 処理 / データ | REDox現行 | REDox UTF-8 | STJ同一変換経路 | STJ UTF-8 | nlohmann |',
    '| --- | ---: | ---: | ---: | ---: | ---: |')
foreach ($group in ($summary | Group-Object Workload,Dataset)) {
    $first = $group.Group[0]
    $values = foreach ($parser in $parsers) {
        $item = $group.Group | Where-Object Parser -eq $parser
        $item.MedianUs.ToString('F3', [Globalization.CultureInfo]::InvariantCulture)
    }
    $table += '| ' + $first.Workload + ' / ' + $first.Dataset + ' | ' + ($values -join ' | ') + ' |'
}
$table | Set-Content -LiteralPath (Join-Path $OutputDirectory 'table.md') -Encoding utf8
Write-Output ($table -join "`n")
