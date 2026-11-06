const { chromium } = require('C:/Users/嵌入式软件实习生电脑02/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
(async () => {
  const browser = await chromium.launch({headless:true, executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
  const page = await browser.newPage({viewport:{width:1440,height:1000}});
  const errors=[]; page.on('pageerror',e=>errors.push(String(e)));
  await page.goto('file:///D:/lwx/lwx-project/AD7682/output/reports/长期测试_1104-1105_BOOST/两块板5VBOOST长期测试报告_1104-1105.html',{waitUntil:'load'});
  await page.locator('img').evaluateAll(xs=>xs.forEach(x=>x.loading='eager'));
  await page.waitForFunction(()=>[...document.images].every(x=>x.complete&&x.naturalWidth>0));
  await page.screenshot({path:'D:/lwx/lwx-project/AD7682/output/reports/长期测试_1104-1105_BOOST/报告预览.png'});
  const text=await page.locator('body').innerText();
  console.log(JSON.stringify(await page.evaluate(()=>({images:document.images.length,broken:[...document.images].filter(x=>!x.naturalWidth).length,tables:document.querySelectorAll('table').length,overflow:document.documentElement.scrollWidth>innerWidth,sections:document.querySelectorAll('section').length}))),JSON.stringify(errors));
  console.log('mapping',/传感器4号 IN0/.test(text),/传感器3号 IN2/.test(text),/传感器3号 IN0/.test(text),/传感器4号 IN2/.test(text));
  await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
