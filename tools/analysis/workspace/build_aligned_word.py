from pathlib import Path
from docx import Document
from docx.shared import Cm,Pt,RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml.ns import qn

ROOT=Path('D:/lwx/lwx-project/AD7682/output/reports/Word曲线横轴对齐_20261023')
dest=ROOT/'10.22晚-10.23早-2板数据曲线_横坐标对齐.docx'
doc=Document()
sec=doc.sections[0]
sec.page_width=Cm(21);sec.page_height=Cm(29.7)
sec.left_margin=sec.right_margin=Cm(1.5)
sec.top_margin=sec.bottom_margin=Cm(1.5)
for name in ['Normal','Title','Heading 1']:
    style=doc.styles[name]
    style.font.name='Microsoft YaHei'
    style._element.get_or_add_rPr().rFonts.set(qn('w:eastAsia'),'Microsoft YaHei')
    style.font.color.rgb=RGBColor(0,0,0)
    style.font.size=Pt(10 if name=='Normal' else 17 if name=='Title' else 14)
    style.paragraph_format.space_after=Pt(6)
doc.styles['Normal'].paragraph_format.line_spacing=1.15
doc.add_paragraph('两块板过夜数据曲线',style='Title')
doc.add_paragraph('2026年10月22日晚至10月23日早。左侧为较早数据，右侧为较晚数据。所有图的时间范围、刻度、绘图区位置和图片宽度一致，可上下直接对照。')
doc.add_paragraph('同类图统一纵轴尺度：温度一组、湿度一组、四路模拟通道一组。使用日志存储值，未重新滤波、插值或补零。')

groups=[
 ('温度曲线',['01_两板温度','02_新板温度','03_老板温度']),
 ('湿度曲线',['04_两板湿度','05_新板湿度','06_老板湿度']),
 ('气压与新板模拟通道',['07_气压计对比','08_新板通道2','09_新板通道4']),
 ('老板模拟通道',['10_老板通道2','11_老板通道4'])
]
for gi,(title,files) in enumerate(groups):
    if gi: doc.add_page_break()
    doc.add_paragraph(title,style='Heading 1')
    if gi==2:
        doc.add_paragraph('老板 U11 全程无有效数据，气压图仅显示另外三个气压计。模拟通道统一使用IN1、IN3标注。')
    elif gi==3:
        doc.add_paragraph('本页与前页新板模拟通道采用相同的时间轴和ADC纵轴范围，可比较变化方向与幅度。')
    for filename in files:
        p=doc.add_paragraph()
        p.alignment=WD_ALIGN_PARAGRAPH.CENTER
        p.paragraph_format.space_after=Pt(9)
        p.add_run().add_picture(str(ROOT/(filename+'.png')),width=Cm(18))
for root in [doc.styles.element,doc._element]:
    for border in root.xpath('.//w:pBdr'):
        border.getparent().remove(border)
doc.save(dest)
assert len(doc.inline_shapes)==11
print(dest)
