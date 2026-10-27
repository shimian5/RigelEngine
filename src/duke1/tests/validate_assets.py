#!/usr/bin/env python3
"""Pixel and metadata parity for the standalone C++ DOS-only asset viewer."""
import json, pathlib, shutil, subprocess, sys
from PIL import Image
binary, data_dir, research_root, output_dir = map(pathlib.Path, sys.argv[1:5])
sys.path.insert(0, str(research_root))
from dn1fmt import parse_tiles, parse_screen, parse_sounds, SCREENS
from dn1fmt.viewer import render_level, load_backdrop_compat
out=output_dir/'parity'; out.mkdir(parents=True,exist_ok=True)
def run(*args, ok=True):
    p=subprocess.run([str(binary),*map(str,args)],capture_output=True,text=True)
    if ok and p.returncode: raise AssertionError('command failed: '+p.stderr)
    if not ok and not p.returncode: raise AssertionError('command unexpectedly succeeded')
    return p
# The scene canvas is independently compared pixel for pixel on every original map.
levels=[f'WORLDAL{x}.DN1' for x in ('1','2','3','4','5','6','7','8','9','A','B','C')]
for name in levels:
    stem=pathlib.Path(name).stem; png=out/(stem+'.reference.png'); info=render_level(data_dir/name,png); ppm=out/(stem+'.ppm')
    p=run(data_dir,name,ppm,'--compat')
    raw=pathlib.Path(str(ppm)+'.rgba').read_bytes()
    if len(raw)!=2048*1440*4: raise AssertionError(f'{name}: wrong RGBA byte count')
    image=Image.frombytes('RGBA',(2048,1440),raw); x,y=info['map_pixel_origin'];w,h=info['map_pixel_size']
    got=image.crop((0,0,w,h)); expected=Image.open(png).convert('RGB').crop((x,y,x+w,y+h)).convert('RGBA')
    if got.tobytes()!=expected.tobytes():
        for i,(a,b) in enumerate(zip(got.tobytes(),expected.tobytes())):
            if a!=b: raise AssertionError(f'{name}: pixel mismatch at byte {i}, pixel {(i//4)%w,(i//4)//w}, channel {i%4}: {a} != {b}')
    if name=='WORLDAL2.DN1' and 'Flat backdrop' not in pathlib.Path(str(ppm)+'.meta').read_text(): raise AssertionError('WORLDAL2 must default to flat')
# Export decoded source assets through C++, then compare every pixel/metadata field independently.
assets=out/'assets';run('--inspect',data_dir,assets);manifest=json.loads((assets/'manifest.json').read_text())
# Require complete coverage, so a missing C++ export cannot make this pass silently.
expected_tiles=set(); expected_screens=set(); expected_sounds=set()
for source in data_dir.glob('*.DN1'):
    stem=source.stem.upper()
    if stem.startswith('WORLDAL'):continue
    if stem in SCREENS:expected_screens.add(source.name);continue
    if stem in ('DUKE1','DUKE1-B'):expected_sounds.add(source.name);continue
    if stem.startswith('DROP'):expected_tiles.add((source.name,'backdrop',None));continue
    for i in range(len(parse_tiles(source.read_bytes()).tiles)):expected_tiles.add((source.name,'tile',i))
actual_tiles={(e['source'],e['kind'],int(pathlib.Path(e['file']).stem.split('_')[-1]) if e['kind']=='tile' else None) for e in manifest['graphics']}
# Renderer export calls backdrop preview kind 'backdrop' (normalize only that descriptive tag).
actual_tiles={(name,'tile' if kind=='tile' else 'backdrop',i) for name,kind,i in actual_tiles}
assert actual_tiles==expected_tiles,('missing/extra exports',expected_tiles-actual_tiles,actual_tiles-expected_tiles)
assert {e['source'] for e in manifest['screens']}==expected_screens
assert {e['source'] for e in manifest['sounds']}==expected_sounds
assert len(manifest['graphics'])==len(actual_tiles),'duplicate image exports'
for entry in manifest['graphics']:
    src=data_dir/entry['source']; raw=(assets/entry['file']).read_bytes()
    if entry['kind']=='tile':
        n=int(pathlib.Path(entry['file']).stem.split('_')[-1]); expected=parse_tiles(src.read_bytes()).tiles[n].rgba()
    else: expected=load_backdrop_compat(src).image.tobytes()
    if raw!=expected: raise AssertionError(f"{entry['source']} {entry['file']}: RGBA mismatch")
for entry in manifest['screens']:
    expected=parse_screen((data_dir/entry['source']).read_bytes()).pixels()
    if (assets/entry['file']).read_bytes()!=expected: raise AssertionError(f"{entry['source']}: screen pixel mismatch")
for entry in manifest['sounds']:
    bank=parse_sounds((data_dir/entry['source']).read_bytes())
    if (entry['declared_size'],entry['count'],entry['unknown'])!=(bank.declared_size,bank.count,bank.unknown): raise AssertionError(f"{entry['source']}: sound bank header mismatch")
    if len(entry['sounds'])!=len(bank.sounds): raise AssertionError(f"{entry['source']}: sound count mismatch")
    for got,s in zip(entry['sounds'],bank.sounds):
        expected={'offset':s.offset,'priority':s.priority,'rate':s.rate,'name':s.name,'terminated':s.terminated,'divisors':list(s.divisors)}
        if got!=expected: raise AssertionError(f"{entry['source']}: sound metadata mismatch: {got} != {expected}")
# Case-insensitive matching works, while collisions and path traversal are rejected.
probe=out/'case.ppm';run(data_dir,'worldal1.dn1',probe,'--flat')
run(data_dir,'../DN1/WORLDAL1.DN1',probe,ok=False)
# Strict mode rejects the incomplete original DROP3; compatibility mode warns and paints only full tiles.
p=run(data_dir,'WORLDAL1.DN1',probe,ok=False)
p=run(data_dir,'WORLDAL1.DN1',probe,'--compat')
if 'DROP3 incomplete (shareware)' not in p.stderr: raise AssertionError('compat warning missing')
drop=load_backdrop_compat(data_dir/'DROP3.DN1'); exported=next(e for e in manifest['graphics'] if e['source']=='DROP3.DN1')
if (assets/exported['file']).read_bytes()!=drop.image.tobytes() or drop.complete_images!=51 or drop.ignored_partial_bytes==0: raise AssertionError('incomplete DROP3 compatibility pixels/metadata mismatch')
# Synthetic replacements retain the user EXE because backdrop metadata comes from it.
def copy_data(target):
    if target.exists():shutil.rmtree(target)
    target.mkdir(parents=True)
    for source in data_dir.glob('*.DN1'):shutil.copyfile(source,target/source.name)
    shutil.copyfile(data_dir/'DN1.EXE', target/'DN1.EXE')
# Same-name complete replacement must use ordinary strict parsing; no filename special case.
replacement=out/'replacement'; copy_data(replacement/'data'); shutil.copyfile(replacement/'data'/'DROP0.DN1',replacement/'data'/'DROP3.DN1')
run(replacement/'data','WORLDAL1.DN1',replacement/'full.ppm')
# Case collisions and symlink escapes fail before bytes can be consumed.
collision=out/'collision';copy_data(collision);shutil.copyfile(collision/'BACK0.DN1',collision/'back0.dn1')
run(collision,'WORLDAL1.DN1',out/'bad.ppm','--flat',ok=False)
symlink=out/'symlink';copy_data(symlink);(symlink/'BACK0.DN1').unlink();(symlink/'BACK0.DN1').symlink_to(data_dir/'BACK0.DN1')
run(symlink,'WORLDAL1.DN1',out/'badlink.ppm','--flat',ok=False)
print(f'asset parity passed: {len(levels)} maps, {len(manifest["graphics"])} tile/backdrop images, {len(manifest["screens"])} screens, {sum(x["count"] for x in manifest["sounds"])} sound slots')
