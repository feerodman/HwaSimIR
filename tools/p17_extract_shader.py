import pathlib,re,subprocess
root=pathlib.Path(__file__).resolve().parents[1]
s=(root/'HwaSim_IR/HwaSim_IR/HwaSimIR.cpp').read_text(encoding='utf8')
s=s[s.index('void HwaSimIR::InitInfraredShader()'):s.index('void HwaSimIR::ApplyInfraredShader')]
out=root/'logs/p17/followup/inspection'
for name,ext in [('vertex_shader','vert'),('fragment_shader','frag')]:
    value=''.join(re.findall(name+r'\s*\+?=\s*R"\((.*?)\)"',s,re.S))
    assert value
    (out/('model_input.'+ext)).write_text(value.replace('#version 100','#version 100\n#define P1_M1_L1_L2_SHADER_FEATURES 1'),encoding='utf8')
subprocess.run([str(out/'shader_export.exe'),str(out/'model_input.vert'),str(out/'model_input.frag'),str(out/'model')],check=True)
whole=(root/'HwaSim_IR/HwaSim_IR/HwaSimIR.cpp').read_text(encoding='utf8')
version='#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler3D;\n#define P6D_CLOUD_MODE 0.0\n'
cloud=whole[whole.index('void HwaSimIR::InitStage7VolumetricCloudShader'):whole.index('#include "IR/P6CloudDensity.h"')]
for prefix,source,names in [('cloud',cloud,['vertexShader','fragmentShader']),('depth',whole,['depthVertex','depthFragment'])]:
    for ext,name in zip(['vert','frag'],names):
        match=re.search(name+r'\s*=\s*\w+\s*\+\s*R"\((.*?)\)"',source,re.S);assert match,name
        (out/(prefix+'_input.'+ext)).write_text(version+match[1],encoding='utf8')
    subprocess.run([str(out/'shader_export.exe'),str(out/(prefix+'_input.vert')),str(out/(prefix+'_input.frag')),str(out/prefix)],check=True)
