import os
import csv
import shutil
from pathlib import Path
from glob import glob

def load_mapping(csv_file):
    mapping = {}
    with open(csv_file, 'r', encoding='utf-8-sig') as f:
        reader = csv.DictReader(f)
        for row in reader:
            original_name = row['original_name'].strip()
            pinyin_name = row['pinyin_name'].strip()
            new_filename = row['new_filename'].strip()
            try:
                mass_kg = float(row['mass_kg'])
            except (ValueError, KeyError):
                mass_kg = 0.0
            mapping[original_name] = (pinyin_name, new_filename, mass_kg)
    return mapping

def process_single_csv(csv_path,body_name):
    identifier = csv_path.stem.split("_", 2)[-1]
    target_dir = Path(f"assets")
    output_xml = Path(f"output_{identifier}.xml")
    target_dir.mkdir(parents=True, exist_ok=True)
    name_mapping = load_mapping(csv_path)
    processed_data = []
    for orig_path in list(name_mapping.keys()):
        if not Path(orig_path).exists():
            print(f"\033[31m文件 {orig_path} 不存在\033[0m")
            continue
        
        pinyin_name, new_name, mass = name_mapping[orig_path]
        dest_path = target_dir / new_name
        
        try:
            shutil.copy2(orig_path, dest_path)
            print(f"\033[32m已复制：{orig_path} -> {dest_path}\033[0m")
        except Exception as e:
            print(f"\033[31m处理 {orig_path} 失败 - {str(e)}\033[0m")
            continue
        
        processed_data.append((pinyin_name, new_name, mass))
    
    mesh_output = [f'<mesh file="{fn}"/>' for _, fn, _ in processed_data]
    geom_output = [f'<geom type="mesh" mesh="{pn}" mass="{m:.4f}" class="{body_name}_default"/>' for pn, _, m in processed_data]
    
    with open(output_xml, "w", encoding='utf-8') as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        f.write("<mujoco>\n")
        f.write(f'<compiler meshdir="{target_dir.name}"/>\n')
        f.write("  <asset>\n")
        f.write("    " + "\n    ".join(mesh_output) + "\n")
        f.write("  </asset>\n\n")
        f.write("  <default>\n")
        f.write(f'  <default class="{body_name}_default">\n')
        f.write('  <geom contype="0" conaffinity="0"/>\n')
        f.write("  </default>\n")
        f.write("  </default>\n\n")
        f.write("  <worldbody>\n")
        f.write(f'  <body name="{body_name}_body" pos="0 0 0">\n')
        f.write("    " + "\n    ".join(geom_output) + "\n")
        f.write("  </body>\n")
        f.write("  </worldbody>\n")
        f.write("</mujoco>")
    
    return len(name_mapping), len(processed_data)

if __name__ == "__main__":
    csv_files = list(Path.cwd().glob("geom_data_*.csv"))
    
    if not csv_files:
        print("\033[33m未找到符合规则的CSV文件（命名格式：geom_data_xxx.csv）\033[0m")
        exit(1)
    
    total_files = total_success = total_failed = 0
    
    for csv_path in csv_files:
        print(f"\n{'='*40}")
        print(f"开始处理：{csv_path.name}")
        body_name = csv_path.name.replace("geom_data_", "").replace(".csv", "")
        
        try:
            total, success = process_single_csv(csv_path,body_name)
            failed = total - success
            total_files += total
            total_success += success
            total_failed += failed
            
            print(f"\n处理完成：{csv_path.name}")
            print(f"成功处理：{success}/{total}")
            print(f"输出文件：output_{csv_path.stem.split('_')[-1]}.xml")
        except Exception as e:
            print(f"处理 {csv_path} 时发生严重错误：{str(e)}")
            continue
        
        print(f"{'='*40}\n")
    
    print("\n全局操作统计：")
    print(f"总处理文件数：{len(csv_files)}")
    print(f"总条目数：{total_files}")
    print(f"总成功数：{total_success}")
    print(f"总失败数：{total_failed}")