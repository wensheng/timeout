import sys
import os.path
import base64


if len(sys.argv)!=2:
    exit("Usage: %s img_file"%sys.argv[0])

_, ext = os.path.splitext(sys.argv[1])
ext = ext[1:].lower()
if not ext in ('jpg', 'jpeg', 'gif', 'png', 'webp'):
    exit("Not a valid image file")

with open(sys.argv[1],'rb') as f:
    print("data:image/%s;base64,%s" % (ext, base64.b64encode(f.read()).decode()))
