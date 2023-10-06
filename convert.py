import pickle, sys

if len(sys.argv) != 2:
   print("usage: python3 " + sys.argv[0] + " <file-name>", file = sys.stderr)
   exit(1)

input_name = sys.argv[1]
output_name = sys.argv[1].removesuffix(".pickle") + ".moon"

model = pickle.load(open(input_name, "rb"))

file = open(output_name, "wb")

file.write(b"\x00Lun")
file.write(model["ars"][0])
file.write(model["ars"][1])
file.write(model["ars"][3])
file.write(model["ars"][4])
file.write(model["ars"][5])
file.write(bytearray([int(model["scale"] * 256)]))

file.close()
