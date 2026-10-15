"""Compile/link saved scene shaders on the board's actual GLES driver; no rendering changes."""
import ctypes as c,glob,json,os,pathlib,sys
for p in glob.glob('/proc/[0-9]*/cmdline'):
    try:a=pathlib.Path(p).read_bytes().decode().split('\0')
    except (OSError,UnicodeError):continue
    if a and pathlib.Path(a[0]).name=='Xorg' and '-auth' in a:
        os.environ['DISPLAY']=':0';os.environ['XAUTHORITY']=a[a.index('-auth')+1];break
E=c.CDLL('libEGL.so.1');G=c.CDLL('libGLESv2.so.2')
def fn(lib,name,ret,args):
    f=getattr(lib,name);f.restype=ret;f.argtypes=args;return f
ptr=c.c_void_p; i=c.c_int; u=c.c_uint
getDisplay=fn(E,'eglGetDisplay',ptr,[ptr]);init=fn(E,'eglInitialize',u,[ptr,ptr,ptr])
display=getDisplay(None);assert init(display,None,None),'eglInitialize'
attrs=(i*11)(0x3033,1,0x3040,0x40,0x3024,8,0x3023,8,0x3022,8,0x3038)
config=ptr();count=i();assert fn(E,'eglChooseConfig',u,[ptr,ptr,ptr,i,ptr])(display,attrs,c.byref(config),1,c.byref(count)) and count.value
fn(E,'eglBindAPI',u,[u])(0x30a0)
context=fn(E,'eglCreateContext',ptr,[ptr,ptr,ptr,ptr])(display,config,None,(i*3)(0x3098,3,0x3038))
surface=fn(E,'eglCreatePbufferSurface',ptr,[ptr,ptr,ptr])(display,config,(i*5)(0x3057,8,0x3056,8,0x3038))
assert fn(E,'eglMakeCurrent',u,[ptr,ptr,ptr,ptr])(display,surface,surface,context)
renderer=fn(G,'glGetString',c.c_char_p,[u])(0x1f01).decode();assert 'Mali' in renderer,renderer
create=fn(G,'glCreateShader',u,[u]);source=fn(G,'glShaderSource',None,[u,i,ptr,ptr]);compile_=fn(G,'glCompileShader',None,[u])
get=fn(G,'glGetShaderiv',None,[u,u,ptr]);info=fn(G,'glGetShaderInfoLog',None,[u,i,ptr,ptr])
program=fn(G,'glCreateProgram',u,[])();report=[]
for suffix,kind in [('vert',0x8b31),('frag',0x8b30)]:
    data=pathlib.Path(sys.argv[1]+'.'+suffix).read_bytes();s=create(kind);p=c.c_char_p(data);source(s,1,c.byref(p),None);compile_(s)
    status=i();get(s,0x8b81,c.byref(status));buf=c.create_string_buffer(16384);info(s,len(buf),None,buf)
    report.append(dict(stage=suffix,compiled=bool(status.value),log=buf.value.decode()))
    fn(G,'glAttachShader',None,[u,u])(program,s)
fn(G,'glLinkProgram',None,[u])(program);status=i();fn(G,'glGetProgramiv',None,[u,u,ptr])(program,0x8b82,c.byref(status))
buf=c.create_string_buffer(16384);fn(G,'glGetProgramInfoLog',None,[u,i,ptr,ptr])(program,len(buf),None,buf)
print(json.dumps(dict(renderer=renderer,stages=report,linked=bool(status.value),linkLog=buf.value.decode())))
sys.exit(0 if status.value else 1)
