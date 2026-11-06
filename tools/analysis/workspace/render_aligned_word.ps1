$ErrorActionPreference = 'Stop'
$taskDocPath = 'D:\lwx\lwx-project\AD7682\output\reports\Word曲线横轴对齐_20261023\10.22晚-10.23早-2板数据曲线_横坐标对齐.docx'
$taskRenderDir = 'D:\lwx\lwx-project\AD7682\tools\analysis\workspace\word_aligned_render'
New-Item -ItemType Directory -Path $taskRenderDir -Force | Out-Null
$taskWord = New-Object -ComObject Word.Application
$taskWord.Visible = $false
$taskWord.DisplayAlerts = 0
$taskDocument = $null
try {
    $taskDocument = $taskWord.Documents.Open($taskDocPath, $false, $true)
    $taskDocument.Repaginate()
    $taskDocument.ExportAsFixedFormat((Join-Path $taskRenderDir 'aligned.pdf'), 17)
    'Pages: ' + $taskDocument.ComputeStatistics(2)
} finally {
    if ($null -ne $taskDocument) { $taskDocument.Close(0) }
    $taskWord.Quit()
}
& 'C:\Users\嵌入式软件实习生电脑02\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\poppler\Library\bin\pdftoppm.exe' -scale-to 1600 -png (Join-Path $taskRenderDir 'aligned.pdf') (Join-Path $taskRenderDir 'page')
if ($LASTEXITCODE -ne 0) { throw 'PDF page rendering failed' }
