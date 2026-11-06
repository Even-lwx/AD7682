const {chromium}=require('C:/Users/嵌入式软件实习生电脑02/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
 const page=await browser.newPage({viewport:{width:1440,height:1000}});
 const errors=[];page.on('pageerror',e=>errors.push(String(e)));
 await page.goto('file:///D:/lwx/lwx-project/AD7682/output/reports/长期测试_1102-1103/两块板长期数据对比报告.html',{waitUntil:'load'});
 await page.locator('img').evaluateAll(imgs=>{for(const img of imgs)img.loading='eager'});
 await page.waitForFunction(()=>[...document.images].every(im=>im.complete&&im.naturalWidth>0));
 console.log(JSON.stringify(await page.evaluate(()=>({images:document.images.length,broken:[...document.images].filter(im=>!im.naturalWidth).length,titles:[...document.querySelectorAll('#cross h3')].map(e=>e.textContent),badNan:document.body.innerText.includes('nan'),horizontalOverflow:document.documentElement.scrollWidth>innerWidth})),null,2));
 await page.screenshot({path:'D:/lwx/lwx-project/AD7682/tools/analysis/workspace/syringe_reports/today_header.png'});
 for(const [label,filename] of [['两块板四个数字气压计同图对比','today_pressure.png'],['各路有效压力扣除5分钟局部基线后的波动','today_noise.png'],['传感器1号的历史波动对比','weekend_comparison.png']]){
  const article=page.locator('article').filter({has:page.locator('h3',{hasText:label})});await article.screenshot({path:'D:/lwx/lwx-project/AD7682/tools/analysis/workspace/syringe_reports/'+filename});
 }
 console.log(JSON.stringify({errors}));await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
