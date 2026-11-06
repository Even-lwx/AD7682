from pathlib import Path
import zipfile
from PIL import Image, ImageOps, ImageDraw

out=Path('tools/analysis/workspace/word_chart_originals')
out.mkdir(parents=True,exist_ok=True)
src=Path('C:/Users/嵌入式软件实习生电脑02/Desktop/10.22晚-10.23早-2板数据曲线.docx')
with zipfile.ZipFile(src) as z:
    names=[n for n in z.namelist() if n.startswith('word/media/')]
    sheet=Image.new('RGB',(1500,330*len(names)), 'white')
    draw=ImageDraw.Draw(sheet)
    for i,name in enumerate(names):
        path=out/Path(name).name
        path.write_bytes(z.read(name))
        im=Image.open(path).convert('RGB')
        print(path,im.size)
        im.thumbnail((1450,295))
        sheet.paste(im,(25,i*330+30))
        draw.text((25,i*330+8),Path(name).name,fill='black')
    sheet.save(out/'contact.png')
