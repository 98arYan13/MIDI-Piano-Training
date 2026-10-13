import sys,struct,json,time
mode=sys.argv[1]
def frame(v):
 b=json.dumps(v).encode();sys.stdout.buffer.write(struct.pack('<I',len(b))+b);sys.stdout.buffer.flush()
if mode=='hang': time.sleep(120)
if mode=='truncate': sys.stdout.buffer.write(b'\x0a\x00\x00\x00{}');sys.stdout.buffer.flush();sys.exit()
if mode=='oversize': sys.stdout.buffer.write(struct.pack('<I',256*1024*1024+1));sys.stdout.buffer.flush();sys.exit()
if mode=='stderr': sys.stderr.write('x'*100000);sys.stderr.flush()
if mode=='crash': sys.exit(2)
length=struct.unpack('<I',sys.stdin.buffer.read(4))[0]
v=json.loads(sys.stdin.buffer.read(length));frame(v)
