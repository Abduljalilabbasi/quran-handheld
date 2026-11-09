from pathlib import Path
import numpy as np
from PIL import Image, ImageOps, ImageDraw, ImageFont
from scipy import ndimage
import hashlib,json,shutil,sys,argparse

parser=argparse.ArgumentParser(description="Build the 604-page CrowPanel SD dataset from existing original PNGs.")
parser.add_argument('--source',type=Path,required=True,help='Directory containing 001.png through 604.png')
parser.add_argument('--output',type=Path,default=Path('build/crowpanel'))
parser.add_argument('--pages',type=int,nargs='*',help='Optional subset, for preview/testing')
args=parser.parse_args()
ROOT=args.output
SOURCE=args.source
OUT=ROOT/'SD_CARD/quran579';OUT.mkdir(parents=True,exist_ok=True)
PREVIEW=ROOT/'previews';PREVIEW.mkdir(exist_ok=True)

def convert(page):
 src=SOURCE/f'{page:03}.png'
 im=Image.open(src).convert('RGBA');bg=Image.new('RGBA',im.size,'white');bg.alpha_composite(im);gray=bg.convert('L')
 # First two source viewBoxes have exceptional surrounding whitespace.
 if page<=2:
  box=ImageOps.invert(gray).point(lambda v:255 if v>8 else 0).getbbox()
  l,t,r,b=box;gray=gray.crop((max(0,l-12),max(0,t-12),min(gray.width,r+12),min(gray.height,b+12)))
 width=780
 for attempt in range(12):
  height=round(gray.height*width/gray.width)
  ink=np.asarray(gray.resize((width,height),Image.Resampling.LANCZOS))<230
  labels,count=ndimage.label(ink,np.ones((3,3),dtype=np.uint8))
  sizes=np.bincount(labels.ravel());yy=np.indices(ink.shape)[0]
  cy=np.bincount(labels.ravel(),weights=yy.ravel())/np.maximum(sizes,1)
  # The normal source has 15 lines; aim at the whitespace after lines 3/6/9/12.
  fractions=[260/1244,504/1244,741/1244,990/1244] if page>2 else [.2,.4,.6,.8]
  cuts=[round(height*f) for f in fractions]
  group=np.searchsorted(cuts,cy,side='right');group[0]=-1
  # Assign boundary diacritics using nearby substantial glyph bodies, not
  # descender pixels. Slight upward offset models the normal mark position.
  objects=ndimage.find_objects(labels)
  majorids=np.flatnonzero(sizes>=max(70,int(150*(width/780)**2)))
  majorids=majorids[majorids!=0]
  left=np.array([objects[i-1][1].start for i in majorids]);right=np.array([objects[i-1][1].stop for i in majorids])
  original_spans=[]
  for k in range(5):
   boxes=[box for i,box in enumerate(objects,1) if group[i]==k]
   original_spans.append((min(b[0].start for b in boxes),max(b[0].stop for b in boxes)))
  for label,box in enumerate(objects,1):
   if sizes[label]<8 or sizes[label]>=max(70,int(150*(width/780)**2)) or min(abs(cy[label]-c) for c in cuts)>24*width/780:continue
   cx=(box[1].start+box[1].stop)/2
   dx=np.maximum(np.maximum(left-cx,cx-right),0)
   dy=cy[label]-(cy[majorids]-12*width/780)
   score=dx*dx+dy*dy
   parent=int(majorids[score.argmin()])
   owner=group[parent];lo,hi=original_spans[owner]
   if score.min()<(48*width/780)**2 and box[0].start>=lo-4 and box[0].stop<=hi+4:
    group[label]=owner
  grouped=group[labels]
  spans=[]
  for k in range(5):
   rows=np.where(np.any(grouped==k,axis=1))[0]
   assert len(rows),(page,k,'empty segment')
   spans.append((int(rows[0]),int(rows[-1])+1))
  maxheight=max(b-a for a,b in spans)
  if maxheight<=248:break
  width=min(width-1,int(width*248/maxheight)-1)
 else:raise RuntimeError((page,'cannot fit'))
 assert width>=600,(page,width)
 frames=[]; parts=[];reconstructed=np.zeros_like(ink)
 for k,(top,bottom) in enumerate(spans):
  mask=grouped==k
  assert not np.any(reconstructed & mask)
  reconstructed|=mask
  frame=np.zeros((272,792),dtype=np.uint8)
  x=(792-width)//2;y=24+(248-(bottom-top))//2
  frame[y:y+bottom-top,x:x+width]=mask[top:bottom]
  assert not frame[:24].any()
  raw=np.packbits(frame,axis=1,bitorder='big').tobytes()
  assert len(raw)==26928
  assert np.array_equal(np.unpackbits(np.frombuffer(raw,dtype=np.uint8).reshape(272,99),axis=1),frame)
  assert int(frame.sum())==int(mask.sum())
  filename=f'{page:03}_{k+1}.bin';(OUT/filename).write_bytes(raw)
  img=Image.fromarray((255-frame*255).astype(np.uint8)).convert('1');frames.append(img)
  parts.append(dict(file=filename,sha256=hashlib.sha256(raw).hexdigest(),ink_pixels=int(mask.sum()),source_top=top,source_bottom=bottom,offset=[x,y]))
 assert np.array_equal(reconstructed,ink)
 if page in [1,2,3,4,50,106,187,255,293,400,420,500,557,582,604] or width<750:
  sheet=Image.new('RGB',(812,5*304+10),'#eeeae1');d=ImageDraw.Draw(sheet)
  for k,img in enumerate(frames):
   d.text((10,10+k*304),f'Page {page} part {k+1}/5 - content width {width}',fill='black')
   screen=img.convert('RGB');sd=ImageDraw.Draw(screen)
   sd.text((4,4),f'{k+1}/{page}',font=(ImageFont.truetype('C:/Windows/Fonts/consola.ttf',18) if Path('C:/Windows/Fonts/consola.ttf').exists() else ImageFont.load_default()),fill='black')
   sheet.paste(screen,(10,32+k*304))
  sheet.save(PREVIEW/f'page{page:03}_all_parts.png')
 return dict(page=page,source_sha256=hashlib.sha256(src.read_bytes()).hexdigest(),scaled_size=[width,height],cuts=cuts,ink_pixels=int(ink.sum()),parts=parts)

if __name__=='__main__':
 from concurrent.futures import ProcessPoolExecutor
 pages=args.pages or list(range(1,605))
 results=[]
 with ProcessPoolExecutor(max_workers=6) as pool:
  for result in pool.map(convert,pages):
   results.append(result)
   if len(results)%25==0:print(f'Converted and verified {len(results)}/{len(pages)}',flush=True)
 (ROOT/('manifest.json' if len(pages)==604 else 'trial_manifest.json')).write_text(json.dumps(results,indent=2),encoding='utf-8')
 print('Widths:',min(r['scaled_size'][0] for r in results),max(r['scaled_size'][0] for r in results),flush=True)
 print('Below 750:',[(r['page'],r['scaled_size'][0]) for r in results if r['scaled_size'][0]<750],flush=True)



