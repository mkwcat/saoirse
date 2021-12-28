import sys, lzma

dol = open(sys.argv[1], "rb")
lzmadol = lzma.open(sys.argv[2], "wb", format=lzma.FORMAT_ALONE)

dol.seek(0, 2)
insize = dol.tell()
dol.seek(0, 0)

inbytes = dol.read(insize)
lzmadol.write(inbytes)
