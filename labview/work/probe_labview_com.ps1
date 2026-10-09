try {
    $app = New-Object -ComObject 'LabVIEW.Application'
    Write-Output "COM connected; Version=$($app.Version)"
    [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($app)
} catch {
    Write-Output "COM failed: $($_.Exception.Message)"
}
