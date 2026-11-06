const { chromium } = require('C:/Users/嵌入式软件实习生电脑02/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
(async () => {
  const browser = await chromium.launch({headless:true, executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
  const page = await browser.newPage({viewport:{width:1440,height:1000}});
  const errors=[]; page.on('pageerror',e=>errors.push(String(e)));
  await page.goto('file:///D:/lwx/lwx-project/AD7682/output/reports/长期测试_1103-1104_BOOST/两块板5VBOOST长期测试报告_1103-1104.html',{waitUntil:'load'});
  await page.locator('img').evaluateAll(xs=>xs.forEach(x=>x.loading='eager'));
  await page.waitForFunction(()=>[...document.images].every(x=>x.complete&&x.naturalWidth>0));
  await page.screenshot({path:'D:/lwx/lwx-project/AD7682/output/reports/长期测试_1103-1104_BOOST/报告预览.png'});
  await page.locator('article').filter({has:page.locator('h3',{hasText:'模拟换算压力与同板数字气压计散点'})}).nth(0).screenshot({path:'D:/lwx/lwx-project/AD7682/output/reports/长期测试_1103-1104_BOOST/scatter_debug.png'});
  await page.locator('article').filter({has:page.locator('h3',{hasText:'模拟换算压力与同板数字气压计散点'})}).nth(1).screenshot({path:'D:/lwx/lwx-project/AD7682/output/reports/长期测试_1103-1104_BOOST/scatter_no4_debug.png'});
  console.log(JSON.stringify(await page.evaluate(()=>({images:document.images.length,broken:[...document.images].filter(x=>!x.naturalWidth).length,tables:document.querySelectorAll('table').length,overflow:document.documentElement.scrollWidth>innerWidth,sections:document.querySelectorAll('section').length}))),JSON.stringify(errors));
  await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
