#/usr/bin/python3
import sys, os, glob
from pathlib import Path

iconsize = 16
resolution = 900
columns = 8
p = Path(__file__).parents[0]
f = open(p.joinpath('theme-variant.svg'), 'r')
themesvg = f.read()
f.close()
for folder in list(p.glob('*')):
    if folder.name != "application" and folder.is_dir():
        for folder2 in list(folder.glob('*')):
            if folder2.is_dir():
                themefile = p.joinpath(f"{folder.name}-{folder2.name}.svg")
                for folder3 in list(folder2.glob('*')):
                    if folder3.name == "actions":
                        out = ""
                        x = -1
                        y = 0
                        counter = -1
                        for img in sorted(list(folder3.glob('*.svg'))):
                            imgname = img.name
                            img = str(img.relative_to(p))
                            counter += 1
                            if counter%columns == 0:
                                y += iconsize + 10
                            x = (counter % columns) * (iconsize + 5)
                            out += f'''
                            <image 
                                xlink:href=".{os.sep}{img}" 
                                y="{y}" 
                                x="{x}" 
                                preserveAspectRatio="none" 
                                inkscape:svg-dpi="{resolution}" 
                                width="{iconsize}" 
                                height="{iconsize}" 
                                style="image-rendering:optimizeQuality" 
                                id="image_{x}_{y}" 
                            />
                            <text 
                                style="font-size:1.46667px;text-align:center;text-anchor:middle;white-space:pre;inline-size:13.3136;fill:#333333;stroke:none" 
                                x="{x+8}" 
                                y="{y+iconsize+3}" 
                                id="text_{x}_{y}"><tspan>{imgname}</tspan></text>
                            '''
                        if os.path.isfile(themefile):
                            os.remove(themefile)
                        f = open(themefile, 'w')
                        f.write(themesvg.replace("</g>",out + "</g>"))
                        f.close()
