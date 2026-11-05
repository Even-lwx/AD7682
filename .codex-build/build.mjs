import fs from 'node:fs/promises';
import path from 'node:path';
import { Presentation, PresentationFile } from '@oai/artifact-tool';

const workspaceDir = 'D:/lwx/lwx-project/AD7682';
const buildDir = path.join(workspaceDir, '.codex-build');
const assetDir = path.join(workspaceDir, 'assets');
const outDir = path.join(workspaceDir, 'output');
const font = 'Microsoft YaHei';
const C = { bg:'#F7F3EE', navy:'#20323F', teal:'#2A9D8F', orange:'#E76F51', yellow:'#F4C95D', ink:'#24313A', muted:'#6D777C', white:'#FFFFFF', pale:'#E9F2F0', paleOrange:'#FBE9E3', line:'#D7D5CE' };
const W=1280,H=720;
function blob(p){ return fs.readFile(p).then(b=>b.buffer.slice(b.byteOffset,b.byteOffset+b.byteLength)); }
function addText(slide, text, x,y,w,h, size=20, color=C.ink, opts={}){
  const s=slide.shapes.add({geometry:'textbox', position:{left:x,top:y,width:w,height:h}, fill:'none', line:{style:'solid',fill:'none',width:0}});
  s.text=text; s.text.style={fontFamily:font,fontSize:size,color,bold:!!opts.bold,italic:!!opts.italic,align:opts.align||'left',valign:opts.valign||'top'};
  if(opts.margin!==undefined) s.text.margin=opts.margin;
  return s;
}
function addRect(slide,x,y,w,h,fill, radius=0, lineFill=fill){
  return slide.shapes.add({geometry:radius?'roundRect':'rect', position:{left:x,top:y,width:w,height:h}, fill, line:{style:'solid',fill:lineFill,width:1}, ...(radius?{borderRadius:'rounded-xl'}:{})});
}
function addLine(slide,x1,y1,x2,y2,color=C.line,width=2){
  return slide.shapes.add({geometry:'line', position:{left:x1,top:y1,width:x2-x1,height:y2-y1}, fill:'none', line:{style:'solid',fill:color,width}});
}
function addImage(slide, file, x,y,w,h, alt, fit='cover'){
  return blob(path.join(assetDir,file)).then(bytes=>slide.images.add({blob:bytes,contentType:'image/jpeg',alt,fit,position:{left:x,top:y,width:w,height:h},geometry:'roundRect',borderRadius:'rounded-xl'}));
}
function note(slide, t){ slide.speakerNotes.text=t; slide.speakerNotes.setVisible(true); }
function footer(slide, n){ addText(slide, `课程汇报｜周小闹自媒体分析`, 56, 687, 400, 16, 11, C.muted); addText(slide, String(n).padStart(2,'0'), 1192, 681, 32, 22, 12, C.muted,{align:'right'}); }

const pres = Presentation.create({slideSize:{width:W,height:H}});

// 1 cover
{
 const s=pres.slides.add(); s.background.fill=C.bg;
 addRect(s,0,0,1280,720,C.navy);
 addRect(s,0,0,1280,14,C.orange);
 addImage(s,'zhou-01.jpg',900,64,280,500,'周小闹公开图片素材').then(()=>{});
 addImage(s,'zhou-02.jpg',725,382,180,230,'周小闹公开图片素材').then(()=>{});
 addText(s,'自媒体博主分析',72,120,560,54,28,C.yellow,{bold:true});
 addText(s,'周小闹',68,186,650,92,72,C.white,{bold:true});
 addText(s,'从“一人分饰多角”到剧情内容产品化',74,294,620,40,24,'#D9E7E6');
 addLine(s,74,376,510,376,C.orange,4);
 addText(s,'大学课程汇报｜基于公开资料与内容样本的观察',74,408,560,32,17,'#D9E7E6');
 addText(s,'关键词：剧情短视频｜角色资产｜平台扩散｜创作者启示',74,455,600,30,15,'#B7C8C8');
 addText(s,'2026.10',74,624,120,24,14,'#B7C8C8');
 note(s,'资料来源：百度百科“周小闹（娱乐类自媒体创作者）”；百度图片搜索“周小闹”。封面照片为公开网页图片素材，仅用于课堂分析展示。');
}

// 2 profile
{
 const s=pres.slides.add(); s.background.fill=C.bg; footer(s,2);
 addText(s,'周小闹：把日常冲突拍成连续剧',56,48,820,48,31,C.navy,{bold:true});
 addText(s,'公开资料显示，他以剧情演绎为主，常用一人分饰多角的方式完成短剧表达。',58,100,900,28,17,C.muted);
 addImage(s,'zhou-03.jpg',870,46,300,375,'周小闹公开图片素材');
 addRect(s,56,170,760,168,C.white,18,C.line);
 addText(s,'账号画像',82,192,160,28,18,C.orange,{bold:true});
 addText(s,'周小闹（本名周昊）',82,230,290,32,25,C.navy,{bold:true});
 addText(s,'黑龙江七台河人｜浙江工业大学毕业｜娱乐类自媒体创作者',82,270,620,26,16,C.ink);
 addText(s,'主要成就：2022年度抖音最具商业内容影响力达人；2022快手金剧奖年度新锐居星',82,304,660,26,14,C.muted);
 addRect(s,56,370,760,238,C.pale,18,C.pale);
 addText(s,'一句话定位',82,394,160,26,18,C.teal,{bold:true});
 addText(s,'用固定场景、密集对白和角色反差，\n把“打工、宿舍、饭店”等熟悉生活变成可追更的剧情内容。',82,430,640,90,28,C.navy,{bold:true});
 addText(s,'分析视角：内容产品｜叙事机制｜传播链路｜商业化信号',82,548,610,24,15,C.muted);
 note(s,'资料来源：百度百科公开词条，检索日期 2026-10-19。人物信息与奖项以公开页面为准。');
}

// 3 content product
{
 const s=pres.slides.add(); s.background.fill=C.bg; footer(s,3);
 addText(s,'内容产品：饭店寒假工',56,48,620,44,31,C.navy,{bold:true});
 addText(s,'“饭店”是低门槛、强冲突、易复用的内容场景。',58,98,760,28,18,C.orange,{bold:true});
 addImage(s,'zhou-02.jpg',54,162,240,322,'饭店寒假工相关公开图片');
 addImage(s,'zhou-04.jpg',314,162,220,322,'周小闹公开图片');
 addText(s,'固定场景',80,505,160,24,16,C.teal,{bold:true}); addText(s,'饭店 / 后厨 / 前台',80,534,190,24,18,C.navy,{bold:true});
 addText(s,'角色关系',340,505,160,24,16,C.teal,{bold:true}); addText(s,'老板、同事、顾客、寒假工',340,534,200,44,18,C.navy,{bold:true});
 addRect(s,610,162,580,322,C.white,18,C.line);
 addText(s,'系列化公式',646,192,200,26,18,C.orange,{bold:true});
 const rows=[['场景','饭店工作日常'],['冲突','甩锅、内卷、抠门、误会'],['角色','一人分饰多角，快速切换'],['结尾','一句“梗”或反转，形成记忆点']];
 rows.forEach((r,i)=>{ const y=244+i*54; addText(s,r[0],650,y,80,24,16,C.teal,{bold:true}); addLine(s,730,y+14,1126,y+14,C.line,1); addText(s,r[1],758,y,380,24,18,C.ink,{bold:i===0}); });
 addText(s,'系列价值：同一套场景可持续产出，角色与口头禅可以跨视频复用。',610,536,560,44,18,C.navy,{bold:true});
 note(s,'资料来源：百度百科“饭店寒假工”词条，公开描述称该系列聚焦大学生假期到饭店打零工的职场日常，以一人分饰多角方式演绎。图片来源：百度图片搜索“周小闹”。');
}

// 4 mechanism
{
 const s=pres.slides.add(); s.background.fill=C.bg; footer(s,4);
 addText(s,'叙事机制：一个人，撑起一组角色',56,48,760,44,31,C.navy,{bold:true});
 addText(s,'辨识度来自“表演切换速度”，而不是复杂布景。',58,98,760,28,18,C.orange,{bold:true});
 addRect(s,70,180,270,300,C.navy,24,C.navy); addText(s,'角色引擎',108,212,190,32,23,C.white,{bold:true,align:'center'});
 addText(s,'同一位创作者\n不同语气 / 表情 / 姿态\n快速切换人物关系',96,278,220,120,26,C.yellow,{bold:true,align:'center'});
 const nodes=[['老板','权力关系'],['同事','利益冲突'],['顾客','情绪触发'],['寒假工','观众代入']];
 nodes.forEach((n,i)=>{ const y=160+i*112; addRect(s,490,y,260,76,i%2?C.pale:C.paleOrange,18,i%2?C.teal:C.orange); addText(s,n[0],516,y+16,120,24,19,C.navy,{bold:true}); addText(s,n[1],516,y+44,160,20,14,C.muted); addLine(s,340,y+38,490,y+38,C.line,2); });
 addRect(s,864,180,330,300,C.white,24,C.line); addText(s,'观众为什么会追？',900,212,250,28,21,C.teal,{bold:true,align:'center'});
 addText(s,'熟悉感',906,276,100,26,20,C.orange,{bold:true}); addText(s,'“这不就是我同事吗”',906,308,230,24,17,C.ink);
 addText(s,'反差感',906,362,100,26,20,C.orange,{bold:true}); addText(s,'一人分饰多角带来惊喜',906,394,240,24,17,C.ink);
 addText(s,'连续性',906,448,100,26,20,C.orange,{bold:true}); addText(s,'角色与口头禅推动追更',906,480,240,24,17,C.ink);
 note(s,'分析为课堂内容分析框架，基于公开视频描述与系列化剧情特征归纳。');
}

// 5 spread
{
 const s=pres.slides.add(); s.background.fill=C.bg; footer(s,5);
 addText(s,'传播链路：从剧情梗到平台扩散',56,48,820,44,31,C.navy,{bold:true});
 addText(s,'内容先让人看懂，再让人记住，最后让人转述。',58,98,760,28,18,C.orange,{bold:true});
 const steps=[['01','生活场景','饭店、宿舍、打工'],['02','冲突压缩','几十秒内完成误会与反转'],['03','角色记忆','口头禅、固定人物、视觉符号'],['04','二次传播','评论模仿、切片、话题扩散']];
 steps.forEach((st,i)=>{ const x=72+i*294; addRect(s,x,200,240,190,i%2?C.pale:C.paleOrange,22,i%2?C.teal:C.orange); addText(s,st[0],x+22,220,50,24,16,i%2?C.teal:C.orange,{bold:true}); addText(s,st[1],x+22,260,190,28,23,C.navy,{bold:true}); addText(s,st[2],x+22,312,190,54,17,C.ink); if(i<3){ addLine(s,x+240,294,x+294,294,C.orange,3); addText(s,'›',x+257,275,25,38,34,C.orange,{bold:true,align:'center'}); }});
 addRect(s,72,474,1116,110,C.navy,20,C.navy); addText(s,'课堂判断',100,500,160,24,16,C.yellow,{bold:true}); addText(s,'周小闹的传播优势，来自“可理解的冲突 + 可复述的角色 + 可连续的系列”。',100,535,1000,30,24,C.white,{bold:true});
 note(s,'传播链路为课堂分析归纳，非平台官方数据。');
}

// 6 scale
{
 const s=pres.slides.add(); s.background.fill=C.bg; footer(s,6);
 addText(s,'账号规模与商业化信号',56,48,760,44,31,C.navy,{bold:true});
 addText(s,'公开报道呈现出“内容爆发—账号扩张—团队提产”的增长路径。',58,98,880,28,18,C.orange,{bold:true});
 const nums=[['2222.2万','2026年3月公开报道中的粉丝数'],['3500万','2026年10月公开报道中的主账号粉丝数'],['100+','2026年公开报道中的月视频产出量']];
 nums.forEach((n,i)=>{ const x=72+i*380; addRect(s,x,186,330,170,i===1?C.navy:C.white,20,i===1?C.navy:C.line); addText(s,n[0],x+24,218,280,48,34,i===1?C.yellow:C.orange,{bold:true}); addText(s,n[1],x+24,286,270,48,16,i===1?'#D9E7E6':C.muted); });
 addRect(s,72,410,1110,158,C.pale,20,C.pale); addText(s,'商业化观察',100,438,180,26,18,C.teal,{bold:true});
 addText(s,'奖项与粉丝规模说明商业价值已经被平台和品牌看见，但内容生产也更依赖稳定的团队协作。',100,482,980,48,23,C.navy,{bold:true});
 addText(s,'可见信号：商业内容影响力达人奖项｜多账号矩阵｜高频视频产出',100,548,940,24,15,C.muted);
 note(s,'资料来源：百度百科（2022年度抖音最具商业内容影响力达人、2022快手金剧奖年度新锐居星）；百度搜索结果中 2026-03-03、2026-10-11 的公开报道。粉丝数与产出量随时间变化，仅作为公开报道截面，不代表实时数据。');
}

// 7 strengths risks
{
 const s=pres.slides.add(); s.background.fill=C.bg; footer(s,7);
 addText(s,'优势与风险：内容越稳定，管理越重要',56,48,820,44,31,C.navy,{bold:true});
 addText(s,'同一套方法带来复利，也会带来同质化与过度消耗。',58,98,760,28,18,C.orange,{bold:true});
 addRect(s,70,180,520,350,C.pale,22,C.teal); addText(s,'优势',104,214,120,28,21,C.teal,{bold:true});
 const pros=['角色资产清晰，观众容易认出','固定场景降低制作成本','剧情可连续，适合追更','口头禅与反差感利于二次传播'];
 pros.forEach((t,i)=>{ addText(s,'✓',108,272+i*54,28,26,21,C.teal,{bold:true}); addText(s,t,148,272+i*54,380,26,18,C.navy,{bold:true}); });
 addRect(s,650,180,520,350,C.paleOrange,22,C.orange); addText(s,'风险',684,214,120,28,21,C.orange,{bold:true});
 const risks=['角色套路固化，创新压力上升','高频产出依赖团队与流程','商业合作可能稀释剧情可信度','热点变化快，内容生命周期变短'];
 risks.forEach((t,i)=>{ addText(s,'△',688,272+i*54,28,26,21,C.orange,{bold:true}); addText(s,t,728,272+i*54,380,26,18,C.navy,{bold:true}); });
 addText(s,'关键管理问题：如何让“固定角色”持续产生新冲突？',70,590,1080,30,22,C.navy,{bold:true,align:'center'});
 note(s,'优势与风险为课堂分析判断，基于公开内容特征归纳。');
}

// 8 implications
{
 const s=pres.slides.add(); s.background.fill=C.bg; footer(s,8);
 addText(s,'对大学生创作者的启示',56,48,760,44,31,C.navy,{bold:true});
 addText(s,'把“我会拍视频”拆成一套可以重复执行的内容系统。',58,98,860,28,18,C.orange,{bold:true});
 const items=[['1','先做一个可复用场景','教室、宿舍、社团、兼职都可以成为内容母题'],['2','给角色写清楚规则','口头禅、动作、利益关系比复杂造型更重要'],['3','把视频当作系列产品','设定固定片头、冲突模板和结尾记忆点'],['4','用评论反推选题','把观众提问变成下一集的剧情触发器']];
 items.forEach((it,i)=>{ const y=172+i*104; addRect(s,78,y,88,64,i%2?C.teal:C.orange,18,i%2?C.teal:C.orange); addText(s,it[0],78,y+12,88,38,28,C.white,{bold:true,align:'center'}); addText(s,it[1],204,y+2,330,30,21,C.navy,{bold:true}); addText(s,it[2],204,y+38,820,24,16,C.muted); });
 addRect(s,78,602,1060,28,C.navy,14,C.navy); addText(s,'课程落点：内容创作的核心竞争力，是把灵感变成稳定产出的结构。',92,601,1030,28,16,C.white,{bold:true,align:'center'});
 note(s,'本页为课堂讨论建议，结合周小闹的公开内容特征提炼。');
}

// 9 conclusion
{
 const s=pres.slides.add(); s.background.fill=C.navy;
 addText(s,'结论',72,86,180,42,28,C.yellow,{bold:true});
 addText(s,'周小闹的案例，说明剧情短视频的竞争力来自“内容结构”。',72,154,1040,48,32,C.white,{bold:true});
 addLine(s,74,240,1190,240,C.orange,4);
 addText(s,'固定场景让生产可持续，角色资产让观众记得住，连续冲突让内容能追更。',74,286,1000,44,24,'#D9E7E6');
 addText(s,'当账号规模扩大后，创作就从个人表演进入团队化生产阶段。',74,360,1000,40,24,'#D9E7E6');
 addRect(s,74,472,1090,94,'#314E59',18,'#314E59');
 addText(s,'参考来源',96,494,150,24,16,C.yellow,{bold:true});
 addText(s,'百度百科｜百度图片｜百度搜索公开报道（检索日期：2026-10-19）',96,530,920,24,15,'#D9E7E6');
 addText(s,'谢谢',74,622,180,40,24,C.white,{bold:true});
 note(s,'参考来源：百度百科“周小闹（娱乐类自媒体创作者）”“饭店寒假工”；百度图片搜索“周小闹”；百度搜索相关公开报道，检索日期 2026-10-19。');
}

for (const [i,slide] of pres.slides.items.entries()) {
  const png = await pres.export({slide, format:'png', scale:1});
  await fs.writeFile(path.join(buildDir, `slide-${String(i+1).padStart(2,'0')}.png`), new Uint8Array(await png.arrayBuffer()));
}
const pptx = await PresentationFile.exportPptx(pres);
await pptx.save(path.join(buildDir,'candidate.pptx'));
console.log('built', pres.slides.items.length);
