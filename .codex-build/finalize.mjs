import path from 'node:path';
import fs from 'node:fs/promises';
import { pathToFileURL } from 'node:url';
const skillDir='C:/Users/嵌入式软件实习生电脑02/.codex/plugins/cache/openai-primary-runtime/presentations/26.1015.12040/skills/presentations';
const { finalizePresentation } = await import(pathToFileURL(path.join(skillDir,'container_tools/artifact_tool_utils.mjs')).href);
const workspaceDir='D:/lwx/lwx-project/AD7682';
const candidatePath=path.join(workspaceDir,'.codex-build','candidate.pptx');
const finalPath=path.join(workspaceDir,'output','周小闹自媒体博主分析-课程汇报.pptx');
const stagingDir=path.join(workspaceDir,'.codex-finalizer');
await fs.mkdir(stagingDir,{recursive:true}); await fs.mkdir(path.dirname(finalPath),{recursive:true});
const result=await finalizePresentation({explicitTotalSlideCount:9,requiredNativeTableOwnerSlides:[],requiredNativeChartOwnerSlides:[],workspaceDir,candidatePath,finalPath,pythonExecutable:'C:/Users/嵌入式软件实习生电脑02/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe',integrityValidatorPath:path.join(skillDir,'container_tools/inspect_presentation_package_integrity.py'),layoutValidatorPath:path.join(skillDir,'container_tools/inspect_presentation_layout_geometry.py'),layoutArgs:['--expected-slide-size-emu','12192000,6858000','--validate-bullet-geometry','--validate-heading-fit'],verifyArtifactToolImport:true,receiptPath:path.join(stagingDir,'validation.json')});
console.log(JSON.stringify(result,null,2));

